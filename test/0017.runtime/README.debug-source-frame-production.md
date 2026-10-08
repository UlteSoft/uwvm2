# Current source-frame production candidate and keeper checks

This patch is a review candidate derived from the held debugger cut. It has no
native result and must not replace a later controller/console cut wholesale.
Verify every immutable before hash, review the diff, then assemble a new coherent
source cut. Existing R4 native/NI/trace/replacement behavior and the metadata
`chvw` compile fix are retained. No runtime packet, code generator, hot memory
access, platform C ABI, guest pointer authority, or file backend is changed.

The new public cold partitions are `source_frames.h/.cppm` and
`source_dwarf_constants.h/.cppm`. Both product aggregates include/export them.
The import graph remains acyclic:

| Partition | Direct language imports |
| --- | --- |
| source_frames | source_dwarf_types, source_scope_path |
| source_dwarf_constants | source_dwarf_types, source_dwarf_variants |
| source_dwarf_objects | types, query, values, variants, source_frames |
| source_dwarf_index | types, variants, source_dwarf_constants |

Every new partition imports the real `fast_io` module. Rebuild its actual PCM
from the fresh module bridge sources; an ordinary-repository LE export does not
establish the ROS export surface. In particular the ROS `mnp::le_get/le_put`
bridge needs the fast_io owner's separately reviewed source cut. Do not conceal
missing exports by including the hosted umbrella in an import-only consumer.

`frame [N|THREAD STOP_ID N]`, `frames [THREAD STOP_ID]` and
`up/down [COUNT|THREAD STOP_ID COUNT]` enumerate/select the current innermost
concrete inline scope, enclosing inline scopes, current physical activation,
then real caller identity labels. A saved selection binds the actual participant,
stop, incarnation, module/function, source owner and runtime/function generations.
Bare commands prefer that explicitly selected participant; otherwise multiple
stopped participants require explicit IDs. Every data query repeats the genuine
combined runtime source/activation proof. Failed ordinal selection retains the
previous cursor. Resume, replacement, new stops and native transition retire it.

`print/ptype THREAD STOP_ID FRAME SELECTOR` and
`locals source THREAD STOP_ID FRAME` are atomic read-only selection/query forms;
they never mutate the shared console cursor. Their DAP frame/scope references
are session-unique and bounded to the complete identified stop. A caller label
has no captured caller PC or local values, so its queries explicitly refuse
without using callee bytes. An outer-frame source next/finish also refuses before
resume; the current source-step origin is still the actual innermost activation.
Select frame 0 for that existing step policy. This is not GDB/LLDB parity.

Direct `DW_AT_const_value` supports integer scalars, exact type-width fixed bits,
signed/unsigned variable-width forms, implicit constants and canonical bool.
Floats, blocks, strings, pointers, aggregates, range overflow and shorter
ambiguous signed fixed values remain unavailable. Constant and storage
attributes on the same concrete variable conflict and become unavailable;
abstract origins never supply a value at a different activation. Lexical
selection uses real ancestry; explicit empty ranges, duplicates and incomparable
overlapping branches cannot become guessed values or addresses. Dynamic bounds,
virtual bases, function calls, arbitrary expressions and pointer following
remain unsupported.

Only the existing private stopped lease and whole-root source-memory copy can
supply frame-relative object bytes. The source locals list retains the existing
direct numeric/constant subset and may report unsupported frame-relative values.
The new pure metadata helpers are cold debug queries; they cannot qualify
checkpoint/continuation restoration. The frame display bounds the entire
worst-case escaped reply to 63,213 bytes, below the 65,536-byte broker cap.

All actual compilation/execution below belongs to the single Linux keeper and
verified 64 GiB cgroup. Do not build or execute this candidate on macOS.
Use fresh output directories, pin source/tool/SDK/standard-library/vendor/link
closure, exact argv/cwd, before/after hashes and return codes. No failed or older
cut may be patched in place and relabelled as this cut.

1. Cheap header components: compile/run `debug_source_frames.cc`,
   `debug_source_frame_scope.cc` and `debug_source_frame_command.cc` with the
   fresh C++26 toolchain and `-I src -I third-parties/fast_io/include`. Compile
   `debug_source_constants.cc` with `-DUWVM_USE_LLVM_JIT`, actual product LLVM
   include definitions and `llvm-config --ldflags --libs debuginfodwarf support
   --system-libs` closure. LLVM's `--cxxflags` must not override the final C++26
   language mode. Keep `-fno-exceptions` profiles aligned with the real product.
