# Keep the course Makefiles authoritative; this file is a workspace convenience.
# Each lab branch owns code/ according to the course delivery contract.
LAB ?= code
UV ?= uv
# Use the locked project environment unless the caller explicitly overrides it.
PYTHON ?= $(UV) run --locked python

.DEFAULT_GOAL := all
.PHONY: all clean qemu debug gdb compdb doctor smoke

all clean qemu debug gdb:
	$(MAKE) -C $(LAB) $(if $(filter all,$@),TARGETS,$@)

# Generate indexing commands from the real build instead of duplicating flags.
compdb:
	$(PYTHON) scripts/compile_commands.py $(LAB)

doctor:
	$(PYTHON) scripts/check_environment.py $(LAB)

# The lab1 kernel intentionally loops forever; the smoke check stops only its QEMU.
smoke: all
	$(PYTHON) scripts/check_environment.py $(LAB) --smoke
