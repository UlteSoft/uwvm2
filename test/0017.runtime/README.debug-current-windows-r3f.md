# Current Windows x64 R3f debug-full source admission

This new wrapper preserves the immutable R3d wrapper and the previously reviewed official fixture/COFF/DWARF/DLL staging implementation. It admits the unchanged ordinary R3c source `sha256:583499a13609fe3ba5dd98b4317a8045528474364ea7a6904df1217c39716b3d` and the genuine ROS R3f source `sha256:bf7328bcebd44d7dea6d97ce38525ad3f72ed3ddd4c2382bfe9149be37133845`.

Supply the actual immutable R3c source manifest and the two actual keeper source derivation records. Their required SHA256 values are:

- R3c: `2970cba891f4741b4b8bee3d11d2c4076c5ffbd2c922040fcc6adb9c0c3d5daf`.
- R3e: `e955d1773c450a1cb6276b5e427e14066ee8ed376894ca4fa675df6e217363bb`.
- R3f: `d74657abc7d09bbeba834a9c4757289f7666699678b3b722126e03fdd69a9bae`.

The wrapper verifies the three disjoint R3c sets (60 + 32 + 156), the sole ROS resolver contract correction, the seven reviewed R3e control dependency changes, and the sole R3f routing leaf. All 254 actual R3e rows must remain byte-for-byte metadata-equal in R3f, whose only additional row is `src/uwvm2/uwvm/run/run.h` SHA `fef37d85849006edfc72c677358ec15fcc43af6e61710b207cf53f0152a9407f`. No stored metadata path is opened, no live override is accepted, and no new ARM64 EH gate or Mach candidate is added to this x64 source lineage. The selected source and helper bytes are checked before and after staging.

Run only in the keeper's admitted Linux 64GiB/no-swap cgroup with the original fresh Windows build receipt. This template preserves all original stage arguments after the new lineage arguments:

```sh
python3 test/0017.runtime/stage_windows_debug_current_r3f_vm.py \
  --source-root "$current_source" --repository ros \
  --r3c-source-manifest "$immutable_r3c_manifest" \
  --r3e-source-derivation "$actual_r3e_derivation" \
  --r3f-source-derivation "$actual_r3f_derivation" \
  --build-receipt "$actual_windows_build_receipt" --out "$new_stage_directory" \
  --product "$actual_windows_product" --launcher "$actual_windows_launcher" \
  --wasm-clang "$qualified_wasm_clang" --wasm-ld "$qualified_wasm_ld" --wasm-tools "$qualified_wasm_tools" \
  --llvm-dwarfdump "$qualified_dwarfdump" --llvm-readobj "$qualified_readobj"
```

The unchanged stage's actual arguments must be used; this template is not a build command or synthetic receipt. `--help` on that immutable stage documents any additional required arguments. Its fixed current CLI cases, actual original compiler/link argv and logs, complete original source/dependency fingerprint, source ID embedded in the PE, LLVM23 COFF AMD64 archive headers, actual DWARF archive and generated config, PE imports plus delay-load DLL closure all remain mandatory. Every actual product consumer TU is checked against the same full main/runtime macro/include/sysroot/ABI contract; test and launcher TUs cannot be substituted as product records.

A successful Linux R3e runtime object may be reused only in its separately reviewed Linux R3f recipe. This Windows pipeline requires actual freshly compiled Windows main/runtime/product-provider objects consumed by the real PE link. A Linux ELF, old R5 PE, or successful source admission cannot qualify this Windows product. The parent non-breakaway Job and child suspended creation/assignment/resumption, sealed regular output file and private input, complete owned process tree retirement remain unchanged in the VM driver. Both `qualification.json` and `r3f-admission.json` must pass before VM transfer/execution.

Source-only status: no Windows compilation, SDK/provider qualification, PE/DLL execution, IDE session or machine-step has been run by this author. The first actual acceptance slice is the existing fixed no-import C DWARF5 source finish case in each diagnostic policy, followed by the existing fixed Wasm/native and unsupported-mode cases through the owned VM launcher. These five cases do not yet qualify replacement, DAP/IDE, server or middle-of-run attach. The newly enabled GNU AArch64 native EH source candidate is independent and cannot be qualified by this x64 pipeline.
