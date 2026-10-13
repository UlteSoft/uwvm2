# EH declaration metadata round trip supplement

This independent component is **source only, uncompiled and unexecuted**. The
frozen 0024 r2 before corpus is unchanged. The official oracle must first parse
and validate each original WAT/binary; no hand-created result counts as an oracle
or runtime pass.

The 12 consumers cover unused struct/array exception fields, a nonnull noexn
field, an unused function with a nondefaultable exception local, tag payloads
and imported tags, and modules with no local function body. Function, GC and
external reference controls must still be accepted when only EH is disabled.
The final empty `_start` is the only pure-validation target where it exists.
For the unrelated-function local probe, this distinguishes preserving the encoded
module declaration from merely checking the current function's locals.

Pure validation uses an actual deep copy of the parsed module. The component
also copies the real type section and, when the proposed fields exist, checks
the copied exception requirement and the initialized runtime's metadata. The
real full/lazy compiler inputs remain the original prepared, linked instance;
all existing lazy units are materialized. A no-body module explicitly skips the
function-only pure API and does not count the skip as a passing observation.
Its full/lazy module admission still has to reject the unused EH declaration.

CLI triples are `accept|reject none|function|table|global|element|local|tag file.wasm`.
Imported-tag inputs require `--provider provider.wasm` first; the true export
is registered as `eh-policy-provider`. With `--initializer-only` first, the
component invokes the actual initializer feature admission after enabled parse
and initialization, using only an EH-disabled policy. A rejecting case must
terminate with the existing fast_io fatal diagnostic naming the declaration and
`--wasm-feature-enable-exceptions`; an unrelated error/status is not proof. The
keeper must record the raw exit status/output separately from compiler rows.

Native compilation, oracle validation and all runs belong to the SSH Linux
keeper's managed shared 64 GiB cgroup. This fixture does not create a test lane,
execute locally, allocate a guest frame for count-zero locals, or prevalidate a
body before fused translation. Inputs are loaded through fast_io RAII mapping,
and output uses fast_io formatting with explicit decimal manipulators.

References consulted:

- [Core 3 heap types](https://webassembly.github.io/spec/core/syntax/types.html#heap-types):
  exn has no concrete subtypes; noexn remains in its exception hierarchy even
  where nonnull values cannot exist.
- [Binary reference types](https://webassembly.github.io/spec/core/binary/types.html#reference-types).
- [Module and tag validation](https://webassembly.github.io/spec/core/valid/modules.html#tags).
- [Official modern exception reference test](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/exceptions/throw_ref.wast)
  and [official nondefaultable local test](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/local_init.wast).
  These probes isolate declaration-policy retention rather than copying full
  assertion scripts or claiming their behavior has already passed.

The r2 source supplement adds `--both-disabled reject function
unused_struct_exn.wasm`: parsing/initialization and its first run keep both
features enabled, then the strict run disables GC and EH together. The existing
GC type requirement must be the first diagnostic in all applicable modes.

For real runtime formatting, build with `UWVM2TEST_EH_RUNTIME_DIAGNOSTIC` and
link the genuine matching runtime product. `--runtime-fatal-int` or
`--runtime-fatal-llvm` invokes the actual public full preparation API after
tightening the real wf policy. No-body EH failures must print the real module,
feature and subject, bounded offset zero and matching memory indication with
ANSI enabled. The binary in `binary_diagnostic_sentinels.json` is an original
invalid missing-function-end body ending at module_end offset26. Its nonempty
expression contains `block void; end`: the final physical 0x0b closes the nested
block and passes the real parser's minimum-end check, but leaves the function
frame open. Preserve its bytes; do not repair it through WAT. Oracle validation
must reject these unchanged bytes; actual fused compilation must print its
`missing_end` diagnostic at that one-past offset.

r4 correction: the historical 23B empty-expression control is preserved in the
JSON, but it fails the actual parser's `missing_code_body_end` at offset23 before
compilation. Its rejection cannot qualify the runtime compiler formatter.
Original frozen r2/r3 archives retain their historical source prediction; r4 is
the corrected unexecuted plan. No parser metadata or raw span is forged.
The keeper must record raw output/status and distinguish supported native
formatter execution from an unavailable macro/link, unrelated native crash or
stub. These r2 additions remain source only and have not been compiled or run.

The r3 test-only source fix guards optional runtime compiler enumerators using
the same int/JIT profile macros as the full graph fixture, and explicitly
disables LLVM object caching in the actual formatter run. It changes no input,
policy expectation or production patch. An unavailable backend remains a
reported failure, not a qualified run. No native compilation is claimed.
