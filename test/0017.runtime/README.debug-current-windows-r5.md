# Current R5 Windows debugger: shortest keeper pipeline

These four new files are source-only. The existing r2 six-file packet is
immutable. The author has done AST/source inspection only, not imported or
executed tests, compilers, PE files or a VM. Root reviewed the new layout and
R5 preflight source. A successful source/SDK stage is not a Windows runtime
result; no old r10 PE may qualify current joint stepping.

## First short Linux component slot

Use the existing keeper-owned64GiB/no-swap cgroup, actual CPU set
0,2,4,6,16-31, and the usual birth-bound process-tree supervisor. Do not run
alongside a heavy build, VM or performance measurement. The first slot needs
no compiler and no Windows API. Actual commands are:

```sh
bash "$SOURCE/tools/ci/require_wasm3_test_cgroup.sh"
PYTHONPATH="$SOURCE/test/0017.runtime" python3 -m unittest discover \
  -s "$R2_SNAPSHOT/uwvm2/test/0017.runtime" \
  -p test_windows_debug_current_stage.py -v
PYTHONPATH="$SOURCE/test/0017.runtime" python3 -m unittest discover \
  -s "$NEW_SNAPSHOT/uwvm2/test/0017.runtime" \
  -p test_windows_debug_current_layout.py -v
```

The old suite contains12 real synthetic methods, stageSHA3d0eed667c1c05eb49905add4520f544e6d4922c805332407e0fad7a39746235
and testSHAb8017b1cdd23fd2687203446e2239d7d5aecb5a560bf852f452d2b9def4d40b2.
The six new controls check native-capture/all-namespace macro differences,
forced SDK/header differences, target/EH/response refusal and complete versus
mixed-architecture COFF formatter records. Synthetic output is deliberately
not official SDK or product evidence.

A six-file r2 snapshot alone lacks import dependencies. The actual complete
R5 source supplies run_debug_source_step_cli.py (5f23172f673ec598189b7c9a81643169cb2c7cb157431c0ad192f6230df9705b)
and run_debug_source_inline_metadata_cli.py (bac9075590e9d12117ca1ecc4cb9bb62fc178a466891496f6c2bd78f93ab9f31).
The immutable new snapshot includes those two modules plus the six r2 files,
so either suite can also use its one complete test directory. Preserve
actual executed test/stage/dependency/Python SHA before/after, exact argv,
raw stdout/stderr, method count/return code, actual cgroup/OOM and retirement
records. The author has not run these commands.

## Materialize one current product source closure

Keeper selects ordinary or ROS independently. Materialize the actual
immutable fused-inline-20261002-r10 repository baseline, overlay exactly the
194-record R5 manifest, and append these test-only files. R5 manifest SHA:
5cad7465e846b03e97b181e85a6149711be6a4a14e57f317a0553e1b29a9f4cd.
Every R5 source/test/tool pin for that repository must match. The unchanged
thread cppm/impl baseline SHA closure also must match; the new authenticated
with_stopped_participant header cannot be omitted again. No live compiler,
runtime, controller or private-publisher WIP is pulled into this build.

Capture the complete actual src+third-parties fingerprint before building,
derive its canonical source_id, and recheck after all builds. The actual
current fingerprint can differ from r10 and from Linux because repository
and dependency closure differ. Never assert a historical source ID on a new
PE or combine a new main with a historical runtime object.

## Fresh direct build, one shared layout prefix

Reuse only an actual qualified LLVM23 AMD64 COFF SDK/library closure and
actual selected GNU Windows SDK/libc++ headers. The generated llvm-config.h
must genuinely define LLVM_VERSION_MAJOR23 and appear in the shared TU's
-I/-isystem search and original consumed-input SHA records. The product must
consume a real AMD64 COFF DebugInfoDWARF archive from that closure. A Linux
ELF archive or a component name is not a provider. If this actual archive is
missing, build only the missing current LLVM component/dependency closure in
the same cgroup; do not advertise a nonexistent library or borrow an
unqualified older provider.

Use one expanded compiler prefix for actual main and runtime. It includes
actual --target=x86_64-w64-windows-gnu, one --sysroot, -std=c++26,
-stdlib=libc++, -fexceptions and -fasynchronous-unwind-tables (or
-funwind-tables), identical -I/-isystem/forced includes, all actual -D/-U
macros and ABI-affecting flags. This preflight compares **all** non-identity
macros, including UWVM=2, UWVM_/UWVM2_, native-capture, _LIBCPP_* and other
namespaces. Only four explicit embedded source identity macros are excluded;
actual PE must still contain the full original source_id. A real TU-only
label mismatch is retained as failed evidence; do not silently expand the
identity exclusion list.

Do not copy the old r10 experimental enable vector. This minimum admits the
current default-off debugger candidate with the actual Stage3 source owners
present but no private publisher selection. If capture is defined, main/RT
must have the same value and actual layout. A disabled capture define is not
interchangeable with its absence in this conservative contract.

Intended compiler shapes for the keeper (not executed command records):

```text
COMMON = actual clang++ + exact Win64 target/sysroot/C++26/libc++/EH/unwind
         + one common includes/macros/ABI prefix + intended optimization
         + -DUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=1
clang++ COMMON -c CURRENT/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp -o NEW/runtime.obj
clang++ COMMON -c CURRENT/src/uwvm2/uwvm/main.default.cpp -o NEW/main.obj
clang++ LINK_PREFIX NEW/main.obj NEW/runtime.obj [other fresh actual objects]
        [actual SDK LLVM23 COFF archives including DebugInfoDWARF]
        [actual crypto/compression/system inputs] -o NEW/uwvm.exe
clang++ LAUNCHER_PREFIX -municode CURRENT/test/0017.runtime/windows_debug_current_launcher.cc
        [actual C++/system inputs] -o NEW/windows_debug_current_launcher.exe
llvm-readobj --file-headers ACTUAL/DebugInfoDWARF_ARCHIVE
```

