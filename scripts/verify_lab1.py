"""Verify Lab1 on a private QEMU instance without modifying kernel sources.

Usage: uv run --locked python scripts/verify_lab1.py [code] [--timeout 45]
Evidence is overwritten in .cache/lab1/verify-*. Failure exits nonzero.
The BSS experiment deliberately changes guest RAM only, never the boot image.
"""

import argparse
import json
from pathlib import Path
import shlex
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def run(command, cwd, timeout=30):
    """Run a bounded command and propagate failed exit codes."""
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True,
                          timeout=timeout, check=True).stdout


def make_value(lab, name):
    """Resolve tool and flag overrides through the course Makefile."""
    return run(['make', '--no-print-directory', '-s', 'print-' + name], lab).strip()


def debug_command(recipe, executable):
    """Extract the actual debug recipe, joining Make's continued shell lines."""
    for line in recipe.replace('\\\n', ' ').splitlines():
        args = shlex.split(line)
        if args and args[0] == executable:
            if '-S' not in args or '-gdb' not in args:
                raise RuntimeError('Debug recipe must pause QEMU and provide GDB.')
            return args
    raise RuntimeError('No QEMU command found in Make debug recipe.')


def unused_port():
    """Select an ephemeral loopback port; QEMU bind failure is not hidden."""
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def stop_child(child):
    """Terminate only the emulator owned by this invocation, with bounded waits."""
    if child.poll() is None:
        child.terminate()
        try:
            child.wait(timeout=3)
        except subprocess.TimeoutExpired:
            child.kill()
            child.wait(timeout=3)


