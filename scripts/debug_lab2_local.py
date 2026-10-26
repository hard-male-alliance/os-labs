"""Reproduce Lab2 debugger observations using isolated local QEMU processes.

Python 3.10+, host GDB with Python, guest gdb-multiarch, QEMU 7.0.0.
Does not change the guest binary, the user's running debugger, or system settings.
"""
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import queue
import re
import signal
import socket
import subprocess
import threading
import time
import traceback


class Debugger:
    def __init__(self, executable, directory, name):
        self.raw = (directory / (name + '.mi.log')).open('w')
        self.log = (directory / (name + '.log')).open('w')
        self.events = queue.Queue()
        self.stops = collections.deque()
        self.console_chunks = []
        self.token = 0
        self.process = subprocess.Popen(
            [executable, '-q', '-nx', '--interpreter=mi2'],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.reader = threading.Thread(target=self._reader, daemon=True)
        self.reader.start()
        self.command('-gdb-set mi-async on')
        self.cli('set pagination off')
        self.cli('set confirm off')

    def _reader(self):
        for line in self.process.stdout:
            self.raw.write(line)
            self.raw.flush()
            if line[:1] in ('~', '@', '&'):
                try:
                    chunk = json.loads(line[1:].strip())
                    self.log.write(chunk)
                    self.console_chunks.append(chunk)
                except (ValueError, TypeError):
                    self.log.write(line)
            elif line.startswith('*stopped'):
                self.log.write(line)
            self.log.flush()
            self.events.put(line.strip())
        self.events.put('EOF')

    def command(self, command, timeout=25):
        self.token += 1
        token = str(self.token)
        self.log.write('(mi) ' + command + '\n')
        self.log.flush()
        self.process.stdin.write(token + command + '\n')
        self.process.stdin.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self.events.get(timeout=max(.01, deadline - time.monotonic()))
            if line == 'EOF':
                raise RuntimeError('Debugger exited: ' + command)
            if line.startswith('*stopped'):
                self.stops.append(line)
            if line.startswith(token + '^'):
                if line.startswith(token + '^error'):
                    raise RuntimeError(line)
                return line
        raise TimeoutError(command)

    def cli(self, command, timeout=25):
        return self.command('-interpreter-exec console ' + json.dumps(command), timeout)

    def capture(self, command):
        start = len(self.console_chunks)
        self.cli(command)
        return ''.join(self.console_chunks[start:])

    def evaluate(self, expression):
        result = self.command('-data-evaluate-expression ' + json.dumps(expression))
        match = re.search(r'value=("(?:[^"\\]|\\.)*")', result)
        if not match:
            raise RuntimeError(result)
        return json.loads(match[1])

    def number(self, expression):
        value = self.evaluate(expression)
        match = re.match(r'(?:\([^)]*\)\s*)?(-?0x[0-9a-fA-F]+|-?\d+)', value)
        if not match:
            raise RuntimeError(expression + ': ' + value)
        return int(match[1], 0) & ((1 << 64) - 1)

    def resume(self, command='continue'):
        self.stops.clear()
        self.cli(command)

    def wait(self, timeout=35):
        if self.stops:
            return self.stops.popleft()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self.events.get(timeout=max(.01, deadline - time.monotonic()))
            if line.startswith('*stopped'):
                return line
            if line == 'EOF':
                raise RuntimeError('Debugger exited while waiting')
        raise TimeoutError('Debugger stop')

    def step(self, command='stepi'):
        self.resume(command)
        return self.wait()

    def interrupt(self):
        # QEMU can block SIGINT while its guest is stopped in the GDB stub.
        # Stop only the native inferior owned by this GDB, never another QEMU.
        children = Path('/proc/%d/task/%d/children' %
                        (self.process.pid, self.process.pid)).read_text().split()
        inferiors = [int(pid) for pid in children
                     if b'qemu-system-riscv64' in Path('/proc/'+pid+'/cmdline').read_bytes()]
        if len(inferiors) != 1:
            raise RuntimeError('Cannot identify this debugger\'s QEMU child')
        os.kill(inferiors[0], signal.SIGSTOP)
        return self.wait()

    def close(self):
        if self.process.poll() is None:
            try:
                self.cli('kill', timeout=4)
            except Exception:
                pass
            try:
                self.command('-gdb-exit', timeout=4)
            except Exception:
                self.process.terminate()
            try:
                self.process.wait(timeout=4)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.reader.join(timeout=2)
        self.log.close()
        self.raw.close()


def unused_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def wait_listen(port):
    """Do not connect a readiness probe to the single-client GDB stub."""
    expected = '%04X' % port
    for _ in range(100):
        for line in Path('/proc/net/tcp').read_text().splitlines()[1:]:
            parts = line.split()
            if parts[1].split(':')[1] == expected and parts[3] == '0A':
                return
        time.sleep(.05)
    raise TimeoutError('QEMU GDB listener')


def qemu_args(root, directory, port):
    return ['-machine', 'virt', '-nographic', '-bios', 'default',
            '-kernel', str(root / 'code/bin/ucore.img'), '-S',
            '-gdb', 'tcp:127.0.0.1:%d' % port,
            '-serial', 'file:' + str(directory / 'serial.log'), '-monitor', 'none']


def connect_guest(root, directory, port, name='guest'):
    guest = Debugger('/usr/bin/gdb-multiarch', directory, name)
    guest.cli('file ' + str(root / 'code/bin/kernel'))
    guest.cli('target remote 127.0.0.1:%d' % port)
    guest.stops.clear()
    return guest


def check(results, name, condition, **details):
    row = dict(name=name, passed=bool(condition), **details)
    results.append(row)
    print(json.dumps(row, ensure_ascii=False), flush=True)
    if not condition:
        raise AssertionError(name)


def allocator_debug(root, output, results):
    directory = output / 'allocator'
    directory.mkdir(exist_ok=True)
    port = unused_port()
    child = subprocess.Popen(['/opt/qemu-7.0.0/bin/qemu-system-riscv64'] +
                             qemu_args(root, directory, port),
                             stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
    guest = None
    try:
        wait_listen(port)
        guest = connect_guest(root, directory, port)
        guest.cli('hbreak *kmalloc if $a0 == 1')
        guest.step('continue')
        guest.step('finish')
        obj = guest.number('$a0')
        page = obj & ~4095
        guest.cli('set $slab = (struct slab *)0x%x' % page)
        guest.cli('p *$slab')
        check(results, 'SLUB allocation', guest.number('$slab->used') == 1 and
              guest.number('$slab->npages') == 1 and
              guest.number('$slab->class') == 0 and
              guest.number('$slab->capacity') == 250,
              object=hex(obj), slab=hex(page), used=guest.number('$slab->used'))
        guest.cli('delete breakpoints')
        guest.cli('hbreak *kfree if (((unsigned long)$a0 & ~0xfffUL) == '
                  '(unsigned long)$slab) && ($slab->used == 1)')
        guest.step('continue')
        guest.cli('p *$slab')
        before = guest.number("'best_fit_pmm.c'::free_area.nr_free")
        guest.step('finish')
        after = guest.number("'best_fit_pmm.c'::free_area.nr_free")
        check(results, 'SLUB last-object page reclamation', after == before + 1,
              before=before, after=after, returned_pages=after-before)
        guest.cli('delete breakpoints')
        guest.cli('hbreak *allocator_checks_done')
        guest.step('continue')
        free = guest.number("'best_fit_pmm.c'::free_area.nr_free")
        check(results, 'Final allocator baseline', free == 31925,
              free_pages=free, pc=hex(guest.number('$pc')))
        guest.cli('p pmm_manager->name')
        guest.cli('info registers pc satp priv')
    finally:
        if guest:
            guest.close()
        child.terminate()
        try:
            child.wait(timeout=3)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait()


def mmu_debug(root, output, results):
    directory = output / 'mmu'
    directory.mkdir(exist_ok=True)
    port = unused_port()
    host = Debugger('/usr/bin/gdb', directory, 'host')
    guest = None
    try:
        host.cli('file /opt/qemu-7.0.0/bin/qemu-system-riscv64')
        host.cli('set args ' + ' '.join(qemu_args(root, directory, port)))
        host.cli('handle SIGPIPE nostop noprint pass')
        host.cli('handle SIGUSR1 nostop noprint pass')
        host.cli('handle SIGUSR2 nostop noprint pass')
        host.cli('handle SIGSTOP stop print nopass')
        host.cli('break riscv_cpu_tlb_fill if address == 0x80209000 && access_type == 1')
        host.resume('run')
        wait_listen(port)
        guest = connect_guest(root, directory, port)
        guest.cli('hbreak *0x80200008')
        guest.step('continue')
        guest.cli('info registers pc t0 a0 satp priv')
        guest.resume('stepi')
        print('BARE FILL STOP', host.wait(), flush=True)
        host.cli('bt 12')
        host.cli('info args')
        host.cli('p/x ((RISCVCPU *)cs)->env.satp')
        host.cli('ptype RISCVCPU')
        host.cli('break get_physical_address if addr == 0x80209000 && !is_debug')
        host.step('continue')
        host.cli('info args')
        host.cli('set $physical_out = physical')
        host.cli('p/x env->satp')
        host.step('until target/riscv/cpu_helper.c:848')
        host.cli('info locals')
        host.step('finish')
        physical = host.number('*$physical_out')
        check(results, 'Bare-mode identity translation', physical == 0x80209000,
              virtual=hex(0x80209000), physical=hex(physical))
        host.cli('break tlb_set_page_with_attrs if vaddr == 0x80209000')
        host.step('continue')
        host.cli('info args')
        host.cli('bt 8')
        check(results, 'Software TLB is used in Bare mode',
              host.number('vaddr') == host.number('paddr'),
              virtual_page=hex(host.number('vaddr')), physical_page=hex(host.number('paddr')))
        host.cli('disable breakpoints')
        host.resume()
        guest.wait()
        guest.cli('delete breakpoints')
        guest.cli('hbreak *get_memory_base')
        guest.step('continue')
        guest.cli('disassemble /r get_memory_base')
        guest.cli('info registers pc satp')
        guest.cli('delete breakpoints')
        guest.step()  # Execute the AUIPC that computes the global's base.
        instruction = guest.capture('x/i $pc')
        load = re.search(r'\bld\s+(\w+),(-?(?:0x[0-9a-f]+|\d+))\((\w+)\)', instruction)
        if not load:
            raise RuntimeError('Expected real get_memory_base LD: ' + instruction)
        destination, displacement, base_register = load.groups()
        load_pc = guest.number('$pc')
        saved_base = guest.number('$' + base_register)
        virtual = (saved_base + int(displacement, 0)) & ((1 << 64) - 1)
        physical_expected = (virtual - 0xffffffff40000000) & ((1 << 64) - 1)
        assembly = guest.capture('disassemble kern_entry')
        fences = re.findall(r'0x([0-9a-f]+)[^\n]*:\s*sfence\.vma', assembly)
        if not fences:
            raise RuntimeError('Missing existing SFENCE.VMA instruction')
        fence_pc = int(fences[-1], 16)

        # Reexecute only an existing SFENCE.VMA and the read-only LD. Preserve
        # the LD operand register. This deliberately cold-starts the software
        # TLB without editing instructions, page tables, SATP or kernel data.
        guest.cli('set $pc = 0x%x' % fence_pc)
        guest.step()
        guest.cli('set $pc = 0x%x' % load_pc)
        guest.cli('set $%s = 0x%x' % (base_register, saved_base))
        host.interrupt()
        host.cli('delete breakpoints')
        host.cli('break riscv_cpu_tlb_fill if address == 0x%x && access_type == 0' % virtual)
        host.cli('break get_physical_address if addr == 0x%x && !is_debug' % virtual)
        host.cli('break tlb_set_page_with_attrs if vaddr == 0x%x' % (virtual & ~4095))
        host.resume()
        guest.resume('stepi')
        print('SV39 FILL STOP', host.wait(), flush=True)
        host.cli('bt 16')
        host.cli('info args')
        host.cli('set $cpu = (RISCVCPU *)cs')

        def tlb_snapshot(label):
            mask = host.number('$cpu->neg.tlb.f[1].mask')
            index = (virtual >> 12) & (mask >> 5)
            entry = '$cpu->neg.tlb.f[1].table[%d]' % index
            host.cli('p/x ' + entry)
            snapshot = dict(label=label, index=index, mask=hex(mask),
                            addr_read=host.number(entry + '.addr_read'),
                            addend=host.number(entry + '.addend'))
            return snapshot

        before_fill = tlb_snapshot('before_first_load')
        check(results, 'Cold software TLB entry is not a read hit',
              (before_fill['addr_read'] & (0xfffffffffffff000 | 8)) != (virtual & ~4095),
              **before_fill)
        # Retain the real C slow-path frame and the generated host-code block.
        # QEMU's normal TLB-hit fast path is emitted host machine code, so a
        # breakpoint in a C helper alone cannot observe every successful load.
        frame_code = '''
original_frame = gdb.selected_frame()
frame = gdb.newest_frame()
while frame:
    if frame.name() == 'load_helper':
        frame.select()
        print('REAL C SOFTWARE-TLB LOOKUP FRAME')
        gdb.execute('info args')
        gdb.execute('info locals')
    if frame.name() == 'cpu_tb_exec':
        frame.select()
        gdb.set_convenience_variable('jit_start', gdb.parse_and_eval('itb->tc.ptr'))
        print('ACTUAL TRANSLATED HOST BLOCK', gdb.parse_and_eval('$jit_start'))
    frame = frame.older()
original_frame.select()
'''
        host.cli('python exec(' + json.dumps(frame_code) + ')')
        host.cli('disassemble /r $jit_start, (char *)$jit_start + 160')
        host.step('continue')
        host.cli('bt 8')
        host.cli('info args')
        host.cli('set $physical_out = physical')
        satp = host.number('env->satp')
        host.step('until target/riscv/cpu_helper.c:876')
        host.cli('info locals')
        root_pa = host.number('base')
        levels = host.number('levels')
        check(results, 'Sv39 configuration', satp >> 60 == 8 and levels == 3,
              satp=hex(satp), root_physical=hex(root_pa), levels=levels)
        host.step('until target/riscv/cpu_helper.c:923')
        host.cli('info locals')
        pte_address = host.number('pte_addr')
        index = host.number('idx')
        iteration = host.number('i')
        shift = host.number('ptshift')
        host.step('next')
        host.step('until target/riscv/cpu_helper.c:941')
        host.cli('info locals')
        pte = host.number('pte')
        check(results, 'Top-level superpage PTE', index == 511 and iteration == 0 and
              shift == 18 and pte_address == root_pa + 511 * 8 and pte == 0x200000cf,
              index=index, iteration=iteration, ptshift=shift,
              pte_physical=hex(pte_address), pte=hex(pte),
              flags=dict(V=bool(pte&1), R=bool(pte&2), W=bool(pte&4),
                         X=bool(pte&8), U=bool(pte&16), A=bool(pte&64), D=bool(pte&128)))
        host.step('until target/riscv/cpu_helper.c:1033')
        host.cli('info locals')
        host.step('finish')
        physical = host.number('*$physical_out')
        check(results, 'Real kernel LD virtual-to-physical translation',
              physical == physical_expected,
              instruction_pc=hex(load_pc), virtual=hex(virtual), physical=hex(physical),
              root_physical=hex(root_pa), mapping_size=1073741824)
        host.step('continue')
        host.cli('info args')
        check(results, 'Sv39 result enters software TLB',
              host.number('vaddr') == (virtual & ~4095) and
              host.number('paddr') == (physical & ~4095),
              virtual_page=hex(host.number('vaddr')), physical_page=hex(host.number('paddr')),
              software_tlb_fill_size=host.number('size'), prot=host.number('prot'))
        host.step('finish')
        after_fill = tlb_snapshot('after_first_fill')
        host_pointer = (virtual + after_fill['addend']) & ((1 << 64) - 1)
        ram_value = host.number('*(unsigned long *)0x%x' % host_pointer)
        check(results, 'Software TLB addend resolves host RAM',
              after_fill['addr_read'] == (virtual & ~4095) and ram_value == 0x80000000,
              host_pointer=hex(host_pointer), value=hex(ram_value), **after_fill)
        host.cli('delete breakpoints')
        count_code = '''
class CountFill(gdb.Breakpoint):
    def __init__(self):
        super().__init__('riscv_cpu_tlb_fill')
        self.condition = 'address == VIRTUAL && access_type == 0'
        self.count = 1
    def stop(self):
        if int(gdb.parse_and_eval('address')) != VIRTUAL or int(gdb.parse_and_eval('access_type')) != 0:
            return False
        self.count += 1
        print('OBSERVED additional data TLB fill', self.count)
        return False
class CountWalk(gdb.Breakpoint):
    def __init__(self):
        super().__init__('get_physical_address')
        self.condition = 'addr == VIRTUAL && !is_debug'
        self.count = 1
    def stop(self):
        candidate = int(gdb.parse_and_eval('addr'))
        debugger_access = bool(gdb.parse_and_eval('is_debug'))
        if candidate != VIRTUAL or debugger_access:
            if candidate == VIRTUAL:
                print('IGNORED debugger-only memory translation is_debug=true')
            return False
        self.count += 1
        print('OBSERVED additional real page walk', self.count)
        return False
fill_counter = CountFill()
walk_counter = CountWalk()
'''.replace('VIRTUAL', '0x%x' % virtual)
        host.cli('python exec(' + json.dumps(count_code) + ')')
        host.resume()
        guest.wait()
        first_value = guest.number('$' + destination)
        check(results, 'First real LD completes', first_value == 0x80000000,
              value=hex(first_value), pc=hex(guest.number('$pc')))

        guest.cli('set $pc = 0x%x' % load_pc)
        guest.cli('set $%s = 0x%x' % (base_register, saved_base))
        guest.step()
        second_value = guest.number('$' + destination)
        host.interrupt()
        host.cli('python gdb.set_convenience_variable("fill_hits", gdb.Value(fill_counter.count))')
        host.cli('python gdb.set_convenience_variable("walk_hits", gdb.Value(walk_counter.count))')
        second_fill_count = host.number('$fill_hits')
        second_walk_count = host.number('$walk_hits')
        after_second = tlb_snapshot('after_second_load')
        check(results, 'Repeated LD hits TLB without another page walk',
              second_value == first_value and second_fill_count == 1 and second_walk_count == 1 and
              after_second['addr_read'] == (virtual & ~4095),
              first_load_fills=1, second_load_fills=second_fill_count,
              page_walks=second_walk_count, value=hex(second_value), **after_second)
        host.resume()

        # Repeat after an actual guest SFENCE.VMA to prove invalidation, rather
        # than infer hits from unchanged data alone.
        guest.cli('set $pc = 0x%x' % fence_pc)
        guest.step()
        guest.cli('set $pc = 0x%x' % load_pc)
        guest.cli('set $%s = 0x%x' % (base_register, saved_base))
        guest.step()
        host.interrupt()
        host.cli('python gdb.set_convenience_variable("fill_hits", gdb.Value(fill_counter.count))')
        host.cli('python gdb.set_convenience_variable("walk_hits", gdb.Value(walk_counter.count))')
        check(results, 'SFENCE.VMA forces another fill and walk',
              host.number('$fill_hits') == 2 and host.number('$walk_hits') == 2,
              fills=host.number('$fill_hits'), walks=host.number('$walk_hits'),
              fence_instruction=hex(fence_pc))
        host.cli('disable breakpoints')
        host.resume()
        guest.cli('hbreak *allocator_checks_done')
        guest.step('continue')
        check(results, 'Kernel finishes after controlled MMU observations',
              guest.number("'best_fit_pmm.c'::free_area.nr_free") == 31925,
              free_pages=guest.number("'best_fit_pmm.c'::free_area.nr_free"))
    finally:
        if guest:
            guest.close()
        host.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=['allocator', 'mmu', 'all'], default='all')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / '.cache/local-debug-20261026'
    output.mkdir(parents=True, exist_ok=True)
    results = []
    result = dict(passed=False, checks=results,
                  image_sha256=hashlib.sha256((root/'code/bin/ucore.img').read_bytes()).hexdigest())
    try:
        if args.phase in ('allocator', 'all'):
            allocator_debug(root, output, results)
        if args.phase in ('mmu', 'all'):
            mmu_debug(root, output, results)
        result['passed'] = True
    except Exception:
        result['error'] = traceback.format_exc()
        print(result['error'], flush=True)
        raise
    finally:
        (output / (args.phase + '-result.json')).write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
