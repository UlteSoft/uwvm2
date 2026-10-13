# Short Core 3 native audit

This is an unexecuted source fixture and a bounded plan for the sole Linux keeper.
It complements frozen 0024/0025 inputs; it is not a passed test or full conformance
claim. Do not run native builds or binaries on the local Mac.

First oracle-validate the new `caught_ref_rethrow.wat` with the keeper's existing
pinned official `wasm-tools`. `_start` calls a function that throws payload 49,
catches both payload and a nonnull exception reference with `catch_ref`, executes
`throw_ref`, catches the original tag again, and traps if the recovered payload
is not 49. Both throws execute: the input is not an unreachable-only type probe.
Rules: [Core 3 instructions](https://webassembly.github.io/spec/core/valid/instructions.html)
and official [try_table tests](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/exceptions/try_table.wast)
and [throw_ref tests](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/exceptions/throw_ref.wast).

The first BEFORE component uses frozen 0024 `two_phase_policy.cc` with *matching
R6 headers/runtime only*: `reject table table_exn.wasm`, then unused exn signature
and original zero-count noexn local bytes. The no-provider first cases require
no dynamic import configuration; the existing provider path/SHA is in plan.json
for a later imported-function case. A strict-policy module accepted by BEFORE
is the regression counterexample, not success. The component changes only EH
policy after actual enabled parsing/initialization.

Then run just three legal new-syntax entry probes and two deliberately invalid
unused bodies, using a matched product. The invalid WATs must be emitted without
forcing their typing valid: official `validate` must reject; the product must
also reject admission. Replace only one feature-enable argument with its disable
counterpart for each independent gate check. Record exact raw commands, binary
and source pins, tool/product versions, raw status/output and related diagnostics.
No broad platform matrix is needed for this development smoke round.

The EH metadata change alters parser/runtime layouts. Never use after headers
with an old R6 SDK/shared runtime; R7 AFTER needs a matching rebuilt runtime. The
actual Linux source/tool/provider cut belongs to the keeper, who records its pins
and runs inside the managed shared 64 GiB cgroup. `plan.json` is a command template,
not evidence that a tool or product exists at a particular path or has run.
