This is a generic Mono WASI managed-to-native C ABI proof project. It is
prepared statically and has not been built or executed yet. It contains no
ROS imports or ROS success claim. Its sibling location and explicit Compile
items keep it outside the original C# project's default source glob.

The pinned toolchain is .NET SDK 8.0.408, WASI runtime pack 8.0.15 and
wasi-sdk 20.0 ([LLVM 16](https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-20)). Use the already installed tools; the builder neither
downloads tools nor patches their targets. Its empty local NuGet feed prevents
network fallback. If installed workload/reference packs or an existing package
cache are missing, restoration must fail; do not install or edit the SDK as a
workaround. `--nuget-packages` may point to the existing pinned package cache;
otherwise the existing NUGET_PACKAGES or user cache is reused.

The private target runs after `_GetNativeFilesForLinking` and before
`_GenerateManagedToNative`, adding the already compiled `hll_native_bridge.o`
to `_WasmNativeFileForLinking` with `ScanForPInvokes=true`. The object's filename
supplies the `hll_native_bridge` module matched by DllImport. The same item is
also consumed by final linking. This deliberately avoids relying on the
commented-out automatic NativeFileReference link item in this pinned SDK.
These are version-specific private hooks; fail and review them if the SDK
changes. [Pinned WASI targets](https://raw.githubusercontent.com/dotnet/runtime/v8.0.15/src/mono/wasi/build/WasiApp.Native.targets),
[pinned P/Invoke generator](https://raw.githubusercontent.com/dotnet/runtime/v8.0.15/src/tasks/WasmAppBuilder/PInvokeTableGenerator.cs).

The C contract is:

```c
uint64_t hll_bridge_copy(int32_t memory_index, uint64_t input_offset,
    uint64_t input_length, uint64_t output_offset, uint64_t capacity);
uint64_t hll_roundtrip_u64(uint64_t value);
```

Managed declarations use int and ulong, including the return values. The copy
returns token 0xFEDCBA9876543210 on success and zero on rejected numeric ranges.
Only memory 0 is accepted. The wasm32 implementation checks both extents for
64-bit overflow and against `memory.size(0)` before narrowing any address.
Offsets are memory-relative guest offsets, never host/managed object addresses.
The managed side uses AllocHGlobal and Marshal.Copy in this same linear
memory. Its pointer conversion explicitly zero-extends wasm32 ToInt32 bits.

The fixture checks a 13-byte UTF-8 payload containing an embedded NUL and
non-ASCII characters against explicit expected bytes. It verifies guards and
unchanged input, cap-short and memory 1 rejection, input/output overflow, and
each uint64 argument's upper 32 bits. Five separate ulong values include zero,
one, the token, bit 63 and UINT64_MAX. Native Linux uses the same C source and
managed Program with actual allocated 64-bit pointers. Its final output
explicitly says that wasm32 address-bound checks are not applicable; only the
WASI executions certify those checks. A conversion-only pointer sentinel
0x80000010 exercises signed-ToInt32 zero-extension without dereferencing it.
No allocations or valid memory accesses above 2 GiB are claimed.

After the coordinator releases the original 64-GiB cgroup, build inside
`docker exec ... chroot /host` on SSH Linux. The following arguments are a run
plan; replace tool/output placeholders with existing admitted absolute paths:

```sh
python3 csharp-native-interop/build.py \
  --dotnet-root DOTNET_8_0_408_ROOT \
  --wasi-sdk PINNED_WASI_SDK_20_0 \
  --out /absolute/new-interop-build --cpu 8

python3 csharp-native-interop/run.py \
  --build /absolute/new-interop-build \
  --arena /tmp/high-level-20261017 \
  --runtime-sdk EXISTING_UWVM_RUNTIME_SDK \
  --wasmtime /home/macromodel/.wasmtime/bin/wasmtime \
  --out /absolute/new-interop-runs --cpu 8
```

The builder validates the real generated P/Invoke module table and both
uint64 prototypes, the exact absolute bridge object in clang-compile.rsp, and
the hook's recorded item metadata. It reuses
`../csharp/retain_symbols.py` to relink the original response arguments with
the sole strip option removed, debug enabled and the AppBundle output selected;
the resulting command must match that precise transformation. Headers/rsp,
symbol link command, raw logs and hashes remain available. The Wasm must have
only WASIp1 imports, a command entry, retained names/DWARF and both native
functions. The builder also runs the independent native shared-library oracle;
its build receipt explicitly does not qualify Wasm execution.

The runner then performs exactly four ordinary cases using that same Wasm:
three current UWVM products and pinned Wasmtime 42.0.1. It verifies full stdout
and normal exit, immutable Wasm/product hashes, and owned-process retirement.
Both scripts require the original exact Docker cgroup with 64 GiB/swap 0, use
RLIMIT_CORE=0, a shared benchmark lock, pidfd cleanup, finite timeouts, an
8 GiB leader RSS/56 GiB shared-anon budget and a 512 MiB disk reserve. Guard or retirement failure stops further
work. Only owned processes are signalled; no artifacts are deleted.

Generated `cases.json` can be appended through prepare_cases.py after the real
build/native oracle succeeds. It defaults to skip_debug=true. A Wasm DBG run
or C-shim DWARF check must be separately requested and qualified; earlier Mono
Wasm DBG results do not prove C# PDB/source-local support for this new module.
No VTune profile or new performance claim is part of this proof.
