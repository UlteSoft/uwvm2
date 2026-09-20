# FreeBSD

## Prerequisites
- Use pkg or ports:
  - `pkg install xmake gcc llvm`
  - or via ports: `/usr/ports/*`

## Examples
```shell
# GCC toolchain
xmake f -m release
xmake

# LLVM/Clang toolchain
xmake f -m release --use-llvm-compiler=y
xmake

# Install
xmake i -o /usr/local
```

## Use GCC
1. Install [[xmake]](https://github.com/xmake-io/xmake/)
2. Install [[GCC]](git://gcc.gnu.org/git/gcc.git)
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
1. `--static=none|non-system|compiler` Static linking policy (`compiler` uses global `-static` where supported)
2. `--march` defaults to `none` (the configured toolchain baseline). Release artifacts must select and test a fixed baseline explicitly; `--march=native` is only for developer-local builds.
3. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported

## Use LLVM
1. Install [[xmake]](https://github.com/xmake-io/xmake/)
2. Install [[llvm]](https://github.com/llvm/llvm-project/releases)
3. Build
```shell
$ xmake f -m [debug|release|releasedbg|minsizerel] --use-llvm-compiler=y
$ xmake
```
4. Install UWVM2
```shell
$ xmake i -o <install_path>
```

### Additional Options
1. `--static=none|non-system|compiler` Static linking policy (`compiler` uses global `-static` where supported)
2. `--march` defaults to `none` (the configured toolchain baseline). Release artifacts must select and test a fixed baseline explicitly; `--march=native` is only for developer-local builds.
3. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported

## Caveat
1. Add `--use-llvm-compiler` when you want to build with the LLVM/Clang compiler toolchain. This only selects the compiler toolchain and does not enable the LLVM AOT backend by itself.