GDB_PROGRAM = r'''
import gdb, json, pathlib, traceback
result = {'checks': [], 'reset_steps': []}
def value(expr):
    """Read a register or symbolic expression as an integer."""
    return int(gdb.parse_and_eval(expr))
def check(name, condition, **data):
    """Record an assertion before raising, preserving negative evidence."""
    result['checks'].append(dict(name=name, passed=bool(condition), **data))
    print('CHECK', name, condition, data)
    if not condition:
        raise RuntimeError(name)
def step():
    """Execute exactly one guest instruction."""
    gdb.execute('stepi')
def reach(address):
    """Use an address breakpoint rather than optimized source-line mappings."""
    gdb.execute('tbreak *0x%x' % address)
    gdb.execute('continue')
    check('reached_0x%x' % address, value('$pc') == address, pc=value('$pc'))
def instructions(symbol):
    """Decode actual executable instructions, including compressed encodings."""
    text = gdb.execute('disassemble ' + symbol, to_string=True)
    print(text)
    import re
    addresses = [int(a, 16) for a in re.findall(r'(0x[0-9a-f]+)\s+<[^>]+>:', text)]
    if not addresses:
        raise RuntimeError('No instructions for ' + symbol)
    return gdb.selected_frame().architecture().disassemble(addresses[0], addresses[-1])
try:
    inf = gdb.selected_inferior()
    arch = gdb.selected_frame().architecture()
    entry = value('&kern_entry')
    init = value('&kern_init')
    check('reset_pc', value('$pc') == 0x1000, pc=value('$pc'))
    check('reset_privilege_M', value('$priv') == 3, privilege=value('$priv'))
    gdb.execute('x/6i 0x1000')
    gdb.execute('x/4gx 0x1018')
    image = pathlib.Path(IMAGE).read_bytes()
    check('image_loaded_before_firmware', bytes(inf.read_memory(entry, len(image))) == image,
          bytes=len(image), address=entry)
    for index in range(6):
        pc = value('$pc')
        op = arch.disassemble(pc, count=1)[0]
        raw = bytes(inf.read_memory(pc, op['length'])).hex()
        step()
        record = dict(index=index, pc=pc, instruction=op['asm'], opcode=raw,
                      next_pc=value('$pc'), a0=value('$a0'), a1=value('$a1'),
                      a2=value('$a2'), t0=value('$t0'))
        result['reset_steps'].append(record)
        print('RESET', record)
    check('reset_to_firmware', value('$pc') == 0x80000000, pc=value('$pc'))
    reach(entry)
    check('kernel_entry_address', entry == 0x80200000, entry=entry)
    check('kernel_privilege_S', value('$priv') == 1, privilege=value('$priv'))
    check('bare_translation', value('$satp') == 0, satp=value('$satp'))
    gdb.execute('info registers pc sp ra a0 a1 a2 satp priv')
    result['kernel_handoff'] = dict(a0=value('$a0'), a1=value('$a1'), a2=value('$a2'),
                                    sp=value('$sp'), ra=value('$ra'))
    ops = instructions('kern_entry')
    ra = value('$ra')
    top, bottom = value('&bootstacktop'), value('&bootstack')
    for op in ops:
        check('entry_instruction_boundary', value('$pc') == op['addr'], instruction=op['asm'])
        step()
        if value('$pc') == init:
            break
    check('tail_reaches_C_entry', value('$pc') == init, pc=value('$pc'))
    check('tail_preserves_ra', value('$ra') == ra, ra=ra)
    check('stack_pointer', value('$sp') == top, sp=value('$sp'), top=top)
    check('stack_size_alignment', top-bottom == EXPECTED_STACK and top % 16 == 0,
          size=top-bottom, expected=EXPECTED_STACK, bottom=bottom, top=top)
    start, end = value('&edata'), value('&end')
    check('BSS_range', end >= start, start=start, end=end, size=end-start)
    if end > start:
        inf.write_memory(start, b'\xa5' * (end-start))
    if end > start:
        check('BSS_poisoned', bytes(inf.read_memory(start, end-start)) == b'\xa5' * (end-start), size=end-start)
    else:
        result['BSS_experiment'] = 'Empty BSS: no real bytes to poison or clear.'
    reach(value('&memset'))
    check('BSS_memset_arguments', value('$a0') == start and value('$a1') == 0 and
          value('$a2') == end-start, a0=value('$a0'), a1=value('$a1'), a2=value('$a2'))
    return_address = value('$ra')
    reach(return_address)
    if end == start:
        # The original zero-count call completed; now run a separate synthetic guest call.
        saved = bytes(inf.read_memory(end, 64))
        registers = ['a%d' % i for i in range(8)] + ['t%d' % i for i in range(7)] + ['ra', 'sp']
        saved_registers = {name: value('$' + name) for name in registers}
        inf.write_memory(end, b'\xa5' * 64)
        gdb.execute('set $pc = &memset')
        gdb.invalidate_cached_frames()
        gdb.newest_frame().select()
        gdb.execute('set $ra = %d' % return_address)
        gdb.execute('set $a0 = %d' % end)
        gdb.execute('set $a1 = 0')
        gdb.execute('set $a2 = 64')
        result['synthetic_clear_probe'] = dict(address=end, size=64, original_size=0)
        reach(return_address)
    if end == start:
        check('synthetic_clear_probe', bytes(inf.read_memory(end, 64)) == bytes(64), size=64)
        inf.write_memory(end, saved)
        for name, original in saved_registers.items():
            gdb.execute('set $%s = %d' % (name, original))
        check('synthetic_probe_restores_state', all(value('$' + name) == original
              for name, original in saved_registers.items()) and bytes(inf.read_memory(end, 64)) == saved)
    if end > start:
        check('BSS_cleared', bytes(inf.read_memory(start, end-start)) == bytes(end-start), size=end-start)
    reach(value('&cprintf'))
    printf_return = value('$ra')
    check('printf_stack_alignment', value('$sp') % 16 == 0, sp=value('$sp'))
    ops = instructions('sbi_console_putchar')
    ecalls = [op for op in ops if op['asm'].split()[0] == 'ecall']
    check('console_has_ecall', len(ecalls) == 1)
    reach(ecalls[0]['addr'])
    check('legacy_console_SBI', value('$a7') == 1 and value('$a0') == ord('('),
          extension=value('$a7'), character=value('$a0'), privilege=value('$priv'))
    # Direct stepi may stop after the SBI trap/return; catch actual mtvec entry.
    reach(value('$mtvec') & ~3)
    check('ecall_enters_M', value('$priv') == 3 and value('$mcause') == 9 and
          value('$mepc') == ecalls[0]['addr'],
          privilege=value('$priv'), mcause=value('$mcause'), mepc=value('$mepc'), pc=value('$pc'))
    reach(printf_return)
    check('printf_returns_S', value('$priv') == 1, privilege=value('$priv'))
    result['passed'] = True
except Exception:
    result['passed'] = False
    result['error'] = traceback.format_exc()
    print(result['error'])
finally:
    pathlib.Path(RESULT).write_text(json.dumps(result, indent=2) + '\n')
if not result['passed']:
    gdb.execute('quit 1')
'''


