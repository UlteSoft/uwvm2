# GC data arrays: fast_io scalar endian parsing, source delta r3

The only production edit replaces `gc_object_store::data_element_value`'s
manual byte/shift scalar loop with `fast_io::parse_by_scan` and
`mnp::le_get<8/16/32/64>`. The caller's canonical numeric layout, complete
subtraction-checked segment extent, null and multiplication checks are unchanged.
The helper proves the four scalar widths before forming either pointer extent;
every scan must return `ok` and consume the whole field. There is no allocation,
RAII access guard, lock, new syscall, decimal parser or guest-visible decoder API.

The 16-byte v128 branch remains the exact original raw byte copy. The value
carrier is `array<byte,16>`; shared SIMD integer lane helpers decode with
fast_io little_endian, and their native vector ABI path is selected only on
little-endian hosts. Splitting v128 into decoded uint64 values and copying their
native representation would reverse each lane on big-endian hosts. This source
change deliberately preserves that representation. Existing LLVM/native SIMD
lowering is independently tested; no old big-endian outcome is assumed.

The existing native packed-array fixture's manual little-endian test producer
also uses bounded `fast_io::print_reserve_define` with `le_put<8/16/32/64>` now.
Explicit unsigned narrowing preserves the previous packed truncation without
a throwing range rejection. Its v128 raw-byte producer remains unchanged.
Historical packed-array snapshots and hashes are not rewritten. A new source
freeze and fresh component executables are required.

`04_data_endian_fastio_20261003.wat` has seven actual mutable GC array types,
unaligned source offset one, new_data and init_data, exact i8/i16 signed/unsigned
values, i32/i64 bit patterns, f32/f64 signalling-NaN reinterpret checks, and v128
i8/i16/i32/i64 first/last lane checks. Zero second elements and readback after
`data.drop` verify initialization and retained copied data. This is Core3
[specified data-array execution](https://webassembly.github.io/spec/core/exec/instructions.html),
not a byte-decoder-only test. It has no WASI imports. Official WAT parsing,
validation, actual Wasm identity and execution in all relevant modes are pending.

`gc_data_fastio_codegen_20261003.cc` instantiates the genuine checked public
array_new_data and array_init_data entry points for matched old/new O3 assembly.
It exposes no private pointer/token bypass. Keeper inspection must show fixed
width scans lower to efficient unaligned endian loads/byte swaps, no scalar
byte-accumulation loop and no unintended calls/checks in the inner element loop;
all original extent, type, mutable-field and token authentication remains.
Check packed numeric representation OFF and ON, native word sizes and a genuine
big-endian QEMU target. Run the real native fixture's bounds/no-mutation/stale,
foreign/canonical and actual cohort reclamation controls, not only WAT success.

The recorded before header is a derived fresh baseline: current candidate header
with this one function replaced by the original r2 function. It preserves all
other source axes, including the separately owned default-OFF single-CAS
publisher candidate. It is not the historical R3c/6ABC binary source. Match exact
other macro axes and source/header/vendor/provider tuple for assembly/performance.
The dense r2 API and bitmap r1 packets remain immutable and do not inherit this
later source change. Source-model checks are not C++ compilation, fast_io native
execution, full product validation, or performance evidence. All actual work
remains inside the sole keeper's existing resource scope.
