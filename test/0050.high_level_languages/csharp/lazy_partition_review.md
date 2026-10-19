# Lazy bitcode object-partition review

This is a read-only review of the proposed worker change against the checked-out
LLVM sources. No VM or compiler was started for this review. The initial C#
debugger attempt was interrupted by the controlling matrix before completion;
the process, controller and seven worker TIDs were subsequently absent from
`/proc`. Its diagnostic record and retirement check are preserved under
`recovery/cold-debug-pid42326/`.

The API and ownership model support the change:

- `BitcodeReader.h:198-206` describes `getLazyBitcodeModule` as lazy function-body
  deserialization. Its default keeps module metadata eager. The borrowed
  `MemoryBufferRef` remains valid because the immutable `source_bitcode` local
  outlives `thread_pool.run` and all worker modules/contexts.
- `Function.cpp:603-604` clears `isMaterializable` in `deleteBodyImpl`.
  `BitcodeReader.cpp:7145-7149` immediately succeeds for a function without that
  flag. Consequently, `Module::materializeAll` cannot reload bodies discarded
  through `deleteBody`. `Globals.cpp:408-415` includes `isMaterializable` in
  `isDeclaration`, so a lazy definition still passes the existing definition
  filter.
- `BitcodeReader.cpp:7263-7325` shows that final materialization also finishes
  deferred module parsing and debug/module-flag upgrades. Completing it before
  verification and object emission preserves that work. All errors must retain
  the worker's failure flag and existing clean single-module fallback.
- Function names, cross-partition exposure, global-definition ownership, inline
  assembly ownership, target attributes, and code-generation options should
  remain identical to the eager path. Lazy parsing changes when bodies enter
  each private LLVMContext, rather than their native call ABI.

GNU Win64's object-local personality is the main special case. It must be
explicitly materialized and validated in every partition, retain its
`LinkOnceODR` linkage and COMDAT, and avoid the normal expose/delete path.
`native_exception_symbols.h:103-118,134-158` defines those invariants. The current
proposed worker preserves that branch. Linux's external personality declaration
remains a declaration and should not be forced into a local definition. Linux
guest success does not qualify native Windows unwind execution.

Two boundaries deserve focused regression checks:

- LLVM handles cross-function `blockaddress` references specially.
  `BitcodeReader.cpp:1099-1128,5200-5224,8810-8815` can materialize a referenced
  function before the requested function finishes, or reject a reference to a
  discarded body. The current Wasm LLVM producer contains no `BlockAddress` or
  `CreateIndirectBr` construction found in this review. Keep error fallback
  intact; an explicit two-pass discard-then-materialize sequence makes the
  ownership ordering easiest to verify. Generic arbitrary LLVM IR with cross-
  partition block addresses is not established by this language matrix.
- Debug metadata still exists per private context with the default API options.
  Lazy bodies should reduce the large IR multiplication, but only a new C# cold
  run can measure peak memory and confirm retained Wasm breakpoints/locals,
  restart, semantic output and retirement. Do not infer a memory bound from the
  API alone.

Use a cross-function Wasm throw/catch and indirect-call/table case with nonzero
`-Rct` to exercise partition references, global ownership, personality references
and native unwind tables. Tests hard-coded to `-Rct 0` bypass the changed worker.
For GNU Win64, COFF object checks should verify a real executable personality
COMDAT in each partition and `.pdata`/`.xdata` relocations, with the existing
production unwind observer where its supported execution environment is
available. Such object checks must be reported separately from native Windows
runtime qualification.
