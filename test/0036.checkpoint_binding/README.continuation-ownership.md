# Continuation ownership validation — R48

State6 structural admission now rejects continuation records that cannot
represent one actual Wasm execution state:

- A frame belongs to one thread; a control and handler belong to one frame.
  Repeated links, sharing between frames and orphan records fail.
- A handler depth must fit its owning frame's control stack.
- An atomic-waiting thread has live frames followed by exactly one wait.
  Runnable and import-parked threads have none. Terminated threads have no
  frames or wait. A wait cannot appear between live frames.

The ownership census uses one bit per object instead of the previous u64
frame-owner entry. It runs in the existing bounded linear graph validation;
no additional object-count-sized owner allocation is introduced. Legal store
aliases, shared memories, GC cycles and exception/trace sharing retain their
existing independent rules. State6 wire fields, identity2/envelope4 and the
runtime/compiler continuation ABI are unchanged.

Both repositories contain identical production state validation and this
standalone native regression. The old-header baseline really writes sixteen
state6 files per repository through FastIO, flushes and requests OS file sync.
Each complete file has a genuine SHA256 footer; it is still structurally
impossible continuation DATA. The fixed native program rejects all sixteen
with invalid_shape, confirms encode emits no header/partial output, and checks
decode leaves its previous output unchanged. It does not merely damage checksums
or mock a successful VM operation.

Eight legal shapes per product roundtrip canonically, including ordinary and
import-parked threads, waiting, terminated, distinct recursive/two-thread
continuations sharing function/tag/memory identities, out-of-order dense object
records and a near-u64-limit logical thread ID. Existing duplicate frame/wait
and wrong-kind refusals remain checked. The current rich debug_checkpoint_codec
regression additionally passes with its GC/extern aliases/cycles, EH/trace,
packed/SIMD/numeric bit preservation, sparse memory64/table64, bounds and typed
event/reverse-plan DATA checks.

Executed on SSH Linux x86_64 in the original 64 GiB / swap0 cgroup: fourteen
guarded commands, six native compilations and six component executions, plus
two creation steps for private test directories. Both products pass. Complete
compiler dependency files bind the actual current state/codec/test sources and
bundled FastIO headers. The shared admission lock and birth/PIDFD retirement
guards are retained; other agents' tasks and files are preserved. No SDK was
copied or rebuilt. C++ file access, text, string construction, output, little
endian parsing and hashing use the existing FastIO APIs.

V1 was a frozen preparation and was not executed. V2's first baseline compile
failed because the fixture's conditional string literal became a raw character
pointer, which FastIO explicitly refused. V3 changes only that fixture output
to mnp::os_c_str and uses distinct dependency outputs. The failure, source cuts,
logs and retirement records are retained; it is not counted as a pass.

This qualifies native C++ graph/codec components. These detached models are not
validated/executing guest modules, a loaded build/provider issuer or protected
management asset. No JIT guest or whole runtime was rebuilt here. Complete
instance publication, execution-continuation restart, full external I/O rollback
and other-platform qualification remain unfinished. Graph admission cannot mint
ASM/native stack access, a runtime stop or executable restore authority.

Remote acceptance:
 /home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r48.json
Local member-hashed evidence and final delivery proof:
 /Users/liyinan/.codex/state/uwvm2-dbg/20261007-r48-continuation-ownership/
