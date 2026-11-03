"""Boot normal and probe Lab3 kernels; check exception returns and real shutdown.

Run with the RISC-V/QEMU tools on PATH, e.g. inside docker/Dockerfile's image:
    python3 scripts/verify_lab3.py --output report/evidence
The extra guest sources only enter a disposable copy under .temp/.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT: Path = Path(__file__).resolve().parents[1]


def run(command: list[str], cwd: Path, log: Path, timeout: float | None = None) -> str:
    try:
        with log.open("w") as output:
            subprocess.run(command, cwd=cwd, stdout=output, stderr=subprocess.STDOUT,
                           timeout=timeout, check=True)
    except subprocess.SubprocessError:
        print(f"Command failed; output retained in {log}:\n{log.read_text()}", flush=True)
        raise
    return log.read_text()


def boot(tree: Path, log: Path) -> tuple[str, float]:
    start = time.monotonic()
    text = run(
        ["qemu-system-riscv64", "-machine", "virt", "-nographic", "-bios", "default",
         "-kernel", "bin/ucore.img", "-monitor", "none", "-no-reboot"],
        tree, log, timeout=25,
    )
    elapsed = time.monotonic() - start
    assert "check_alloc_page() succeeded!" in text, log
    assert text.splitlines().count("100 ticks") == 10, log
    assert "panic" not in text.lower(), log
    return text, elapsed


def prepare_probe(tree: Path) -> None:
    shutil.rmtree(tree, ignore_errors=True)
    shutil.copytree(ROOT / "code", tree, ignore=shutil.ignore_patterns("obj", "bin", ".qemu*"))
    guest = tree / "kern/test"
    guest.mkdir()
    for source in (ROOT / "scripts/lab3").iterdir():
        shutil.copy2(source, guest / source.name)
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(
        "KCFLAGS\t\t+=", "KINCLUDE += kern/test\nKSRCDIR += kern/test\n\nKCFLAGS\t\t+=", 1,
    ))
    init = tree / "kern/init/init.c"
    text = init.read_text()
    text = "void lab3_check_exceptions(void);\nvoid lab3_check_timer(void);\n" + text
    text = text.replace("    clock_init();", "    lab3_check_exceptions();\n    clock_init();", 1)
    # Keep the original enable statement and its comment intact.
    anchor = "    /* do nothing */"
    text = text.replace(anchor, "    lab3_check_timer();\n\n" + anchor, 1)
    init.write_text(text)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / ".temp/lab3-validation")
    args = parser.parse_args()
    output: Path = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    run(["make"], ROOT, output / "build.log")
    _, normal_time = boot(ROOT / "code", output / "boot.log")
    print("PASS: normal boot, allocator initialization, ten timer reports and QEMU shutdown")

    probe = ROOT / ".temp/lab3-probe"
    prepare_probe(probe)
    run(["make"], probe, output / "probe-build.log")
    text, probe_time = boot(probe, output / "probe.log")
    assert text.count("Breakpoint at") == 2
    assert text.count("Illegal instruction at") == 2
    assert "[Extra] 16/32-bit breakpoints and illegal instructions: PASS" in text
    timer_return = re.search(r"\[Extra\] Repeated timer return with SIE restored, ticks=(\d+): PASS", text)
    assert timer_return is not None and int(timer_return[1]) >= 200
    print("PASS: 16/32-bit exception EPC advance, preserved a0, disabled SIE and cleared sscratch")
    print("PASS: repeated timer return restores SIE; shutdown still occurs after 1000 ticks")

    sources = list((ROOT / "code").rglob("*.c")) + list((ROOT / "code").rglob("*.S"))
    sources += list((ROOT / "code").rglob("*.h"))
    sources += [ROOT / "code/Makefile", ROOT / "code/tools/kernel.ld"]
    sources += list((ROOT / "scripts/lab3").iterdir())
    sources += [Path(__file__).resolve(), ROOT / "scripts/lab3-timer.gdb", ROOT / "docker/Dockerfile"]
    manifest = {
        "normal_boot_seconds": round(normal_time, 3),
        "probe_boot_seconds": round(probe_time, 3),
        "timer_reports_per_boot": 10,
        "ticks_per_report": 100,
        "sources": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in sorted(sources)},
        "tools": {name: subprocess.check_output([name, "--version"], text=True).splitlines()[0]
                  for name in ("riscv64-unknown-elf-gcc", "qemu-system-riscv64", "gdb-multiarch")},
    }
    (output / "validation.json").write_text(json.dumps(manifest, indent=2) + "\n")


if __name__ == "__main__":
    main()
