# Keep the course Makefiles authoritative; this file is a workspace convenience.
# Each lab branch owns code/ according to the course delivery contract.
LAB ?= code
HOSTCC ?= cc
UV ?= uv
# Keep tool caches inside the workspace, including in restricted environments.
export UV_CACHE_DIR ?= $(CURDIR)/.cache/uv
# Use the locked project environment unless the caller explicitly overrides it.
PYTHON ?= $(UV) run --locked python

.DEFAULT_GOAL := all
.PHONY: all clean qemu debug gdb compdb doctor smoke test grade verify property

all clean qemu debug gdb test grade:
	$(MAKE) -C $(LAB) $(if $(filter all,$@),TARGETS,$@)

# Generate indexing commands from the real build instead of duplicating flags.
compdb:
	$(PYTHON) scripts/compile_commands.py $(LAB)

doctor:
	$(PYTHON) scripts/check_environment.py $(LAB)

# Boot-banner check only; this does not verify memory allocation correctness.
smoke: all
	$(PYTHON) scripts/check_environment.py $(LAB) --smoke

verify:
	$(PYTHON) scripts/verify_lab2.py $(LAB)

# The host model executes the production Best-Fit code, not a reimplementation.
property:
	mkdir -p .cache/lab2
	$(HOSTCC) -std=gnu99 -O2 -g -Wall -Werror -Wno-unused \
		-fno-builtin -nostdinc -I$(LAB)/libs -I$(LAB)/kern/mm -I$(LAB)/kern/debug \
		tests/best_fit_property.c -o .cache/lab2/best-fit-property
	.cache/lab2/best-fit-property
