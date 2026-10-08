# Real language debugger qualification and narrow candidates

This production candidate adds compiler-produced C, C++ and Rust fixtures,
an actual-product console recipe, and the real source-frame/constant partitions.
The earlier `.proposed` review components remain unimported historical inputs;
the product uses `source_frames` and `source_dwarf_constants` instead. The patch
has source/AST review only and has not been applied to the shared production
checkout or executed natively. The Linux keeper must run every compiler, fixture,
component and product invocation inside the verified 64 GiB cgroup.

The fixtures cover nested concrete inline instances, two lexical variables
named `shadow`, a real physical call, a two-dimensional array and an owned stack
object. C++ adds a nonvirtual base member, signed/unsigned bit fields and a
signed enumeration. Rust adds a signed payload enum, an `Option<NonZeroU32>`
niche enum and ordinary constant locals that the producer may represent with
`DW_AT_const_value`. Their emitted representation must be inspected rather
than inferred from the source. Each `_start` computes and checks 42.

`run_debug_language_experience_cli.py` starts the fresh product itself and uses
the existing pipe console and official LLVM row oracle. The new recipe adds no
product file backend, Wasm parser or handwritten decimal parser. `print` and
`ptype` queries use the actual participant and stop-id. The recipe compares the
actual live Code offset to the official compiler's line/column row, repeats a
query at the same stop, and checks stale stop denial. Expected literals only
serve as workload assertions after an authentic product reply; they are never
substituted for unavailable guest values. Rust core inline source rows are
advanced with real Wasm stepping and never relabelled as fixture rows.

The independent actual cases are:

- `aggregate`: array element 15; C++ base 17, signed field -3, flags 42 and
  `cold=-1`; Rust real active `Negative` payload -9 and niche `Some(5)`.
- `lexical`: actual inner `shadow=23`, later outer `shadow=7`, and rejection of
  the previous stop-id. An unavailable constant local is a failing capability.
- `inline-vars`: actual nested `middle/inner` scope at the producer's inner
  statement, with readable `inner_cookie=4`.
- `next`: the real source `over` skips the leaf, lands at the caller after-call
  statement and reads `child=16` and the still-active inner shadow.
- `finish`: the real physical leaf returns to the caller and `leaf_cookie`
  cannot leak into caller scope. GDB's return-value display remains a distinct
  unqualified capability; this case does not claim it.
- `frame-selection`: genuine leaf/caller activation identities precede explicit
  frame/up/down checks. Current leaf values must remain readable, a selected
  caller must explicitly refuse data access, an out-of-range selection must
  leave selection unchanged, and down must restore the real leaf. This tests a
  caller-unavailable boundary; it never qualifies selected caller values.
- `inline-frame-selection`: at one real nested inline stop, three same-named
  `shadow` variables must read 104/204/7 in inner/middle/current physical scopes.
  Both atomic frame/value queries and persistent frame/up/down are checked.
  A real Wasm step retires the previous selected stop.
- `scalar-constants`: real leaf variables must display signed -31, unsigned 23
  and canonical true. The official LLVM dump separately records direct
  `DW_AT_const_value` attributes for those names. A successful value from an
  expression or guest-memory location does not qualify the direct-attribute
  path, and a source literal never supplies a missing value.

The default matrix uses C/C++/Rust, DWARF4/5, O0/O1, instruction/unwind and all
eight cases. Unavailable or wrong required values produce a case receipt and a
nonzero result; they cannot turn into a skip or a successful summary. During
development the keeper may request a cheap subset. A successful subset does
not set `all_required_capabilities_passed`. The fixture recipe checks the exact
product link output, SHA-256, unchanged source/dependency fingerprint and all
current `src` inputs against its fresh build receipt. The keeper must separately
prove the complete toolchain/SDK/link closure; an old runtime or binary is not
qualified by this recipe.

Example, using keeper-resolved absolute tool and fresh receipt paths:

```sh
python3 test/0017.runtime/run_debug_language_experience_cli.py \
  --source-root "$SOURCE_ROOT" --out "$FRESH_CASE_DIR" \
  --uwvm "$FRESH_UWVM" --build-receipt "$FRESH_BUILD_RECEIPT" \
  --wasm-clang "$CLANG" --wasm-ld "$WASM_LD" --rustc "$RUSTC" \
  --wasm-tools "$WASM_TOOLS" --llvm-dwarfdump "$DWARFDUMP" \
  --language c --dwarf-version 5 --optimization 1 \
  --diagnostic-policy unwind --case aggregate --case lexical --case next --case finish
```

Use `--ros` for the ROS product's LLVM-full `-Raot` spelling. Keep a fresh output
directory per product/cut; actual logs and `.case.json` retain every failing
value, command, row and source identity. Do not relink/hotpatch a running source
cut or combine old receipt pins with a changed product tree.

