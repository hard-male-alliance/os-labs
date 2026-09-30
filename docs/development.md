# Development environment and CLion integration

## Scope and architecture

The `main` branch contains shared tooling and course documentation only. Each
experiment branch is created from `milestone/common-base` and owns `code/` and
`report/`. The course Makefile inside `code/` remains authoritative. The root
Makefile dispatches to `LAB` (default: `code`); it does not replace the kernel
build with a host CMake executable. Switch to an experiment branch before building.

The project builds a freestanding RV64 kernel with its own headers, not a Linux
userspace application. Ubuntu's `riscv64-linux-gnu` GCC/binutils can build this
lab because the build uses `-nostdinc` and links directly with `ld -nostdlib`.
The existing optimization level, kernel logic, linker layout, and symbol names
are unchanged.

Toolchain selection prefers `riscv64-unknown-elf-gcc` when available, then falls
back to `riscv64-linux-gnu-gcc`. Set `GCCPREFIX` explicitly to override it.

## Dependencies

On the tested Ubuntu 24.04 host:

```sh
sudo apt update
sudo apt install gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu \
    qemu-system-misc gdb-multiarch make python3
```

Installation is the user's responsibility; the project scripts never run sudo.
CLion's bundled GDB 17.1 also accepts `set architecture riscv:rv64` on the tested
installation, so the shared Remote Debug configuration uses bundled GDB. The
terminal `make gdb` uses a cross-toolchain GDB if present, otherwise
`gdb-multiarch`.

## Locked Python tooling environment

Python utilities are managed by uv as a non-package project. `pyproject.toml`
declares `requests` and `beautifulsoup4`; `uv.lock` records their exact resolved
versions, transitive dependencies and distribution hashes. `.python-version`
pins the default interpreter to CPython 3.14.7. The project scripts support
Python 3.12 or newer, but use the pinned version for the shared workflow.

From the repository root:

```sh
uv sync --locked
uv run --locked python scripts/compile_commands.py code
uv run --locked python scripts/check_environment.py code
```

The root Makefile's Python targets use `uv run --locked python` automatically;
there is no need to activate `.venv`. `--locked` fails when the lockfile does not
match the project configuration instead of silently re-resolving dependencies.
`UV=/path/to/uv` selects a different uv executable. An explicit `PYTHON=...`
overrides the launcher for compatibility but bypasses automatic uv isolation.

Course-documentation scripts use the same environment:

```sh
uv run --locked python scripts/fetch_lab2026.py
uv run --locked python scripts/validate_lab2026.py
```

Their original cache prerequisites remain unchanged: the fetch script expects
`.cache/lab2026/index.html`; validation uses the cached original HTML responses.
Pandoc, the RISC-V toolchain, QEMU and GDB are external system tools, not Python
packages, and are not locked by uv.

Keep `pyproject.toml`, `uv.lock`, and `.python-version` in version control; ignore
the machine-local `.venv/`. To change dependencies, use `uv add` / `uv remove`;
to deliberately upgrade locked versions, use `uv lock --upgrade` followed by
`uv sync --locked`. Normal tool execution should not upgrade the lockfile.

Environment setup was validated with `uv sync --locked`, `uv lock --check`,
imports of both third-party dependencies, and the existing Make Python targets.
The kernel build system stays on Makefile; CMake migration was declined.

