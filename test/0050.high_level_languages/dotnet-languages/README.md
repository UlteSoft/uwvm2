# F# and Visual Basic WASI producer examples

These are real F# and Visual Basic applications. Both check virtual class
dispatch, arrays, generic collections, UTF-16 strings, UTF-8 conversion,
exception matching, checked overflow, command arguments and a deterministic
32-bit checksum. F# also checks records, discriminated unions, pattern matching
and a functional sequence reduction. VB uses strict typing and a dictionary.
They need no ROS imports, mounted directories, clocks or random source.

The pinned route uses .NET SDK 8.0.408, its F# 8.0.403 package, the .NET 8.0.15
`wasi-experimental` workload and wasi-sdk-20.0. The SDK includes both language
compilers. WASI output bundles the managed assemblies with the Mono runtime;
this is managed IL interpreted inside a Wasm module, with native runtime code
compiled to Wasm. It is not direct F#/VB native AOT. The native reference uses
CoreCLR, so equal results do not establish equal compiler strategies or speed.
The upstream [Mono WASI documentation](https://github.com/dotnet/runtime/blob/v8.0.15/src/mono/wasi/README.md)
describes the experimental workload. Use the pinned versions when reproducing
these fixtures; a newer SDK/workload needs separate qualification.

## Build and run

Install/select the SDK and workload before building. Set these paths to your
installed tools, and use separate absolute output directories for every
language, configuration and target:

```sh
export DOTNET_ROOT=/absolute/path/to/dotnet-8.0.408
export WASI_SDK_PATH=/absolute/path/to/wasi-sdk-20.0
bash build.sh fsharp Release wasi /absolute/output/fsharp-release-wasi
bash build.sh vb Debug wasi /absolute/output/vb-debug-wasi
bash build.sh fsharp Release native /absolute/output/fsharp-release-native
bash build.sh vb Debug native /absolute/output/vb-debug-native

"$DOTNET_ROOT/dotnet" /absolute/output/fsharp-release-native/publish/highlevel-fsharp.dll
"$DOTNET_ROOT/dotnet" /absolute/output/vb-debug-native/publish/highlevel-vb.dll 17
uwvm --run /absolute/output/fsharp-release-wasi/AppBundle/highlevel-fsharp.wasm
uwvm --run /absolute/output/vb-debug-wasi/AppBundle/highlevel-vb.wasm 17
```

Select the ordinary execution backend required by your UWVM build. For the
recorded manual LLVM products, ordinary `uwvm2` uses `-Rcm full -Rcc jit` and
`uwvm2-stable`/`uwvm2-ros` use `-Raot`. Put runtime options before `--run`.
The standalone Wasm modules do not need a Java-style argument/environment
compatibility switch. The application receives its own arguments; the runtime
program name is not its first managed argument.

With no arguments the program uses 1000 iterations. `17` selects 17 iterations.
A malformed integer, zero, a negative/out-of-range value or more than one
argument prints `<language> invalid iterations` to stderr and exits with status
2. Valid output lists all checked semantic values and the unsigned checksum.
Both native and Wasm builds use the same language source. Run both Debug and
Release configurations when changing a compiler, runtime or fixture.

To reproduce offline, set `HLL_NUGET_CONFIG` to a NuGet configuration whose
package sources contain only the SDK's `FSharp/library-packs` directory and an
existing `Microsoft.NET.ILLink.Tasks` 8.0.15 package directory. Set
`NUGET_PACKAGES`, `DOTNET_CLI_HOME` and `TMPDIR` to writable, bounded build
directories. The Linux qualification uses those local package sources and a
512 MiB CoreCLR GC heap limit. The limit is a test resource profile, not a
universal deployment requirement. Publishing does not install a workload.

## Debug information and host language options

Both configurations emit portable PDBs. The pinned .NET WASI linker target
unconditionally passes `-Wl,-s`; `retain_symbols.py` checks that exact recipe,
removes that one strip flag, relinks the same inputs and retains the PDB.
Unexpected linker recipes are rejected. Preserve the sources, PDB, final Wasm
and generated link command together.

Use `-Rdbg` with the backend appropriate to your LLVM product to inspect common
Wasm entry/state, functions, instructions, stack, memory and native-runtime
DWARF. A portable PDB describes managed IL; retaining it does not implement a
host bridge for F# records, discriminated unions, VB objects or managed locals.
Those language-level capabilities need separate actual qualification. Producer
construction and Wasmtime reference checks alone do not certify UWVM DBG.

All fifteen host debugger-language options default on. Keeping all of them
enabled is recommended, especially because these modules contain C/C++ runtime
frames. For a constrained build, `--debug-language-fsharp=n` or
`--debug-language-vb=n` under `--debug-languages=individual` trims that host
specialization wherever implemented; it does not reject legal F#/VB Wasm,
remove Mono, change WASI authority or disable ordinary execution. Rebuild all
translation units consistently. See the complete
[language-option guide](../../../docs/debugger/language-options.md).

All qualification compilation, execution, debugging and performance work for
this task runs on SSH Linux in the original 64 GiB cgroup with swap disabled.
Untimed correctness runs and their durations are separate from native/VTune
performance results. These basic examples make no ROS SDK/API parity claim.