## Historical review components and production bridge

`candidates/language_inline_frame_view.h.proposed` converts a checked immutable
concrete scope path to GDB-style frame ordinals: innermost inline first,
enclosing inline next, then the current physical function. It copies bounded
names using `fast_io::concat_std`. `move_up/down` perform subtraction-based
range proofs before scalar narrowing/addition/subtraction. These are metadata
cursors, not memory authority. Production integration must keep the real
source/activation binding alive, filter variables by the selected concrete
scope, and invalidate selection on resume, replacement, native trap or source
retirement. No caller values are captured by this candidate. A physical caller
needs independent authenticated activation/captured-locals support.

`candidates/language_scalar_constant.h.proposed` decodes only a direct, already
bounded LLVM-parsed scalar `DW_AT_const_value`. It supports integer
`sdata/udata/implicit_const`, exact fixed-width integer representations and
canonical bool values; it rejects out-of-range values, floats, strings, blocks,
pointers and aggregates explicitly. Short fixed-width signed values whose top
bit is set are ambiguous and remain unavailable rather than choosing an
extension convention. The candidate does not merge an abstract origin, choose
between contradictory location/constant attributes or broaden concrete scope
availability. Those index/controller rules require a separate reviewed patch.
Accepted constants become owned `constant_value` plans, never addresses.

`candidates/language_selected_scope.h.proposed` adds the missing selected-frame
lookup rule. The requested ordinal must belong to the current concrete
physical/inline path. A variable is visible only in that scope or a currently
active lexical descendant with no intervening inline/physical subprogram.
Lexical shadowing uses ancestor relationships; incomparable active branches,
duplicate chosen identities and same-scope duplicates remain ambiguous. An
explicitly empty lexical range remains inactive. The output contains only a
bounded record index and copied variable/scope identity, with no location or
guest value. The production caller must reauthenticate the selected scope and
actual source activation before choosing a location or copying storage.

The three `debug_language_*_candidate.cc` components exercise finite malformed
metadata, budgets and integer semantics; their hand-built records **do not**
qualify compiler metadata, source values or real frame commands. Suggested
keeper compilation uses C++26, the fresh product's fast_io/include closure and
the actual LLVM headers/libraries for the scalar FormValue test. The inline
cursor and selected-scope components need no LLVM runtime library. Compile and execute all under
the cgroup, and save the exact argv/tool/input SHA-256 before and after.

## Reference contracts and remaining gaps

GDB's [frame selection](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Selection.html)
defines frame 0 as the innermost activation and makes variables depend on the
selected frame. Its [source stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html)
defines `next` at the originating stack level and `finish` at the selected
frame's return. LLDB likewise exposes selected-frame variables in its
[official tutorial](https://lldb.llvm.org/use/tutorial.html). The candidate connects frame/up/down to current physical and concrete inline
scopes using jointly authenticated source/activation queries. Physical caller
identities are labels only; their source PC and locals were not captured.
Selected outer-frame next/finish and physical caller values remain unavailable.
Backtrace display therefore cannot imply those language features work.

[DWARF5](https://dwarfstd.org/doc/DWARF5.pdf) defines variable constants and
concrete inline scopes. The still-open
[fixed-form signedness issue 260921.1](https://dwarfstd.org/issues/260921.1.html)
explains the ambiguity of shorter signed fixed constants. The
[Wasm DWARF convention](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md)
maps code addresses to Wasm Code offsets and describes local/stack/global
locations; it does not permit treating those addresses as host pointers.

Dynamic bounds, virtual base evaluation, full C++/Rust expression execution,
callee result capture, selected physical caller variables, pointer-following,
split/external DWARF and full IDE parity remain unqualified. Unsupported
metadata or unreadable storage must remain explicitly unavailable. These
language values are not a complete VM checkpoint or continuation state.


The production metadata counterparts are `source_frames.h/.cppm` and
`source_dwarf_constants.h/.cppm`. The controller binds the cursor to the real
participant, stop label, top incarnation, module/function, runtime/function
generations and source-owner control block. It repeats the private combined
source+activation query for each lookup, retires selection before resume/new
stops, and never treats an ordinal as a memory capability. Locals-source lists
still expose only the existing direct numeric/constant subset; unreadable
frame-relative values stay explicitly unavailable, while individual `print`
queries may use the already authenticated whole-root memory-copy bridge.

`debug_source_frames.cc`, `debug_source_frame_scope.cc`,
`debug_source_constants.cc` and `debug_source_frame_command.cc` are finite
production-API component tests. The new module capsule and import-only consumer
verify the actual frame/constant export closure. They cannot establish a genuine
compiler-produced value, runtime ownership or IDE acceptance. Use the actual
console recipe for that evidence; no native result is included in this patch.
