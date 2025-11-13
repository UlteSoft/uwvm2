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
3. `--mingw-min` Minimum windows compatible version, supported from Win11 all the way to Win95
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
3. `--mingw-min` Minimum windows compatible version, supported from Win11 all the way to Win95
4. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported