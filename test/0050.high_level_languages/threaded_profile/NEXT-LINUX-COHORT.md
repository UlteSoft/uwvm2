# Main and stable: next Linux source cohort and qualification plan

STATUS: SOURCE INSTALLED LOCALLY; NOT BUILT OR EXECUTED. Linux currently belongs
to the ROS peer. No SSH/transfer/read/build/VM/collector action is authorized by
this plan itself. Obtain the coordinated original-container window first.
This plan owns only uwvm2 and uwvm2-stable; ROS production and every SDK scope
remain with their respective owners. Historical evidence is never relabelled.

The installation receipt binds 66 production paths per repository, using each
repository's exact original SHA/new-path absence. The effective source is Root
composition-r1 plus its Runtime/callback r2 overlay, including callback197a70ae
and the three genuine native preparation/install gates. All original unrelated
experimental source remains. No Git reset, commit, push or whole ROS Runtime
copy is involved. The copied qualification-r2 subdirectory is an exact 50-file
SOURCE packet. Its README's earlier authoring-time 'public repositories were
not modified' sentence describes the original private packet; this installation
now places copies and the selected actual production source in these two repos.
It does not turn that packet into build/test evidence.

1. Release and admission

After the ROS peer explicitly releases the existing 64GiB container/cgroup,
record its actual container ID, current boot ID, resolved membership/cgroup
inode, memory.max=68719476736, swap.max=0, effective CPUs, memory.events and disk
space. Do not reuse old boot/inode values or create/change limits. Pin tool/SDK
paths and SHAs from the actual current files. Use fresh persistent cohort and
output paths; preserve all failed attempts and original products.

2. Fresh Linux source/build cohort

Transfer a fresh immutable snapshot of the two actual source trees plus their
required fast_io/boost/bizwen dependencies, source-fingerprint helper, build
recipes, and this exact SOURCE packet. No SDK code generation is needed. Before
build, verify all 132 installed source hashes against the local receipt, then
record the actual whole production/dependency fingerprint using the real
repository fingerprint helper INSIDE Linux. The 132 paths alone are not a whole
source identity. Pin all other dirty experimental bytes as actual input.

Reuse the existing pinned compiler/LLVM archives only after their real hashes
are re-established. Preserve main's external LLVM23git versus stable's pinned
bundled23.1.1 route. Do not rebuild LLVM without a separate need/window. A new
build-helper revision is required: the historical build_products.py explicitly
sets UWVM_DISABLE_INT and therefore CANNOT be run unchanged to qualify -Rint.
For the real dual-backend product, retain existing O2/g1/link/include flags and
remove UWVM_DISABLE_INT; explicitly set UWVM_USE_UWVM_INT, UWVM_USE_LLVM_JIT and
UWVM_USE_THREAD_LOCAL. Do not set unsupported fallback TLS, FAST_MATH, disabled
EH/GC parser flags or silent ordinary fallback. All actual commands and macro
choices must be recorded; these source-only planned flags are not a build PASS.

Build the actual Runtime, main and Host API TUs and link a fresh product per
repo with real pthread/native support, compiler/link/library before-after SHAs,
source ID, binary SHA and build receipts. Use owned_process/pidfd controllers,
core0, serial TUs/serial repositories, initially CPUs4-7, own8GiB/shared50GiB and
disk2GiB floors unless the coordinated window establishes stricter limits. Do
not discard a failed receipt or reuse a stale successful identity. If a guard
fails, retain the attempt and diagnose before making any next revision.

3. Real finite-profile CLI and public-API embedding qualification

With actual BIN_SHA/TOOL_SHA/BOOT/CG_INODE/MEMBERSHIP substituted, this command
uses the real runner CLI. COHORT/CG are the fresh Linux physical paths:

```sh
taskset -c 4 python3 "$COHORT/uwvm2/test/0050.high_level_languages/threaded_profile/qualification-r2/run_cli.py" \
  --binary "$COHORT/products/uwvm2/uwvm" --binary-sha256 "$BIN_SHA" \
  --backend int --wat-tool "$WAT_TOOL" --wat-kind wasm-tools \
  --wat-tool-sha256 "$TOOL_SHA" \
  --source-manifest-sha256 556db283eb6d1656a22bdab5e6f316d5bd0f97956be1444a2129146fc03e2db1 \
  --cgroup "$CG" --cgroup-inode "$CG_INODE" --membership "$MEMBERSHIP" \
  --boot "$BOOT" --cpu 4 --out "$COHORT/results/uwvm2-int-cli-r1"
```

