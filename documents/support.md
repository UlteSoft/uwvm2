# Supported platforms

## POSIX Environment
* Linux: (x86\_64, i386, aarch64, arm, loongarch64, powerpc64, riscv, mips, etc.)-linux-(gnu, android, ohos, musl, llvm, mlibc, uclibc, etc.)
* Darwin: (macosx, iphoneos, watchos): (x86\_64, i386, aarch64, arm, powerpc64)-apple-darwin
* FreeBSD: (x86\_64, i386, aarch64, arm, powerpc64, etc.)-freebsd(Version)
* NetBSD: unknown(50+)-netbsd
* OpenBSD: unknown-openbsd
* DragonflyBSD: (x86\_64, i386, aarch64, arm, powerpc64, etc.)-dragonfly
* SerenityOS: (x86\_64, i686)-serenity
* SunOS: (sparc, x86)-(sunos, solaris)-gnu
* Compilers: GCC >= 16, LLVM Clang >= 21

## Windows NT (MSVC)
* The windows-msvc toolchain defines its supported platforms and API levels through the Windows SDK. Currently, it only supports WINNT 10.0 (Windows 10, Windows Server 2016, Windows Server 2019, Windows Server 2022, Windows 11, Windows Server 2025).
* (x86\_64, i686, aarch64, arm, arm64ec)-windows-msvc
* Compilers (MSVC Target): LLVM Clang >= 21, ~~MSVC~~ (Not Support Yet)

## Windows NT (MinGW)
* Supported system versions: Windows NT 5.0 (Windows 2000, Windows Server 2000), Windows NT 5.1 (Windows XP), Windows NT 5.2 (Windows XP 64bit, Windows Server 2003), Windows NT 6.0 (Windows Vista, Windows Server 2008), Windows NT 6.1 (Windows 7, Windows Server 2008 R2), Windows NT 6.2 (Windows 8, Windows Server 2012), Windows NT 6.3 (Windows Blue, Windows Server 2012 R2), Windows NT 10.0 (Windows 10, Windows Server 2016, Windows Server 2019, Windows Server 2022, Windows 11, Windows Server 2025)
* Platforms that compile, but are not tested: Windows NT 4.0, Windows NT 3.51, Windows NT 3.5, Windows NT 3.1
* (x86\_64, i686, aarch64, arm, arm64ec)-windows-gnu (aka. unknown(4)-w64-mingw32)
* Compilers (MinGW Target): GCC >= 16, LLVM Clang >= 21

## Windows 9x (MinGW)
* Supported system versions: Windows 9x 4.0 (Windows 95), Windows 9x 4.1 (Windows 98), Windows 9x 4.9 (Windows ME)
* i686-windows-gnu (aka. i686-w64-mingw32)
* Compilers (MinGW Target): GCC >= 16, LLVM Clang >= 21

## Cygwin
* Supported Windows versions: Windows 7 (NT 6.1) and later
* Compilers (Cygwin Target): GCC >= 16, LLVM Clang >= 21

## DOS
* MS-DOS, FreeDOS (DJGPP): i586-msdosdjgpp
* Compilers (MSDOS-DJGPP Target): GCC >= 16

## WASM-WASI
* Self‑hosted WASI builds for `wasm32` and `wasm64` (WASI p1–p3). Threads/shared memory are not supported; browser runtimes require an adapter. Build guide: [wasm-wasi.md](documents/how-to-build/wasm-wasi.md)
* Compilers (WASM-WASI Target): LLVM Clang >= 21

## Host C Library Environment
* This project's Newlib build does not provide executable binaries, offering only linkable object files and libraries for flexible integration within higher-level systems or plugin frameworks. It does not support wasi and only supports plugins using weak symbol extensions.
* newlib: unknown-none-eabi
* avr: avr
* Compilers (None Target): GCC >= 16, LLVM Clang >= 21

## Freestanding Environment
* Freestanding support is only partially implemented for specific modules, such as the parser and interpreter components.
* Platforms where `CHAR_BIT` is not equal to 8 are not currently supported.
* This platform’s C++ runtime environment requires developers to explicitly define a custom memory allocator concept to fulfill the requirements of the C++ Heap Abstract Machine.
The system does not provide a default global allocator; instead, it delegates all heap allocation behavior to user-defined implementations through the `fast_io::custom_global_allocator` interface.
* ELF file-format: unknown-unknown-elf
* PE file-format: unknown-windows-gnu + `-ffreestanding`
* Compilers (Freestanding): GCC >= 16, LLVM Clang >= 21

# C++ Version Required
- Full ISO C++26

# Compiler Support
- LLVM Clang >= 21 (recommended)
- GCC >= 16
- (MSVC hasn't support C++26 currently, not supported at this time)

# C++ Standard Library Support
- GNU libstdc++ >= 15
- LLVM libc++ >= 21
- MSVC STL >= 202501 (Cross-compile to unknown-windows-msvc using Clang)
