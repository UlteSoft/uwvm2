# Direct imported Wasm checkpoint producer

SOURCE ONLY; no official WAT validation, native build/execution, LLVM verification,
ASM/performance or cross-platform qualification has run for this packet.

Prerequisite: dynamic-call R2 producer plus actual nested EH/reference/result-root
seam, canonical expected-parent/actual-child result admission and original resume
ABI v2, composed by ROOT into one fresh resumable policy. This proposal adds no
new runtime bridge or authority API and does not edit any host/WASIp1 provider.

A cold compiler witness follows the real initialized imported-function chain.
Before every alias cursor change it proves exact imported-record membership in
the active initialized registry; before reading a final defined record it proves
exact local-vector membership and its parsed declaration/body presence. Null,
interior, one-past, foreign records and host/dl/weak/unresolved leaves do not
produce an imported waiting caller. This is metadata selection, not executable
or resume authority. Original runtime checkpoint effect dispatch still checks
its actual resolved import cache, canonical full source/publication, generation,
profile, target identity and exact ABI before provider entry. Real Wasm children
preserve their parent activation; true native host calls keep the existing
foreign-operation scope and sticky nonreplayable guard.

The direct `0x10` same-walk rich validator produces its existing exact waiting
and after tuple when this real imported Wasm witness is present. Its physical
call path already materializes waiting and installs actual normal-return SSA
PHIs/root handoff. Ordinary/profile-zero and observe_values policies retain the
previous import exclusion and generated bridge choices. No second normative
validator, body decoder or memory-access IR is introduced. Tail transfer remains
unchanged. This does not independently qualify complete instance/WASIp1 replay.

Finite actual-native test:
`debug_checkpoint_actual_direct_import_continuation_runtime main.wasm provider.wasm instruction|unwind direct`
or
`debug_checkpoint_actual_direct_import_continuation_runtime forwarded-main.wasm provider.wasm instruction|unwind forward alias.wasm`.
Compile all 4 included modern WAT fixtures with the current official wasm-tools,
then compile fresh production/compiler/test objects from the ROOT-composed
immutable source. Both products x both policies x 2 routes = 8 runtime cells,
60s per process, only via Linux keeper in the original 64 GiB cgroup. No Mac run.
The main/provider/optional alias files are owned immutable source inputs parsed
and initialized together, not synthetic cached targets or a test-owned bridge.
A real provider instruction pause has 2 live typed frames: the main's imported
call waits with its i64 prefix; provider has actual i64 parameter, initialized
nondefaultable i31 local and live i64 operand. The test retires the original,
changes main memory through a real guest call, resumes the true imported child,
recheckpoints/retire/resumes again, checks 1176, prefix counters=1, postcapture
counter=1 and main memory remains4321. Thus neither main nor provider prefix is
reexecuted. The forwarding route traverses a third module's real imported
record without inventing a native alias activation. Existing canonical owner,
retirement, result-extent, no-original-live and stale-generation rejections stay
required. Header/module and QEMU/native ISA qualification remain pending.

Run the existing actual `debug_checkpoint_host_effects_runtime` regressions with
this same immutable composition to ensure ordinary host imports remain rejected
for replay. Do not count a source witness or clean compile as a runtime PASS.
Primary rule: [Core 3.0 call execution](https://webassembly.github.io/spec/core/exec/instructions.html#exec-call)
and [module import matching](https://webassembly.github.io/spec/core/exec/modules.html#instantiation).
