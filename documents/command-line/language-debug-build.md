# Select language debugger adapters at build time

Language runtime support and debugger adapters have separate build contracts.
These options select debugger code; they do not enable ROS imports, grant WASI
or ROS permissions, load a language runtime, or certify usable source debugging.
The shared Wasm debugger and raw DWARF machinery remain independent.

## Build exactly the languages you need

Use an explicit list to override every previously saved individual switch:

```sh
xmake f -c -m release --debug-languages=python
xmake -j2
```

This defines `UWVM_DEBUG_LANGUAGE_PYTHON=1` and all fourteen other language
macros as `0`. Selecting Python does not automatically select C++, Java, Mono,
Go, Rust or another interpreter. A list may contain several exact names:

```sh
xmake f -c -m release --debug-languages=java,csharp
xmake f -c -m release --debug-languages=none
xmake f -c -m release --debug-languages=all
```

`none` removes all language adapter selections. `all` is an explicit request
for all selections. Unknown, duplicate or empty list entries are errors.
Names are lowercase and whitespace is not silently discarded.

The default selection mode is `individual`; every individual switch defaults
to `y`, so a fresh configuration enables all fifteen adapters. In that mode
each adapter can be disabled independently. The following disables only Python
and leaves the other fourteen enabled:

```sh
xmake f -c --debug-languages=individual \
  --debug-language-python=n
```

An explicit list, `none` or `all` takes precedence over saved individual
booleans. Switch back to `individual` before editing those booleans again.
The installed xmake may reset unspecified individual options during a new
`xmake f` invocation. Repeat the required individual switches explicitly; an
explicit language list fixes all fifteen output macros regardless of that cache.

| Name | Individual xmake option | Compiler macro |
| --- | --- | --- |
| c | `--debug-language-c=y/n` | `UWVM_DEBUG_LANGUAGE_C` |
| cpp | `--debug-language-cpp=y/n` | `UWVM_DEBUG_LANGUAGE_CPP` |
| rust | `--debug-language-rust=y/n` | `UWVM_DEBUG_LANGUAGE_RUST` |
| go | `--debug-language-go=y/n` | `UWVM_DEBUG_LANGUAGE_GO` |
| tinygo | `--debug-language-tinygo=y/n` | `UWVM_DEBUG_LANGUAGE_TINYGO` |
| zig | `--debug-language-zig=y/n` | `UWVM_DEBUG_LANGUAGE_ZIG` |
| java | `--debug-language-java=y/n` | `UWVM_DEBUG_LANGUAGE_JAVA` |
| csharp | `--debug-language-csharp=y/n` | `UWVM_DEBUG_LANGUAGE_CSHARP` |
| fsharp | `--debug-language-fsharp=y/n` | `UWVM_DEBUG_LANGUAGE_FSHARP` |
| vb | `--debug-language-vb=y/n` | `UWVM_DEBUG_LANGUAGE_VB` |
| python | `--debug-language-python=y/n` | `UWVM_DEBUG_LANGUAGE_PYTHON` |
| javascript | `--debug-language-javascript=y/n` | `UWVM_DEBUG_LANGUAGE_JAVASCRIPT` |
| assemblyscript | `--debug-language-assemblyscript=y/n` | `UWVM_DEBUG_LANGUAGE_ASSEMBLYSCRIPT` |
| moonbit | `--debug-language-moonbit=y/n` | `UWVM_DEBUG_LANGUAGE_MOONBIT` |
| kotlin | `--debug-language-kotlin=y/n` | `UWVM_DEBUG_LANGUAGE_KOTLIN` |

Outside xmake, each undefined language macro defaults to `1` as well. Define
all fifteen explicitly when building a minimal selection by hand. All macros
accept exactly `0` or `1`; other values fail compilation. The same
resolved configuration reaches the frontend and runtime targets through
`def_build`, including named-module builds. The C++ coroutine partition is
omitted when C++ is disabled, including its header and module imports.
The existing C++ stock pretty-printer extensions and Rust stock formatter are
conditionally compiled. Disabled native-language expression profiles refuse
language-specific parsing. Go and TinyGo retain separate producer selection.

