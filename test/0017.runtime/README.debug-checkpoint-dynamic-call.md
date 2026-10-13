# Dynamic call typed checkpoint continuation

This private source proposal adds actual fused LLVM producer/consumer edges for
normally returning `call_ref` and `call_indirect` in both products. It depends on
the nested EH R3, nested reference R3, their exact saved-control/result-root seam,
actual original ABI v2, and the final combined resumable compiler policy chosen
by ROOT. Do not copy an old policy/version label from a historical proposal.
The existing live observation policy and profile-zero guest body are unchanged.

The same existing validator consumes and type-checks arguments, the trailing
reference/table selector and rich result tuple once. New metadata copies that
post-call tuple; it never rereads body bytes or runs another normative matcher.
The before layout owns `prefix + N arguments + one trailing operand`; the waiting
parent retains only its prefix (plus locals/saved controls). Publishing waiting
is after real bounds/null/canonical target checks and before entering the callee.
Actual return values use the existing typed/raw result PHIs and precise root
handoff. A restore-only waiting edge uses the canonical prepared child owner,
not another table/ref lookup. Tail opcodes keep their existing retirement and
musttail ABI; this proposal adds no waiting parent to their retiring activation.

Finite real-VM test (SOURCE ONLY, not run):
`debug_checkpoint_actual_dynamic_call_continuation_runtime <wasm> <instruction|unwind> <kind>`
where kind is `indirect32`, `indirect64`, `ref-global`, `ref-local`, or `ref-subtype-eh`.
Use the matching WAT fixture after official current wasm-tools parse+validation.
Build fresh runtime/compiler/test objects from the composed immutable source.
Run the 2 products x 2 call-stack policies x 5 fixtures = 20 cells with a 60s
process deadline, exclusively in the original Linux 64 GiB cgroup keeper lease.
Do not run this native test on the Mac. Both header and module product configs
must be compiled; ISA/libc/QEMU qualification remains a separate required run.

Every test performs a genuine source parse, initializer, fused full compilation,
real child instruction pause and actual typed capture. It rejects resume while
the old native execution is alive, retires/drains that execution, mutates the
actual table/global target, proves a fresh dynamic guest call reaches the new
callee, then resumes the saved two-frame chain. A second child checkpoint,
retirement and continuation must preserve old prefixes, execute the child's
postcapture effect exactly once, leave the new target's counter unchanged, and
produce 1176. Nondefaultable i31 child local, live i64 operand, and (ref-local)
nondefaultable caller function local are real Core3 syntax, not mock packets.
The fifth fixture executes catch_ref and throw_ref before the dynamic call,
keeps its actual nonnullable exception local in the waiting parent, and returns
an actual `(i64, ref child_box)` tuple through a declared `(i64, ref base)`
function-supertype call. The resumed parent reads the inherited field and must
still produce 1176; canonical subtype projection is a required dependency.
Foreign shared_ptr ownership, wrong output size, stale retired pause and stale
code owner remain rejected. This test inherits the established canonical host
retirement API and must not be "fixed" by weakening its authority guards.

This source slice does not by itself qualify complete instance/file/GC rollback,
WASIp1 resource replay, cross-module import ownership, or covariant function
subtype return admission. The latter requires the separately owned canonical
expected-parent vs actual-child result projection patch. No native/ASM/perf,
official parse, release eligibility or all-platform pass is claimed here.
Ordinary/observer IR comparisons and reserved-debugger performance still need
actual source-matched Linux measurements after ROOT composes the final policy.

Primary rules: Core 3.0 instruction validation and execution, specifically
[call_ref](https://webassembly.github.io/spec/core/valid/instructions.html#valid-call-ref)
and [call_indirect](https://webassembly.github.io/spec/core/exec/instructions.html#exec-call-indirect),
[table text syntax](https://webassembly.github.io/spec/core/text/modules.html#tables),
and official [call_ref tests](https://github.com/WebAssembly/spec/blob/main/test/core/call_ref.wast).
