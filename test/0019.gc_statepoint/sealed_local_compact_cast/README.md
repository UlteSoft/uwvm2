These modern Core 3 WAT inputs keep an older immutable object alive in a real
anyref table while more than two allocation polls occur. Their `run` export
returns 42 in the same-type, canonical-equal and local-subtype cases. The f32
case returns the original i32 bits 0x7fa12345; it performs no floating arithmetic
on that signaling NaN.

They are SOURCE fixtures awaiting official wasm-tools assembly and same-byte
Wasmtime/UWVM execution on Linux. No result or performance qualification is
claimed by these files. The collector/cast/cache candidates remain explicitly
guarded and disabled by default until actual whole-runtime qualification.

Use gc_workload_matrix.py older32 with roots65536/steps131072 for the alternating
older-read/newborn-write loop and independent checksum. These small end-only
casts cannot by themselves prove that the cache stayed active during the loop.
Actual LLVM IR, native mapping and entry/allocation receipts are required.

No entry, pause or canonical descriptor can be fabricated to test a positive.
The genuine CLI-owned source, initializer, shared/exclusive admission and
execution generation must survive through the last immediate getter. Include
stop/pause/foreign/dead-hole/refill/teardown sanitizer tests before activation.
