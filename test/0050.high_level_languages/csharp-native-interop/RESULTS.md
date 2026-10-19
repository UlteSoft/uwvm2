The generic Mono WASI C ABI proof passed the native Linux oracle and all four
ordinary Wasm executions on 2026-10-17 UTC. The source and planning README
recorded in `work-r2/sources/` remain unchanged so the receipts can be checked
against their original inputs. This later result is kept separately.

| Execution | Result | Exact stdout | Normal exit | Owned processes retired |
| --- | --- | --- | --- | --- |
| .NET 8.0.15 native Linux + the C shared library | PASS | native fixture | 0 | yes |
| uwvm2, current final product | PASS | WASI fixture | 0 | yes |
| uwvm2-stable, current final product | PASS | WASI fixture | 0 | yes |
| uwvm2-ros, current final product | PASS | WASI fixture | 0 | yes |
| Wasmtime 42.0.1 | PASS | WASI fixture | 0 | yes |

All builds and executions used SSH Linux, the original Docker cgroup
`docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope`,
memory.max 68719476736, swap.max 0, CPU 8 and core dumps disabled. Boot was
`33464c92-7dae-4410-b9f7-92e3edf249b9`. Cgroup memory event counters remained
zero. The final driver ran 17:04:02–17:05:25 UTC; all five command identities
and its driver had disappeared when the retirement receipt was written. The
shared benchmark lock was observed free after completion.

The same Wasm bytes were used in all four engines. Its SHA-256 is
`9146595a544c878bb68c0dd0392db88879cf431800bb906e7b32f204ea62d77c`.
The bridge object's SHA-256 is
`1ae83d01fdf00d314e37b24bbe7f51a78c75574f25e13f2bf12c938e0cfb321c`.
The ordinary execution receipt SHA-256 is
`08b88de406e0f92386438245a9c32f91c76c0a7e060e517705d0dd8ef838e469`.

The generated P/Invoke table contains the actual `hll_native_bridge` module,
the copy function's int32 + four uint64 parameters and uint64 return, and the
independent uint64 roundtrip. The recorded item has ScanForPInvokes=true;
the exact object occurs once in the real linker response file. The final
Wasm retains names/DWARF and both bridge function names, and imports only
WASIp1. The original SDK targets were neither modified nor replaced.

The runtime fixture verified the high-bit token 0xFEDCBA9876543210, five ulong
roundtrips, exact UTF-8 plus embedded-NUL copying, unchanged input and guards,
and all rejection cases without writing the canary. WASI additionally checked
the high 32 bits of numeric arguments and signed-pointer zero-extension. The
0x80000010 pointer sentinel was never dereferenced; no valid allocation or
access above 2 GiB is claimed. Native Linux verifies the same C/managed ABI
with real allocated pointers; its fixture explicitly excludes wasm32 bounds.

The retained local evidence is in `evidence-r2/`:

- `work-r2/receipt.json`, `sources/` and `commands/`: successful build and
  native oracle, exact tool/source identities and raw logs.
- `work-r2/link-evidence/`: real `pinvoke-table.h`, injected item metadata,
  clang response file, retained-symbol relink command and Wasm inspection.
- `work-r2/wasi/AppBundle/hll-native-interop.wasm`, `bridge/` and `native/`:
  the tested small deliverables. Large SDKs and build intermediates stay on
  Linux.
- `references-r2/receipt.json` and `commands/`: all four ordinary executions.
- `references-r2.launcher.json` and `references-r2-retirement.json`: launcher
  admission/exit and the final identity/lock observations.
- `evidence-transfer-inventory-r2.json`: Linux-produced sizes/SHA-256 for all
  110 copied files (10,999,438 bytes), checked against the local copies.
- `work-r1/` and `references-r1/`: failed attempts retained unchanged. The
  first build's overstrict DOTNET_ROLL_FORWARD=Disable prevented the pinned
  ILLink servicing runtime from resolving release 8.0.15. The final builder
  removes that global override and requests 8.0.15 explicitly for the native
  oracle. The first reference attempt stopped on the shared lock before any
  VM started. Neither attempt was promoted to success.

The original Linux arena paths are
`/tmp/high-level-20261017/csharp-native-interop/work-r2` and
`/tmp/high-level-20261017/csharp-native-interop/references-r2`.
For a new run, use new output directories and the build/run commands in the
planning README, with the existing `csharp/dotnet`, `csharp/nuget` and
`csharp/wasi-sdk-20.0` paths from the arena's `csharp/env.sh`.

This qualifies ordinary generic managed-to-native C ABI calls in one WASIp1
memory. ROS imports/functions, source debugging, C# PDB locals and VTune were
not exercised by this proof. The recorded wall durations include cold loading
and compilation and are functional-test observations, not performance results.
