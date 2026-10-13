# Complete preload resource-alias census fixture (SOURCE only)

`debug_checkpoint_complete_preload_runtime.cc` adds a new actual two-module
complete-state input. It preserves the separate single-module R2 fixture and all
older debugger VIEW tests. Both products use byte-identical new fixture sources.
No production source is changed by this fixture proposal.

The provider owns three functions, two table64 tables, one memory64 memory,
eleven globals, one tag and two passive data/two passive element segments. It
creates the same modern GC/packed/SIMD/extern/exception graph as the single-module
R2 input, including first catch_ref and genuine throw_ref re-catch aliases. The
consumer imports the actual callback function, both tables, the memory, all
globals and the tag. It has its own separately parsed types and two local
functions. Only the provider setup writes; the later two consumer participants
read their imported references, so the fixture does not introduce guest races.

Main and provider images are owned and adopted BEFORE either module is parsed.
The setup uses the existing native full_preload_input/source initializer sealing
route. After real fused compilation, it obtains dense module IDs only through
the actual source's assigned/bound members. It checks distinct owned files and
parser contexts, rejects mismatched actual member/ID and epoch combinations, and
compares each graph module's copied bytes with the entire respective pre-parse
image. Equal recursive/function types do not collapse module or instance owners.

The fresh complete-census oracle requires every consumer import in the five Core
resource index spaces to link to the ONE actual provider resource node. The
consumer frame's defined-reference declarations retain the consumer type/module
context while their struct/array object identities retain the provider owner.
It also retains the full R2 assertions: 65 structures, four arrays with distinct
identities and 1024 aliases, one exception and one original trace after re-catch,
exact typed payload/GC aliases, sparse memory64 pages 0/2 with zero page 1, a real
non-default table run, a type-only empty nonnullable typed-function table64, live
and dropped passive segments, complete locals/operands, and detached DATA after
reset. The provider callback's function ID must equal the consumer import ID.

Admission negatives again use a real extra GC entry, incomplete/duplicate owner
sets, fake shared-owner pointee/control-block aliases, insufficient object quota,
a zero recording label, previous actual pause owners/tickets, and resumed/reset
execution. Each must return no graph. A reversed valid complete owner set must
work. No transport number, module ID, query VIEW or graph node authorizes the
real producer or an executable restore. The graph remains immutable copied DATA;
`whole_restore=0` is deliberately printed.

The observer accepts only the actual assigned main module's function index 2
(one imported function precedes its setup/run locals). Each thread has six typed
locals and two real operands during its 4096-nop window. At most 8 real entry pairs
are attempted; exactly 2 complete stable-window snapshots must pass. All returned
graphs are structurally validated even if a valid pause lands at the final drop
and therefore does not count toward that window. Pause deadlines are 20 seconds;
the keeper must impose a separate finite process/build timeout covering joins.
The declared memory is 192 KiB with maximum 320 KiB; copied graph quotas are 2 MiB
encoded DATA, 1 MiB payload, 1024 objects, 8192 links, 16384 values, 2 threads/2 frames.
These limits do not establish LLVM compiler RSS or benchmark performance.

Native qualification is pending. No official WAT parse/validate, C++ compilation,
LLVM verification, machine execution, endian or other-platform test has run for
these new files. Source checks, paired equality and private textual patch checks
are the only completed work. The sole Linux keeper must first assemble and
validate BOTH new WAT files with current official wasm-tools in the existing
64 GiB cgroup, then build/link fresh matching runtime, host and fixture objects.
The fixture defines its own main. Production main is a separate fresh compile-only
.o/.d/log check and must NEVER be linked into the fixture executable.

Run the resulting binary with `<main-wasm> <provider-wasm> instruction` and
`<main-wasm> <provider-wasm> unwind`, for both products, in the same cgroup with
finite keeper timeouts. Both WATs require Core 3.0, including memory64/table64, modern
try_table/throw_ref, typed nondefaultable references, GC and SIMD. The provider's
reserved module name is exactly `checkpoint-instance-provider`, matching the
consumer's import strings and native preload descriptor. Cache is disabled;
observe_values is explicitly selected before authoritative fused lowering.
Do not run this new fixture on macOS. Do not overlay stale complete-census
manager/cold API leaves: preserve actual retirement and complete-capture wrapper
friends/adapters, observation 12, genuine generation/preload ownership, schema 5
and standalone checkpoint module seam in the parent's exact final composition.

Core 3.0 references: [module import instantiation](https://webassembly.github.io/spec/core/exec/modules.html),
[defined/reference type matching](https://webassembly.github.io/spec/core/valid/matching.html),
[store/module instance structure](https://webassembly.github.io/spec/core/exec/runtime.html),
and [throw_ref execution](https://webassembly.github.io/spec/core/exec/instructions.html).
Canonical native membership, dense DATA IDs, sparse chunks and quota policy are
implementation contracts. Complete graph capture does not itself implement
whole-resource restoration, persisted Wasm/cache/build identity, replay or
reissued native continuation rights.