def verify(lab, timeout):
    """Build, inspect boot state, and retain logs even if a child fails."""
    cache = ROOT / '.cache' / 'lab1'
    cache.mkdir(parents=True, exist_ok=True)
    result_path = cache / 'verify-result.json'
    result_path.write_text(json.dumps({'passed': False, 'error': 'Verification not completed'}) + '\n')
    tools = {name: make_value(lab, name) for name in ('cc', 'gdb', 'qemu', 'cflags', 'kcflags')}
    (cache / 'verify-build.log').write_text(run(['make', '--no-print-directory', 'TARGETS'], lab, timeout))
    # Evaluate stack macros with the selected compiler and exact course flags.
    command = shlex.split(tools['cc']) + shlex.split(tools['cflags'] + ' ' + tools['kcflags'])
    macro = subprocess.run(command + ['-E', '-P', '-x', 'c', '-'], cwd=lab,
                           input='#include <mmu.h>\n#include <memlayout.h>\nKSTACKSIZE\n', text=True,
                           capture_output=True, check=True, timeout=30).stdout.strip().splitlines()[-1]
    # GCC emits a numeric arithmetic expression; GDB evaluates it, not host Python eval.
    port = unused_port()
    recipe = run(['make', '--no-print-directory', '-n', 'V=', 'GDB_PORT=' + str(port), 'debug'], lab)
    qemu = debug_command(recipe, tools['qemu'])
    if qemu[qemu.index('-gdb') + 1] != 'tcp:127.0.0.1:' + str(port):
        raise RuntimeError('Debug endpoint is not the selected private loopback port.')
    script = cache / 'verify-commands.gdb'
    script.write_text('set pagination off\nset confirm off\nset architecture riscv:rv64\n'
                      'file bin/kernel\nset remotetimeout 10\ntarget remote 127.0.0.1:%d\npython\n' % port +
                      'IMAGE = %r\nRESULT = %r\nEXPECTED_STACK = int(gdb.parse_and_eval(%r))\n' %
                      (str(lab / 'bin/ucore.img'), str(result_path), macro) + GDB_PROGRAM +
                      '\nend\ndetach\nquit\n')
    gdb_command = shlex.split(tools['gdb']) + ['-q', '-nx', '--batch', '-x', str(script)]
    (cache / 'verify-tools.json').write_text(json.dumps(dict(tools=tools, qemu=qemu,
                                                          gdb=gdb_command, stack_expression=macro), indent=2) + '\n')
    with (cache / 'verify-qemu.log').open('wb') as output:
        child = subprocess.Popen(qemu, cwd=lab, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT)
        try:
            # Do not connect a readiness probe: it would consume QEMU's GDB client slot.
            time.sleep(0.3)
            if child.poll() is not None:
                raise RuntimeError('QEMU exited before GDB attach; inspect verify-qemu.log.')
            try:
                debug = subprocess.run(gdb_command, cwd=lab, text=True, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, timeout=timeout)
                (cache / 'verify-gdb.log').write_text(debug.stdout)
            except subprocess.TimeoutExpired as error:
                (cache / 'verify-gdb.log').write_bytes(error.stdout or b'')
                raise RuntimeError('GDB timed out; inspect verify-gdb.log.') from error
        finally:
            stop_child(child)
    result = json.loads(result_path.read_text())
    banner = '(THU.CST) os is loading ...' in (cache / 'verify-qemu.log').read_text(errors='replace')
    result['checks'] = result.get('checks', []) + [dict(name='kernel_boot_banner', passed=banner)]
    result['passed'] = bool(result.get('passed') and banner and debug.returncode == 0)
    result_path.write_text(json.dumps(result, indent=2) + '\n')
    if not result['passed']:
        raise RuntimeError('Lab1 assertions failed; inspect ' + str(result_path))
    print('PASS: %d checks; evidence: %s' % (len(result['checks']), cache))


def main():
    """Provide a bounded CLI with truthful nonzero failure status."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('lab', nargs='?', type=Path, default=ROOT / 'code')
    parser.add_argument('--timeout', type=float, default=45)
    args = parser.parse_args()
    try:
        if args.timeout <= 0:
            raise ValueError('--timeout must be positive')
        verify(args.lab.resolve(), args.timeout)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        result_path = ROOT / '.cache' / 'lab1' / 'verify-result.json'
        if result_path.exists():
            result = json.loads(result_path.read_text())
            result.update(passed=False, error=str(error))
            result_path.write_text(json.dumps(result, indent=2) + '\n')
        print('FAIL:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
