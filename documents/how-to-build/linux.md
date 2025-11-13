# Linux

## Prerequisites
- Install toolchains and xmake using your distro's package manager:
  - Debian/Ubuntu: `sudo apt install build-essential gcc g++ llvm clang xmake`
  - Fedora/RHEL: `sudo dnf install gcc gcc-c++ llvm clang xmake`
  - Arch: `sudo pacman -S base-devel gcc llvm clang xmake`

## Examples
```shell
# GCC toolchain
xmake f -m release
xmake

# LLVM/Clang toolchain
xmake f -m release --use-llvm=y
xmake

# Install
sudo xmake i -o /usr/local
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
1. `--static` Static links
2. `--march` The default is native, which uses the cpu designator to control it
3. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported

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
3. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported

## Caveat
1. You must add `--use-llvm` if you use llvm underneath, otherwise it will fail to compile, including but not limited to symbolic linking of `gcc` to `clang`
2. On some platforms such as the latest Android which only has llvm, you must also add `--use-llvm`, same as note 1.
3. On some platforms, such as Android, the static link option will have no effect.