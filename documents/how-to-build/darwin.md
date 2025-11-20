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

### Enumerated values

- macOS:
  - `MACOS_SEQUOIA`: macOS 15.0 Sequoia
  - `MACOS_SONOMA`: macOS 14.0 Sonoma
  - `MACOS_VENTURA`: macOS 13.0 Ventura
  - `MACOS_MONTEREY`: macOS 12.0 Monterey
  - `MACOS_BIG_SUR`: macOS 11.0 Big Sur
  - `MACOS_CATALINA`: macOS 10.15 Catalina
  - `MACOS_MOJAVE`: macOS 10.14 Mojave
  - `MACOS_HIGH_SIERRA`: macOS 10.13 High Sierra
  - `MACOS_SIERRA`: macOS 10.12 Sierra
  - `MACOS_EL_CAPITAN`: macOS 10.11 El Capitan
  - `MACOS_YOSEMITE`: macOS 10.10 Yosemite

- iOS:
  - `IOS_18`: iOS 18.0
  - `IOS_17`: iOS 17.0
  - `IOS_16`: iOS 16.0
  - `IOS_15`: iOS 15.0
  - `IOS_14`: iOS 14.0
  - `IOS_13`: iOS 13.0
  - `IOS_12`: iOS 12.0
  - `IOS_11`: iOS 11.0

- tvOS:
  - `TVOS_18`: tvOS 18.0
  - `TVOS_17`: tvOS 17.0
  - `TVOS_16`: tvOS 16.0
  - `TVOS_15`: tvOS 15.0
  - `TVOS_14`: tvOS 14.0
  - `TVOS_13`: tvOS 13.0

- watchOS:
  - `WATCHOS_11`: watchOS 11.0
  - `WATCHOS_10`: watchOS 10.0
  - `WATCHOS_9`: watchOS 9.0
  - `WATCHOS_8`: watchOS 8.0
  - `WATCHOS_7`: watchOS 7.0

- visionOS:
  - `VISIONOS_2`: visionOS 2.0
  - `VISIONOS_1`: visionOS 1.0

You can also use a custom format: `"platform:version"` (for example, `"macos:10.15"`, `"ios:13.0"`).

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
