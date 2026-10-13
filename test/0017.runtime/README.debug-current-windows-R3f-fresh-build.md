# Fresh current R3f Windows x64 build recipe

This is a source recipe for the sole keeper. It has not been executed, and it does not qualify an SDK, executable or Windows VM. It uses the existing task cgroup entry and the current R3f staging/Job pipeline; it introduces no resource supervisor and no ARM64 EH gate.

Use the immutable ordinary R3c source `sha256:583499a13609fe3ba5dd98b4317a8045528474364ea7a6904df1217c39716b3d` or the ROS R3f source `sha256:bf7328bcebd44d7dea6d97ce38525ad3f72ed3ddd4c2382bfe9149be37133845`. Materialize the new R3f staging wrapper and its pinned helpers as test tooling, without replacing frozen production files. The helper hashes and actual R3e/R3f lineage hashes are in README.debug-current-windows-r3f.md. Old R10, R3d/R3e and Linux objects are excluded.

The historical Windows scripts record these actual locations, which must be inventoried again before admission rather than assumed still qualified:

- Clang/LLD: `/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/{clang++,ld.lld}`.
- MinGW sysroot: `/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M/windows-sdk`.
- LLVM base: `.../wasm3-resume-20260924/llvm-win64-ros9-recovered`; the independent LLVM Support repair candidate is `.../builds/windows-current-r10-cofffix-sdk-repair-20261002-r1`.
- Historical SSL: `/home/macromodel/.xmake/packages/o/openssl/1.1.1-w/fe6328eec5694b1fb0d5264984e29d7a`.

The old scripts and repaired-Support receipt are discovery evidence only. Validate the real latest SDK certificate, source derivation, generated `llvm/Config/llvm-config.h` version 23, all consumed headers and ordered archive closure, including `libLLVMDebugInfoDWARF.a`, X86 and MCDisassembler. Run the official `llvm-readobj --file-headers` archive oracle and preserve its actual direct argv/tool/archive/log hashes; every member must be AMD64 COFF. The Support repair must be represented by its actual qualified archive and source derivation, with the unchanged base archives proven. No successful Linux SDK/archive can replace this check. If current DWARF libraries are absent, stop the product build and first rebuild/qualify that missing target-native closure using the existing LLVM builder; do not hide unresolved symbols with a stub.

Run all actual compilers serially in the existing admitted 64GiB/no-swap cgroup (20 allowed CPUs, the agreed E-core build subset). Call the source snapshot's `tools/ci/require_wasm3_test_cgroup.sh` first. The keeper's existing owner/drain machinery must observe the complete build tree, no competing performance/VM lane, unchanged OOM counters and complete retirement. Do not add a second cgroup or new guard implementation.

Construct one direct Clang common argv for all three product TUs:

```text
CLANG++ --target=x86_64-w64-windows-gnu --sysroot=ACTUAL_SYSROOT
  -nostdinc++ -isystem ACTUAL_SYSROOT/include/c++/v1
  -isystem ACTUAL_SYSROOT/x86_64-w64-mingw32/include
  -std=c++26 -stdlib=libc++ -fexceptions -fno-rtti
  -fasynchronous-unwind-tables -O3 -g0 -Wno-undefined-inline
  EXACT_COMMON_MACROS
  -I ACTUAL_SSL/include -I ACTUAL_GENERATED_LLVM/include
  -I ACTUAL_MATCHING_LLVM_SOURCE/llvm/include -I CURRENT_SOURCE/src
  -I CURRENT_SOURCE/third-parties/bizwen/include
  -I CURRENT_SOURCE/third-parties/fast_io/include
  -I CURRENT_SOURCE/third-parties/boost_unordered/include
```

This is an argv specification, not a shell-expanded command or proof that paths exist. The macro vector must use the actual prior admitted Windows product configuration with its exact interpreter/LLVM/version/cache features, updated only for the genuine current source ID and the agreed current qualification profile. Required product profile: `UWVM_USE_LLVM_JIT`, `UWVM_USE_UWVM_INT`, `UWVM_USE_THREAD_LOCAL`, `UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=1`, and `UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=1` identically in main, runtime and host provider. If the chosen admitted profile omits a test capture gate, omit it in all three and record that narrower profile instead; do not label it the capture-enabled candidate. Include the real SSL cache-verifier define if using that provider. All experimental GC/private-leaf gates remain genuinely absent in this baseline. Do not add `UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT` or the Mach x64 gate.

