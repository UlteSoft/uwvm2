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
