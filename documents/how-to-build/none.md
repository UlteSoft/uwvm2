# None (newlib bare-metal)

Targets without an OS (newlib-based). Supports GCC and LLVM.

## Prerequisites
- Install xmake: https://xmake.io
- Install a cross toolchain for your target and ensure tools are in PATH.
  - Common GCC triplets:
    - ARM: `arm-none-eabi-*`
    - AArch64: `aarch64-none-elf-*`
    - RISC-V: `riscv64-unknown-elf-*` (or `riscv32-unknown-elf-*`)
    - MIPS: `mipsel-unknown-elf-*`
  - LLVM/Clang: use `clang` with `--target=<triplet>` and a newlib sysroot
    - Example sysroots: from your cross GCC install, or prebuilt newlib SDKs

## Examples (GCC)
```shell
# ARM (Cortex-M) example with arm-none-eabi GCC
xmake f -m release \
  --cc=arm-none-eabi-gcc \
  --cxx=arm-none-eabi-g++ \
  --ar=arm-none-eabi-ar \
  --as=arm-none-eabi-as \
  --ld=arm-none-eabi-gcc \
  --ranlib=arm-none-eabi-ranlib
# or: $ xmake f -m release --sdk=$ARM_NONE_EABI_TOOLCHAIN_PATH

# Build
xmake

# Install to an output folder
xmake i -o ./out/none-arm
```

## Examples (LLVM/Clang + newlib)
```shell
# RISC-V bare-metal using clang with newlib sysroot
SYSROOT=/opt/riscv64-unknown-elf
TARGET=riscv64-unknown-elf

# Configure xmake to use clang/llvm tools
xmake f -m release --use-llvm=y \
  --cc=clang \
  --cxx=clang++ \
  --ld=clang \
  --ar=llvm-ar \
  --ranlib=llvm-ranlib

# You may need to export flags (set in your environment or xmake config)
export CC=clang
export CXX=clang++
export CFLAGS="--target=${TARGET} --sysroot=${SYSROOT}"
export CXXFLAGS="--target=${TARGET} --sysroot=${SYSROOT}"
export LDFLAGS="--target=${TARGET} --sysroot=${SYSROOT}"

xmake

# Install to an output folder
xmake i -o ./out/none-riscv
```

### Additional Options
- `--static` Static links (typical for bare-metal)
- `--march` Tune for your MCU/SoC architecture if needed
- `--use-cxx-module=y` Only if your toolchain supports C++ modules

## Caveats
- No OS services; ensure startup files, linker scripts, and minimal runtime are provided if required by your target.
- Some targets require `-nostdlib`, custom `crt0`, and a linker script; integrate them via xmake target rules if needed.
