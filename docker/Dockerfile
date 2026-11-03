FROM debian:trixie-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       make gcc gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
       qemu-system-misc opensbi gdb-multiarch python3 \
       xvfb xauth xterm x11-utils imagemagick fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
CMD ["bash"]
