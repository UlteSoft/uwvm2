# Darwin
macosx, iphoneos, watchos, tvos

## Prerequisites
- Ensure Command Line Tools are available:
  - `xcode-select --install`
- Optional package manager: Homebrew
  - Install LLVM: `brew install llvm`
  - Install GCC: `brew install gcc`
  - Install xmake: `brew install xmake`

## Examples
```shell
# GCC toolchain (default)
xmake f -m release
xmake

# LLVM/Clang toolchain
xmake f -m release --use-llvm=y
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
1. `--static` Static links
2. `--march` The default is native, which uses the cpu designator to control it
3. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported
4. `--apple-platform` Set Apple platform target and minimum OS version (see below)

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
4. `--apple-platform` Set Apple platform target and minimum OS version (see below)

## Build for Apple platforms (iOS / watchOS / tvOS / visionOS)

`xmake/option.lua` defines an `apple-platform` option. It controls the `-mtargetos` flag and the minimum OS version (`-m*-version-min`) when using Clang/LLVM.

Use it at configure time, for example:

```shell
# iOS
xmake f -m release --use-llvm=y --apple-platform=IOS_18

# tvOS
xmake f -m release --use-llvm=y --apple-platform=TVOS_18

# watchOS
xmake f -m release --use-llvm=y --apple-platform=WATCHOS_11

# visionOS
xmake f -m release --use-llvm=y --apple-platform=VISIONOS_2

# Custom version (example)
xmake f -m release --use-llvm=y --apple-platform=ios:17.0
```

Then build and install as usual:

```shell
xmake
xmake i -o <install_path>
```

## Caveat
1. You must add `--use-llvm` if you use llvm underneath, otherwise it will fail to compile, including but not limited to symbolic linking of `gcc` to `clang`
2. Currently llvm does not have static linking on darwin.
