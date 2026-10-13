This SOURCE successor requires the immutable i32 dual-sink R2 AFTER
(manifest 820cdca86bff358b7cd48b140215a0e7019dab9144afa4eec37090e4816ae179).
It is not an executable product or permission to run local native tests.

The original INT function walker alone reads i64.const, local.get and scalar
memargs, selects the exact actual memory declaration and consumes typed operands
through the shared entire-arity kernel. Only after success does its synchronous
optional sink receive unsigned constant bits, initialized local indices, and
validated_scalar_memory_event. That event carries the actual first-decoded
memory index/address width/offset/alignment and whole pop/push byte effects. The
sink calls the existing LLVM normalized scalar lowering; it has no source pointer,
LEB parser, operand validator or second body walker. Exact-NTTP original ring
fixups and all local functions precede the original private factory seal.

The new emission_policy.emit_integer_scalar_memory defaults to false. Selecting
it permits straight-line i32/i64 signatures/local providers plus the nineteen
integer scalar load/store opcodes. Four floating scalar opcodes, other opcodes,
structured/dead control, gaps/replay/skipped combine immediates, failed native
lowering and optional quotas remain native-unavailable DATA. Legal Wasm still
finishes its original ring validation and emission. A failed optional native
fragment never turns an unused legal body into a validation failure. The normal
runtime compiler default transaction remains compile-time discarded; its ring
memory helpers, mmap protection, safety checks, guard costs and musttail streams
are unchanged. This is SOURCE pruning, not a measured assembly equivalence.

integer-memory64.wat contains an empty start followed by all twelve integer
load variants and seven stores with roundtrip loads. The data bytes deliberately
have high bits set, so exact signed/unsigned extension and Wasm little-endian
loads matter. Memory64 stores include the 12-byte i64-address/i32-value stack
transition. Real i64 parameter and zero-initialized local_get exercise the two
provider routes. A second memory32 declaration/load and nonzero memory64 memarg
offset prove that address kind and selected index must remain the original
per-instruction identity. Legal floating/i31/dead bodies are admitted while the
limited native slice declines them.

The component actually prepares parser/initializer-owned runtime memory, executes
the original INT artifact and moves each actual verified LLVM module to MCJIT.
It calls the exact typed original ABI, using the actual selected TargetMachine
triple/DataLayout/entry convention and real symbols. Context dies after engine;
source/runtime outlives both. None of these tests has been compiled or run.
No hand-made LLVM module, copied decoder or arbitrary native-PC operation replaces
the real producers. LLVM provider, target ISA, module BMI, helper symbols and
native kernel acceptance must be established separately by the Linux keeper.

The three unused negatives are first parsed by the official wasm-tools oracle:
wrong memory64 i32 address, wrong i64.store value and over-alignment. Acceptance
requires an actual code offset plus the applicable typing/alignment diagnostic,
not arbitrary nonzero exit, disabled feature, command/I/O failure or signal.
The component checks the exact original diagnostic inside that actual unused
second expression. finite_tests.py executes only those finite parsed sources and
an already authenticated fresh component. It does NOT compile all variants.

finite_build_variants.json contains sixteen real nonempty cached-register-ring
musttail cells (four cumulative combine levels times four soft/heavy delay bits)
and two original memory-stack baselines. The actual split option comes from the
existing make_tailcall_fully_split_opt<3,3,8,8>. Initial i32 R2's thirty-two cells
used SIZE_MAX stacktop ranges; they do not count as cached-ring qualification.
Actual build/ABI/assembly, memory guards and native results are all pending.

Quotas bound selected retained owners and per-function local/operation requests.
The module operation debit still counts retained successful fragments, not all
attempted partial IR work. This is not a total LLVM allocator RSS guarantee or
recoverable-OOM/performance claim. No runtime/lazy/tiered READY publication is
wired by this successor, no source/execution/debug/checkpoint credential is
created, and whole-tiered single-typed-walk/full-Core3 coverage remains unfinished.
All actual tests must run through the original Linux 64 GiB cgroup keeper;
Mac C++/BMI/native/VM execution is prohibited for this SOURCE packet.

Specification: https://webassembly.github.io/spec/core/valid/instructions.html#memory-instructions
Binary memarg: https://webassembly.github.io/spec/core/binary/instructions.html#memory-instructions
Integer memory execution: https://webassembly.github.io/spec/core/exec/instructions.html#memory-instructions
