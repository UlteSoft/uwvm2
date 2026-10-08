# Ordinary uwvm2 Windows x64 LLVM-full qualification

Build from a private frozen source copy. Run the Windows cross-build, fixture
generation, and staging in one bounded Docker scope with
`memory.max=68719476736`, `memory.swap.max=0`, and CPU affinity
`0,2,4,6,16-31`. The Windows QEMU process runs in the existing Linux test
scope, which has the same limits and contains the Linux platform tests; it is
not the cross-build scope. Record both scope names, limits, and process/thread
membership separately. During the guest run, invoke
`record_windows_vm_cgroup.py --qemu-pid PID --linux-scope SCOPE --output FILE`
to record every QEMU thread in the Linux test scope. Pass `--source-id`,
`--product`, and `--expected-product-sha256` to bind that VM record to an
exact source and PE certificate. Keep provenance logs on
`/tmp` or `/dev/shm` and place the
large Xmake BMI/object tree on persistent disk with `--disk-build`; Xmake sees
the disk tree through the output directory's `xmake-build` symlink. This avoids
charging its many generated module files to the cgroup's tmpfs memory. Record the source hash
before and after the build and never call a cross-linked PE a Windows test pass.
Remove Finder `._*` sidecars from the **private build copy** before invoking
Xmake: the source fingerprint excludes them, but Xmake's `*.cppm` glob would
mistake AppleDouble metadata for C++ modules. The builder rejects an unclean
copy and the frozen candidate remains untouched.

Use `tools/ci/build_windows_full_llvm_external.py` with explicit `--source-root`,
`--prebuilt-llvm`, `--llvm-source`, `--sdk`, `--openssl-root`, `--output`,
`--docker-scope`, `--xmake`, `--clang-bin`, `--lld`, `--llvm-readobj`, and
`--llvm-ar`, and `--disk-build` for a full build. The external LLVM directory must carry `qualified-llvm.json`,
`consumer-link.rsp`, generated target headers, the Release CMake cache, and the
certified Windows x64 COFF archives. The script independently checks every
archive member and hash, verifies the LLVM source manifest, then cross-builds
`uwvm.exe`, `uwvm-debug-server.exe`, and the two Windows test launchers. It
records the external LLVM origin rather than claiming the ordinary repository
bundles LLVM. The target-specific `llvm-config` adapter returns CMake's exact
ordered archive response, including repeated libraries; it never falls back
to a Linux LLVM installation.

If a full build stops before it writes `build.json`, repeat the same command
with `--resume` to reuse its disk BMI/object tree. The builder refuses resume
when the frozen source, build recipe, tools, LLVM archive closure, cgroup, or
disk tree differs; each attempt gets a separate log directory.
For a fresh build, the builder removes generated `.xmake` project state from
the private source copy before configuring; this prevents an old checkout's
absolute paths or feature settings from entering the frozen-source PE. Resume retains
the matching incremental state.

Use an x86-64 Windows SDK and Clang with working C++ named modules. With Xmake
3.0.5 and Clang 22, set `XMAKE_PROGRAM_DIR` to a private copy of Xmake's program
tree containing `tools/ci/patches/xmake-3.0.5-clang22-modulefile.patch` from the
ROS repository, if that compiler requires the module-file probe correction.
The ordinary builder hashes this private support file and all tools.
For the bounded Windows Clang 22 named-module build, Xmake compiles the cold
CLI call-stack option callback's runtime BMI at `-O0` and all CLI-target
consumer BMIs at `-O0`. The callback also omits its redundant aggregate
command-line import and formats its cold invalid-mode error in bounded print
calls; `-O0` alone exceeded the 64 GiB limit before those source fixes.
`uwvm_runtime` owns the JIT, validation, and memory execution objects, which
retain release optimization. Check the actual Clang commands for both callback
copies and the hot runtime modules before calling the PE qualified.

