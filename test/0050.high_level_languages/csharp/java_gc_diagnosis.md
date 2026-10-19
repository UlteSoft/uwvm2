# Java GC stall diagnosis and WASI pointer-table regression

The same unchanged TeaVM FULL artifact (SHA-256
`356043bf711df30ec65745f9221a6606b28819fd6528844d57115f532277fe95`)
stalled on all three pre-fix UWVM products while passing Wasmtime. Stable and
ROS each timed out after 20 seconds, after JIT completion and the initializer's
metrics. This ruled out a problem confined to the ordinary SDK build.

Two short GDB observations of a newly launched, verified task-owned ROS
process found execution inside Wasm function 209, TeaVM `GC.sweep`, called
through `System.gc()` from `Semantics.allocations()`. A guest-memory snapshot
found heap start `0x53cd0`, end `0x153cd0`, and sweep cursor `0x53db4`.
At that cursor the words were `0, 0, 62`, followed by argv0's UTF-8 pathname.
TeaVM's object-size helper treats a zero class header as a free block and reads
its size from the next word. Both were zero, so the sweep cursor did not advance.

The host's `args_get` appended `argv[argc] = NULL` outside the advertised
pointer table. TeaVM allocated an exact-size int array followed by a byte array;
the extra pointer write cleared that byte array's class header. The same
extra-pointer assumption affected `environ_get`, and both 32/64-bit variants.
The [WASI P1 contract](https://github.com/WebAssembly/WASI/blob/snapshot-01/phases/snapshot/witx/wasi_snapshot_preview1.witx)
uses the sizes call's exact count. [Wasmtime 42.0.1's implementation](https://github.com/bytecodealliance/wasmtime/blob/v42.0.1/crates/wasi/src/p1.rs)
writes only those pointers and each string's NUL byte.

`run_wasi_vector_contract.py` proves the contract with actual core Wasm:
args/environ, counts 0/1/3, a directly adjacent canary, and pointer tables ending
exactly at linear-memory end. Old UWVM reproduced all 12 failures. Fixed
products passed all 12 in each repository (36/36), and Wasmtime passed all ten
CLI-configurable references. Wasmtime CLI cannot configure argc zero; that case
is explicitly covered by UWVM and the native units instead.

The four existing get unit tests now check 32/64-bit real host functions with
zero/one/multiple entries, table-end canaries, exact memory-end tables, UTF-8,
per-string NULs, and a string-buffer tail canary. All four compiled against old
header overlays and reproduced the corruption. All four passed against each
repository's fixed headers (12/12). The wasm64 macro and required success
output prevent a disabled-feature empty main from being counted as a pass.

`run_memory_entry_contract.py` independently checks that initializer writes
overriding active data, i64 values, the last word of a page, memory growth, and
new-page data survive the command entry. Both fixtures passed all three
repositories and Wasmtime. Static review also found that eager publication
returns without rebuilding when already ready. The stall was not a reset or
active-data replay between the two entries.

`wasi_vector_results.json` preserves negative controls, fixed native/guest
results, memory-lifecycle results, and the valid GDB snapshots. GDB attached
only after verifying the owned process command line and original cgroup;
each diagnostic process was then killed and reaped. No Java artifact or
production source was modified by these diagnostic scripts.
