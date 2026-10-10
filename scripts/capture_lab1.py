"""Capture real XTerm windows displaying unmodified Lab1 evidence panels.

Usage: uv run --locked python scripts/capture_lab1.py
Requires optional host tools Xvfb, xterm, xwininfo and ImageMagick import.
Input: .cache/lab1/panels/*.txt or report/evidence/panels/*.txt.
Output: report/images/<panel>.png.
These are terminal screenshots of recorded output, not a live debugger claim.
"""

import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def stop(process):
    """Reap only the child owned by this script, even on capture failure."""
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)


def window_id(environment, title, terminal):
    """Wait for the named terminal to be mapped on our private X server."""
    for _ in range(50):
        if terminal.poll() is not None:
            raise RuntimeError("XTerm exited before its window was captured")
        tree = subprocess.run(
            ["xwininfo", "-root", "-tree"], env=environment,
            capture_output=True, text=True, timeout=3, check=True,
        ).stdout
        for line in tree.splitlines():
            if f'"{title}"' in line:
                return re.search(r"0x[0-9a-fA-F]+", line).group()
        time.sleep(0.1)
    raise RuntimeError("The evidence terminal did not map in time")


def capture(panel, environment):
    """Show an existing verbatim evidence panel and capture its real window."""
    title = f"Lab1 evidence: {panel.stem} (recorded output)"
    rows = sum(max(1, math.ceil(len(line.expandtabs(8)) / 122))
               for line in panel.read_text().splitlines()) + 2
    if rows > 60:
        raise RuntimeError(f"Panel too long for a complete screenshot: {panel}")
    terminal = subprocess.Popen([
        "xterm", "-title", title, "-geometry", f"122x{max(18, rows)}",
        "-fa", "DejaVu Sans Mono", "-fs", "11", "-bg", "#10151f",
        "-fg", "#e2e8f0", "-hold", "-e", "cat", str(panel),
    ], env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        identifier = window_id(environment, title, terminal)
        time.sleep(0.6)
        output = ROOT / "report" / "images" / f"{panel.stem}.png"
        subprocess.run(
            ["import", "-window", identifier, str(output)],
            env=environment, check=True, timeout=10,
        )
        print(f"Captured real terminal: {output.relative_to(ROOT)}")
    finally:
        stop(terminal)


def main():
    """Use an automatically assigned private display; never touch user windows."""
    for tool in ("Xvfb", "xterm", "xwininfo", "import"):
        if not shutil.which(tool):
            raise RuntimeError(f"Optional screenshot dependency missing: {tool}")
    panel_root = ROOT / ".cache" / "lab1" / "panels"
    if not panel_root.is_dir():
        panel_root = ROOT / "report" / "evidence" / "panels"
    panels = sorted(panel_root.glob("*.txt"))
    if not panels:
        raise RuntimeError("No evidence panels exist; run verification first")
    (ROOT / "report" / "images").mkdir(parents=True, exist_ok=True)
    read_fd, write_fd = os.pipe()
    server = subprocess.Popen([
        "Xvfb", "-displayfd", str(write_fd), "-screen", "0", "1600x1200x24",
        # WSLg mounts pathname sockets read-only; use Linux abstract sockets.
        "-nolisten", "tcp", "-nolisten", "unix",
    ], pass_fds=(write_fd,), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    os.close(write_fd)
    try:
        import select
        if not select.select([read_fd], [], [], 5)[0]:
            raise RuntimeError("Private X server did not start in time")
        display = os.read(read_fd, 64).decode().strip()
        environment = dict(os.environ, DISPLAY=f":{display}")
        for panel in panels:
            capture(panel, environment)
    finally:
        os.close(read_fd)
        stop(server)


if __name__ == "__main__":
    main()