Reference: [uv locking and syncing](https://docs.astral.sh/uv/concepts/projects/sync/).

## Command-line workflow

Run from the repository root:

```sh
make                  # Build code/bin/kernel and code/bin/ucore.img.
make doctor           # Check the selected compiler, binutils, QEMU and GDB.
make compdb           # Regenerate compile_commands.json from actual Make commands.
make smoke            # Build and check lab1's boot banner; stop the child QEMU.
make verify           # Lab1 only: assert reset/firmware/stack/initialization/SBI.
make qemu             # Boot the kernel interactively.
make debug            # Start QEMU paused, listening only on 127.0.0.1:1234.
```

In a second terminal:

```sh
make gdb
```

At the GDB prompt, `break kern_init` followed by `continue` reaches the kernel.
Use QEMU's `Ctrl-a x` escape to exit an interactive session. The kernel's final
infinite loop is intentional; lack of process termination is not a boot failure.
Override `GDB_PORT` consistently in both terminals if port 1234 is occupied.
The shared CLion configuration has the default port; update its connection field
when using a different port.

For another experiment, create its `labx` branch from the milestone and put
its source in `code/`; see the root README. `LAB` is an optional source-directory
override, not a branch selector. `make smoke` currently defines an expectation
only for lab1. The distributed lab1 tree references `tools/grade.sh` but does not
contain it, so `make grade` is not available. The smoke check is not a substitute
for the course grader or functional correctness tests.

## CLion project model

The shared `.idea/misc.xml` links the root Makefile and keeps the repository
root as the content root. Its build dispatches to `code/` on an experiment branch.
CLion can extract the compile commands from the real course build. Lab-specific
run configurations are committed only on their corresponding experiment branch.
This supplies the RISC-V compiler, GNU C99 flags, and per-source include paths.

If CLion previously failed to import the missing `riscv64-unknown-elf-gcc`, click
**Reload Makefile Project** in the Makefile tool window. Changing files on disk
does not establish that the live IDE has successfully re-imported them. If the
settings change is not reflected, reopen the project and reload its Makefile.
Do not disable diagnostics or add host system headers to disguise a failed import.

`make compdb` creates a supplemental compilation database with eight entries
for the current lab1. It is generated from a forced dry run of the same Makefile,
not a hand-maintained list of flags. Regenerate after changing source membership,
compiler flags, toolchain or selected lab. Absolute machine-local paths mean the
generated JSON is intentionally ignored by Git. It can also be opened as a
Compilation Database project in CLion if native Makefile import is unsuitable;
there is no need to load both project models at once.

## Lab1 branch CLion run configurations

| Configuration | Action | Purpose |
| --- | --- | --- |
| Lab1 Build | Run | Execute the actual Makefile `TARGETS` build. |
| Lab1 QEMU | Run | Boot with the default OpenSBI firmware. |
| Lab1 QEMU GDB Server | Run | Boot paused with a localhost-only GDB endpoint. |
| Lab1 Remote Debug | Debug | Connect guest-aware GDB using `code/bin/kernel` symbols. |

First **Run** `Lab1 QEMU GDB Server`, then **Debug** `Lab1 Remote Debug`.
Do not debug the QEMU host process when intending to debug the guest kernel.
The remote configuration's connection field is `127.0.0.1:1234`, not the full
GDB command `target remote ...`; CLion supplies that command itself.
Stopping the debugger does not necessarily stop QEMU. Stop the emulator's Run
session separately to free the port. No automatic before-launch rebuild is
configured for the attach step: the server configuration builds before booting,
and the symbol file must match the running image.

## Historical environment verification (2026-09-30, before branch layout)

- Ubuntu 24.04.5 under WSL2; CLion build `CL-262.9437.136`.
- Compiler: `/usr/bin/riscv64-linux-gnu-gcc` (GCC 13 toolchain).
- QEMU: 8.2.2; default OpenSBI reports version 1.3.
- Terminal GDB: `gdb-multiarch` 15.1; CLion bundled GDB: 17.1.
- The linked kernel is an RV64 executable with entry point `0x80200000`.
- Build succeeded; all seven C translation units passed syntax checks using the
  generated database's compiler arguments; one additional entry covers assembly.
- The boot smoke check observed `(THU.CST) os is loading ...`; output is retained
  in `.cache/lab1/boot.log`.
- MCP observed all four shared run configurations and executed `Lab1 Build`
  successfully (exit status 0).
- CLion Remote Debug connected successfully. Pausing the guest showed
  `kern_init` at `lab1/kern/init/init.c:12`, `$pc = 0x8020003a`, and the local
  `message` pointing to the expected boot string.
- At the last indexing probe before user reload, `get_compiler_info` still
  reported no resolve configuration. Runtime debugging is verified; successful
  live code indexing must be confirmed after the Makefile reload.

### Compatibility finding

The original `-device loader,file=...,addr=0x80200000` command copied the kernel
into memory but the tested default OpenSBI reported `Domain0 Next Address: 0`.
No kernel banner appeared. Switching to QEMU's supported `-kernel bin/ucore.img`
boot interface produced the expected banner without changing kernel code.

References:

- [QEMU RISC-V firmware and kernel boot options](https://www.qemu.org/docs/master/system/target-riscv.html)
- [CLion Makefile project support](https://www.jetbrains.com/help/clion/makefiles-support.html)
- [CLion compilation databases](https://www.jetbrains.com/help/clion/compilation-database.html)
- [CLion Remote Debug configuration](https://www.jetbrains.com/help/clion/remote-debug.html)

## CMake migration evaluation (declined; historical experiment)

**Historical recommendation (not adopted):** for this CLion-centered learning workspace, use native
CMake targets with Ninja as the primary build model. Keep only thin Make command
compatibility entry points after migration. Do not add a dummy CMake target that
invokes the existing Makefile while maintaining a second list of indexing flags.
The existing Makefile remains authoritative until migration is explicitly approved.

### Evidence from an isolated prototype

Prototype files are under `.temp/cmake-evaluation/`; build products and logs are
under `.cache/cmake-evaluation/`. No production CMakeLists, presets, or IDE model
switch were installed for this evaluation.

The prototype uses a `Generic` / `riscv64` toolchain, GNU C and preprocessed
assembly, static-library compiler checks, explicit source membership and native
`add_executable(kernel)`. It preserves the existing compiler flags and linker
script, but invokes GCC as the link driver with `-nostdlib -static -no-pie` and
`--build-id=none` to control defaults absent from the original direct `ld` call.
This link-driver change needs equivalence checks during a real migration.

Observed results:

- CMake configured successfully with GCC 13.3.0.
- Ninja compiled seven C files and one assembly file, linked the kernel, and
  generated the raw image.
- ELF reports RV64, EXEC, double-float ABI and entry address `0x80200000`.
- QEMU 8.2.2 with default OpenSBI printed the expected lab1 boot banner.
- CMake automatically emitted eight compilation database entries.
- An unchanged rebuild reported `ninja: no work to do`.
- Deleting only the prototype image caused exactly the image-generation step to
  run on rebuild; the kernel was not unnecessarily relinked.
- A byte comparison with the earlier Make image could not be performed because
  `code/bin/ucore.img` was absent at comparison time. No binary identity claim is
  made. CLion indexing/debugging against this prototype was not tested.

### Intended production boundaries

Use a small root CMakeLists, a RISC-V toolchain file and shared CMake presets.
Declare compiler/include options on targets, expose `qemu`, `debug` and smoke
checks, and make the linker script an explicit link dependency. Use an
output-based custom command for `ucore.img`, not merely an unconditional shell
step or a post-build hook that may fail to recreate a deleted image.

Keep `make`, `make qemu`, `make debug`, and other supported entry points as
wrappers over CMake if required. Treat expected `bin/kernel` / `bin/ucore.img`
paths and grader behavior as compatibility contracts; future course graders
have not been audited. Prefer building out of tree and explicitly publishing
compatibility artifacts rather than sharing object directories between the two
systems. Do not invent a generic multi-architecture framework for eight files.

Acceptance criteria: preserve kernel load/entry addresses, ABI, relevant section
layout and boot behavior; validate a real GDB breakpoint; verify CLion's compiler
model; confirm header/linker-script/image incremental dependencies; preserve
supported Make commands and course-facing artifact paths; audit each later lab
when it is added. No build-speed improvement is claimed without benchmarks.

This is an IDE and dependency-model improvement, not a change to kernel semantics.
CMake still needs configuration/reload, and guest debugging still requires the
QEMU GDB endpoint. Its main benefit is replacing Make output inference and the
custom compilation-database extraction script with a native target model.

Additional references:

- [CMake cross-compilation toolchains](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [CMake link dependencies](https://cmake.org/cmake/help/latest/prop_tgt/LINK_DEPENDS.html)
- [CMake compilation database export](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html)
- [CLion CMake presets](https://www.jetbrains.com/help/clion/cmake-presets.html)
- [Zephyr's production CMake/Ninja build model](https://docs.zephyrproject.org/latest/build/cmake/index.html)
- [Build Systems a la Carte, ICFP 2018](https://www.microsoft.com/en-us/research/publication/build-systems-la-carte/)

## Lab1 completion verification (2026-10-01)

The `lab1` branch now includes `scripts/verify_lab1.py` and `make verify`.
This complements, rather than replaces, `make smoke` or an official grader.
See [verification contracts and actual results](lab1-verification.md),
[source/requirement audit](lab1-source-notes.md), and
[the complete report](../report/report.md). The verifier is stdlib-only, uses
the locked Python launcher and the existing Make recipes, selects a private
loopback GDB port, applies timeouts, and reaps only its own emulator.

The screenshot utility is optional. It uses a separate Xvfb display and XTerm
to capture the preserved output panels. Full evidence is committed under
`report/evidence/`; scratch runs and negative-test fixtures stay in `.cache/`
and `.temp/`. No host dependencies were installed by these scripts.
