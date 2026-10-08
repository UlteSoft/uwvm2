# MIPS64 native-debug local branch ownership

The native debugger owns the exact emitted Wasm function, not its ELF text
section. On static N64 pre-R6 MIPS, LLVM RuntimeDyld redirects even local
`R_MIPS_26` jump relocations through section-tail trampolines. An internal
`J` can therefore leave the proved function. The debugger must refuse that
transfer; the trampoline must never become a code, register or memory grant.

The debug provenance producer attaches `uwvm.native.debug.pcrel` only to
its instrumented functions. The MIPS branch-expansion pass converts one-MBB
local `J` instructions to PC-relative `B` before the existing range pass.
Long-branch expansion, the static C host-call ABI, calls, unmarked functions,
MIPS16, microMIPS and R6 paths retain their existing behavior. This change
does not authorize indirect jumps, returns, delay-slot control transfers,
native stack reads or function-external stubs.

## Build the LLVM library used by the JIT

Ordinary uwvm2 needs the matching external LLVM source repair:
[llvm23-linux-mips64-debug-pcrel-branches.patch](../../tools/ci/patches/llvm23-linux-mips64-debug-pcrel-branches.patch).

From an exact matching LLVM 23 monorepo checkout, first check and apply the
patch, with `UWVM_SOURCE` set to this repository's absolute path:

```sh
patch --dry-run -p1 < "$UWVM_SOURCE/tools/ci/patches/llvm23-linux-mips64-debug-pcrel-branches.patch"
patch -p1 < "$UWVM_SOURCE/tools/ci/patches/llvm23-linux-mips64-debug-pcrel-branches.patch"
```

Rebuild the target's actual LLVM library and relink uwvm2 against it. Patching
Clang, copying headers or changing a capability macro does not repair the JIT
backend. Keep the other required native-owner/loader/ABI repairs in that same
qualified LLVM build. Use a fresh object cache or `-Rllvm-cache-path disable`
when checking the rebuilt product.

Enable `--linux-native-debug=y` in the existing Linux LLVM-JIT/Clang xmake
configuration (`--execution-jit=llvm --use-llvm-compiler=y`). Runtime and CLI
must use the same owner-table setting and qualified SDK. This explicit build
option is required; passing `-Rdbg` to an older or unqualified binary does not
add a native trap/owner backend.

uwvm2-ros already carries this change in its bundled
`llvm/lib/Target/Mips/MipsBranchExpansion.cpp`. Build that source through the
normal bundled-LLVM path; do not apply the external patch twice.

## Regression checks

Run all compilers, LLVM tools, QEMU processes and runtime tests in the existing
64 GiB Linux test cgroup. Preserve the source/library hashes, exact arguments,
logs and process-retirement record.

`test/0017.runtime/native_provenance_numeric_mcjit.cc` creates and relocates
an actual conditional branch function. The marked N64 function must have
direct branches and zero function-external direct targets. Its unmarked
control must retain RuntimeDyld's trampoline behavior. These component checks
do not execute the generated branch probe or grant debugger authority.

Qualify the actual product separately with
`debug_native_linux_runtime.cc` and
`run_debug_native_cross_linux_cli.py`: require real `si` and `ni`
transitions, positive i64/f32/f64 register values, the large i64 constant and
result, hidden stack/runtime storage, and retained stops on refused transfers.
An all-unavailable view, component-only result or successful assembly listing
does not satisfy that product check.
