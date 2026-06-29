# Darwin
macosx, iphoneos, watchos, tvos

## Prerequisites
- Ensure Command Line Tools are available:
  - `xcode-select --install`
- Use the latest available Darwin SDK (typically via the newest Xcode / Command Line Tools) when building.
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
1. `--static=none|non-system|compiler` Static linking policy (`non-system` avoids Darwin's unsupported global `-static`)
2. `--march` The default is native, which uses the cpu designator to control it
3. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported
4. `--apple-platform` Set Apple platform target and minimum OS version (see below)

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
1. `--static=none|non-system|compiler` Static linking policy (`non-system` avoids Darwin's unsupported global `-static`)
2. `--march` The default is native, which uses the cpu designator to control it
3. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported
4. `--apple-platform` Set Apple platform target and minimum OS version (see below)

## Static release artifacts

Darwin does not support fully static executables in the ELF/Linux sense. For
release artifacts, use `--static=non-system` and keep Apple platform libraries
dynamic. If the LLVM installation only provides `libLLVM*.dylib` and not
`libLLVM*.a`, build the static release artifact with the LLVM JIT backend
disabled.

If a release artifact enables LLVM JIT and statically links LLVM archives, the
LLVM archives must be built for the same CPU baseline as the artifact. Do not
link an AVX2-built LLVM archive into an SSSE3/SSE4.2/AVX artifact; LLVM itself
may execute instructions that are unavailable on the target machine. Either
build one conservative LLVM archive for the lowest supported baseline and reuse
it for higher tiers, or build one LLVM archive per release tier.

Use `--march=none` for all release artifact builds. Select x86_64 ISA tiers with
explicit feature flags such as `--cxflags="-mssse3"` instead of `--march`.

Common environment:

```shell
export SYSROOT=/path/to/apple-darwin-sysroot
export PATH="$SYSROOT/../llvm/bin:$PATH"
```

Apple Silicon release artifact:

```shell
xmake f -y -c -m release \
  --plat=macosx --arch=arm64 \
  --use-llvm-compiler=y \
  --sysroot="$SYSROOT" \
  --llvm-target=aarch64-apple-darwin \
  --use-cxx-module=n \
  --static=non-system \
  --execution-jit=none \
  --march=none \
  --openssl-root=/opt/homebrew \
  --ccache=n \
  --ldflags="-nostdlib++"
xmake -j1 uwvm
cp build/macosx/arm64/release/uwvm build/aarch64-apple-darwin-uwvm2
```

Intel macOS release tiers:

| Artifact | Extra compiler flags | Intended baseline |
| --- | --- | --- |
| `x86_64_SSSE3-apple-darwin-uwvm2` | `-mssse3` | Core 2 Duo class x86_64 Macs |
| `x86_64_SSE4_2-apple-darwin-uwvm2` | `-msse4.2` | Nehalem / Westmere |
| `x86_64_AVX-apple-darwin-uwvm2` | `-mavx` | Sandy Bridge / Ivy Bridge |
| `x86_64_AVX2-apple-darwin-uwvm2` | `-mavx2 -mbmi -mbmi2` | Haswell and newer |
| `x86_64_AVX2_PRFCHW-apple-darwin-uwvm2` | `-mavx2 -mbmi -mbmi2 -mprfchw` | Broadwell and newer |

Build one Intel tier:

```shell
xmake f -y -c -m release \
  --plat=macosx --arch=x86_64 \
  --use-llvm-compiler=y \
  --sysroot="$SYSROOT" \
  --llvm-target=x86_64-apple-darwin \
  --use-cxx-module=n \
  --static=non-system \
  --execution-jit=none \
  --march=none \
  --openssl-root="$PWD/build/deps/openssl-src-x86_64" \
  --ccache=n \
  --cxflags="-mavx2 -mbmi -mbmi2" \
  --ldflags="-nostdlib++"
xmake -j1 uwvm
cp build/macosx/x86_64/release/uwvm build/x86_64_AVX2-apple-darwin-uwvm2
```

Check that only Apple system libraries remain dynamic:

```shell
file build/aarch64-apple-darwin-uwvm2
otool -L build/aarch64-apple-darwin-uwvm2
arch -x86_64 build/x86_64_AVX2-apple-darwin-uwvm2 --version
```

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
xmake f -m release --use-llvm-compiler=y --apple-platform=IOS_18

# tvOS
xmake f -m release --use-llvm-compiler=y --apple-platform=TVOS_18

# watchOS
xmake f -m release --use-llvm-compiler=y --apple-platform=WATCHOS_11

# visionOS
xmake f -m release --use-llvm-compiler=y --apple-platform=VISIONOS_2

# Custom version (example)
xmake f -m release --use-llvm-compiler=y --apple-platform=ios:17.0
```

Then build and install as usual:

```shell
xmake
xmake i -o <install_path>
```

## Caveat
1. Add `--use-llvm-compiler` when you want to build with the LLVM/Clang compiler toolchain. This only selects the compiler toolchain and does not enable LLVM JIT by itself.
2. Darwin does not support the global `-static` strategy used by `--static=compiler`. Use `--static=non-system` for release builds when your non-platform dependencies, such as LLVM, provide static archives.
