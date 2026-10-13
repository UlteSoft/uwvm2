# Source metadata display authority candidate

This source candidate adds embedded-only inline metadata to the current physical
Wasm frame in LLVM-full `bt THREAD`. This metadata-only portion does not evaluate
DWARF expressions, pieces, registers or memory, and it does not invent caller
Wasm PCs. The separate [Stage2 numeric source candidate](README.debug-source-dwarf-values.md)
adds only genuine current cooperative copied scalar values via `locals source THREAD`;
its source/binary qualification is independent of this metadata-only witness. The existing Wasm `locals` command remains a bounded captured Wasm-slot
view; it is not source-language variable evaluation. DAP's existing frame IDs
and opaque native-PC references gain no read, breakpoint or disassembly rights.
The two repositories carry the same helper/API/fixture sources. Production build
and runtime behavior remain unverified until exact fresh-source cgroup evidence
exists; the frozen Windows and performance builds are separate candidates.

The standalone Stage1 parser remains a metadata component. A new production
DWARF dependency contract is required only for run/controller consumers that
actually call its LLVM-backed parser. The runtime binding/position APIs do not
include or link LLVM DWARF/debugger code. The independent inline query helper
uses only copied metadata and cannot authenticate a running frame.

## Actual data flow

1. The actual CLI owning-source initializer parses and initializes the selected
   source. A real canonical `full_source_instance` owns its parser file/registry.
   A request/filename, observer, integer address or aliasing shared pointer is not
   a source validation or code-lifetime proof.
2. Native full compilation completes the existing fused validation and IR
   emission for every local function, with `err==ok` and the complete local count.
   Only the actual matching initialized main-module record stores the pending
   debug-source fused epoch. Complete native symbols, context and engine ownership
   must then be published before its dedicated debug-source epoch/source validation
   record is usable. The numeric-plan epoch/admission/factory is unchanged.
3. `llvm_jit_debug_bind_source_host_api` holds a genuine execution-domain lease
   and the publication lock. It checks the ready actual unique code publication,
   owning-source control block/main module/assigned ID, actual validation and code
   epoch, and original function generations. Its private constructor creates an
   opaque binding; no caller-created image or public observation-view address can
   create one. Code ownership stays unique; source/index pins alone do not own it.
4. The binding copies bounded embedded custom payloads and original expression
   offsets from that real source. Name and payload bounds are checked before any
   byte inspection/copy. Defaults are 64 selected sections, 64-byte names, 1 MiB
   per payload, 8 MiB total and 65,536 functions. `run.h` passes only the opaque
   binding to controller setup. Controller obtains its image directly from the
   runtime, then Stage1 copies buffers into its immovable LLVM metadata owner.
   No source path, DWO, external URL/loader or arbitrary host pointer is opened.
5. The controller has its management mutex but holds no pause-domain guard while
   calling `llvm_jit_debug_source_position_host_api`. That API owns the ONE
   `with_stopped_participant` guard, which validates the actual ticket, complete
   parked set, selected live slot and cooperative/native state, then supplies the
   actual slot location. The request cannot supply a module/function/decimal PC.
6. Lock order is real `execution_domain.try_enter()` lease, one pause-domain lock,
   publication lock. Reset copies/clears debug control outside the publication
   lock, closes control, drains actual leases, then destroys unique code/registry
   resources under publication ownership. A guarded position callback never
   re-enters the domain, LLVM, guest code, loader, compiler or a memory reader.
   It returns only copied module/function/Code offset and actual generations.
7. The actual ready publication, source owner/control block, runtime/source epoch,
   original function generation and current emitted safe-point bitmap must all
   match. Original source metadata is generation one. After any successful
   function replacement, that function's original metadata is unavailable even
   if replacement bytes/ABI happen to be identical. Old code/CFI remains retained
   through the existing execution drain; this patch changes no owner retirement.
8. Only a matching current innermost physical trace may be expanded. The pure
   metadata query requires one concrete physical subprogram and a unique nested
   concrete inline chain containing the actual Code PC. Absent lexical range attributes
   inherit their parent; explicit inactive lexical ranges exclude descendants.
   Abstract origins contribute descriptions only. Overlapping siblings/physical
   roots fail closed. Query caps: 65,536 scopes/ranges, depth/frames 64, 16 KiB
   copied name/path bytes. Failed queries publish no partial chain.

`inline NAME call-site FILE:LINE:COLUMN` is owned display metadata. Its call-site
location is not the physical caller's Wasm PC or current source line. Physical
`#N` console records retain their existing format. Native traps and missing,
unsupported, ambiguous, stale or over-budget metadata produce explicit
`inline metadata unavailable: ...` reasons. Current source position comes from
actual instruction safe points; DWARF range endpoints are not automatically
certified as decoded Wasm instruction boundaries.