A narrow descendant accepts Clang's bare
-DUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT as semantic value 1, matching
top-level xmake add_defines, as well as the explicit =1 shape above. Only this
gate token is normalized for the main/runtime contract; absent/0/2/undef and
all duplicate gate definitions are rejected, and every other macro retains its
exact original token. Keep genuine compiler argv unchanged. This source-only
normalization and the copied-MC unit's exact1 Windows guard are not a PE/native
or module-link qualification; those checks remain pending keeper execution.

Retain every actual direct compiler/linker/tool argv/cwd, tool SHA, complete
raw log, original successful return code, consumed-input SHA and actual
output SHA. The outer resource supervisor may use env or a wrapper, but the
command records contain the genuine direct primary LLVM invocation. Response
files remain unavailable until a separately reviewed original expansion
adapter exists; do not reconstruct commands from old PE/object metadata.

Use the original r2 schema1 build receipt without changing its meaning.
Append exactly these actual original-evidence fields:

```text
llvm_config_header = {path,sha256}
debug_info_dwarf_headers = {
  argv: [actual llvm-readobj, --file-headers, actual archive], cwd,
  returncode: 0, tool_sha256, log, log_sha256, input_sha256
}
```

The COFF oracle must show Format COFF-x86-64, Arch x86_64, AddressSize64bit
and Machine IMAGE_FILE_MACHINE_AMD64 for **every** emitted archive member.
Empty, partial or mixed-architecture output fails. No actual original log
or field is inferred from this example.

## Official stage and exact current VM slice

In the same admitted cgroup, then run:

```sh
python3 "$SOURCE/test/0017.runtime/stage_windows_debug_current_r5_vm.py" \
  --source-root "$SOURCE" --repository ros \
  --r5-source-manifest "$R5/source-manifest.json" \
  --product "$BUILD/uwvm.exe" --launcher "$BUILD/windows_debug_current_launcher.exe" \
  --build-receipt "$BUILD/current-debug-build-receipt.json" \
  --wasm-clang "$WASM_CLANG" --wasm-ld "$WASM_LD" \
  --wasm-tools "$WASM_TOOLS" --llvm-dwarfdump "$DWARFDUMP" \
  --llvm-readobj "$READOBJ" --out "$NEW_STAGE"
```

Use --repository ordinary for ordinary uwvm2, and one --dll argument for each
actual non-system provider. The unchanged r2 stage recursively qualifies
both ordinary and delay DLL imports for product, launcher and each provider,
including their actual AMD64 PE/SHA. No extra provider is silently copied.
It officially compiles C17 O1 DWARF5, validates Wasm, verifies DWARF and emits
the full line/inline/Code oracle from current zero-import fixtures.

R5 additionally checks all current frozen pins, common TU layout, generated
LLVM23 config and original COFF DWARF archive oracle. Preserve
r5-admission.json separately. It must have passed=true, exact R5 pin and
actual qualification_sha256, with all original inputs unchanged. An existing
base qualification.json without a passing R5 admission is **not** this
R5 lane's admission. Neither file grants source/local/native runtime access;
that authority still comes solely from real controller/runtime tickets.

Only after this stage, retire all cross-build children. Keeper may start the
already managed Windows11 x64 VM with KVM in that SAME64GiB/no-swap cgroup,
without another build/benchmark. Keep the actual disk model, VM process
birth/cgroup/CPU/memory evidence, trusted bootstrap and exact packet SHA.
Copy the complete accepted stage and invoke its unchanged five-case driver:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  C:\current-r5-stage\run_windows_debug_acceptance_current_vm.ps1 `
  -ArtifactRoot C:\current-r5-stage -Repository ros `
  -QualificationSha256 <actual successful qualification SHA256>
```

The launcher is created suspended, assigned a private non-breakaway
kill-on-close Job, then resumed. Its private stdin plus regular product
stdout/stderr preserve sealed-console admission. Actual child Job inheritance,
full NTSTATUS, zero active Job members and completed capture drain are
required before another case. Do not replace those output files with pipes.

The five actual cases are C5 O1 physical source finish under instruction and
unwind strategies, actual Wasm plus TWO adjacent decoded native instruction
traps under both strategies, and exact unsupported-mode fatal. Preserve the
actual participant, fresh stop labels, official line/column/Code extent,
actual native trapped TO PC, real guest52/58 result, full statuses and raw
transcripts. Do not count fallback, parser error, loader failure or NTSTATUS
crash as an unsupported-mode pass.

This does not claim current Windows C++/Rust variables, all source policies,
inline/tail/EH step matrices, hot replacement, DAP/server or midrun attach.
Those remaining product cases require later narrow actual tests; the old
r10 sixteen-case EH result is preserved separately and is not debugger PASS.

The Win r5a source-only contract correction requires this native qualification
flag explicitly in both actual main and runtime argv; missing, zero-valued or
undefined flags fail before staging. This gate selects the qualification
backend and does not prove native execution, source stepping or IDE acceptance.
The seventh synthetic layout control is pending keeper execution.
