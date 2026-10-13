This source-only proposal gives pure Core3 validation and actual INT/LLVM fused call_ref cases one reference-operand matcher. Ordinary lazy/tiered dispatch already materializes through the same respective full compiler. It preserves each opcode case's sole bounded typeidx read, original argument/result/underflow transitions and interpreter ring/musttail/LLVM runtime ABI; it adds no guest instruction or body walk. It does not establish all Core3 validator parity.

Rich metadata always uses the same canonical subtype relation; erased funcref, unrelated heaps with the same native ABI, and stale legacy Bot/index witness flags do not bypass it. Legacy function-only tables preserve their exact bounded projected signature behavior. The header-first unit builds a real valid recursive_type_context, then checks canonical subtype identity and callback bounds; it is not guest execution evidence.

The ten WATs require actual pinned official wasm-tools parse and validation before current-product runs. Positives execute `_start` and check actual results; the 100000 tail-call case must retain bounded host-stack behavior. The nofunc case is standard-valid and must trap at execution. The late-unused invalid body must be rejected even if `_start` succeeds by itself, including ordinary lazy/tiered policies with global admission. Function-references and tail-call OFF runs require their exact opcode/policy diagnostics, not arbitrary nonzero exits. Pure validation, INT full/lazy, LLVM full/lazy/tiered and both ROS full modes require one coherent fresh source/SDK compilation; historical objects cannot qualify this change. Native, oracle, sanitizers, named modules, assembly and performance remain false.

Official consulted sources (2026-10-04):
https://webassembly.github.io/spec/core/valid/instructions.html#valid-call-ref
https://webassembly.github.io/spec/core/valid/instructions.html#valid-return-call-ref
https://webassembly.github.io/spec/core/valid/matching.html
https://github.com/WebAssembly/spec/blob/main/test/core/call_ref.wast