Run serially for int/llvm and uwvm2/uwvm2-stable, changing actual product pin
and fresh output path each time. Declarations are 38 INT +42 LLVM per repo,
160 planned CLI cases across two repos. Four LLVM-only cases are outside INT
coverage, never counted successful skips. The tool is required; missing tools,
WAT parse failure, unsupported actual shape, signal-kill, or wrong exact output
are failures. The runner records real spawn/entry/completion markers, SHA-bound
WAT/Wasm, exact stdout/errno oracles, raw stderr, deadlines, actual PID/start
ownership, sampled RSS and retired pidfds. It does not manufacture native TIDs.

The actual UWVM commands are -Rint or -Raot, -Rct0 (separate arguments), explicit
-WFE-threads/-WFE-exceptions/-WFE-simd, the six finite --wasi-threads values,
-Wpre provider.wasm env and --run command.wasm. The r2 native fixture waits for
actual child progress at60004 BEFORE READY; its post-join oracle stays strict.

Build native_embedding.cc as the real main TU linked with the SAME integrated
Runtime/Host objects and exact product flags; preserve a genuine build receipt
and native ELF SHA. Then run_native.py uses the same arguments as run_cli.py
plus --embedding-build-receipt PATH and --embedding-build-receipt-sha256 SHA.
The binary argument is that actual embedding ELF, not the CLI product. Run two
modes (last-owner/controller-stop) for each backend and repo: 8 planned cases.
There is no fake Admission/CompilerLease, test provider or public TLS-destructor
injection. Compiler queue faults/grow rollback/private stop-entry/TLS sentinel
gaps remain explicitly unqualified until genuine reachable fixtures exist.

4. Existing ordinary/debug language regression on new products

Rebind the existing immutable producer Wasm/source/DWARF/oracles to these NEW
product/whole-source identities in fresh reports. Preserve prior PASS/FAIL
receipts. At minimum this worklist covers the existing two-repo subsets: 62
ordinary functional rows (31 each), 20 full DBG language workflows (10 each),
12 Java args/env cases (6 each) and 8 source workflows (4 each); all are PLANNED
counts only. Use genuine entry/step/breakpoint/continue/restart/quit and exact
strict output assertions already reviewed. C#/Python full managed locals are
not implied by native Wasm debug; use the documented source-level scope.
Re-run C#/Java/MoonBit/JS/AS and CPython fixtures on the new source as actually
supported. Keep unsupported producer limits distinct from normal success.

Retain real C/C++ argv/environ libc ABI, entry/start-once/explicit ABI, parallel
partition, C# native interop and stock CPython16-probe tests. The old fast suite's
two-product subset is 64 UWVM commands plus23 Wasmtime references =87, not the
old three-product119 or a fake new PASS. Native/Wasmtime exact old artifact
references may be read-reused with their historical labels, or genuinely rerun.
Stock CPython two products+native+Wasmtime is64 planned probes, not80 new PASS.

5. Performance and cross-platform qualification

Only after all functional ownership closes and the host is quiet, coordinate
exclusive ordinary same-Wasm engine benchmarks (warmup1+5 randomized matched
repeats, current producer/engine SHAs, AOT compile separated) and actual VTune
hardware+Hotspots+Microarchitecture with real gated READY/G/DONE/D kernels.
Keep raw outputs, collector setup/sample qualification, PMU/platform settings
preflight/periodic/final records and rate-drift failures. Historical C3 r3
environment-drift FAIL remains FAIL; its execution rows are not a valid final
performance cohort. No ratio is emitted for failed/unavailable external cases.

Actual Windows/QEMU products and the peer's source-only QGA executor still need
their real native build and run qualification. Never count a missing QEMU/tool
or Host-killed guest as success, and never call host-emulator PMU samples native
guest-ISA hardware data. Do not run any of these steps from this Mac.
