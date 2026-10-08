This is an auxiliary native component, prepared after the ROS r2 four-PE build
and static gate. It has not been compiled or run on Windows. The whole-product
Win11 baseline remains the first required test; this component must not replace
or overwrite its failure evidence.

The fixture includes the exact frozen production SectionMemoryManager,
native_exception_symbols and native_exception_landingpad headers. Its observer
owns the production final manager through composition and forwards allocation,
reserve, stub, TLS, object notification, symbol lookup, registration,
finalization and deregistration callbacks. Fixed-size observation records avoid
telemetry allocations during those callbacks. In particular, it preserves the
production contiguous Win64 reserve and executable, local COMDAT musttail
personality wrapper. A plain external personality declaration would recreate
the already-fixed `.xdata` NX-stub path and is deliberately not used.

The four native cases are normal return, typed catch after an inner cleanup,
foreign resume to a typed C++ host catch, and genuine __cxa_rethrow after typed
catch. They use the actual process C++ ABI/type object and verified LLVM IR.
The no-RTTI LLVM closure is preserved; cold real throw/catch discovers the
immutable type_info object through the SDK. Native object/IR files are saved
using fast_io. No throw/rethrow/resume is marked noexcept.

Before executing generated code, the tool logs all callbacks and section
addresses, copied final .pdata entries, actual RtlLookupFunctionEntry results,
ImageBase/RVAs and VirtualQuery permissions. OS-returned entry pointers must
belong to a complete still-owned .pdata entry before any read. Code and xdata
RVA extents and prefix/codes/trailer lengths are proved before reading. The
bounded decoder understands x64 UNWIND_INFO prefixes v1/v2; it reports codes
as raw bytes and does not claim complete unwind-opcode validation. A chain
trailer is reported, not recursively interpreted. New/unsupported encoding
is a component preflight limitation, not a whole-product semantic result.
The active inner frame is queried using a real compiler-produced return PC,
not entry+offset or a guessed frame-pointer walk. After engine destruction,
the same copied PCs must no longer resolve to registered dynamic tables.

`run_win64_production_unwind_observer.py` is opt-in and Linux-only. It needs an
actual completed product build.json, its same qualified LLVM archive
certificate and real compiler-rt builtins certificate. It preserves clang++
and ld.lld invocation spelling and never uses source-live BMIs/VM objects.
It independently fingerprints frozen src/third-parties before and after,
hashes source headers, auxiliary controls, actual tools and all archive inputs,
and retains command logs and every failure. One E-core, a sampled 2 GiB whole
process-tree RSS watchdog, 63e9 shared guard/startup reserve and 16 GiB disk
floor apply under the existing hard 64 GiB/swap0 cgroup. No observer process
starts outside that cgroup. Actual descendant births/cgroups and pidfds protect
cleanup from PID reuse; the runner starts no VM, server or target process.
Tool success only records `component-pe-built-awaiting-real-win64-run` and
`qualified=false`.

For the current ROS diagnostic baseline use source ID
`sha256:55246b7b19aad36e74eb5687c205aef0630bb629968f50fba962bb31edacacf7`.
The whole product PE remains
`f764442c6fdfb651a67d6bf5b70ab040dea5a4b1159cc45b45dd3c1043a53a31`.
Source files under frozen r2 are immutable. Tests/runners are separately hashed
auxiliary inputs. The ordinary mirror is not an ordinary PE qualification.

After its cross-build, copy summary.json to qualification.json beside its
exact observer.exe on the private read-only artifact image. Run the separately
hashed PS script only in the explicitly budgeted Win11 VM after the mandatory
baseline evidence hash is known. It verifies manifest/PE hashes, bounds the
process to 60 seconds, and returns capped stdout/stderr plus generated object
and IR bytes through the existing <=2 MiB private result endpoint. Every result
states the component scope and retains the baseline hash. It does not install
WER registry settings, alternative page permissions, a vectored handler,
another function table, SDK mocks or any product patch.

This preparation proves no target behavior. Actual PE compilation, SDK/COFF
checks, VM execution and system unwind observations remain pending until the
resource lane is explicitly handed over.
