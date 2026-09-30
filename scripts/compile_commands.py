"""Generate a compilation database from the course Makefile's dry-run output.

Usage: uv run --locked python scripts/compile_commands.py code
The Makefile remains the only source of compiler flags and source membership.
"""

import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess


def collect_commands(lab):
    """Extract compile invocations without building or cleaning existing outputs."""
    output = subprocess.check_output(
        ["make", "-B", "-n", "--no-print-directory", "V=", "TARGETS"],
        cwd=lab, text=True,
    )
    commands = []
    for line in output.splitlines():
        args = shlex.split(line)
        if not args or "-c" not in args or "-o" not in args:
            continue
        compiler = shutil.which(args[0])
        if not compiler:
            raise RuntimeError(f"Compiler not found: {args[0]}")
        source = args[args.index("-c") + 1]
        if Path(source).suffix not in (".c", ".S"):
            continue
        args[0] = compiler
        commands.append({
            "directory": str(lab), "file": str(lab / source),
            "arguments": args, "output": str(lab / args[args.index("-o") + 1]),
        })
    if not commands:
        raise RuntimeError("No compile commands found; check the lab and toolchain.")
    return commands


def main():
    """Write machine-local absolute paths; the generated file is not versioned."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lab", type=Path, nargs="?", default=Path("code"))
    args = parser.parse_args()
    commands = collect_commands(args.lab.resolve())
    dest = Path(__file__).resolve().parents[1] / "compile_commands.json"
    dest.write_text(json.dumps(commands, indent=2) + "\n")
    print(f"Wrote {len(commands)} compilation commands to {dest}")


if __name__ == "__main__":
    main()
