# C# / .NET Mono to WASI Preview 1

This fixture compiles real C# to a standalone WebAssembly core module using
official .NET SDK **8.0.408**, `wasi-experimental` workload/runtime **8.0.15**,
and **wasi-sdk 20.0** (Clang 16.0.0). It uses Mono's IL interpreter compiled to
Wasm; it does not claim NativeAOT or direct C# instruction lowering.

Run all provisioning, compilation, validation, debugger runs, and profiling
inside the existing SSH Linux 64 GiB cgroup. Keep the task directory on a
filesystem with enough room; the toolchains and build need about 1 GiB. Do not
commit the downloaded SDKs or generated Wasm.

```sh
python3 bootstrap.py /tmp/high-level-20261017/csharp
. /tmp/high-level-20261017/csharp/env.sh
./build.sh /tmp/high-level-20261017/csharp/build
```

`bootstrap.py` checks the SDK archive's fixed SHA-512 and wasi-sdk archive's
fixed SHA-256. The workload uses the SDK's bundled manifests with
`--skip-manifest-update`, fixing its packages at 8.0.15. It puts SDK state,
NuGet caches, and temporary files under the supplied task directory.
`DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` lets the SDK run in minimal Linux
containers without ICU. The guest also enables invariant globalization/timezone.

The resulting `build/AppBundle/highlevel-csharp.wasm` contains its managed
assemblies and Mono runtime. No guest filesystem preopen or separate managed
assembly installation is needed. Its only imports belong to
`wasi_snapshot_preview1`: `sock_accept`, `args_get`, `args_sizes_get`,
`environ_get`, `environ_sizes_get`, `clock_res_get`, `clock_time_get`,
`fd_advise`, `fd_close`, `fd_fdstat_get`, `fd_fdstat_set_flags`,
`fd_filestat_get`, `fd_filestat_set_size`, `fd_pread`, `fd_prestat_get`,
`fd_prestat_dir_name`, `fd_read`, `fd_readdir`, `fd_seek`, `fd_tell`, `fd_write`,
`path_filestat_get`, `path_open`, `path_readlink`, `path_unlink_file`,
`poll_oneoff`, `proc_exit`, and `random_get`. Runtime linkage imports APIs that
this fixture does not necessarily call, such as sockets. This is a P1 core
module, not a WASI P2 component and not a browser/JavaScript host bundle.
UWVM must include its real P1 socket exports: configure
`--enable-local-imported-wasip1-socket=wasip1` in xmake, or define
`UWVM_ENABLE_WASIP1_SOCKET` consistently when building it. A host built without
socket support cannot link Mono's `sock_accept` import, even though this
program does not open a socket.

`Program.cs` checks class inheritance and virtual dispatch, arrays and a generic
`List<int>`, UTF-16 strings containing non-ASCII characters, explicit managed
exceptions, bounds exceptions, and checked arithmetic overflow. A deterministic
xorshift kernel prevents a successful hello-world from standing in for language
semantics and supplies a repeatable workload. The default is 20,000 iterations;
an optional first guest argument selects an integer from 1 to 100,000,000 for
profiling. Compare the default standard output exactly with `expected.txt` and
require exit status zero. `cases.json` also fixes the 100,000/1,000,000 iteration
outputs and invalid argument cases (zero, negative, upper bound, nonnumeric,
and Int32 overflow). Invalid arguments print `csharp invalid iterations` to
standard error and exit with status 2; nonnumeric and overflow parsing exercises
the corresponding managed exception handlers.

For debugging, `build.sh` retains the native Wasm function-name section and
DWARF plus the managed portable PDB. .NET 8.0.15's official WASI targets
unconditionally add `-Wl,-s`, even with `WasmNativeStrip=false`.
`retain_symbols.py` relinks the exact generated response-file inputs after
removing that flag and adding `-g`; it records the actual compiler command in
`symbol-link-command.json`. It fails if the pinned recipe changes. This permits
UWVM instruction/function/Wasm-source debugging. A portable PDB by itself does
not provide UWVM with managed C# breakpoints, IL frames, or C# local variables;
those need a Mono debugger protocol integration and must be reported separately.
The retained DWARF describes Mono's native runtime/driver C sources; it is not
a C# source map for `Program.cs`.

