# Actual source into / next / finish qualification

Source-only candidate: this runner and its fixtures have not been compiled or executed locally or remotely yet. Native results must come from the keeper's fresh product closure in the existing Linux 64 GiB / swap-zero / admitted CPU cgroup. Neither AST parsing nor the synthetic step-policy unit is a product source-debugging PASS.

`run_debug_source_step_cli.py` creates six official O1 fixtures (C, C++, Rust; embedded DWARF4/5) and drives the real `-m debug-jit` console in both instruction and unwind policies. The fixture has consecutive nested inline calls at two distinct call sites, a physical noinline callee, and recursive calls with volatile work after return so they cannot simply become a tail-recursion loop. The guest traps unless both exact computed results are correct. This first slice does not qualify source mapping through typed/host tails; the separate activation GC/tail native witness covers producer identity, not language-level source stepping.

The runner's six cases are actual `into`, physical `over`, physical `out`, concrete inline `out`, repeated same-name inline call-site display, and recursive same-function `out`. It observes a genuine entry breakpoint, deletes it, then uses actual Wasm stops to find a mapped statement origin. Source commands must return a later real stop-id and an official statement row. Physical next must skip the callee; physical finish must reach its own currently paused caller; recursive finish must retire one actual activation even when the caller has the same function name. The inline case must leave the particular current inline instance and retain only its real outer path. No caller PC or runtime credential is made from diagnostic frame names. The actual console prints inline frames inner-to-outer; receipts preserve that raw display and explicitly reverse only the owned assertion path to outer-to-inner. Display names and call-site strings do not mint the concrete DIE or activation identities used by the controller.

Every observed current source location is checked against `llvm-dwarfdump`'s real line sequence at the fixture's checked Code-relative offset. The independent file decoder is a bounded fixture oracle and grants no runtime address, expression, source-owner or read authority. It explicitly parses the actual Import vector and rejects every import (plus duplicate or nonconsumed Import vectors), so local Code indices are never silently used as module function indices with imported functions. It accepts only these compiler fixtures' numeric locals, consumes the complete Code payload, and respects `end_sequence` and duplicate row addresses. A toolchain that introduces an import fails this focused fixture qualification. It records actual discriminators; a toolchain that emits only zero discriminators is explicitly recorded as not covering nonzero discriminator stepping, rather than relabelled as a pass for that feature. Current console text does not expose discriminator values directly.

## Actual tools and receipts

Pass the keeper's resolved official LLVM `clang`, `wasm-ld`, `llvm-dwarfdump`, official `wasm-tools`, and installed official `rustc` with its `wasm32-unknown-unknown` core library. Nothing is installed or downloaded by this runner. The runner records tool file hashes and versions; it pins actual Rust target core/compiler-builtins archives when Rust is selected. C/C++ are compiled with `-g -gdwarf-4/5 -O1`; Rust uses full debuginfo, the selected DWARF version, no split debug files, one codegen unit and the explicit supplied linker. Every emitted module must pass official Wasm validation, DWARF verification, and retain actual nested/repeated inline DIEs before any product case runs.

The required build receipt must be assembled from the keeper's actual completed build; a CLI-claimed hash or an old runtime object is insufficient. Its JSON fields are:

```json
{
  "binary_path": "/actual/fresh/uwvm",
  "binary_sha256": "actual linked file SHA256",
  "link_argv": ["/actual/linker", "...", "-o", "/actual/fresh/uwvm"],
  "link_cwd": "/actual/build/working/directory",
  "link_returncode": 0,
  "source_before_file": "/actual/build/source-before.json",
  "source_after_file": "/actual/build/source-after.json"
}
```

The two source fingerprint files come from the actual before/after build using `tools/ci/wasm3_source_fingerprint.py`, must be identical, and include every source/dependency byte plus a verified canonical source_id. The runner matches the current complete `src` membership/bytes to that actual build record. This checks receipt consistency; it does not replace the keeper's original complete compile/object/link provenance. Record those original commands, object hashes, dependency closure and logs separately. Tests reject a different linked output, source tree, imported parser path, modified product, tool, fixture, receipt, source file, or added/removed source file. Only a newly created output directory is used. The final JSON retains every actual tool/console argv, status, transcript hash, mapped PC/line/column/discriminator/statement row, and failed partial cases.

## Keeper commands

Use actual paths from the qualified tool/product receipts; the uppercase names below are task-specific shell variables supplied by the keeper. Run only inside the admitted cgroup.

```sh
python3 "$SOURCE_ROOT/test/0017.runtime/run_debug_source_step_cli.py" \
  --source-root "$SOURCE_ROOT" --uwvm "$FRESH_PRODUCT" \
  --build-receipt "$BUILD_RECEIPT" --wasm-clang "$WASM_CLANG" \
  --wasm-ld "$WASM_LD" --rustc "$RUSTC" --wasm-tools "$WASM_TOOLS" \
  --llvm-dwarfdump "$LLVM_DWARFDUMP" --out "$NEW_OUT" \
  --language c --dwarf-version 5 --case finish
```

This short first slice still runs both diagnostic policies. Omit the last line's selectors for all six language/version fixtures and all six cases. Add `--ros` for the ROS product (`-Raot`); ordinary product selects `-Rcc jit -Rcm full`. Every case also uses qualified `-Rllvm-exception-dispatch native-unwind` and disables object caching. Unsupported producers/targets, missing statements or an optimized-away test origin fail qualification rather than being silently counted as passing.

This does not qualify source variables, arbitrary native memory, external/split DWARF, IDE protocols, midrun attach, native assembly stepping, function replacement or cross-platform source stepping. Those require their independent actual witnesses and product closures.

Primary references: [WebAssembly DWARF code-address and custom-section conventions](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md#code-addresses), [Clang debug options](https://clang.llvm.org/docs/ClangCommandLineReference.html), [Rust codegen/debug/linker options](https://doc.rust-lang.org/rustc/codegen-options/index.html), and [llvm-dwarfdump verification/section options](https://llvm.org/docs/CommandGuide/llvm-dwarfdump.html). The row decoder follows LLVM's actual `DWARFDebugLine::Row::dump` formatting (ROS bundled source: `third-parties/llvm/llvm/lib/DebugInfo/DWARF/DWARFDebugLine.cpp`, table header/dump near lines 510–526), not a fabricated line-table schema.

The five/six numeric row formats also have primary versioned evidence: [LLVM 14 Row::dump](https://github.com/llvm/llvm-project/blob/llvmorg-14.0.6/llvm/lib/DebugInfo/DWARF/DWARFDebugLine.cpp#L452-L465) emits five fields; [LLVM 18 Row::dump](https://github.com/llvm/llvm-project/blob/llvmorg-18.1.8/llvm/lib/DebugInfo/DWARF/DWARFDebugLine.cpp#L458-L472) and bundled LLVM23 emit six including OpIndex. Nonzero OpIndex is rejected for this Wasm oracle. `test_debug_source_step_oracles.py` is a separate synthetic helper unit for actual-import index refusal and these dump formats; it is not compiler, DWARF or product qualification. Run it only in the keeper cgroup before the shortest C DWARF5 finish product slice.
