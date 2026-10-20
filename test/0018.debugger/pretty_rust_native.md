# Native Rust pretty-printer acceptance

The original Rust fixture exercises Vec<i32>, String, &[i32], &str,
Vec<String> and Vec<()>. Real source-line stops were qualified with rustc
1.90.0 (LLVM 20.1.8), wasm32-wasip1 and DWARF 4/5 in all three LLVM-full
executables. The native producer also checks its values on natural exit.
The same tests cover watch, hover, variables, indexed pagination and stale
references after execution resumes.

Modern RawVecInner erases its data pointer to u8. The printer uses the Vec<T>
and RawVec<T> owned template DIEs to recover the element type, then validates
the actual field layout and copied integer/pointer masks. Borrowed fat
references use LLVM's plain-local memory-location recipe and the same
authenticated all-stopped guest-memory reader. Unknown layouts remain
unavailable. This qualification does not establish support for every Rust
compiler ABI or a genuine Wasm64 producer.

Unit is an actual zero-byte Rust base DIE with a single value. A Vec<()> keeps
its logical length and usize::MAX effective capacity. Unit payload expansion
performs no memory reads and does not fabricate integer bits.

The index and numeric-query metadata string budgets are bounded at 8 MiB,
counting repeated owned attribute uses. Explicit smaller budgets continue to
fail closed. Immutable schema proofs retain no PC, locals, values, stop,
generation, pointer or memory-read authority. Foreign metadata spans and
stricter limits take the full validation path.

Use the checked-in fixtures/pretty_rust_native.rs with a wasm32-wasip1
standard library, debuginfo=2, opt-level=0, panic=abort, force-frame-pointers=yes
and dwarf-version=4 or 5. Export pretty_rust_ready. Pass the unchanged Wasm,
its matching absolute source path and exact compiler identity to
run_dap_pretty_producers.py --case rust. All execution tests require the
existing Linux 64 GiB/no-swap cgroup; --ros selects the ROS executable profile.

pretty_printer_rust_std.cc additionally checks the original compiler's
DWARF, erased storage types, nested strings, zero-sized elements, index plans,
unknown masks and bounds. It is a copied-DATA component test, separate from
real guest qualification. test_dap_source_pretty.py checks all 256 byte values
and removes only one validated quote/backslash framing layer for display.
Opaque names and summaries never become expressions, paths or memory handles.


The VecDeque profile uses the actual owning VecDeque<T>/RawVec<T> template
DIEs, including the modern erased RawVecInner pointer. It validates distinct
head/length/capacity/pointer fields, unsigned target-width counters, pointer
availability, and overflow across the entire declared allocation before reading
a displayed prefix. Wrapped prefixes use at most two bounded owned copies;
logical subscripting follows the same ring mapping. VecDeque<String> expands
nested strings through the existing reader and VecDeque<()> performs no payload
reads. Unknown layouts and failed copies preserve the raw object atomically.

fixtures/pretty_rust_deque.rs is an independent real Rust standard-library
producer with wrapped integer/string queues and zero-sized elements. Use the
same rustc1.90 wasm32-wasip1 DWARF4/5 options, export pretty_rust_deque_ready,
and select run_dap_pretty_producers.py --case rust-deque. The driver checks
watch/hover/variables, indexed pagination across the physical wrap, expressions
such as (*values)[2], original program exit, and stale-reference rejection.
pretty_printer_rust_deque.cc additionally checks malformed schemas, unknown
masks, whole-allocation overflow, prefix/read budgets and failed-second-copy
rollback using the unchanged producer metadata. These component checks do not
establish all Rust/compiler ABIs, Wasm64 or a full foreign LLVM VM.


Rc<T>, Arc<T> and their Weak<T> profiles derive sized thin-pointer layouts
from the actual NonNull/control-block/template DIEs. RcInner/ArcInner field
offsets and target-width unsigned usize counters must be proved before reading.
No compiler-version table or host allocation offset selects the layout.
Unsupported fat/unsized pointers and unknown layouts preserve the raw object.

The summary shows logical strong and weak counts, excluding the implicit
weak reference held by the live strong cohort. Its named value, strong and weak
children are detached copies. A live owner can expand nested String/Vec values
and unit payloads. An expired Weak reports strong=0 and weak=0 without reading
the destroyed payload. The empty Weak sentinel requires no guest-memory read.
Locked Arc weak counters, ambiguous or overlapping fields, unavailable pointer
bits, guest-width overflow and failed/partial copies fall back atomically.

fixtures/pretty_rust_rc.rs is an unchanged real Rust standard-library ownership
producer. Use the same Rust1.90 wasm32-wasip1 DWARF4/5 options, export
pretty_rust_rc_ready, and select run_dap_pretty_producers.py --case rust-rc.
The driver checks live/expired/empty Weak, nested String/Vec, unit payloads,
watch/hover/variables, named pagination, original exit and stale references.
For its borrowed arguments, print *number displays the Rc and print **number
selects its pointee; (**numbers)[1] combines Arc Deref and logical Vec indexing.
Weak never receives a Deref operation. No query calls guest code or changes a
reference count.

pretty_printer_rust_rc.cc additionally checks the original compiler metadata
and the schema, mask, bounds, copy and read-budget failures. QEMU replay of
exported owned metadata exercises host ELF/ABI differences separately from
native DWARF parsing and real source sessions. It does not establish a Wasm64
producer, every Rust ABI, or a full foreign LLVM/source VM. Current exact
qualification and measurement limits are recorded in pretty_current_qualification.md.