`reference_results.json` records the 2026-10-17 Linux cgroup reference checks:
all eight cases passed both native .NET 8.0.15 and Wasmtime 42.0.1. UWVM,
debugger, and VTune results belong to the suite's common report; the independent
references do not substitute for those checks.

`run_entry_edges.py` generates small standard core Wasm fixtures for runtime
entry regressions independently of .NET. It checks a typed i32/i64 explicit
entry after initialization, a start through two preloaded import aliases,
same-index execution once, and init-time `proc_exit(17)` in ordinary and managed
debugger execution. Its debugger check retains the exit status, performs normal
quit, waits for actual CLI retirement, and checks terminal restoration. Run with
`--out` pointing at a new task directory, `--expected-cgroup` identifying the
existing 64 GiB cgroup, `--uwvm` identifying the candidate, and optionally
`--wasmtime` for an independent reference. Use `--runtime-tests` to locate the
repository's `test/0017.runtime` console harness when running a copied script.
For the same-index reference, Wasmtime explicitly invokes a separate check
export so its automatic command invocation does not intentionally call the same
function twice; UWVM's default entry policy requires that index to run once.

`HighLevelCSharpTimed.csproj` explicitly compiles `ProgramTimed.cs`; the original
project excludes it. Build with `build-timed.sh /absolute/target/build-timed`.
The separate `highlevel-csharp-timed.wasm` runs the same semantic checks, warms
`Kernel(1)` and the clock, then brackets only the selected kernel with
`Stopwatch.GetTimestamp`. It prints the original checksum line followed by
`csharp kernel iterations=N ticks=X frequency=Y`; divide ticks by frequency for
seconds. Mono startup, semantic checks, method resolution and initial clock
resolution precede the timer. `timed_reference_results.json` records matching
native .NET and Wasmtime checksums for 1000, 20000, and 1000000 iterations. Those
concurrent correctness-run timing observations are not a controlled performance
comparison. The timed build does not rewrite the semantic Wasm.

`perf-timed-cases.json` supplies the 1,000,000-iteration ordinary case and
20,000-iteration debugger/hardware case, with the full semantic output check,
monotonic tick clock, and guest-reported frequency. Keep the original
`timed_reference_results.json` as historical evidence when rebuilding after a
host restart. `recover.sh /absolute/arena /absolute/wasmtime` rebuilds the pinned
semantic/timed/native/C-ABI artifacts, retains native Wasm symbols and portable
PDBs, and writes fresh native/Wasmtime results under `arena/recovery/`. Run it
only in the existing admitted Linux cgroup; it does not launch any UWVM binary.
Build and reference logs belong to the recovery directory. Download archives
are removed after checksum verification and extraction; toolchains, symbols,
and compiler response inputs remain available for debugger/profiler validation.
The post-reboot rebuilt artifact identities and 22 reference checks are recorded
in `recovery/reference_results.json`; the timed performance manifest names this
rebuilt artifact. The original reference JSON files retain their historical
artifact hashes and do not qualify a later rebuilt binary.

Official upstream references:

- [.NET 8 WASI execution model](https://github.com/dotnet/runtime/blob/v8.0.15/src/mono/wasi/README.md)
- [.NET WASI workload overview](https://devblogs.microsoft.com/dotnet/extending-web-assembly-to-the-cloud/)
- [Pinned WASI build targets and unconditional strip flag](https://github.com/dotnet/runtime/blob/v8.0.15/src/mono/wasi/build/WasiApp.Native.targets)
- [Pinned wasi-sdk release](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-20)
- [Pinned Stopwatch Unix timestamp and frequency](https://github.com/dotnet/runtime/blob/v8.0.15/src/libraries/System.Private.CoreLib/src/System/Diagnostics/Stopwatch.Unix.cs)
- [Pinned native timestamp uses CLOCK_MONOTONIC](https://github.com/dotnet/runtime/blob/v8.0.15/src/native/libs/System.Native/pal_time.c)