In the same cgroup, run `prepare_windows_debug_fixtures.py` with Clang,
`wasm-ld`, `wasm-tools`, Wasmtime, and genuine `rustc` with `wasm32-unknown-unknown`.
Also run `prepare_windows_eh_hot_replace.py` with wasm-tools and Wasmtime 48;
its oracle proves the `() -> ()` target ABI stays unchanged while a thrown
payload changes from 29 to 30 across two generated indirect Wasm calls.
Use `stage_windows_debug_vm.py` with the same `--docker-scope` and
`--eh-replace-fixtures` pointing to that oracle output, then
`windows_debug_artifact_server.py` to stage
the exact PE/fixture hashes and collect the guest result. The VM runs
`run_native_step_windows_product_vm.ps1` for instruction/unwind native steps,
Wasm steps, nonzero executable Wasm breakpoints and immediate-byte rejection,
C/C++ DWARF 4/5 and Rust DWARF 5 source steps and exact source-line breakpoints,
function replacement and rejection while the target function is active on a
stopped Wasm stack,
cross-function EH replacement in both instruction/unwind policies with its
independent exceptions-off gate,
new Core 3 typed `call_ref` and polymorphic-bottom `throw_ref`/`br_on_null` syntax with their
individual feature gates, non-full debug-jit rejection, and protected-input checks. Run
`run_windows_broker_host_vm.ps1` in a trusted Windows host console, passing
the five staged input SHA-256 values from the broker-stage manifest. It
verifies downloaded bytes before starting the broker, reads the new pipe name
and 256-bit capability only from that console's output buffer, and passes them
in memory to `run_windows_control_broker_vm.ps1`. The probe checks wrong
capability, guest-descendant denial, command forwarding, and detach. The host
orchestrator POSTs one result without the capability after both PEs and the
broker exit status are verified; the final verifier binds both PE hashes to
the frozen build certificate.
For IDE protocol acceptance, stage `stage_windows_dap_vm.py` from the same
qualified PE and main stage, then create its hash-bound QMP command with
`prepare_windows_dap_bootstrap.py --engine ordinary`. Serve this separate DAP
stage on port 18022. `run_windows_dap_host_vm.ps1` captures each broker
capability from the trusted console and sends it to the real Windows Python
adapter over anonymous stdin. It runs a Wasm/native step case and a DWARF C
source-line case, posting only sanitized DAP responses. If the guest has no
usable Python, the result is an explicit `python-unavailable-in-win11-guest`
failure; the native broker pass does not substitute for DAP acceptance.
Create a distinct broker stage with `stage_windows_broker_vm.py` from the
already verified main stage, then serve it on a separate local port/prefix
through `windows_debug_artifact_server.py`. The server accepts one result POST
per stage, so the main debugger, Core 3, and broker results use three stage
directories. Pass the broker stage to the final verifier.
After all three stages exist, run `prepare_windows_guest_bootstrap.py` with
the build and stage directories. It rechecks every staged byte and emits
three hash-bound, secret-free PowerShell `-EncodedCommand` lines for QMP in
main, Core 3, broker order. Resume QEMU and use read-only screenshots only
until the Win11 desktop/login state is known. Execute the first two commands
and wait for their separate result POSTs. The broker command verifies its
runner hash before launch and supplies five staged input hashes; from that
point do not screenshot, OCR, redirect, or persist its real console output.
Observe only the broker stage's bounded `result.json` on the host.
Launch a PowerShell console with the short `powershell.exe` Run command, then
type each generated line into that console with QMP `type-file` and press
Return. The broker line is about 2.3 KiB, so it should not be typed into the
Windows Run dialog, and QMP's per-key delay may take several minutes.

Also stage the 8 GC/i31/struct/array/table64 fixtures, the GC constant-expression
initializer, the executable `exnref` structure payload, the `ref.func` table
initializer, the EH `try_table` plus shared-atomic fixture, three typed-branch
fixtures, and eight Core 3 exception-reference cast/call and cross-function EH fixtures with
`stage_windows_core3_full_vm.py`. Pass the
frozen ordinary source and PE build, the existing GC/EH reference evidence
directories, the GC constant-expression oracle evidence, and the fixed
`exnref` Wasm evidence, the fixed table-initializer Wasm, the source-matched
eight-fixture cast/call/EH oracle directory (`--gap-dir`), the exact
`wasm-tools` and Wasmtime binaries, the bounded Docker
scope, and a distinct tmpfs output directory. Staging checks the previous
reference records, reparses the frozen `exnref` and table-initializer WAT,
reruns parse, validation, and Wasmtime on the 23 Wasm binaries, including the
expected `ref.cast` trap, an `exnref` call-frame case, and two escaping
cross-function throws that succeed with GC disabled, and
records each fixture, reference log, tool, PE, LLVM certificate, and source
hash. Serve its `stage.json` through `windows_debug_artifact_server.py` on a
separate port/prefix and run `run_core3_windows_full_vm.ps1` in the same VM.
That runner executes each GC sample under both `instruction` and `unwind`,
tests GC, table64, reference-type, and table-instruction feature gates,
and checks the GC initializer with `extended-const` disabled and the GC
feature independently gated, then runs EH/shared atomic and its
exception/thread gates under both policies. It also runs the `exnref`
structure-payload fixture with JIT full under both call-stack policies; each
policy checks exceptions, GC, and reference-type feature-off rejection. The
eight cast/call/EH fixtures run under both policies, with independent GC and
exceptions gates; the nullable cast failure must produce a fatal VM call stack.
The `exnref` call-frame ABI and cross-function EH fixtures must still succeed when GC is disabled. This
qualified Windows debugger PE is built with `--execution-int=none`; the
interpreter full/lazy execution matrix is qualified separately on Linux.
The table-initializer fixture runs with UWVM's independent
`table-initializer` gate enabled and `function-references` disabled, then with
`extended-const` and `table-initializer` each disabled. Wasmtime's standard
positive oracle uses its default function-references setting because it groups
table expression initializers under that proposal.

Acceptance requires `result.json` to report `passed=true`, the broker check
and Core 3 supplemental result to pass, and all PE, fixture, source, external LLVM, build-recipe, and tool
hashes to remain equal to their certificates. The ROS product has a separate
build and Windows VM qualification.
Run `verify_windows_full_qualification.py` with the final build directory,
all three stage directories, both guest results, the broker result, and a QEMU
cgroup record bound to the exact source ID and PE SHA. It checks all 42
debugger/security and 111 Core 3 guest cases, all 53 current reference outcomes
(including the expected Wasmtime cast trap),
and the complete hash chain before writing the ordinary Windows pass record.
The final verifier also binds the guest-observed launcher and Wasm fixture
hashes, Core 3 oracle/certificate hashes, and Windows x64 OS identity reported
by each of the main, Core 3, and broker runners.