The source identity macro is `UWVM2_BUILD_SOURCE_ID=u8"sha256:..."`; its actual original argv token must preserve the quotes as part of the C++ literal. Never reconstruct it using shell interpolation. All nonidentity macros (including every `UWVM2_` test/capture flag), undefines, target, include order, forced includes, sysroot and C++ ABI/EH flags must pass the unchanged `windows_debug_current_layout_contract.py` for every actual consumed product TU. Do not include standalone tests/launcher records in that product list.

Use that same common argv followed by each exact source and a distinct new `-o` with `-c`:

1. `src/uwvm2/runtime/lib/uwvm_runtime.default.cpp` -> new `runtime.obj`.
2. `src/uwvm2/uwvm/main.default.cpp` -> new `main.obj`.
3. `src/uwvm2/uwvm/host_api.default.cpp` -> new `host_api.obj`.

Capture actual original direct argv, cwd, compiler SHA, source/dependency SHA and depfile, return code, raw stdout/stderr, kernel resource/time record and resulting object SHA for each command. Stop at its first failure. Do not reuse the R3e Linux runtime, R3c Linux native unit, historical PE or old Windows object.

Link only those three new objects with the real target-native libc++/libc++abi, GNU SEH unwinder, Windows system imports and the qualified ordered LLVM/DWARF closure. Preserve the existing required Support repair, actual compiler-rt builtins and crypt32 closure from their real receipts. Preserve the existing `ws2_32`, `ntdll`, `shell32`, SSL and any z/zstd providers actually consumed; inventory full ordinary and delay imports later. The existing historical link selected the console subsystem and 16MiB stack; preserve those product settings. A successful static link is not an import/DLL or guest-execution result.

Flatten the qualified LLVM consumer response into the actual direct link argv before execution and preserve the source response hash and exact expansion. Do not reorder/archive-deduplicate it or silently replace the Support archive. The current staging contract rejects unexpanded response-file argv. Any ld-only tokens must be translated explicitly for the actual driver or use a direct original LLD invocation; preserve that exact command, not a retrospective rewritten command. Record actual rc/log/tool/objects/all linked archives/output hashes.

Build `windows_debug_current_launcher.cc` independently with its actual Unicode entry (`-municode`) and matching Windows API/provider SDK. It is a separate command/record, never a product macro-layout proof. The launcher and existing PowerShell owner use sealed regular files/private input and suspend -> non-breakaway Job assign -> resume, then full tree drain; do not switch stdout/stderr to pipes for convenience.

Emit the existing schema-1 stage build receipt from the actual commands, not a synthesized successful template. It requires the complete original `src` and bundled-dependency fingerprint before/after (same canonical source ID embedded in the PE), every actual input/tool/log/output hash, `product_compiles`, `product_link`, `launcher_build`, LLVM23 config and DWARF archive plus its original COFF oracle. Keep the actual cgroup/cpuset/OOM proof. The original stage and R3f wrapper will verify these, all recursive ordinary+delay non-system DLLs, their PE architecture and the fixed official C5 fixture oracle.

Finally run the exact `stage_windows_debug_current_r3f_vm.py` invocation in its adjacent README with actual product/launcher/tool/receipt paths and each real non-system DLL. Both `qualification.json` and `r3f-admission.json` must pass. Retire the cross-build tree before starting the Windows x64 KVM guest in the same cgroup. First VM slice is the existing five-case `run_windows_debug_acceptance_current_vm.ps1`: official source finish and actual Wasm/native under both policies plus unsupported-mode fatal. Keep actual NTSTATUS, final Job member count and complete retirement. These cases do not yet qualify native disassembly pane, full locals, hot replacement, DAP/IDE/server or midrun attach. Those later cases need separate actual evidence; do not upgrade scope from this recipe.