2. Actual export closure: fresh fast_io PCM, then types, source_scope_path,
   variants, pieces, query, values, source_frames, source_dwarf_constants,
   objects, selectors and index. Disable implicit modules and supply explicit
   previous name-to-PCM references at every command. Precompile
   `debug_source_frames_module_capsule.cppm` under the real debugger primary
   module name and compile/link/run import-only `debug_source_frames_module.cc`.
   Test both no-LLVM and the actual LLVM-enabled profile in both repositories.
   This focused capsule does not substitute for actual full product compilation.
3. Fresh actual runtime/main products with this reviewed patch and matching
   roots/bridges. Reuse no old binary or stale LLVM/fast_io PCM. Produce the exact
   successful link and complete source fingerprint receipt required by
   `run_debug_language_experience_cli.py`.
4. Start with C, DWARF5, O1, unwind, cases `frame-selection`,
   `inline-frame-selection`, `scalar-constants`. Then C++/Rust and instruction
   policy. Only after capabilities pass broaden to DWARF4/5, O0/O1, both policies
   and all eight cases. Required unavailable values produce nonzero failure.
   A cheap subset cannot establish the complete matrix or direct-constant path.

Each fixture computes 42 through actual C/C++/Rust execution. The three nested
same-name shadows are 104/204/7 at one real inner inline stop. New real leaf
constants are signed -31, unsigned 23 and true. Official `llvm-dwarfdump
--show-form --debug-info --debug-line --debug-ranges --debug-rnglists` records
attribute forms independently of the product query. Direct constant-path
qualification requires an actual named direct attribute and a successful product
value, not a literal or a synthetic FormValue. The finite header/module tests
only qualify their component contracts. Current physical caller reads, selected
outer next/finish, return-value display, full expression evaluation and complete
IDE/platform acceptance remain distinct missing qualifications.

R2 review follow-up (source candidate; native qualification pending)

* Indexing registers all bounded compilation units, then performs a complete
  DIE attribute preflight before any semantic name/type/abstract-origin lookup.
  Attribute names are unique per DIE (DWARF5 section 2.2); the private proof memo
  counts attributes once across reference traversal and semantic walking.
* A directly present DW_AT_location, including an empty DWARF4/5 location list,
  conflicts with direct DW_AT_const_value storage. No fallback scalar appears.
* A successfully read direct integer/boolean constant emits a source-origin row
  with actual stop/thread/Code offset and concrete variable/scope/type DIE keys.
  This is metadata provenance, not a pause ticket, native pointer or read token.
  The real producer CLI joins those keys to the official LLVM dump and concrete
  parent/type/direct-attribute identity. A same-name DIE inventory or a readable
  DW_OP/captured/guest-copy value cannot qualify the direct-attribute path.
* Scalar locals verify independent 1024-row/16KiB input-string budgets before
  formatting, escape C0/C1/non-ASCII/bidi uniformly, and reserve headers/origin
  and a complete trailer inside 63KiB. When needed, only complete rows are kept
  followed by source-variables shown=N total=T truncated=true. DAP displays an
  explicit read-only omission row; it does not infer missing values or authority.
* Small actual-LLVM serialized DWARF4/5 tests are
  debug_source_constant_attributes_index.cc (empty locations, both duplicate
  constant orders, forward type duplicate, exact attribute budgets). These are
  actual parser components, not compiler-produced language/product qualification.
  debug_source_scalar_reply.cc exercises the actual formatter's legal 1024-row
  reply, terminal escapes, independent input cap and maximum-width origin.
  test_language_constant_origin.py and all test_dap*.py are protocol/text join
  tests only. Run them on Linux inside the keeper cgroup before the larger real
  C/C++/Rust producer replay; no local import/test/native run is authorized.

R2 native component argv additions: use the same frozen include paths,
-DUWVM_USE_LLVM_JIT, LLVM23 llvm-config --cxxflags --ldflags --system-libs
--libs debuginfodwarf support and fresh per-product sources/PCM as R1. Compile
and run debug_source_constant_attributes_index.cc and debug_source_scalar_reply.cc
as separate executables in the keeper's 64GiB cgroup. Link closure must match
actual configured product LLVM, not a host SDK. Run each product's existing
7 test_dap*.py suites and test_language_constant_origin.py separately; their
success is protocol qualification only. Then a fresh product link receipt and
actual C DWARF5 O1 unwind scalar-constants/frame-selection/inline-frame-selection
are the first genuine production checks, followed by C++/Rust DWARF4/5 O0/O1
instruction/unwind. Caller locals/PC, non-current frame source next/finish,
GDB finish return value, arbitrary source expressions remain explicit gaps.
