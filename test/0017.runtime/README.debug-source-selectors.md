# Read-only source object selectors: v4 source qualification

The finite grammar is `NAME(.member|[signed-decimal-index])*`. A root name can
contain exact `::`-qualified producer names. ASCII identifiers and exact numeric
member names are accepted. Each index uses fast_io decimal scanning with exact
token consumption; identifier categories use fast_io character classification.
Names are copied with fast_io concat. Calls, casts, assignments, pointer arrows,
dereferences, arithmetic, whitespace-separated expressions and arbitrary DWARF
evaluation are outside this grammar.

This provides a subset of [GDB variable/member/array queries](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Variables.html)
and [GDB expressions](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Expressions.html).
It does not claim the general GDB language evaluator or [LLDB variable formatter
system](https://lldb.llvm.org/use/variable.html). No host or guest function is
executed and no pointer is followed. Unsupported/dynamic metadata is explicit
unavailable. The runtime must authenticate the actual root variable/source stop,
copy its entire bounded object under one private ticket, and revalidate returned
activation/source identity and offset before passing the owned copy here.

`parse_source_object_selector` returns owned `root_name` and finite steps.
`query_selected_object_value` and its `with_known_bits` overload operate only on
the immutable metadata graph and that already-owned root byte span. Type-only
queries use `query_selected_object_type`; active Rust variant fields still need
a proven discriminant and remain unavailable if their metadata cannot select one.
The output contains owned display nodes, no metadata/span pointers or read
callbacks. Failures clear output and never publish a partial selected path.

Array indexing retains true lower bounds, inclusive signed index domains and
remaining multidimensional row metadata. Signed `INT64_MIN` lower bounds are
supported with checked unsigned ordinal differences. Dimensions/element size,
stride multiplication, parent extent and offset addition are checked before
selection. Dynamic bounds, column-major/noncontiguous layouts, invalid domains
and out-of-range indices are rejected. Direct path resolution allows index 500
within a proven 1000-element object without materializing 499 preceding entries
or defeating the normal display truncation budget.

C++ directly declared fields hide inherited fields. Constant nonvirtual base
paths can supply a unique field. Repeated bases remain ambiguous; virtual or
dynamic offsets cannot become offset zero. One traversal/edge budget covers all
base branches and all selector steps, while a depth limit bounds cycles. Selected
bit-fields retain the original owner type/base and relative data-bit offset.
They cannot act as arrays or structures. Active variant fields require all
discriminant bits; unknown bits in unrelated fields preserve other precision.

Function-local statics currently follow their actual defining physical source
scope. Exact qualified names cannot manufacture a different active function.
Actual producer tests verify availability in that scope and explicit unavailable
across functions. Extending cross-function static access would need a separate
static-data lookup/authority rule, never a fabricated frame-base binding.

The Linux keeper alone compiles/runs the components and actual producer cases
in the existing 64 GiB/swap0 cgroup. `debug_source_dwarf_selectors.cc` covers
negative/MIN/domain-overflow arrays, partial rows, inheritance hiding/ambiguity,
unknown bases/cycles, owner-relative bit-fields, active variants and failed
grammar/budget/type-index cases. Fresh real C/C++/Rust producer metadata tests
also query actual array/member layouts, C++ inherited fields/bit-fields and deny
implicit self-pointer dereference. These metadata tests do not qualify the
production stopped-root memory-copy bridge, native code generation or parity.

The Rust aggregate fixture uses `#[unsafe(no_mangle)]` because the runner selects
[Rust Edition 2024](https://doc.rust-lang.org/edition-guide/rust-2024/unsafe-attributes.html).
Earlier frozen v1/v2/v3 sources retain their original bytes; native results must
be attributed to the exact new frozen candidate, SDK/tool and executable hashes.
