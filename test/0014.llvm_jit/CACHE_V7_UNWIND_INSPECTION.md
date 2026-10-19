# Current v7 new-syntax unwind and private-object inspection (SOURCE only)

This fifth increment retains exact public/previous-private preimages and modifies
three paths per repository. Predecessors include checkpoint15, codec33-r4,
cli27-r2, cross12 and integration12 plus actual production core7/CLI45.

New-syntax native-unwind signed mode now requires the integrated production CLI
seal, actual authenticated cold publication and two fresh-process verified hits,
in addition to its original trap/frame/CFI policy checks. It uses bounded child
PIPEs/pidfd retirement and parent-only logs with required --execution-guard.
Its explicit disabled-cache semantic mode remains available without process-seal
side effects and reports persistent_cache_hit_qualified=false. A disabled semantic
run cannot qualify signed replay or performance.

Generic0072 inspect_jit private-cache mode now requires actual cold/store plus
warm/warm-again verified hits and stable cache bytes before any object decoding.
The exact transitive v7 decoder Source pin from cli27-r2 remains required. Its
separate exclusive-test-capture mode preserves its existing test-macro product/
permission gates and reports authentication false. It cannot supply evidence of
authenticated cache replay. Import-only users of instruction_evidence do not load
the executor. Empty plans fail. New execution_guard data is required for the
actual runner, as in cli27-r2. Numeric-plan/QEMU/runtime origin ineligibility is
still a real positive-test failure, never hidden by capture or disabled mode.

No candidate import/compile/generation/link/test/native/Guest run or SSH I/O
occurred here. No public source was installed and no historical result is
reclassified. Lazy/tiered, DBG-with-cache/refusal semantics, remaining consumers,
complete builds/platforms and VTune/security qualification are still pending.
All eventual execution is restricted to the released existing64GiB/swap0 SSH
Linux cgroup. These Source changes do not assert that any platform passed.
