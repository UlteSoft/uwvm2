# Canonical scalar memory typed transition candidate

This PRIVATE source candidate has not been applied, compiled or run. It is a
narrow descendant of the frozen bulk-memory candidate, which in turn descends
from atomic R2. All those parent changes require exact review and composition
before the sole Linux keeper receives a coherent source/product cut. The
currently applied scalar23 normalized event remains an ancestor, not an actual
native qualification of this candidate.

The pure Core3 validator, integer translator and LLVM translator now pass the
first checked memarg plus scalar opcode value type to one shared fixed-arity
typed operand sequence. A load consumes the selected memory address type. A
store consumes its numeric value and then that address. Total reachable arity
is checked before any pop. Actual normalized types use the existing common
reference metadata projection, so reference-only heap Bot cannot satisfy a
numeric operand; value Bot and absent polymorphic remainder remain distinct.

All bounded memarg scans, cursor safety comments, selected-memory metadata,
load result pushes, static offsets and actual opcode lowering are preserved.
No raw byte reader or runtime guard is added. For a store with both operands
wrong, the reported error now follows the shared top-first pop sequence and
reports the stored value before the address. All three adapters use this same
priority. Underflow still outranks either mismatch, and memory32 address,
memory64 address and stored-value diagnostic payload formats are preserved.

The rules follow the official [Core3 memory validation instructions](https://webassembly.github.io/spec/core/valid/instructions.html)
and [validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html).
The exact common sequence kernel is already present in the working tree; it is
a pinned dependency, not a second copied implementation.

Paired test/0036.scalar_memory_semantics leaves provide eight return assertions
for i32/i64/f32/f64 memory operations at both address widths, sixteen complete
unused valid polymorphic bodies, ten invalid WAST bodies, and a constexpr
component covering sixteen valid sequences plus underflow, mismatch index,
dual mismatch, current-frame boundary, value Bot and reference-only Bot cases.
Each memory has one initial and maximum page. The existing scalar23/boundary
test/0032.fused_scalar_memory gate must also be run against fresh products;
these focused assertions do not replace it.

The finite actual runner pins its tools/products/wrappers and requires the
existing cgroup gate. Assembly, C++ compilation, real semantic results,
musttail/ring behavior, LLVM direct materialization, generated assembly and
performance remain pending. This slice does not finish retained lazy plans,
tiered reuse, all other typed opcode families or admission before observable
initialization effects.
