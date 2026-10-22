"""Build and verify real Lab2 guests, retaining reproducible raw evidence.

Usage: python scripts/verify_lab2.py [code] [--timeout 45]
Each invocation owns its QEMU processes and private localhost GDB endpoint.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / '.cache' / 'lab2'
MARKERS = (
    'check_alloc_page() succeeded!',
    'allocator checks: best-fit shadow traces passed',
    'allocator checks: buddy rounding, shadow traces and coalescing passed',
    'allocator checks: slub boundaries, payloads, nonalias and reclamation passed',
    'allocator checks: all passed',
)


def run(command, cwd, timeout=45):
    return subprocess.run(command, cwd=cwd, text=True, capture_output=True,
                          timeout=timeout, check=True).stdout


def make_value(lab, name):
    return run(['make', '--no-print-directory', '-s', 'print-' + name], lab).strip()


def stop(child):
    if child.poll() is None:
        child.terminate()
    try:
        child.wait(timeout=3)
    except subprocess.TimeoutExpired:
        child.kill()
        child.wait(timeout=3)


def unused_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def boot(lab, command, memory, timeout):
    """Observe the completed journey, not merely an early boot banner."""
    path = CACHE / f'verify-ram-{memory}.log'
    with path.open('wb') as output:
        child = subprocess.Popen(command + ['-m', str(memory)], cwd=lab,
                                 stdin=subprocess.DEVNULL, stdout=output,
                                 stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                text = path.read_text(errors='replace')
                if all(marker in text for marker in MARKERS):
                    break
                if re.search(r'panic|\bFAIL\b', text, re.I) or child.poll() is not None:
                    break
                time.sleep(0.05)
        finally:
            stop(child)
    text = path.read_text(errors='replace')
    expected_end = min(0x80000000 + memory * 1024**2, 0x88000000)
    match = re.search(r'managed: \[0x([0-9a-f]+), 0x([0-9a-f]+)\), free starts at 0x([0-9a-f]+)', text)
    passed = bool(all(marker in text for marker in MARKERS)
                  and not re.search(r'panic|\bFAIL\b', text, re.I)
                  and match and int(match[1], 16) == 0x80000000
                  and int(match[2], 16) == expected_end)
    result = dict(memory_mib=memory, passed=passed, managed_end=expected_end,
                  log=str(path.relative_to(ROOT)))
    if match:
        result['free_start'] = int(match[3], 16)
        result['expected_free_pages'] = (expected_end - result['free_start']) // 4096
    return result


GDB_PROGRAM = r'''
import gdb, json, traceback
result = {'checks': []}
def val(expression):
    return int(gdb.parse_and_eval(expression)) & ((1 << 64) - 1)
def check(name, condition, **data):
    result['checks'].append(dict(name=name, passed=bool(condition), **data))
    print('CHECK', name, bool(condition), data)
    if not condition:
        raise RuntimeError(name)
def reach(symbol):
    address = val('&' + symbol)
    breakpoint = gdb.Breakpoint('*0x%x' % address, type=gdb.BP_HARDWARE_BREAKPOINT, temporary=True)
    gdb.execute('continue')
    check('reached_' + symbol, val('$pc') == address, pc=val('$pc'))
try:
    reach('kern_init')
    check('high_virtual_C_entry', val('$pc') >= 0xffffffffc0200000, pc=val('$pc'))
    check('Sv39_at_entry', val('$satp') >> 60 == 8, satp=val('$satp'))
    check('entry_stack', val('&bootstack') < val('$sp') <= val('&bootstacktop') and val('$sp') % 16 == 0,
          sp=val('$sp'), bottom=val('&bootstack'), top=val('&bootstacktop'))
    reach('allocator_checks_done')
    check('supervisor_mode', val('$priv') == 1, privilege=val('$priv'))
    check('Sv39_after_checks', val('$satp') >> 60 == 8, satp=val('$satp'))
    root = gdb.parse_and_eval('(unsigned long *)&boot_page_table_sv39')
    check('identity_mapping_removed', int(root[2]) == 0, root_2=int(root[2]))
    expected_leaf = (0x80000 << 10) | 0xcf
    check('high_mapping_preserved', int(root[511]) == expected_leaf,
          root_511=int(root[511]), expected=expected_leaf)
    check('satp_root_matches', val('$satp') & ((1 << 44) - 1) == val('satp_physical') // 4096,
          satp=val('$satp'), root_physical=val('satp_physical'))
    check('physical_virtual_offset', val('va_pa_offset') == 0xffffffff40000000, offset=val('va_pa_offset'))
    check('managed_page_bound', val('npage') == 0x88000000 // 4096, npage=val('npage'))
    manager = gdb.parse_and_eval('pmm_manager->name').string()
    check('best_fit_selected', manager == 'best_fit_pmm_manager', manager=manager)
    free_count = val("'best_fit_pmm.c'::free_area.nr_free")
    check('free_pages_restored', free_count == EXPECTED_FREE, free=free_count, expected=EXPECTED_FREE)
    head = val("&'best_fit_pmm.c'::free_area.free_list")
    node = val("'best_fit_pmm.c'::free_area.free_list.next")
    total, blocks, previous_end = 0, 0, None
    while node != head:
        check('free_list_bounded', blocks < val('npage'), blocks=blocks)
        page = gdb.parse_and_eval('(struct Page *)(%d - (unsigned long)&((struct Page *)0)->page_link)' % node)
        address = int(page)
        count = int(page.dereference()['property'])
        flags = int(page.dereference()['flags'])
        check('free_block_invariants', count > 0 and flags & 2 and not flags & 1 and
              (previous_end is None or previous_end < address), address=address, pages=count)
        previous_end = address + count * gdb.lookup_type('struct Page').sizeof
        total += count
        blocks += 1
        node = int(gdb.parse_and_eval('((list_entry_t *)%d)->next' % node))
    check('free_list_accounting', total == free_count and blocks > 0, total=total, free=free_count, blocks=blocks)
    result['passed'] = True
except Exception:
    result['passed'] = False
    result['error'] = traceback.format_exc()
    print(result['error'])
finally:
    pathlib = __import__('pathlib')
    pathlib.Path(RESULT).write_text(json.dumps(result, indent=2) + '\n')
if not result['passed']:
    gdb.execute('quit 1')
'''


def debug(lab, command, debugger, expected_free, timeout):
    port = unused_port()
    result_path = CACHE / 'verify-gdb-result.json'
    result_path.write_text(json.dumps(dict(passed=False, error='GDB not completed')))
    script = CACHE / 'verify-commands.gdb'
    script.write_text('set pagination off\nset confirm off\nset architecture riscv:rv64\n'
                      'file bin/kernel\nset remotetimeout 10\n'
                      f'target remote 127.0.0.1:{port}\npython\n'
                      f'RESULT = {str(result_path)!r}\nEXPECTED_FREE = {expected_free}\n' +
                      GDB_PROGRAM + '\nend\ndetach\nquit\n')
    qemu = command + ['-m', '128', '-S', '-gdb', f'tcp:127.0.0.1:{port}']
    with (CACHE / 'verify-gdb-qemu.log').open('wb') as output:
        child = subprocess.Popen(qemu, cwd=lab, stdin=subprocess.DEVNULL,
                                 stdout=output, stderr=subprocess.STDOUT)
        try:
            time.sleep(0.3)
            if child.poll() is not None:
                raise RuntimeError('Private QEMU exited before GDB attach')
            args = shlex.split(debugger) + ['-q', '-nx', '--batch', '-x', str(script)]
            try:
                completed = subprocess.run(args, cwd=lab, text=True, stdout=subprocess.PIPE,
                                           stderr=subprocess.STDOUT, timeout=timeout)
            except subprocess.TimeoutExpired as error:
                (CACHE / 'verify-gdb.log').write_bytes(error.stdout or b'')
                raise RuntimeError('GDB timed out; inspect .cache/lab2/verify-gdb.log') from error
            (CACHE / 'verify-gdb.log').write_text(completed.stdout)
        finally:
            stop(child)
    result = json.loads(result_path.read_text())
    result['passed'] = bool(result.get('passed') and completed.returncode == 0)
    return result


def panels(result):
    directory = CACHE / 'panels'
    directory.mkdir(exist_ok=True)
    matrix = ['Lab2 RAM matrix: recorded real QEMU output', '']
    for row in result['ram_matrix']:
        matrix.append(f"RAM {row['memory_mib']:3} MiB: {'PASS' if row['passed'] else 'FAIL'}; managed end 0x{row['managed_end']:x}")
    matrix.extend(['', 'Kernel image SHA-256: ' + result['image_sha256'],
                   'Each guest: raw ucore.img via QEMU -kernel, default OpenSBI.',
                   '256 MiB RAM is deliberately capped at PHYSICAL_MEMORY_END (128 MiB).'])
    (directory / 'lab2-ram-matrix.txt').write_text('\n'.join(matrix) + '\n')
    log = (CACHE / 'verify-ram-128.log').read_text(errors='replace')
    selected = [line for line in log.splitlines() if any(word in line for word in
                ('memory management:', 'memory:', 'managed:', 'succeeded', 'passed', 'LAB2'))]
    (directory / 'lab2-allocator-checks.txt').write_text('Lab2 allocator checks: recorded real QEMU output\n\n' + '\n'.join(selected) + '\n')
    checks = ['Lab2 GDB assertions: recorded real debugger output', '']
    checks += [line for line in (CACHE / 'verify-gdb.log').read_text().splitlines() if line.startswith('CHECK')]
    (directory / 'lab2-gdb.txt').write_text('\n'.join(checks) + '\n')


def verify(lab, timeout):
    CACHE.mkdir(parents=True, exist_ok=True)
    result_path = CACHE / 'verify-result.json'
    result = dict(passed=False, ram_matrix=[])
    result_path.write_text(json.dumps(result))
    panel_root = CACHE / 'panels'
    for name in ('lab2-ram-matrix.txt', 'lab2-allocator-checks.txt', 'lab2-gdb.txt'):
        (panel_root / name).unlink(missing_ok=True)
    try:
        build = subprocess.run(['make', '--no-print-directory', 'TARGETS'], cwd=lab,
                               text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=timeout)
        (CACHE / 'verify-build.log').write_text(build.stdout)
        build.check_returncode()
        command = shlex.split(make_value(lab, 'qemu')) + [
            '-machine', 'virt', '-nographic', '-bios', 'default', '-kernel', 'bin/ucore.img']
        result['qemu_command'] = command
        result['image_sha256'] = hashlib.sha256((lab / 'bin/ucore.img').read_bytes()).hexdigest()
        for memory in (32, 64, 128, 256):
            row = boot(lab, command, memory, timeout)
            result['ram_matrix'].append(row)
            print(f"RAM {memory} MiB: {'PASS' if row['passed'] else 'FAIL'}", flush=True)
            if not row['passed']:
                raise RuntimeError(f'RAM {memory} MiB failed; inspect {row["log"]}')
        expected = result['ram_matrix'][2]['expected_free_pages']
        result['gdb'] = debug(lab, command, make_value(lab, 'gdb'), expected, timeout)
        if not result['gdb']['passed']:
            raise RuntimeError('GDB assertions failed; inspect verify-gdb.log')
        result['passed'] = True
        panels(result)
        print(f"PASS: RAM matrix and {len(result['gdb']['checks'])} real guest GDB assertions; evidence: {CACHE}")
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        result_path.write_text(json.dumps(result, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('lab', type=Path, nargs='?', default=ROOT / 'code')
    parser.add_argument('--timeout', type=float, default=45)
    args = parser.parse_args()
    try:
        if args.timeout <= 0:
            raise ValueError('--timeout must be positive')
        verify(args.lab.resolve(), args.timeout)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print('FAIL:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