The minimal owning-source binding currently covers the actual canonical selected
main source. Foreign/preloaded module graphs without genuine per-module owners
remain metadata-unavailable. Existing Wasm/line/native controls remain usable.
No C/C++/Rust source-variable or physical caller-location completion is claimed.

## Counterexamples that must fail

| Counterexample | Rejection boundary |
| --- | --- |
| Resume, then pause again at identical module/function/offset | Old pause ticket serial is stale; callback is never invoked. |
| Same binding pointer with another shared control block | Runtime compares its actual publication-owned binding/control block; image/position outputs are cleared. |
| Same domain pointer with another shared control block | Actual runtime-owned debug domain must match; a real slot alone is insufficient. |
| Unknown/zero participant, pending request, incomplete park | Domain refuses before supplying a location. |
| Native one-instruction external park retaining an earlier Wasm slot | `externally_parked` refuses position authority; console never recycles old Wasm PC/trace/locals. |
| Body-only same-ABI replacement to generation two | Current entry and bitmap generation differ from original source generation one. |
| Reset while an old binding strongly retains the file/index | Control closes, actual leases drain, source validation retires and runtime epoch changes; strong data lifetime grants no new query authority. |
| Fresh source loaded at an allocator-reused address | Actual publication identity, runtime epoch and owning control blocks must all match; addresses alone cannot authenticate. |
| Abstract-origin ranges/locations or an ambiguous inline sibling | Only concrete matching ranges are used; no partial/display chain is emitted. |
| Missing/split/external/malformed DWARF or recoverable allocation failure | No partial index is published; metadata stays unavailable and existing debugger controls remain usable. |

## Focused remote qualification

No test below was compiled or run on the development machine. The keeper must
use its existing Linux 64 GiB/no-swap/verified CPU-set cgroup and a separately
frozen fresh candidate, never mix this source into old Windows/r11 artifacts.

```sh
python3 "$ROOT/test/0017.runtime/run_debug_source_metadata_authority.py" \
  --source-root "$ROOT" --runtime-build "$FRESH_RUNTIME_BUILD" \
  --runtime-object-sha256 "$KEEPER_VERIFIED_RUNTIME_OBJECT_SHA256" \
  --cxx "$CXX" --wasm-tools "$WASM_TOOLS" --out "$EVIDENCE/source-authority"
```

Pass SDK ABI/link flags through repeated `--cxxflag`/`--ldflag` as necessary. The
runner's actual `test.command` must link precisely this qualified `runtime.o`,
contain one direct compiler invocation/output and no hidden response/side-output
arguments. The original command/tool/source hashes and before/after source
identity are recorded. An expected SHA proves file bytes/path identity only; the
keeper's actual compile receipt still supplies source/build closure. The runner
explicitly does not certify that closure itself.

The pure inline unit tests cover concrete chains, range boundaries, lexical
exclusion, abstract metadata, ambiguous siblings, cycles and budgets. Domain
tests use actual participating/parked threads and ticket/resume exclusion. Their
external-park negative tests domain accounting only, not a real kernel trap.
The actual native witness uses the production CLI initializer, full validator
and engine publisher, tests owner aliases, actual same-ABI replacement to gen2,
stale tickets, reset and a fresh source under instruction/unwind stack policies.

After independent Stage1 compiler/tool qualification and a fresh product linked
against its actual new LLVM DWARF closure:

```sh
python3 "$ROOT/test/0017.runtime/run_debug_source_inline_metadata_cli.py" \
  --source-root "$ROOT" --uwvm "$FRESH_UWVM" \
  --fixture-dir "$STAGE1_OFFICIAL_FIXTURES" --wasm-tools "$WASM_TOOLS" \
  --out "$EVIDENCE/source-inline-cli" --native-step
```

For ROS add `--ros`. The six actual C/C++/Rust DWARF4/5 O1 fixtures are validated
by official Wasm tooling before execution. Both stack policies require an actual
inline display, stale original metadata after exact same-body replacement and
explicit missing/malformed/external metadata rejection. Derived negative modules
are valid Wasm with deliberately unavailable DWARF. `--native-step` additionally
requires a real qualified machine step and proves that its actual native trap
cannot display the previous inline chain. Unsupported native platforms must omit
that option and cannot claim the kernel/native step case as passed.

Primary conventions: [WebAssembly DWARF](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md),
[LLVM DWARFContext](https://llvm.org/doxygen/classllvm_1_1DWARFContext.html),
[llvm-dwarfdump](https://llvm.org/docs/CommandGuide/llvm-dwarfdump.html).
