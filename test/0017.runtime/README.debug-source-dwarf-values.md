# Direct numeric source values, Stage2 source candidate

`locals source THREAD` uses the existing authenticated `locals` operation and
selected stopped participant. It can display **only the current cooperative
frame's copied numeric parameters/locals** when embedded DWARF supplies a finite
supported location. Raw `locals THREAD` and the frozen DAP grammar/capabilities
remain unchanged. This candidate was not compiled/run on the development host;
no production, platform or complete C/C++/Rust debugger capability is claimed.

The actual JIT debug safe point already copies at most 256 real local slots,
parameters first, each a zero-padded 16-byte slot. No new guest IR or normal-run
memory path is added. The runtime now attaches the actual current full-entry
function generation selected alongside the real current body/local types. The
controller retains only copied slots, the **captured opaque pause ticket**, the
complete actual bridge location and that function generation. Resume, cancelled
pause, native-step invalidation and exit clear the captured binding.

For a source query, controller mutex protects those copies. The existing runtime
source-position API acquires a genuine execution-generation lease, ONE pause-domain
guard and then the publication guard. The controller must not wrap this call in
another `while_stopped` callback. The API checks genuine shared ownership for the
source binding and control domain, current source validation/debug publication
epoch, participant pause ticket, original function generation and actual safe-point
bitmap. Controller then checks the returned module, function, runtime epoch,
function generation and `Code-relative expression_begin + captured offset`
against the complete captured location. A decimal PC, raw/aliasing shared pointer,
current pause ticket substituted for an old captured ticket or DAP frame ID is
not permission. Parsing never supplies this authority.

The LLVM-free `source_dwarf_values` component accepts immutable index records
and already-owned copied slots. It first proves a unique active concrete
physical/inline chain. Lexical blocks with **absent** range attributes inherit
the parent; explicitly declared empty/inactive ranges exclude nested inline and
variable records. Abstract origins supply descriptive name/type metadata only.
Active explicit locations take precedence over one default location; ambiguity,
malformed indices and budgets clear all output.

