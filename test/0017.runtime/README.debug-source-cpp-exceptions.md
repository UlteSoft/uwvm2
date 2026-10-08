# Genuine C++ exceptions and embedded source scopes

This new fixture is source-only and has not been compiled, linked or executed. It preserves the existing C/C++/Rust source-step fixtures, runners and immutable R3 product candidates. Only the sole keeper may run the following short commands inside the admitted Linux 64 GiB, swap-zero cgroup. This file introduces no runner, SDK build, ABI shim, debugger permission or additional execution lane.

`fixtures/debug_source_step_cpp_exceptions.cc` contains genuine C++ typed throw/catch, a cross-function throw, `throw;`, an equal-layout wrong exception type, a real inline call in the outer catch, and seven RAII cleanup objects. Its `_start` traps unless the final result is 10, the original payload is 7, exactly two typed catches and one rethrow occurred, and the seven cleanup markers sum to 127. Before rethrow the leaf/inner-try cleanups must total count 2/sum 5; outer-handler entry requires count 5/sum 47; after leaving that catch requires count 6/sum 111. The outer function cleanup gives the final count 7/sum 127. No output or runtime status string can satisfy those guest assertions instead of real execution.

Source markers identify the leaf, inner/outer try, typed catches, rethrow, inline catch body and post-catch statement. Types and references are real C++ objects in guest memory. The existing bounded numeric source-value implementation need not expose class fields, by-reference catch variables or frame-base locations: unsupported locations remain unavailable. A source DIE or source display never grants host memory access.

## First independent qualification: compiler object and DWARF

Use actual resolved official LLVM tools with WebAssembly code generation; pin their file bytes/version and the source before and after. Create a new output directory with the keeper's current guard. Supply `CPP_EH_CLANG`, `CPP_EH_DWARFDUMP`, `CPP_EH_OBJDUMP`, `CPP_EH_READOBJ`, `CPP_EH_SOURCE` and `CPP_EH_OUT` from the real tool/source receipts. No C++ headers or exception runtime are needed merely to emit this object; unresolved guest ABI symbols in an object are expected and are not a runnable module.

```sh
"$CPP_EH_CLANG" --target=wasm32-wasip1 -std=c++20 -O1 -g -gdwarf-5 \
  -fwasm-exceptions -mllvm -wasm-use-legacy-eh=false -fno-lto \
  -fno-inline-functions -fno-optimize-sibling-calls -c "$CPP_EH_SOURCE" \
  -o "$CPP_EH_OUT/cpp-eh-dwarf5.o"
"$CPP_EH_DWARFDUMP" --verify "$CPP_EH_OUT/cpp-eh-dwarf5.o"
"$CPP_EH_DWARFDUMP" --debug-info --debug-line --debug-ranges --debug-rnglists \
  "$CPP_EH_OUT/cpp-eh-dwarf5.o"
"$CPP_EH_READOBJ" --sections --symbols --relocations "$CPP_EH_OUT/cpp-eh-dwarf5.o"
"$CPP_EH_OBJDUMP" --disassemble "$CPP_EH_OUT/cpp-eh-dwarf5.o"
```

Preserve each actual argv, return code, raw stdout/stderr, object/tool/source SHA and section/relocation report. Start with DWARF5/O1; use the identical flags except `-gdwarf-4` and a fresh filename for DWARF4 later. `-fno-inline-functions` does not disable the explicitly `always_inline` catch helper. Do not strip debug sections or use split/DWO DWARF, LTO, a native target or Emscripten JS exception emulation.

Record the actual compiler DIE tags and ranges. DWARF defines `DW_TAG_try_block` and `DW_TAG_catch_block`, but a compiler may encode these scopes with `DW_TAG_lexical_block` instead; this object is not counted as testing the new tag branches unless its actual dump contains them. The separate `debug_source_dwarf_index.cc` encoded-parser unit exercises both exact tags, absent/explicit-empty/active/inactive ranges, in 18 cells. That unit's real LLVM23 compile/run is independent of any C++ ABI library and must run first. An object with unresolved relocations is not an embedded executable source index or a product source-step PASS.

## Link only when the actual guest ABI is qualified

The matching standard-Wasm-EH libc++abi, libunwind, libc and compiler-builtins closure must exist for the exact Wasm32 target. Retain their original compile flags, archives/member hashes and actual resolved link trace. The ABI libraries must use standard EH too; this fixture cannot make a legacy-EH ABI compatible by changing only its own flag. If unavailable, keep the successful object/DWARF evidence and mark executable qualification unavailable, without delaying the existing no-runtime C/Rust/C++ source-step cases. Do not build a new SDK solely to run this first slice, invent `__cxa_*` stubs, accept unresolved symbols, translate exceptions through JS, or link a native/A64 Windows exception component.

With a qualified official WASI SDK/sysroot, `CPP_EH_SYSROOT` and the matching driver from its receipt, the smallest static command is:

