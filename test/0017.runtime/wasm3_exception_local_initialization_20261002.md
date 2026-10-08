# Core 3 exception/local initialization interactions

This source-only addition extends the existing fused-validation runner with
eight called-body fixtures. No product compiler, runtime, or pure validator
changes are included. No local Wasm assembly or VM execution was performed.

The [Core 3 validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html)
records nondefaultable local initialization separately from operand types and
resets it to each control frame's entry checkpoint on exit. `throw` makes the
operand stack polymorphic; an unset nondefaultable local is still invalid.
The [instruction rules](https://webassembly.github.io/spec/core/valid/instructions.html)
require `local.get` to see an initialized local. `try_table` catch payloads
and `catch_all_ref` exception references enter the target label's result stack.
Setting a local outside the exited frames initializes it there. An existing
outer initialization survives an inner assignment, including a caught throw.

Four valid fixtures execute genuine throw/catch paths and check either their
non-null struct payload, a surviving root, or `throw_ref` rethrowing the captured
exception. Four invalid controls test inner-only initialization, nested frames,
exception locals, and a get after a throw in unreachable code. `_start` calls
every probe so lazy compilation cannot omit the tested body. The exn-only pair
also uses the existing explicit GC-disabled policy, because exception reference
types must not depend on enabling GC. Numeric and struct controls are separate.

The sole Linux keeper should run each `eh-local-*` case through the existing
runner with official `wasm-tools` validation first, then pure validation,
interpreter full/lazy, and LLVM full/lazy (plus lazy verification where supplied).
ROS runs pure validation and its two full engines. Use the current admitted
64 GiB cgroup and an independently bound current executable/source identity.
Assembler errors, unrelated crashes, and CLI/loader failures cannot substitute
for the expected code validation error. Source review alone is not a pass.

The original runner beforeimage and immutable existing evidence are preserved.
All pure-validator code remains confined to `validation/standard/wasm3`.