Supported values are exact DW_ATE boolean, signed/signed-char,
unsigned/unsigned-char widths 1/2/4/8 and float widths 4/8. Plans are only direct
`DW_OP_WASM_location` current-function local + terminal `DW_OP_stack_value`, integer
scalar const/lit + stack_value and exact-width small implicit numeric bytes.
The terminal stack-value restriction has a compiler-specific reason:
ROS's `third-parties/llvm/llvm/lib/CodeGen/AsmPrinter/DwarfExpression.cpp`,
`DwarfExpression::addWasmLocation` (lines 843–854 in the reviewed source), encodes
`TI_LOCAL_INDIRECT` as the same Wasm local subopcode 0 while assigning a memory
location; ordinary locals instead have implicit-value semantics. LLVM emits
`DW_OP_stack_value` for implicit value descriptions. See the
[official LLVM source](https://raw.githubusercontent.com/llvm/llvm-project/main/llvm/lib/CodeGen/AsmPrinter/DwarfExpression.cpp).
Therefore a plain Wasm local expression remains unavailable as a variable value;
it may represent a by-value argument's linear-memory address. Existing decoder
unit tests retain separate direct+stack-value and plain-local indirect negatives.
No convention text is used to authorize the omitted dereference.

A local index must be a valid u32 and inside the real captured count/total.
Integer local carriers must be actual i32/i64 and wide enough; floats must match
actual f32/f64. Complete actual native carriers are copied **before** low-source-bit
masking/sign display, so a big-endian host never uses an incorrect byte prefix.
Implicit DWARF bytes are decoded little endian with `fast_io::parse_by_scan` /
`le_get`; integer constants are not truncated to the Wasm address width. Numeric
output uses fast_io. Float output preserves raw bits/NaN payloads.

Pointer/reference/aggregate/packed/unknown-language encodings, fp16, indirect
locals, fbreg/CFA/registers, globals/operand stack, pieces, dereferences, calls and
all unsupported expressions are explicitly unavailable. There is no expression
interpreter, source/external/DWO loader, host pointer dereference or memory-value
query. Metadata `DW_AT_const_value` and variable-piece evaluation are outside this
first piece. A native one-instruction trap invalidates the old cooperative slots;
no real native-PC/register map is available, so source values are unavailable.
Replacement generation 2 cannot reuse original source locations even for an
identical body. No physical caller's Wasm PC or values are guessed.

Default finite-query limits: 65,536 scopes/types/variables/ranges/locations,
ancestry depth 64, captured slots 256, result variables 1,024, metadata strings
1 MiB and result strings 16 KiB. Parse budgets remain Stage1's bounded embedded
input limits; neither layer claims an exact LLVM internal heap cap. Keeper
execution still requires the controlled 64 GiB/no-swap verified-CPU cgroup.

## Qualification packets, remote only

The keeper must build a fresh runtime object and product with this exact source
candidate, including the actual production LLVM-DWARF closure. Frozen Windows and
r11 objects/archives are not inputs. `--runtime-object-sha256` matches the actual
qualified object in the direct compiler argv; it is **not itself** proof of that
object's source/build provenance. Keeper compile/source receipts provide that.

```sh
python3 "$ROOT/test/0017.runtime/run_debug_source_dwarf_values.py" \
  --source-root "$ROOT" --runtime-build "$FRESH_RUNTIME_BUILD" \
  --runtime-object-sha256 "$FRESH_RUNTIME_SHA" --cxx "$CXX" \
  --wasm-tools "$WASM_TOOLS" --out "$EVIDENCE/source-values-runtime"
```

This runs finite numeric/query/formatter units and the real native producer witness
under instruction/unwind. The real two-function fixture verifies actual parameter
5 (not operand-stack guesses), body-local layout, original/replaced generations,
source and control alias owners, stale tickets after resume and a fresh stop,
actual replacement and reset. Its small scope/type metadata is explicitly
synthetic; it does not qualify official compiler locations.

First rerun Stage1 with the updated empty-range parser/query source and its
actual C/C++/Rust DWARF4/5 O0/O1 compiler + `llvm-dwarfdump --verify` receipts.
Synthetic embedded-DWARF tests cover absent lexical ranges, low_pc==high_pc and
an explicitly empty `.debug_ranges` list, with child inline/value exclusion.
Then use a fresh complete product to verify actual compiler-generated source values:

```sh
python3 "$ROOT/test/0017.runtime/run_debug_source_dwarf_values_cli.py" \
  --source-root "$ROOT" --uwvm "$FRESH_DEBUG_PRODUCT" \
  --fixture-dir "$EVIDENCE/dwarf-stage1" --wasm-tools "$WASM_TOOLS" \
  --out "$EVIDENCE/source-values-cli" --native-step
```

Use `--ros` for ROS. `--native-step` requires an independently qualified native
backend; never count omitted native cases as passed. This runner verifies the
six official C/C++/Rust O1 DWARF4/5 formal parameters under both trace policies,
repeat-query stability at the same actual pause, same-body replacement rejection,
missing/malformed/external metadata and native-trap value rejection. A compiler
fixture without a supported direct parameter must fail with the actual transcript,
not be counted as variable support. Every case records commands/logs and source,
product, fixture, tool and receipt hashes. It verifies the actual runner/import
paths, exact compiler fixture source hashes, current Stage1 parser components and
successful official-tool command/log receipts, and checks all input/tool/receipt
hashes before and after qualification. Actual Stage1/Stage2 summaries do not
claim caller/memory/native-register values, all source-level step policies, DAP
source variable scopes or cross-platform debugger completion.

Primary semantics: [WebAssembly DWARF conventions](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md).
Code addresses are offsets in the Wasm Code section content; Wasm locals refer
to the current function, and operand-stack indices start at the bottom. Native
register/frame value support requires a real published native-PC mapping and
bounded location plans; copying a source expression into an LLVM evaluator
would not provide either mapping or read authority.
