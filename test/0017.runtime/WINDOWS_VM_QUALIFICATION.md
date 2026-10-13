# Windows x64 LLVM-full debugger qualification

Build and test on a private copy of one frozen source tree. Keep the original
source snapshot read-only. Every compilation task runs in a Docker cgroup with
`memory.max=68719476736`, `memory.swap.max=0`, and CPU set
`0,2,4,6,16-31`. The Windows QEMU/KVM process and its artifact server must run
in the **same** 64 GiB cgroup as the Linux VM tests; verify each PID's
`/proc/<pid>/cgroup` before resuming the guest. Keep small logs and staged
artifacts in host `/tmp`. Large Xmake module BMI trees should use persistent
disk when tmpfs pages plus Clang RSS approach the memory cap; first verify at
least 40 GiB available and record that storage path. Retain source fingerprints
and failed logs; a PE cross-link is not a Windows runtime result.

For ROS, first build the pinned LLVM 23.1.1-uwvm-ros.10 libraries for
`x86_64-w64-windows-gnu` with
`tools/ci/windows_llvm_x64_toolchain.cmake`. CMake's ASM target must equal the
C/C++ target. Run `tools/ci/qualify_windows_ros_llvm_prebuilt.py` in the cgroup
with the frozen ROS source, target build, native `llvm-readobj`, native
`llvm-ar`, and exact Docker scope. It checks every member of the ordered
consumer's static archive closure is `IMAGE_FILE_MACHINE_AMD64` and publishes
`qualified-llvm.json` only on success. Preserve the source manifest, CMake
cache, archive SHA values and original objects if a repair was needed.

`tools/ci/build_windows_full_llvm_qualified.py` takes the frozen **private**
source copy, qualified LLVM directory, Windows SDK, OpenSSL root, bootstrap
Clang directory, COFF-capable `ld.lld`, xmake binary, exact Docker scope, and a
fresh output directory in `/tmp` or on sufficiently free persistent disk. It checks the source before/after, invokes the
explicit qualified-LLVM route, builds and installs `uwvm.exe` and
`uwvm-debug-server.exe`, and builds the two test-only Windows launchers. Its
`--configure-only` option exercises the source/LLVM/module-compiler contract
without compiling the product. Use `--allow-persistent-output` to put the
entire module build tree on disk, and `--resume` only with the exact previously
configured source fingerprint and build output. Use a bootstrap Clang that passes the project
named-module initializer probe; a working LLVM library cannot repair that
compiler defect. `build.json` says `cross-built-awaiting-real-windows-vm` until
the VM test actually passes.
For bounded Windows Clang 22 module builds, select the cold CLI callback's
per-file `optimize = "none"` recipe and pass `--verbose-commands`; the recorded
build log must show this callback compiled once with `-O0`, while the JIT and
runtime translation units retain release `-O3`. `build.json` records the
recipe, tool, driver, log, and PE hashes for the VM stage.

With Xmake 3.0.5 and Clang 22, Xmake's module-file flag probe supplies a
nonexistent PCM. Clang correctly rejects that input, causing Xmake to report
that supported C++ modules are unavailable. Apply
`tools/ci/patches/xmake-3.0.5-clang22-modulefile.patch` to a **private copy** of
Xmake's program directory and pass it as `XMAKE_PROGRAM_DIR`; do not alter the
shared Xmake installation. The builder records the Xmake, compiler, scanner,
linker, and patched module-support file hashes in `build.json` and checks that
they remain unchanged through the build.

In that same cgroup, run
`test/0017.runtime/prepare_windows_debug_fixtures.py` with Clang, wasm-ld,
wasm-tools and **genuine** Rust rustc with `wasm32-unknown-unknown` installed.
The fixture summary records SHA-256 for C and C++ DWARF 4/5, Rust DWARF 5,
native single-step, and function-replacement Wasm inputs. Stage the exact
product and fixture bytes with `test/0017.runtime/stage_windows_debug_vm.py`.
First run `prepare_windows_eh_hot_replace.py` in the same cgroup with
wasm-tools and Wasmtime 48; pass its output as `--eh-replace-fixtures` when
staging. The oracle checks an unchanged `() -> ()` target ABI and verifies
cross-function `try_table` payloads 29 before replacement and 30 afterward.
Start `test/0017.runtime/windows_debug_artifact_server.py` with that staging
directory and its `stage.json`; it serves only listed basenames, verifies each
SHA-256 and accepts one bounded result submission.

Run QEMU/KVM with the ephemeral writable qcow2 and UEFI variables in host
`/tmp`. If `/dev/kvm` is available only in a separate Docker scope, launch
QEMU paused with `-S`, migrate its PID and artifact server PID to the main Linux
cgroup, verify memory/swap/CPU limits and `/proc/<pid>/cgroup`, then resume
through QMP. The VM executes
`run_native_step_windows_product_vm.ps1` from the staging URL. It checks
instruction/unwind native disassembly steps, Wasm stepping, C/C++/Rust source
`into`/`over`/`out`, function replacement success and rejection, and direct
console input/handle alias rejection. It also replaces the inactive throwing
function before entry and checks both instruction/unwind cross-function catches
and the independent exceptions-off gate. For host-authorized late attach, start
the installed `uwvm-debug-server.exe` from a real Windows host console with
`uwvm.exe -m run -Rct 0 ... --run program.wasm`; keep its printed pipe name
and capability in that console. Use `stage_windows_broker_vm.py` to create a
separate SHA-checked broker stage and artifact server on port 18019; the main
debugger stage accepts only one result POST. Invoke
`run_windows_control_broker_vm.ps1` from a separate host console with those
values and the staged PE paths. The broker result records both executable
hashes, but never its pipe name or capability. It tests wrong capability,
guest-descendant denial, normal command forwarding and detach. Never store
the capability in a guest-visible WASI path or test artifact.

For IDE protocol acceptance, stage `stage_windows_dap_vm.py` from the same
qualified PE and main stage, then create its hash-bound QMP command with
`prepare_windows_dap_bootstrap.py --engine ros`. The separate artifact server
uses port 18022. `run_windows_dap_host_vm.ps1` captures each broker capability
from the trusted console and sends it to the real Windows Python adapter over
anonymous stdin. It runs a Wasm/native step case and a DWARF C source-line
case, posting only sanitized DAP responses. If the guest has no usable Python,
the result is an explicit `python-unavailable-in-win11-guest` failure; the
native broker pass does not substitute for DAP acceptance.

Stage Core 3 supplemental inputs with `stage_windows_core3_vm.py` using the
same frozen source/PE, three source-matched typed-branch Wasmtime fixtures,
and the eight-fixture `--gap-dir` oracle. The latter covers null and retained
exception-reference casts, a required fatal `ref.cast` failure, a real
non-null `exnref` call-frame token, a cross-function `catch_ref`, and a
two-call indirect Win64 EH fixture whose catch checks the exact payload.
Run `run_core3_windows_vm.ps1` under both
`instruction` and `unwind`; each fixture has independently checked GC and
exceptions switches. The call-frame ABI and cross-function EH fixtures must
run with GC disabled, while disabling exceptions must reject them. The final verifier expects all 116
Core 3 guest cases and binds the oracle digest through qualification, stage,
and guest result.

Acceptance requires the VM's posted `result.json` to report `passed=true`,
the PE/fixture SHA values to match `build.json` and `stage.json`, the broker
late-attach test to pass, and all source/LLVM hashes to remain unchanged.
Repeat separately for ordinary uwvm2 and ROS; ordinary uses its externally
qualified LLVM and must not treat ROS's bundled-library certificate as proof
of ordinary's own executable build or Windows VM behavior.