A switch is an admission point for an adapter, not evidence that its producer,
logical frames, GC integration or debugger UI is complete. In particular the
managed/interpreter-backed adapters remain under development. Enabling a flag
must never be reported as a successful language debugging test.

## CPython guest snapshot bridge

The optional ROS CPython embedding accepts
`-DUWVM_DEBUG_LANGUAGE_PYTHON=1`. Its legacy
`UWVM_PYTHON_DEBUG_SNAPSHOT` spelling is retained; supplying both with different
values is a compile error. With both absent, the snapshot hook is off.

The guest interpreter and Host VM are separate builds. Pass the selected guest
macro to the matching CPython/WASI compilation and link the actual provider
objects only when selected. Enabling a Host xmake flag does not manufacture
those guest objects or qualify their ABI. Ordinary Python programs do not need
this optional hook.

## Compile and runtime checks

`build-options/language_debug_probe.cc` exercises the actual scalar-language
admission, separate Go/TinyGo metadata gates and C++/Rust stock formatter gates.
It checks that disabled profiles refuse work and do not call the memory reader.
An external matrix supplies `UWVM_TEST_EXPECT_MASK` to verify compiler defines.

Actual validation belongs in the existing SSH Linux 64 GiB cgroup with swap
zero. Capture the original source, compiler, xmake configuration, command lines,
exit status and outputs. Build into a new owned directory; retain failed cases.
Compile the adapter probes and full VM separately. A probe PASS is not a full
CLI build, Wasm guest run, ROS delivery or usable debugger qualification.

The earlier adapter matrix (captured before the defaults changed to enabled) covers all 64 combinations of the six native
profiles, each of the nine additional language profiles alone, and all profiles
together, in Debug and Release for each repository. That is 444 builds and
444 executions. It does not claim exhaustive coverage of all 32,768 possible
15-bit selections. Separate xmake checks must cover explicit-list precedence,
individual switches and malformed configuration rejection. Full VM builds must
also cover backend, module/header and ROS configurations independently.

## Reproduce an adapter check without the full product dependency graph

From the repository root, use the standalone test project:

```sh
XMAKE_STATS=false xmake f -P test/0050.high_level_languages/build-options \
  -c -m debug --ccache=n --debug-languages=python \
  --debug-language-cpp=y --matrix-expected-mask=1024
XMAKE_STATS=false xmake build -P test/0050.high_level_languages/build-options \
  -j1 language_debug_probe
xmake run -P test/0050.high_level_languages/build-options language_debug_probe
```

The expected marker is `LANGUAGE_BUILD_SELECTION_OK 1024`: the explicit Python
list overrides the simultaneously enabled individual C++ switch. Use release
mode and `--debug-languages=cpp,rust --matrix-expected-mask=6` for the mixed
formatter profile. This test project imports the real option module, feature
header and adapter headers. It checks all five C++ extension headers as direct
includes. Its success is scoped to these adapters; it does not certify the full
CLI, named modules, a managed runtime's debugging, ROS delivery or performance.

See the [actual Linux matrix records](../../test/0050.high_level_languages/results/native-budget/language-debug-options-20261018/README.md)
for source identities, command/output records, probe sizes and remaining product
configuration failures. All executions in that record use the existing SSH
Linux 64 GiB cgroup with swap disabled.

The [interpreter product and language runtime continuation](../../test/0050.high_level_languages/results/native-budget/language-runtime-control-edges-20261019/README.md)
records later full header/interpreter product selections and unchanged Java,
C#, Python, JavaScript, AssemblyScript and MoonBit producer artifacts. Read its
exact frozen-source and backend scope separately from the adapter-only checks.

The [default-on matrix and actual ROS VM sizes](../../test/0050.high_level_languages/results/native-budget/language-defaults-on-size-20261019/README.md)
record the later default change: 210 adapter builds/runs, 99 macro checks,
ten interpreter products and 380 retained Guest expected outcomes. Earlier
default-off records remain historical. Read the explicit backend, module,
frozen-source and measurement boundaries before comparing sizes.