```sh
"$CPP_EH_CLANG" --target=wasm32-wasip1 --sysroot="$CPP_EH_SYSROOT" \
  -fwasm-exceptions -mllvm -wasm-use-legacy-eh=false -fno-lto \
  -nostartfiles -Wl,--no-entry -Wl,--export=_start \
  -Wl,--export=source_exception_leaf -Wl,--export=source_exception_rethrow \
  -Wl,--export=source_exception_outer "$CPP_EH_OUT/cpp-eh-dwarf5.o" \
  -lunwind -o "$CPP_EH_OUT/cpp-eh-dwarf5.wasm"
"$CPP_EH_WASM_TOOLS" validate "$CPP_EH_OUT/cpp-eh-dwarf5.wasm"
"$CPP_EH_WASM_TOOLS" print "$CPP_EH_OUT/cpp-eh-dwarf5.wasm"
"$CPP_EH_DWARFDUMP" --verify "$CPP_EH_OUT/cpp-eh-dwarf5.wasm"
"$CPP_EH_DWARFDUMP" --debug-info --debug-line --debug-ranges --debug-rnglists \
  "$CPP_EH_OUT/cpp-eh-dwarf5.wasm"
```

`-fwasm-exceptions` is required at link time to select the exception-enabled C++ libraries; `-lunwind` is explicit. Capture the actual driver linker invocation and resolved archive members, including the selected exception-enabled libc++abi, rather than assuming a library directory name proves the ABI. This fixture defines `_start`, needs no language-level global constructors, and `-nostartfiles` avoids a second CRT `_start`; if the matching ABI needs additional startup initialization, preserve that actual failure instead of skipping it or changing the fixture assertions. The Wasm libc/ABI dependency is only for real guest C++ exception semantics; this task adds no WASI P2/P3 functionality.

Official output must validate and show standard `try_table` plus `throw`/`throw_ref` as applicable, preserve actual embedded DWARF4/5 custom sections and the catch inline instance, and contain no legacy `try`, `catch`, `catch_all`, `delegate` or `rethrow` instruction. In WAT match complete instruction tokens, not C++ symbol names or substrings of `try_table`, `catch_ref` clauses or custom-section strings. The C++ `throw;` is language syntax, not permission to accept a legacy Wasm `rethrow` opcode. Verify actual function imports and the exported module function indices; the existing source-step runner deliberately rejects imports and is not used unchanged for this ABI-linked module.

## Future shortest actual product cases

After the official module and a fresh current full LLVM product are independently qualified, run each case in a new real `-m debug-jit` session with `-Rllvm-exception-dispatch native-unwind`, object cache disabled, exceptions explicitly enabled, and each of `-Rllvm-call-stack instruction` and `unwind`. Ordinary selects `-Rcc jit -Rcm full`; ROS selects `-Raot`. Use the actual binary/tool/source/module and original build/link receipts. No Linux object or historical R3 source label qualifies a different Windows/macOS product.

Resolve `source_exception_leaf`, `source_exception_rethrow`, `source_exception_outer` from the actual official module export/index report, including all imported functions. Add a genuine entry breakpoint at `(module 0, actual function index, offset 0)`, continue/wait, read the real selected participant, delete the breakpoint, and advance unmapped prologue only with bounded actual `step wasm THREAD` requests. A mapped statement from `bt THREAD` must match the official Code-relative line sequence and actual source marker; its stop-id must be nonzero and fresh. Do not use fabricated thread IDs, source addresses, offset-zero activation identity, symbols/depth as credential, or manually mint source owners.

* At `CPP_EH_INNER_CALL`, source `into` must enter the genuine leaf activation and later reach the typed inner catch through real exceptional control flow.
* From the mapped `CPP_EH_LEAF_ENTRY`, `step source THREAD out` must retire that throwing leaf and reach an actual mapped statement in its real rethrow caller's handler, not fabricate a post-call row.
* From `CPP_EH_OUTER_CALL`, `step source THREAD over` must skip the real child throw/rethrow activations and reach an actual outer-handler statement. From a mapped inner catch, `out` must likewise reach the genuine outer activation. Exact returned statement selection comes from the official line/range oracle; an optimized-away marker is unavailable, not silently accepted.
* The real `source_exception_inline` display belongs only to its concrete catch instance at a covering PC; it must not appear in the leaf/try or post-catch stop. Nonzero discriminators are qualified only if the real row producer emits them.

Continue each session to genuine guest exit 0 so all result, type-match, rethrow and RAII assertions actually execute. Preserve raw command/response streams and mapped PC/line/column/discriminator/range evidence. These future cases are not implemented by a new automatic runner here and have not run. The current live metadata tag fix is a narrow source candidate beyond immutable R3; executable qualification of that fix requires a product built from its own reviewed source closure, never relabelling an older R3 binary.

Primary references: [WASI SDK C++ exception flags and ABI selection](https://github.com/WebAssembly/wasi-sdk/blob/main/CppExceptions.md), [LLVM standard versus legacy Wasm EH option](https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/WebAssembly/WebAssemblyTargetMachine.cpp), [Clang EH and cleanup generation](https://clang.llvm.org/docs/LLVMExceptionHandlingCodeGen.html), [llvm-dwarfdump verification and section options](https://llvm.org/docs/CommandGuide/llvm-dwarfdump.html), and [DWARF5](https://dwarfstd.org/doc/DWARF5.pdf) section 3.8, printed pages 93-94. The bundled ROS LLVM23 source also defines the legacy default at `third-parties/llvm/llvm/lib/Target/WebAssembly/WebAssemblyTargetMachine.cpp:73-81`; provider/tool versions and real output still determine qualification.
