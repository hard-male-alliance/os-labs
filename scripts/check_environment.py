"""Check course tools and optionally verify lab1's boot message in QEMU.

Usage: uv run --locked python scripts/check_environment.py code [--smoke]
No packages are installed, and no privileged commands are run.
"""

import argparse
from pathlib import Path
import shlex
import shutil
import subprocess
import sys


def make_value(lab, variable):
    """Ask the authoritative Makefile which executable it will use."""
    return subprocess.check_output(
        ["make", "--no-print-directory", "-s", f"print-{variable}"],
        cwd=lab, text=True,
    ).strip()


def check_tools(lab):
    """Report compiler, emulator and debugger availability independently."""
    missing = []
    for name in ("cc", "ld", "objcopy", "objdump", "qemu", "gdb"):
        command = make_value(lab, name)
        path = shutil.which(command)
        print(f"{name:8} {path or 'MISSING: ' + command}")
        if not path:
            missing.append(name)
    return missing


def smoke(lab):
    """Check the boot banner and clean up only the child process we started."""
    emulator = make_value(lab, "qemu")
    recipe = subprocess.check_output(
        ["make", "--no-print-directory", "-n", "V=", "qemu"],
        cwd=lab, text=True,
    ).replace("\\\n", " ")
    commands = [shlex.split(line) for line in recipe.splitlines()]
    command = next((args for args in commands if args and args[0] == emulator), None)
    if command is None:
        raise RuntimeError("No QEMU command found in the Makefile's qemu recipe.")
    process = subprocess.Popen(command, cwd=lab, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        output, _ = process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            output, _ = process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()
    cache = Path(__file__).resolve().parents[1] / ".cache" / lab.name
    cache.mkdir(parents=True, exist_ok=True)
    log = cache / "boot.log"
    log.write_bytes(output)
    if b"(THU.CST) os is loading ..." not in output:
        raise RuntimeError(f"Kernel boot banner not observed; inspect {log}")
    print(f"PASS: kernel boot banner observed; log: {log}")


def main():
    """Distinguish a missing dependency from a failed kernel boot."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lab", type=Path, nargs="?", default=Path("code"))
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()
    lab = args.lab.resolve()
    missing = check_tools(lab)
    if args.smoke:
        if "qemu" in missing:
            return 1
        smoke(lab)
    return int(bool(missing))


if __name__ == "__main__":
    sys.exit(main())
