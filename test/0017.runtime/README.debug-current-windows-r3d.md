# Current R3 Windows debugger staging

This new source-only preflight keeps the existing regular-output launcher,
non-breakaway Job and five-case Windows driver unchanged. It has not been
executed, compiled or used to start a VM by its author. The old R5 wrapper
and all historical R3/R5 packets remain immutable.

It admits only the actual current source identities reported by the sole
keeper: ordinary `sha256:583499a13609fe3ba5dd98b4317a8045528474364ea7a6904df1217c39716b3d`
and ROS `sha256:ce797300723ff12b4a3253fc0287504dc20103824b54b97eb0f7e5f4abc6985a`.
These identify source content, not a Windows executable or SDK. The R3c
manifest `2970cba891f4741b4b8bee3d11d2c4076c5ffbd2c922040fcc6adb9c0c3d5daf`
has 60 direct, 32 R2 ancestor and 156 R5 dependency records; all are disjoint.
After collecting those pins, only ROS native-code API changes from
`888670c0eda866829024d61e8bb8221826fa6d95bd929cbb22b6ff935f5889ec`
to `a35f0ac277a550b6f85c7945377f9948ad620542bb1489eaa238192e9fe1003a`.
This is its real borrowed-pointer resolver fix, not an ordinary-runtime copy.
The original R3c failure and later ROS main/control dependency failure remain
separate evidence; a future source revision needs a separately reviewed pin.

Materialize exactly that source and the eight pinned staging/oracle/driver
helpers. Build fresh Win64 main/runtime and every consumed host provider
with one actual `--target=x86_64-w64-windows-gnu`, SDK/sysroot,
C++26/libc++/EH/unwind and nonidentity macro/include/ABI prefix.
Windows native stepping must be exact value 1; Clang's bare gate and =1 are
the only accepted equivalent spellings. All product compile records must
match the main/runtime prefix and be consumed by the actual PE link.
Launcher and standalone test compile records remain separate. Keep actual
direct argv, tool/output/source/raw-log SHA, return codes and source
fingerprint before/after. Never borrow current Linux ELF/runtime objects or
SDK archives as Windows qualification.

Use real LLVM23 AMD64 COFF libraries including DebugInfoDWARF and X86/MC
disassembly closure, actual generated llvm-config.h and Windows libc++/
unwind/system providers. The unchanged stage verifies original link inputs,
the complete source/dependency fingerprint, embedded source ID, every
DWARF archive COFF header and recursive ordinary plus delay DLL imports.
Do not replace original records with reconstructed argv.

The sole keeper runs staging inside the admitted 64GiB/no-swap cgroup, after
retiring previous build/test children:

```sh
python3 "$SOURCE/test/0017.runtime/stage_windows_debug_current_r3d_vm.py" \
  --source-root "$SOURCE" --repository ros \
  --r3c-source-manifest "$R3C_MANIFEST" \
  --product "$BUILD/uwvm.exe" --launcher "$BUILD/windows_debug_current_launcher.exe" \
  --build-receipt "$BUILD/current-debug-build-receipt.json" \
  --wasm-clang "$WASM_CLANG" --wasm-ld "$WASM_LD" --wasm-tools "$WASM_TOOLS" \
  --llvm-dwarfdump "$DWARFDUMP" --llvm-readobj "$READOBJ" --out "$NEW_STAGE"
```

Use ordinary for ordinary and add each actual non-system provider with --dll.
A passing `r3d-admission.json` binds the unchanged official qualification
SHA and fixed current source; a legacy R5 admission cannot replace it.
Only after the cross-build tree retires may the keeper start the managed
Windows x64 VM in that same cgroup and run the existing five-case driver.
Keep private stdin, regular stdout/stderr and actual suspend→Job assign→resume,
full NTSTATUS, zero remaining Job members and drain completion.

Those five cases cover official C5 O1 source finish, Wasm and two adjacent
decoded native steps under instruction/unwind, and genuine unsupported-mode
fatal. They do not qualify R3 bounded disassembly panes, full C++/Rust locals,
hot replacement, DAP/IDE/server or midrun attach. Those actual results remain
pending; no source preflight is a product or final platform-matrix PASS.
