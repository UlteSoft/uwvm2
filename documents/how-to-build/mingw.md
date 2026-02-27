# MinGW

## Prerequisites
- Recommended: MSYS2 (MinGW-w64)
  - Install: https://www.msys2.org/
  - 64-bit env: MSYS2 MinGW x64 shell
  - Packages: `pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-llvm mingw-w64-x86_64-clang mingw-w64-x86_64-xmake`
- Alternatively: MinGW-w64 standalone toolchain + xmake release binaries

## Examples
```shell
# GCC toolchain (MinGW-w64)
xmake f -m release
xmake

# LLVM/Clang toolchain
xmake f -m release --use-llvm=y
xmake

# Install
xmake i -o C:/uwvm2
```

## Use GCC
1. Install [[xmake]](https://github.com/xmake-io/xmake/)
2. Install [[GCC]](https://sourceforge.net/projects/mingw-w64/)
3. Build
```shell
$ xmake f -m [debug|release|releasedbg|minsizerel]
$ xmake
```
4. Install UWVM2
```shell
$ xmake i -o <install_path>
```

### Additional Options
1. `--static` Static links
2. `--march` The default is native, which uses the cpu designator to control it
3. `--mingw-min` Minimum windows compatible version (see below for all values)
4. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported

## Use LLVM
1. Install [[xmake]](https://github.com/xmake-io/xmake/)
2. Install [[llvm]](https://github.com/llvm/llvm-project/releases)
3. Build
```shell
$ xmake f -m [debug|release|releasedbg|minsizerel] --use-llvm=y
$ xmake
```
4. Install UWVM2
```shell
$ xmake i -o <install_path>
```

### Additional Options
1. `--static` Static links
2. `--march` The default is native, which uses the cpu designator to control it
3. `--mingw-min` Minimum windows compatible version (see below for all values)
4. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported

## `--mingw-min` values

`xmake/option.lua` defines a `mingw-min` option to control the minimum supported Windows version by setting `_WIN32_WINNT`, `_WIN32_WINDOWS` and `WINVER` macros.

Available values:

- `default`: Use the compiler’s default macros (typically MinGW 0x0A00, MSVC not defined).
- Newer Windows / Server:
  - `WS25`: Windows Server 2025, `-D_WIN32_WINNT=0x0A00`
  - `WIN11`: Windows 11, `-D_WIN32_WINNT=0x0A00`
  - `WS22`: Windows Server 2022, `-D_WIN32_WINNT=0x0A00`
  - `WS19`: Windows Server 2019, `-D_WIN32_WINNT=0x0A00`
  - `WS16`: Windows Server 2016, `-D_WIN32_WINNT=0x0A00`
  - `WIN10`: Windows 10, `-D_WIN32_WINNT=0x0A00`
- Windows 8.x / Server 2012.x:
  - `WS12R2`: Windows Server 2012 R2, `-D_WIN32_WINNT=0x0603`
  - `WINBLUE`: Windows 8.1, `-D_WIN32_WINNT=0x0603`
  - `WS12`: Windows Server 2012, `-D_WIN32_WINNT=0x0602`
  - `WIN8`: Windows 8, `-D_WIN32_WINNT=0x0602`
- Windows 7 / Server 2008 R2 / 2008 / Vista:
  - `WS08R2`: Windows Server 2008 R2, `-D_WIN32_WINNT=0x0601`
  - `WIN7`: Windows 7, `-D_WIN32_WINNT=0x0601`
  - `WS08`: Windows Server 2008, `-D_WIN32_WINNT=0x0600`
  - `VISTA`: Windows Vista, `-D_WIN32_WINNT=0x0600`
- Windows 2000 / XP / Server 2003:
  - `WS03`: Windows Server 2003, `-D_WIN32_WINNT=0x0502`
  - `WINXP64`: Windows XP 64bit, `-D_WIN32_WINNT=0x0502`
  - `WINXP`: Windows XP, `-D_WIN32_WINNT=0x0501`
  - `WS2K`: Windows Server 2000, `-D_WIN32_WINNT=0x0500`
  - `WIN2K`: Windows 2000, `-D_WIN32_WINNT=0x0500`
- Windows 9x:
  - `WINME`: Windows ME, `-D_WIN32_WINDOWS=0x0490`
  - `WIN98`: Windows 98, `-D_WIN32_WINDOWS=0x0410`
  - `WIN95`: Windows 95, `-D_WIN32_WINDOWS=0x0400`
- Windows NT:
  - `NT400`: Windows NT 4.0, `-D_WIN32_WINNT=0x0400`
  - `NT351`: Windows NT 3.51, `-D_WIN32_WINNT=0x0351`
  - `NT350`: Windows NT 3.5, `-D_WIN32_WINNT=0x0350`
  - `NT310`: Windows NT 3.1, `-D_WIN32_WINNT=0x0310`

If you set an unsupported value, you may see errors like: `version not recognized: Windows Version not recognized`.

## Windows 9x note (thread_local)

When targeting Windows 9x (`WIN95`/`WIN98`/`WINME`) with MinGW-w64 + libstdc++, you must disable C++ `thread_local` in UWVM2:

```shell
xmake f -m release --mingw-min=WIN98 --use-thread-local=n
xmake
```

Reason: libstdc++'s TLS support requires at least Windows XP on some configurations/toolchains. See GCC Bug 108222:
https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108222

Additionally, running on Windows 9x may require extra prerequisite components (depending on your toolchain/build options). Install prerequisites from:
https://github.com/UlteSoft/uwvm2-prerequisites/tree/master/win95

## Note
If you encounter a crash when the program exits and this occurs with ASAN enabled, it is due to mingw's libstdcxx not supporting cross-module TLS. In this case, adding `--static=y` for static linking will resolve the issue.

```
=================================================================
==5524==ERROR: AddressSanitizer: access-violation on unknown address 0x000000000111 (pc 0x000000000111 bp 0x114ccdaddf20 sp 0x003892bff1b8 T0)
==5524==Hint: pc points to the zero page.
==5524==The signal is caused by a UNKNOWN memory access.
==5524==Hint: address points to the zero page.
    #0 0x000000000110  (<unknown module>)
    #1 0x7ff99eca5c38  (D:\tool-chain\x86_64-w64-mingw32\lib\libstdc++-6.dll+0x3be985c38)
    #2 0x7ffa8a06bc74  (C:\WINDOWS\System32\ucrtbase.dll+0x18004bc74)
    #3 0x7ffa8a06b896  (C:\WINDOWS\System32\ucrtbase.dll+0x18004b896)
    #4 0x7ffa8a06b84c  (C:\WINDOWS\System32\ucrtbase.dll+0x18004b84c)
    #5 0x7ff99ec8113b  (D:\tool-chain\x86_64-w64-mingw32\lib\libstdc++-6.dll+0x3be96113b)
    #6 0x7ff99ec81216  (D:\tool-chain\x86_64-w64-mingw32\lib\libstdc++-6.dll+0x3be961216)
    #7 0x7ffa8d51f739  (C:\WINDOWS\SYSTEM32\ntdll.dll+0x18015f739)
    #8 0x7ffa8d3cbc32  (C:\WINDOWS\SYSTEM32\ntdll.dll+0x18000bc32)
    #9 0x7ffa8d44d3fe  (C:\WINDOWS\SYSTEM32\ntdll.dll+0x18008d3fe)
    #10 0x7ffa8d44c5cd  (C:\WINDOWS\SYSTEM32\ntdll.dll+0x18008c5cd)
    #11 0x7ffa8c2518aa  (C:\WINDOWS\System32\KERNEL32.DLL+0x1800418aa)
    #12 0x7ffa8a0c0092  (C:\WINDOWS\System32\ucrtbase.dll+0x1800a0092)
    #13 0x7ff61e1a13be in __tmainCRTStartup /home/luo/mingw/mingw-w64-crt/crt/crtexe.c:261:7
    #14 0x7ff61e1a13f5 in mainCRTStartup /home/luo/mingw/mingw-w64-crt/crt/crtexe.c:180:9
    #15 0x7ffa8c23e8d6  (C:\WINDOWS\System32\KERNEL32.DLL+0x18002e8d6)
    #16 0x7ffa8d44c48b  (C:\WINDOWS\SYSTEM32\ntdll.dll+0x18008c48b)

==5524==Register values:
rax = 0  rbx = 120e1c5a1900  rcx = 2081a6baab0  rdx = 0
rdi = 7ffa8c24c620  rsi = 2081a6d3b70  rbp = 114ccdaddf20  rsp = 3892bff1b8
r8  = 120e1c5a1900  r9  = 114ccdaddf20  r10 = 7ff99ec80000  r11 = 3892bff2b8
r12 = 7ffe0385  r13 = 1  r14 = 3892bff2d8  r15 = 2081a6d3ae0
AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: access-violation (<unknown module>)
==5524==ABORTING
```
