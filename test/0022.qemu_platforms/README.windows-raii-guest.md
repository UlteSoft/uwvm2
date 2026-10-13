# Windows RAII standalone guest queue

The first real R1 cross-build stopped at the owner fixture's incorrect
`strvw(pointer, length)` call. The original two-stage logs, exit code 1,
provider census, PIDFD retirement and retired lane ticket are preserved in
`windows_raii_cold_r1_actual_failure_20261003.json`. It created no COFF object,
PE or guest result. The R2 source retains the exact complete R1 header/backend
closure and replaces only the two identical fixture leaves with the owner's
bounded `basic_io_scatter_t<char>` and `concat_std` repair. New binaries and
results require a separate immutable R2 run.

The independent R4 LLVM-static cold group completed all 30 actual stages
for both products with EH and noEH in 54.70 seconds, at a 448,225,280-byte
peak RSS. Each fresh COFF/PE binds compiler MD, CRT, static compiler-rt and
SDK libunwind archives, imports, symbol relocations and raw bridge assembly.
All four PEs import only the eleven recorded Windows API-set/kernel32/ntdll
names; none imports the old GNU unwind DLL. The original PIDFD children
and ticket retired with init-only/64GiB/swap0/OOM0.
`windows_raii_cold_r4_llvm_static_actual_20261003.json` binds all 65 preserved
raw/source/object/PE files. This is standalone SDK cross-build evidence:
actual Windows DLL bytes, assembly review, VEH/GPR/handle/cancel/death tests,
named modules and fresh runtime/main/host products remain unqualified.
It does not qualify the ROS vendored LLVM/runtime product closure.

`windows_raii_regular_launcher.cc` is an independently owned test source. It
opens two distinct, exclusive regular files with `fast_io::native_file` and
uses `fast_io::nt_file` for the duplicate stdin handle and both actual child
handles. It forms the writable quoted command with `wconcat_fast_io`. Its
attribute buffer has a fixed, aligned 64 KiB owner; the SDK's actual extent
must fit before initialization. The inherited handle list has exactly stdin,
stdout and stderr. No job handle or launcher capture pipe is in that list.

The PowerShell parent uses the exact existing non-inheritable kill-on-close
Job owner, SHA-256
`05d73c0d84fb085bc1b7576e06a307ac436635b0bb031dbc25d7a7b90ffab4f3`.
It admits the launcher while suspended. The launcher creates the component
while suspended, immediately adopts both returned handles, observes job
inheritance and only then resumes the primary thread. No breakaway flag or
breakaway job limit is used. On failure the launcher retires only its actual
child handle before closing it; the outer owner also waits for the complete
Job to become empty. `IsProcessInJob(any job)` is only one witness: isolation
depends on this exact outer Job and its default child inheritance. These rules
follow Microsoft's [Job object documentation](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
and [CreateProcessW handle and mutable-buffer contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).

`run_windows_raii_component_vm.ps1` accepts a flat, hash-pinned qualification
artifact. It requires four distinct fresh PE variants, actual assembly review
and guest DLL closure before execution. It checks the actual Win64 interop
layout, launcher and source hashes, full child status, bounded regular logs,
four exact isolated death statuses, complete VEH/GPR/step/cancellation output,
and complete owned Job retirement. Earlier product PEs cannot satisfy this
qualification. Neither header compilation nor a raw disassembly automatically
proves the Win64 shadow space, alignment or final POPFQ/RET bridge.

The launcher and guest runner are **source only**. They require their own
fresh cross-build, imports/assembly/provider review and real Windows execution.
This suite qualifies only the immutable standalone RAII component. The new NT
readonly synchronization factory, replacement file consumer, debugger product
runtime/main/host ABI, named modules, live VM checkpoints and other platforms
remain independent qualifications. All compilation, ISO/overlay preparation,
QEMU and guest execution must use the original shared PIDFD guardian and lane
ticket in the same 64 GiB, swap-free Linux cgroup. Original Windows disks and
firmware templates remain read-only; each attempt uses private writable state.
