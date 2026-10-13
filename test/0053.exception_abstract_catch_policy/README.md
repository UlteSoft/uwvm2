# Core 3 exception catch matching with GC disabled

SOURCE proposal, not compiled or executed. `wasm_exception_control::core3_matching` is the real helper used by both fused INT and LLVM full walkers. Ordinary lazy/tiered need the separately reviewed R3 fused-admission change; this packet does not claim current lazy rejects an unused invalid body before `_start`.

The old helper used a function-only fallback without the abstract Core 3 hierarchy when the runtime context is null/empty. A real `-WFD-gc` ordinary `0x60` type section keeps that legacy parser path; its context is empty and initializer publishes `nullptr`. Consequently `tag (param (ref null noexn))` with a catch label `exnref` was rejected by the compiler even though the pure wasm3 validator uses the shared context-aware matching rule. The fix calls exactly that existing matcher. It adds no pass, body read, stack operation, runtime check, guest IR, memory guard, or emission metadata.

Primary normative sources, retrieved 2026-10-04:
- https://webassembly.github.io/spec/core/valid/instructions.html#valid-catch — a tagged payload tuple must match its outer label; catch_ref appends nonnull exn.
- https://webassembly.github.io/spec/core/valid/matching.html#match-heaptype — noexn is below exn; nullability and heap families remain distinct.
- https://webassembly.github.io/spec/core/appendix/algorithm.html — one-pass stack/type algorithm integrated with decoding.

`exception_abstract_catch_policy.cc` makes 42 checks on the real runtime catch helper across null, owned-empty, and genuine validated populated contexts. Its checks do not qualify a VM body or native backend.

The 6 modern WAT inputs exercise actual `catch`, `catch_ref` plus `throw_ref`, nullable noexn payloads, invalid nonnullable labels, invalid exn-to-extern family conversion, and a late unused invalid body. Each positive checks its actual catch payload. No GC allocation/opcode, indexed heap, explicit subtype/recursive-group prefix, or typed function-reference opcode is used. Runtime feature settings disable GC and function-references independently while enabling exceptions/reference-types. The parser must use a legacy function-only context; keeper should record metadata once when compiling the fixture.

The standalone runner invokes the existing actual-process 64 GiB/no-swap/topology cgroup guard before any parser/VM child. 30s per command; wasm-tools first assembles all 6 and classifies all 6. Modern Wasmtime executes the 3 positives with GC disabled. Ordinary 11 mode/stack configurations plus ROS 3 configurations yield 84 acceptance cells, followed by 42 exact exceptions-disabled diagnostic cells. Tiered T1 is forced separately, while regular tiered cells do not prove T2 promotion. No crash, timeout, unrelated parser/CLI failure, LLVM resource failure, or retained artifact failure can count as an invalid-body validation success. Cache is disabled for these checks.

Commands in the keeper's original cgroup:
```
<clang include/options matching immutable product> exception_abstract_catch_policy.cc -o <component>
<component>
python3 test/0053.exception_abstract_catch_policy/run_exception_abstract_catch_policy.py --uwvm <ordinary> --wasm-tools <current> --wasmtime <current> --source-id <exact combined cut> --out <new result dir>
python3 test/0053.exception_abstract_catch_policy/run_exception_abstract_catch_policy.py --uwvm <ros> --wasm-tools <current> --wasmtime <current> --source-id <exact combined cut> --out <new result dir> --ros
```

Protected wasm1p1/wasm2 validator folders and WASIp1 are untouched. ROOT should include this helper policy in the final compiler/cache validation revision if the combined revision is frozen; this packet does not overwrite independently pending revision/cache hunks. Source-only inspection is not a conformance, assembly, native EH, performance, platform, or full-task PASS.
