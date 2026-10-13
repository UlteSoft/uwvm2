# Source-only genuine native EH component

This project is ready for the keeper to configure against **actually built**
paired runtime archives. It has not been compiled or executed locally. The
provider patch is still unapplied and requires root source review before
applying to a separate experiment checkout. Live vendor/runtime/GC/checkpoint
files are not changed. The first execution scope is Linux x86_64 Clang ELF;
an architecture guard rejects broader unsupported qualification.

The C++ component uses real owning tag instances and real C++ `throw`, typed
catch, ordered tag-identity mismatch and `throw;`. This is a native component
analogue of the later cold Wasm tag dispatch, not a full Wasm validation or
VM test. The separate C99 TU creates genuine CFI frames without LSDA and
disables tail calls only in the test fixture. Original `_Unwind_Backtrace`
records are captured at the actual throwing leaf. First-search observer C
frames must match their original PC/CFA/FDE records; deep bounds require
overlap rather than falsely claiming a full original trace.

The source includes genuine foreign unwind: a fresh heap object with the
**public** `_Unwind_Exception` prefix and a registered cleanup. Only its
language-owned class/cleanup fields are initialized by the client; private
fields are left to the real unwinder. A standard-layout/offset-zero assertion
justifies recovering this component's own containing object in cleanup. It
does not cast a C++ private header. A host catch-all cleans it once while the
guest typed catch must decline it. An unknown C++ type is separately tested
because it is not equivalent to a foreign unwind class.

There are 150 semantic cases: six serial cases plus four actual concurrent
threads, each with 32 guest throws and four unknown-type declines. Every
observed chain also tests busy-arm rejection; the first throw per thread and
the serial short chain test stale generation disarm while a new observation
is armed. The tag mismatch uses genuine `__cxa_rethrow`, with one cleanup and
only one observer finish. A 96-deep chain hits the observer's 64-record bound
without stopping native propagation. Cancellation/no-throw scopes disarm.
Unknown-type and direct foreign raises consume/clear arm state and cannot
attach their old context to a later unarmed guest throw.

ON requires exactly 136 first-search finishes, genuine handler0 C frames,
original-CFI matches, unchanged typed payloads/tags and correct cleanup.
OFF executes the same real native exception cases and original capture,
but emits no observer API references or replacement provider stubs. It must
report zero observer finishes/frames. Neither variant removes diagnostics
or provides a performance result. Callbacks write only bounded preallocated
records/scalars and read a trivial executable-local thread cookie; they do
not print, allocate, lock, call the OS, register observers or invoke user code.

## Root source review points

- The header/object/type association originates inside the actual paired
  `__cxa_throw` after `__cxa_init_primary_exception`. Pointer equality checks
  use real provider addresses; there is no guessed `__cxa_exception` layout,
  forged C++ exception class or application write to unwind private fields.
- Single-thread pending arm is copied into provider TLS by value. A foreign
  primary consumes and discards it. Actual RaiseException consumes only its
  matching header and clears TLS before first-search callbacks. The live
  local descriptor is cleared before phase 2, including metadata/bound/fatal
  returns. Later rethrows never reuse this observation. Exact caller/thread
  ownership and source-bound typeinfo must be established by the real host.
- The observer call is after successful step/procedure lookup and before
  `handler != 0`. Native personality actions, reason codes, protected-stack
  resume paths and cleanup order are unchanged. No cursor cloning exists in
  A1. The callback contract forbids nested registration/reentry; busy arm
  declines. Generation wrap permanently declines rather than reuse.
- Association/callback functions are not guest imports. A later production
  host bridge needs true provider/code pins and a scope guard; this component
  is not that production admission. Arbitrary native-code callers are outside
  the Wasm guest trust boundary, and public exported provider symbols alone
  are not authority to map them into a module.

## Experiment build closure and admission

The companion source archive freezes the complete current ROS runtime
subset, all fast_io include bytes, paired-static preset, API/implementation
leaf, patch, test source and this project. It leaves provider files unchanged.
External Clang/Clang++, CMake/Ninja/Python, compiler-rt builtins, target libc,
sysroot, linker and inspection tools remain genuine keeper inputs that must
be bound to hashes and actual versions. No local build or peak-memory estimate
is used to permit macOS execution.

After root approves the exact patch, apply it only to the extracted provider
experiment tree. Build two independent runtime directories from that same
patched tree: OFF with no observer define, ON with
`-DUWVM_EXPERIMENTAL_NATIVE_EH_PHASE1_OBSERVER=1` in both C and C++ flags.
Use the frozen `paired-static.cmake`, actual Clang, explicit libunwind include
path for libcxxabi, exceptions/RTTI/threads ON, and build the actual
`cxx_static`, `cxxabi_static`, `unwind_static`, `cxx-headers` targets. Request
CMake File API codemodel/cache and resolve actual output paths, not assumed
`lib` locations or renamed system archives. Each component must use its own
variant's actual generated `__config_site`, copied C++ headers and archives.

Configure this project using `EH_CXX_INCLUDE`, `EH_UNWIND_INCLUDE`,
`EH_FAST_IO_INCLUDE`, `EH_CXX_ARCHIVE`, `EH_CXXABI_ARCHIVE`,
`EH_UNWIND_ARCHIVE`, and matching `EH_OBSERVER_ON=ON/OFF`. The standalone
plan generator emits argument arrays for configure/build/inspect/run; it
never executes them, applies a patch or provides cgroup authorization.
Builds must run inside the keeper's actual 64 GiB/swap-zero cgroup and
approved E-core set; execute on its approved P core. Do not use a taskset-only
substitute for cgroup membership or run this project on the local Mac.

Require actual compile commands to prove the paired macro/options and
generated headers; actual link map and ELF symbols to identify the three
paired archives; `readelf` loader dependencies to reject external libc++,
libc++abi, libunwind, libstdc++ or libgcc_s EH providers; and actual FDE/code
inspection to verify the C chain has CFI but no LSDA/personality. Bind each
before/after source, archive and executable hash and process UID/TID/cgroup
receipt. The linker driver must use genuine bound compiler-rt builtins and
`--unwindlib=none`; inability to establish a single paired provider is a
failure, not permission to test whichever unwinder is available.

Only then execute OFF and ON and validate the structured semantic receipt.
Preserve build, loader, callback or cleanup failures as failures. A successful
receipt qualifies this prefix component for that actual provider/ABI only;
it does not qualify full Wasm3 behavior, saved exnref immutable traces,
Windows/other targets, full suffix capture, debugger/checkpoint integration,
or latency. The next suffix interface belongs inside the actual provider;
no application cast or memcpy of opaque cursor storage is permitted.
