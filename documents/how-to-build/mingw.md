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
