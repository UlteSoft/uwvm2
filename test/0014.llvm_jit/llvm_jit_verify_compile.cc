#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include "native_unwind_test_policy.h"

namespace
{

    // Generated from test/0014.llvm_jit/nontrivial_start.wat to keep this test
    // self-contained and independent from a host-installed wat2wasm.
    // Regenerate with wat2wasm + xxd -i if the .wat fixture changes.
    inline constexpr ::std::array<unsigned char, 98uz> nontrivial_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u,
        0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au,
        0x3du, 0x01u, 0x3bu, 0x01u, 0x01u, 0x7fu, 0x41u, 0x00u, 0x21u, 0x00u, 0x02u, 0x40u,
        0x03u, 0x40u, 0x20u, 0x00u, 0x41u, 0x05u, 0x48u, 0x45u, 0x0du, 0x01u, 0x20u, 0x00u,
        0x41u, 0x01u, 0x6au, 0x21u, 0x00u, 0x0cu, 0x00u, 0x0bu, 0x0bu, 0x20u, 0x00u, 0x41u,
        0x05u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x00u, 0x41u, 0x2au, 0x36u, 0x02u,
        0x00u, 0x41u, 0x00u, 0x28u, 0x02u, 0x00u, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u, 0x00u,
        0x0bu, 0x0bu};

    // Generated from a tiny `_start` fixture that uses `select`, stores the
    // result to a local, and traps if the selected value is wrong.
    inline constexpr ::std::array<unsigned char, 56uz> select_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u,
        0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au, 0x18u, 0x01u, 0x16u, 0x01u, 0x01u,
        0x7fu, 0x41u, 0x0au, 0x41u, 0x14u, 0x41u, 0x01u, 0x1bu, 0x21u, 0x00u, 0x20u, 0x00u,
        0x41u, 0x0au, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    // The block is otherwise valid as an i32-result block, but ff 7f is an invalid overlong spelling of the direct i32 blocktype.
    inline constexpr ::std::array<unsigned char, 43uz> overlong_i32_blocktype_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u,
        0x0au, 0x0bu, 0x01u, 0x09u, 0x00u, 0x02u, 0xffu, 0x7fu, 0x41u, 0x00u, 0x0bu, 0x1au, 0x0bu};

    // Generated from unaligned_memory_start.wat.  Wasm memarg align=4 is only a hint, so an i32 load/store at address 1
    // is valid and must execute without LLVM being given a false four-byte alignment guarantee.
    inline constexpr ::std::array<unsigned char, 68uz> unaligned_memory_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u,
        0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au,
        0x1fu, 0x01u, 0x1du, 0x00u, 0x41u, 0x01u, 0x41u, 0x92u, 0xe8u, 0xd8u, 0xc2u, 0x07u,
        0x36u, 0x02u, 0x00u, 0x41u, 0x01u, 0x28u, 0x02u, 0x00u, 0x41u, 0x92u, 0xe8u, 0xd8u,
        0xc2u, 0x07u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    // Generated from Wasmtime's wasi_misaligned_pointer.wat regression fixture.  Unlike ordinary Wasm loads/stores above, the WASI Preview 1 WITX
    // guest ABI requires pointer arguments to be aligned to their pointee type.  fd_write receives ciovec-array address 1 and must therefore trap.
    // Source: https://github.com/bytecodealliance/wasmtime/blob/main/tests/all/cli_tests/wasi_misaligned_pointer.wat
    inline constexpr ::std::array<unsigned char, 106uz> wasi_misaligned_pointer_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x0cu, 0x02u, 0x60u,
        0x04u, 0x7fu, 0x7fu, 0x7fu, 0x7fu, 0x01u, 0x7fu, 0x60u, 0x00u, 0x00u, 0x02u, 0x23u,
        0x01u, 0x16u, 0x77u, 0x61u, 0x73u, 0x69u, 0x5fu, 0x73u, 0x6eu, 0x61u, 0x70u, 0x73u,
        0x68u, 0x6fu, 0x74u, 0x5fu, 0x70u, 0x72u, 0x65u, 0x76u, 0x69u, 0x65u, 0x77u, 0x31u,
        0x08u, 0x66u, 0x64u, 0x5fu, 0x77u, 0x72u, 0x69u, 0x74u, 0x65u, 0x00u, 0x00u, 0x03u,
        0x02u, 0x01u, 0x01u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u, 0x13u, 0x02u, 0x06u,
        0x6du, 0x65u, 0x6du, 0x6fu, 0x72u, 0x79u, 0x02u, 0x00u, 0x06u, 0x5fu, 0x73u, 0x74u,
        0x61u, 0x72u, 0x74u, 0x00u, 0x01u, 0x0au, 0x0fu, 0x01u, 0x0du, 0x00u, 0x41u, 0x01u,
        0x41u, 0x01u, 0x41u, 0x01u, 0x41u, 0x00u, 0x10u, 0x00u, 0x1au, 0x0bu};

    // A typed select immediate is a vector with exactly one result type.  An
    // empty vector used to be accepted as an untyped select by the legacy
    // translator, although WebAssembly 1.1 requires this module to be rejected.
    inline constexpr ::std::array<unsigned char, 57uz> select_t_empty_result_types_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u,
        0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au, 0x19u, 0x01u, 0x17u, 0x01u, 0x01u,
        0x7fu, 0x41u, 0x0au, 0x41u, 0x14u, 0x41u, 0x01u, 0x1cu, 0x00u, 0x21u, 0x00u, 0x20u,
        0x00u, 0x41u, 0x0au, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    // WebAssembly 1.1 scalar fixture:
    // - i32.extend8_s and i64.extend8_s require the sign-extension feature;
    // - select_t exercises typed select immediate decoding;
    // - i32.trunc_sat_f32_u requires nontrapping float-to-int.
    inline constexpr ::std::array<unsigned char, 90uz> wasm1p1_scalar_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u,
        0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au, 0x3au, 0x01u, 0x38u, 0x00u, 0x41u,
        0x80u, 0x01u, 0xc0u, 0x41u, 0x80u, 0x7fu, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x42u,
        0x80u, 0x01u, 0xc2u, 0x42u, 0x80u, 0x7fu, 0x52u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u,
        0x05u, 0x41u, 0x07u, 0x41u, 0x00u, 0x1cu, 0x01u, 0x7fu, 0x41u, 0x07u, 0x47u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x43u, 0x00u, 0x00u, 0xc0u, 0xbfu, 0xfcu, 0x01u, 0x41u, 0x00u,
        0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    inline constexpr ::std::string_view wasm1p1_scalar_runtime_args{
        "--runtime-llvm-jit-cache-path disable --wasm-feature-wasm1.1"};

    inline constexpr ::std::string_view wasm1p1_all_runtime_args{
        "--runtime-llvm-jit-cache-path disable --wasm-feature-wasm1.1"};

    // WebAssembly 1.1 scalar edge fixture.  This covers every sign-extension
    // opcode and every saturating float-to-int opcode at clamp/NaN boundaries.
    inline constexpr ::std::array<unsigned char, 247uz> wasm1p1_scalar_edges_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u,
        0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au, 0xd6u, 0x01u, 0x01u, 0xd3u, 0x01u,
        0x00u, 0x41u, 0xffu, 0x01u, 0xc0u, 0x41u, 0x7fu, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu,
        0x41u, 0x80u, 0x80u, 0x02u, 0xc1u, 0x41u, 0x80u, 0x80u, 0x7eu, 0x47u, 0x04u, 0x40u,
        0x00u, 0x0bu, 0x42u, 0xffu, 0x01u, 0xc2u, 0x42u, 0x7fu, 0x52u, 0x04u, 0x40u, 0x00u,
        0x0bu, 0x42u, 0x80u, 0x80u, 0x02u, 0xc3u, 0x42u, 0x80u, 0x80u, 0x7eu, 0x52u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x42u, 0x80u, 0x80u, 0x80u, 0x80u, 0x08u, 0xc4u, 0x42u, 0x80u,
        0x80u, 0x80u, 0x80u, 0x78u, 0x52u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x43u, 0x00u, 0x00u,
        0xc0u, 0x7fu, 0xfcu, 0x00u, 0x41u, 0x00u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x43u,
        0x00u, 0x00u, 0x80u, 0xbfu, 0xfcu, 0x01u, 0x41u, 0x00u, 0x47u, 0x04u, 0x40u, 0x00u,
        0x0bu, 0x44u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xe0u, 0x41u, 0xfcu, 0x02u,
        0x41u, 0xffu, 0xffu, 0xffu, 0xffu, 0x07u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x44u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xf0u, 0x41u, 0xfcu, 0x03u, 0x41u, 0x7fu,
        0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x43u, 0x00u, 0x00u, 0xc0u, 0x7fu, 0xfcu, 0x04u,
        0x42u, 0x00u, 0x52u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x43u, 0x00u, 0x00u, 0x80u, 0xbfu,
        0xfcu, 0x05u, 0x42u, 0x00u, 0x52u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x44u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0xe0u, 0x43u, 0xfcu, 0x06u, 0x42u, 0xffu, 0xffu, 0xffu,
        0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0xffu, 0x00u, 0x52u, 0x04u, 0x40u, 0x00u, 0x0bu,
        0x44u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xf0u, 0x43u, 0xfcu, 0x07u, 0x42u,
        0x7fu, 0x52u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    // WebAssembly 1.1 bulk-memory fixture for memory.init, data.drop,
    // memory.copy, and memory.fill.
    inline constexpr ::std::array<unsigned char, 118uz> wasm1p1_bulk_memory_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u,
        0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0cu,
        0x01u, 0x01u, 0x0au, 0x43u, 0x01u, 0x41u, 0x00u, 0x41u, 0x00u, 0x41u, 0x00u, 0x41u,
        0x10u, 0xfcu, 0x0bu, 0x00u, 0x41u, 0x04u, 0x41u, 0x01u, 0x41u, 0x04u, 0xfcu, 0x08u,
        0x00u, 0x00u, 0x41u, 0x08u, 0x41u, 0x04u, 0x41u, 0x04u, 0xfcu, 0x0au, 0x00u, 0x00u,
        0xfcu, 0x09u, 0x00u, 0x41u, 0x04u, 0x2du, 0x00u, 0x00u, 0x41u, 0x05u, 0x2du, 0x00u,
        0x00u, 0x6au, 0x41u, 0x08u, 0x2du, 0x00u, 0x00u, 0x6au, 0x41u, 0x0bu, 0x2du, 0x00u,
        0x00u, 0x6au, 0x41u, 0xf8u, 0x00u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu, 0x0bu,
        0x09u, 0x01u, 0x01u, 0x06u, 0x0au, 0x14u, 0x1eu, 0x28u, 0x32u, 0x3cu};

    // A minimal passive data segment whose only proposal instruction is
    // data.drop.  Keeping it separate from the larger bulk-memory fixture
    // verifies that the preflight reports the actual first unsupported
    // subopcode rather than a generic materialization failure.
    inline constexpr ::std::array<unsigned char, 53uz> wasm1p1_data_drop_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u,
        0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0cu,
        0x01u, 0x01u, 0x0au, 0x07u, 0x01u, 0x05u, 0x00u, 0xfcu, 0x09u, 0x00u, 0x0bu, 0x0bu,
        0x04u, 0x01u, 0x01u, 0x01u, 0x78u};

    // A minimal reference-types instruction fixture.  This distinguishes the
    // unsupported ref.null lowering from table and bulk-memory capability
    // misses.
    inline constexpr ::std::array<unsigned char, 39uz> wasm1p1_ref_null_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u,
        0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au, 0x07u, 0x01u, 0x05u, 0x00u, 0xd0u,
        0x70u, 0x1au, 0x0bu};

    // The target of ref.func is declared solely by its function export.  The
    // reconstructed runtime-validation module must preserve that declaration
    // before the reduced backend rejects ref.func in capability preflight.
    inline constexpr ::std::array<unsigned char, 52uz> wasm1p1_export_declared_ref_func_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x03u, 0x02u, 0x00u, 0x00u, 0x07u, 0x13u, 0x02u, 0x06u, 0x5fu,
        0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x01u, 0x06u, 0x74u, 0x61u, 0x72u, 0x67u,
        0x65u, 0x74u, 0x00u, 0x00u, 0x0au, 0x0au, 0x02u, 0x02u, 0x00u, 0x0bu, 0x05u, 0x00u,
        0xd2u, 0x00u, 0x1au, 0x0bu};

    // A minimal table.get body makes the table capability diagnostic
    // independent of the larger table/bulk-memory fixture and pins its exact
    // function-relative instruction offset.
    inline constexpr ::std::array<unsigned char, 47uz> wasm1p1_table_get_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
        0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u,
        0x0au, 0x09u, 0x01u, 0x07u, 0x00u, 0x41u, 0x00u, 0x25u, 0x00u, 0x1au, 0x0bu};

    // Even without a reference instruction, an externref local cannot be
    // represented by the current scalar-only LLVM local/entry ABI.
    inline constexpr ::std::array<unsigned char, 38uz> wasm1p1_externref_local_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u,
        0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au, 0x06u, 0x01u, 0x04u, 0x01u, 0x01u,
        0x6fu, 0x0bu};

    // WebAssembly 1.1 table/reference fixture covering ref.null,
    // ref.is_null, ref.func, table.get/set, table.init/drop/copy/grow/size/fill.
    inline constexpr ::std::array<unsigned char, 181uz> wasm1p1_table_ref_bulk_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x04u, 0x03u, 0x00u, 0x00u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u,
        0x00u, 0x08u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u,
        0x00u, 0x02u, 0x09u, 0x06u, 0x01u, 0x01u, 0x00u, 0x02u, 0x00u, 0x01u, 0x0au, 0x84u,
        0x01u, 0x03u, 0x02u, 0x00u, 0x0bu, 0x02u, 0x00u, 0x0bu, 0x7cu, 0x01u, 0x01u, 0x7fu,
        0xfcu, 0x10u, 0x00u, 0x41u, 0x08u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0xd0u, 0x70u,
        0xd1u, 0x21u, 0x00u, 0xd2u, 0x00u, 0xd1u, 0x20u, 0x00u, 0x6au, 0x21u, 0x00u, 0x41u,
        0x00u, 0xd0u, 0x70u, 0x41u, 0x02u, 0xfcu, 0x11u, 0x00u, 0x41u, 0x02u, 0x41u, 0x00u,
        0x41u, 0x02u, 0xfcu, 0x0cu, 0x00u, 0x00u, 0xfcu, 0x0du, 0x00u, 0x41u, 0x04u, 0x41u,
        0x02u, 0x41u, 0x02u, 0xfcu, 0x0eu, 0x00u, 0x00u, 0xd2u, 0x00u, 0x41u, 0x01u, 0xfcu,
        0x0fu, 0x00u, 0x41u, 0x08u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0xfcu, 0x10u, 0x00u,
        0x41u, 0x09u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x00u, 0x25u, 0x00u, 0xd1u,
        0x20u, 0x00u, 0x6au, 0x21u, 0x00u, 0x41u, 0x04u, 0x25u, 0x00u, 0xd1u, 0x20u, 0x00u,
        0x6au, 0x21u, 0x00u, 0x41u, 0x08u, 0xd0u, 0x70u, 0x26u, 0x00u, 0x41u, 0x08u, 0x25u,
        0x00u, 0xd1u, 0x20u, 0x00u, 0x6au, 0x41u, 0x03u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu,
        0x0bu};

    // WebAssembly 1.1 SIMD fixture covering v128 memory ops, constants,
    // splat, lane extraction, arithmetic, equality, and all_true.
    inline constexpr ::std::array<unsigned char, 132uz> wasm1p1_simd_basic_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u,
        0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au,
        0x5fu, 0x01u, 0x5du, 0x01u, 0x01u, 0x7fu, 0x41u, 0x00u, 0xfdu, 0x0cu, 0x01u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0xfdu, 0x0bu, 0x02u, 0x00u, 0x41u, 0x00u, 0xfdu, 0x00u, 0x02u, 0x00u,
        0x41u, 0x05u, 0xfdu, 0x11u, 0xfdu, 0xaeu, 0x01u, 0xfdu, 0x1bu, 0x02u, 0x21u, 0x00u,
        0xfdu, 0x0cu, 0x06u, 0x00u, 0x00u, 0x00u, 0x07u, 0x00u, 0x00u, 0x00u, 0x08u, 0x00u,
        0x00u, 0x00u, 0x09u, 0x00u, 0x00u, 0x00u, 0x41u, 0x00u, 0xfdu, 0x00u, 0x02u, 0x00u,
        0x41u, 0x05u, 0xfdu, 0x11u, 0xfdu, 0xaeu, 0x01u, 0xfdu, 0x37u, 0xfdu, 0xa3u, 0x01u,
        0x20u, 0x00u, 0x41u, 0x08u, 0x46u, 0x71u, 0x45u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    // WebAssembly 1.1 multi-value fixture. The helper returns two i32s and
    // `_start` consumes them. The current native LLVM ABI is scalar, so pure
    // AOT must reject this module during capability preflight instead of
    // routing it through the interpreter.
    inline constexpr ::std::array<unsigned char, 59uz> wasm1p1_multivalue_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x09u, 0x02u, 0x60u,
        0x00u, 0x02u, 0x7fu, 0x7fu, 0x60u, 0x00u, 0x00u, 0x03u, 0x03u, 0x02u, 0x00u, 0x01u,
        0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x01u,
        0x0au, 0x15u, 0x02u, 0x06u, 0x00u, 0x41u, 0x0au, 0x41u, 0x20u, 0x0bu, 0x0cu, 0x00u,
        0x10u, 0x00u, 0x6au, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    inline constexpr ::std::string_view wasm1p1_multivalue_runtime_args{
        "--runtime-llvm-jit-cache-path disable --wasm-feature-wasm1.1"};

    // The function itself has the MVP `() -> ()` type, but its block uses the
    // valid Wasm 1.1 s33 type-index form `(type 0)`.  ROS does not yet lower
    // type-index/multi-value block signatures, so AOT must reject it before IR
    // allocation rather than reclassifying this as an illegal one-byte MVP
    // blocktype or retaining partial IR.
    inline constexpr ::std::array<unsigned char, 39uz> wasm1p1_type_index_block_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u,
        0x0au, 0x07u, 0x01u, 0x05u, 0x00u, 0x02u, 0x00u, 0x0bu, 0x0bu};

    // Generated from:
    // (module
    //   (type $t0 (func))
    //   (type $t1 (func (param i32)))
    //   (memory 1)
    //   (func $leaf_trap (type $t1) (param i32)
    //     local.get 0
    //     i32.load
    //     drop)
    //   (func $middle (type $t1) (param i32) local.get 0 call $leaf_trap)
    //   (func $wrapper (type $t1) (param i32) local.get 0 call $middle)
    //   (func $_start (type $t0) (export "_start") i32.const -1 call $wrapper))
    //
    // The out-of-bounds load traps at a real Wasm access site. Every generated
    // Wasm function remains a concrete native function, so explicit unwind must
    // recover the four physical frames without synthesizing inline frames.
    inline constexpr ::std::array<unsigned char, 75uz> native_unwind_trap_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x08u, 0x02u, 0x60u, 0x00u, 0x00u,
        0x60u, 0x01u, 0x7fu, 0x00u, 0x03u, 0x05u, 0x04u, 0x01u, 0x01u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u,
        0x00u, 0x01u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x03u,
        0x0au, 0x1fu, 0x04u, 0x08u, 0x00u, 0x20u, 0x00u, 0x28u, 0x02u, 0x00u, 0x1au, 0x0bu, 0x06u, 0x00u,
        0x20u, 0x00u, 0x10u, 0x00u, 0x0bu, 0x06u, 0x00u, 0x20u, 0x00u, 0x10u, 0x01u, 0x0bu, 0x06u, 0x00u,
        0x41u, 0x7fu, 0x10u, 0x02u, 0x0bu};

    [[maybe_unused]] void test_runtime_entry()
    {
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t module{};
        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option opt{};
        ::uwvm2::validation::error::code_validation_error_impl err{};

        [[maybe_unused]] auto storage{
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_all_from_uwvm(module, opt, err, 0uz)};
    }

    [[nodiscard]] bool test_runtime_table_resolution_guards()
    {
        namespace llvm_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
        using imported_table_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t;

        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t module{};
        module.imported_table_vec_storage.resize(2uz);
        auto& first{module.imported_table_vec_storage.index_unchecked(0uz)};
        auto& second{module.imported_table_vec_storage.index_unchecked(1uz)};
        first.link_kind = imported_table_t::imported_table_link_kind::imported;
        first.target.imported_ptr = ::std::addressof(second);
        second.link_kind = imported_table_t::imported_table_link_kind::imported;
        second.target.imported_ptr = ::std::addressof(first);
        if(llvm_details::resolve_runtime_table_storage(module, 0u) != nullptr) [[unlikely]]
        {
            ::std::cerr << "cyclic imported-table aliases did not fail closed\n";
            return false;
        }

        ::std::array<::std::uint64_t, 2uz> storage{};
        ::std::uint64_t external{};
        ::std::size_t index{};
        if(llvm_details::classify_runtime_storage_pointer(storage.data(), storage.size(), storage.data() + 1uz, index) !=
               llvm_details::runtime_storage_pointer_membership::element ||
           index != 1uz) [[unlikely]]
        {
            ::std::cerr << "exact runtime-storage element was not recognized\n";
            return false;
        }
        if(llvm_details::classify_runtime_storage_pointer(storage.data(), storage.size(), ::std::addressof(external), index) !=
           llvm_details::runtime_storage_pointer_membership::outside) [[unlikely]]
        {
            ::std::cerr << "cross-module runtime-storage pointer was not classified as external\n";
            return false;
        }

        auto const misaligned_address{reinterpret_cast<::std::uintptr_t>(storage.data()) + 1u};
        auto const misaligned_pointer{reinterpret_cast<::std::uint64_t const*>(misaligned_address)};
        if(llvm_details::classify_runtime_storage_pointer(storage.data(), storage.size(), misaligned_pointer, index) !=
           llvm_details::runtime_storage_pointer_membership::invalid) [[unlikely]]
        {
            ::std::cerr << "misaligned in-range runtime-storage pointer did not fail closed\n";
            return false;
        }
        return true;
    }

    [[maybe_unused]] void test_parser_entry()
    {
        using Wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
        using Wasm1P1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
        using module_storage_t =
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Wasm1, Wasm1P1>;

        [[maybe_unused]] module_storage_t module_storage{};
        [[maybe_unused]] bool ok{
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::validate_all_wasm_code_for_module(
                module_storage, {}, {})};
    }

    [[nodiscard]] ::std::string quote_argument(::std::filesystem::path const& path)
    {
        return ::std::string{"\""} + path.string() + "\"";
    }

    void append_extra_args(::std::string& command, ::std::string_view extra_args)
    {
        if(extra_args.empty()) { return; }
        command.push_back(' ');
        command.append(extra_args);
    }

    [[nodiscard]] bool has_llvm_cache_path_arg(::std::string_view args)
    {
        return args.find("-Rllvm-cache-path") != ::std::string_view::npos ||
               args.find("--runtime-llvm-jit-cache-path") != ::std::string_view::npos;
    }

    void append_default_llvm_cache_disable_arg(::std::string& command, ::std::string_view extra_args)
    {
        if(has_llvm_cache_path_arg(extra_args)) { return; }
        command.append(" -Rllvm-cache-path disable");
    }

    [[nodiscard]] int run_system_command(::std::string const& command)
    {
#ifdef _WIN32
        auto const wrapped{::std::string{"cmd.exe /S /C \""} + command + "\""};
        return ::std::system(wrapped.c_str());
#else
        return ::std::system(command.c_str());
#endif
    }

    [[nodiscard]] bool read_text_file(::std::filesystem::path const& path, ::std::string& text)
    {
        ::std::ifstream input(path);
        if(!input) [[unlikely]]
        {
            ::std::cerr << "failed to open text file: " << path << '\n';
            return false;
        }

        text.assign(::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{});
        if(input.bad()) [[unlikely]]
        {
            ::std::cerr << "failed to read text file: " << path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] ::std::filesystem::path find_uwvm_binary(::std::filesystem::path dir)
    {
        for(;;)
        {
            auto const candidate{dir / "uwvm"};
            if(::std::filesystem::exists(candidate)) [[likely]] { return candidate; }
#ifdef _WIN32
            auto const windows_candidate{dir / "uwvm.exe"};
            if(::std::filesystem::exists(windows_candidate)) [[likely]] { return windows_candidate; }
#endif
            if(dir == dir.root_path()) [[unlikely]] { return {}; }
            dir = dir.parent_path();
        }
    }

    template <::std::size_t N>
    [[nodiscard]] bool write_fixture(::std::filesystem::path const& wasm_path, ::std::array<unsigned char, N> const& wasm_bytes)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(wasm_path.parent_path(), ec);
        if(ec) [[unlikely]]
        {
            ::std::cerr << "failed to create fixture directory: " << wasm_path.parent_path() << '\n';
            return false;
        }

        ::std::ofstream output(wasm_path, ::std::ios::binary | ::std::ios::trunc);
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to open fixture output: " << wasm_path << '\n';
            return false;
        }

        output.write(reinterpret_cast<char const*>(wasm_bytes.data()), static_cast<::std::streamsize>(wasm_bytes.size()));

        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to write fixture output: " << wasm_path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool run_trap_command(::std::string const& command, ::std::filesystem::path const& output_path, char const* label)
    {
        auto const full_command{command + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[llvm_jit] " << full_command << '\n';

        auto const status{run_system_command(full_command)};
        if(status == 0) [[unlikely]]
        {
            ::std::cerr << "uwvm trap command unexpectedly returned success for " << label << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] ::std::vector<::std::size_t> collect_call_stack_func_indices(::std::string_view output)
    {
        ::std::vector<::std::size_t> result{};
        constexpr ::std::string_view prefix{" func_idx="};
        ::std::size_t pos{};

        for(;;)
        {
            pos = output.find(prefix, pos);
            if(pos == ::std::string_view::npos) { return result; }
            pos += prefix.size();

            ::std::size_t value{};
            auto digit_pos{pos};
            while(digit_pos != output.size())
            {
                auto const ch{output[digit_pos]};
                if(ch < '0' || ch > '9') { break; }
                value = value * 10uz + static_cast<::std::size_t>(ch - '0');
                ++digit_pos;
            }

            if(digit_pos != pos) { result.push_back(value); }
            pos = digit_pos;
        }
    }

    [[nodiscard]] ::std::string strip_ansi_codes(::std::string_view text)
    {
        ::std::string out{};
        out.reserve(text.size());

        for(::std::size_t i{}; i != text.size();)
        {
            if(text[i] == '\x1b' && i + 1uz < text.size() && text[i + 1uz] == '[')
            {
                i += 2uz;
                while(i != text.size())
                {
                    auto const ch{text[i++]};
                    if(ch >= '@' && ch <= '~') { break; }
                }
                continue;
            }

            out.push_back(text[i++]);
        }

        return out;
    }

    [[nodiscard]] bool check_native_unwind_call_stack_trap_output(::std::filesystem::path const& output_path)
    {
        ::std::string output{};
        if(!read_text_file(output_path, output)) [[unlikely]] { return false; }

        auto const plain_output{strip_ansi_codes(output)};
        auto const has_call_stack_header{plain_output.find("Call stack:") != ::std::string::npos};
        auto const has_module{plain_output.find(" module=") != ::std::string::npos};
        auto const function_indices{collect_call_stack_func_indices(plain_output)};
        ::std::vector<::std::size_t> const expected_function_indices{0uz, 1uz, 2uz, 3uz};
        if(has_call_stack_header && has_module && function_indices == expected_function_indices) [[likely]] { return true; }

        ::std::cerr << "expected concrete LLVM AOT unwind frame chain 0 -> 1 -> 2 -> 3, got:";
        for(auto const function_index: function_indices) { ::std::cerr << ' ' << function_index; }
        ::std::cerr << "\n" << output << '\n';
        return false;
    }

    [[nodiscard]] bool probe_default_call_stack_unwind(::std::filesystem::path const& uwvm_path,
                                                       ::std::filesystem::path const& executable_dir,
                                                       bool& default_uses_unwind)
    {
        auto const artifact_dir{executable_dir / "test-artifacts" / "0014.llvm_jit"};
        auto const wasm_path{artifact_dir / "default_call_stack_probe.wasm"};
        auto const log_path{artifact_dir / "default_call_stack_probe.log"};
        if(!write_fixture(wasm_path, select_start_wasm)) [[unlikely]] { return false; }

        auto const command{quote_argument(uwvm_path) + " -Raot -Rllvm-cache-path disable -Rclog file " + quote_argument(log_path) + " --run " +
                           quote_argument(wasm_path)};
        ::std::cout << "[llvm_jit] " << command << '\n';

        auto const status{run_system_command(command)};
        if(status != 0) [[unlikely]]
        {
            ::std::cerr << "uwvm returned non-zero status while probing default LLVM JIT call-stack policy: " << status << '\n';
            return false;
        }

        ::std::string log{};
        if(!read_text_file(log_path, log)) [[unlikely]] { return false; }

        if(::uwvm2test::native_unwind::matches_policy(log, "unwind"))
        {
            default_uses_unwind = true;
            return true;
        }

        if(::uwvm2test::native_unwind::matches_policy(log, "instruction"))
        {
            default_uses_unwind = false;
            return true;
        }

        ::std::cerr << "unable to determine default LLVM JIT call-stack policy from log:\n" << log << '\n';
        return false;
    }

    [[nodiscard]] bool run_native_unwind_trap_fixture(::std::filesystem::path const& uwvm_path, ::std::filesystem::path const& executable_dir)
    {
        auto const artifact_dir{executable_dir / "test-artifacts" / "0014.llvm_jit"};
        auto const wasm_path{artifact_dir / "native_unwind_trap.wasm"};
        if(!write_fixture(wasm_path, native_unwind_trap_wasm)) [[unlikely]] { return false; }

        bool default_uses_unwind{};
        if(!probe_default_call_stack_unwind(uwvm_path, executable_dir, default_uses_unwind)) [[unlikely]] { return false; }
        if(!default_uses_unwind)
        {
            ::std::cout << "[llvm_jit] skip explicit unwind trap fixture: default call-stack policy is not unwind\n";
            return true;
        }

        auto const output_path{artifact_dir / "native_unwind_aot.out"};
        auto const log_path{artifact_dir / "native_unwind_aot.log"};
        auto const command{quote_argument(uwvm_path) +
                           " -Raot -Rllvm-policy max -Rllvm-cache-path disable -Rllvm-call-stack unwind -Rclog file " +
                           quote_argument(log_path) + " --run " + quote_argument(wasm_path)};
        if(!run_trap_command(command, output_path, "aot native unwind")) [[unlikely]] { return false; }
        ::std::string log{};
        return read_text_file(log_path, log) && ::uwvm2test::native_unwind::matches_policy(log, "unwind") &&
               check_native_unwind_call_stack_trap_output(output_path);
    }

    [[nodiscard]] bool run_command(::std::string const& command, char const* label)
    {
        ::std::cout << "[llvm_jit] " << command << '\n';

        auto const status{run_system_command(command)};
        if(status == 0) [[likely]] { return true; }

        ::std::cerr << "uwvm returned non-zero status for " << label << ": " << status << '\n';
        return false;
    }

    [[nodiscard]] bool run_aot_shortcut(::std::filesystem::path const& uwvm_path,
                                        ::std::filesystem::path const& wasm_path,
                                        ::std::string_view extra_args = {})
    {
        auto command{quote_argument(uwvm_path) + " -Raot"};
        append_default_llvm_cache_disable_arg(command, extra_args);
        append_extra_args(command, extra_args);
        command += " --run " + quote_argument(wasm_path);
        return run_command(command, "runtime-aot shortcut");
    }

    [[nodiscard]] bool run_aot_compile_failure(::std::filesystem::path const& uwvm_path,
                                               ::std::filesystem::path const& wasm_path,
                                               ::std::string_view expected_reason,
                                               ::std::string_view expected_detail = {},
                                               ::std::string_view extra_args = {})
    {
        auto command{quote_argument(uwvm_path) + " -Raot"};
        append_default_llvm_cache_disable_arg(command, extra_args);
        append_extra_args(command, extra_args);
        command += " --run " + quote_argument(wasm_path);

        auto output_path{wasm_path};
        output_path += ".aot-compile-failure.out";
        if(!run_trap_command(command, output_path, "unsupported native AOT lowering")) [[unlikely]] { return false; }

        ::std::string output{};
        if(!read_text_file(output_path, output)) [[unlikely]] { return false; }
        auto const plain_output{strip_ansi_codes(output)};
        if(plain_output.find("LLVM AOT capability preflight rejected") != ::std::string::npos &&
           plain_output.find(expected_reason) != ::std::string::npos &&
           (expected_detail.empty() || plain_output.find(expected_detail) != ::std::string::npos) &&
           plain_output.find("LLVM AOT materialization failed") == ::std::string::npos) [[likely]]
        {
            return true;
        }

        ::std::cerr << "expected unsupported native AOT lowering to fail during capability preflight with '" << expected_reason << "'";
        if(!expected_detail.empty()) { ::std::cerr << " and '" << expected_detail << "'"; }
        ::std::cerr << ":\n" << output << '\n';
        return false;
    }

    [[nodiscard]] bool run_aot_validation_failure(::std::filesystem::path const& uwvm_path,
                                                  ::std::filesystem::path const& wasm_path,
                                                  ::std::string_view expected_diagnostic,
                                                  ::std::string_view extra_args = {})
    {
        auto command{quote_argument(uwvm_path) + " -Raot"};
        append_default_llvm_cache_disable_arg(command, extra_args);
        append_extra_args(command, extra_args);
        command += " --run " + quote_argument(wasm_path);

        auto output_path{wasm_path};
        output_path += ".aot-validation-failure.out";
        if(!run_trap_command(command, output_path, "invalid Wasm 1.1 immediate")) [[unlikely]] { return false; }

        ::std::string output{};
        if(!read_text_file(output_path, output)) [[unlikely]] { return false; }
        auto const plain_output{strip_ansi_codes(output)};
        if(plain_output.find(expected_diagnostic) != ::std::string::npos) [[likely]] { return true; }

        ::std::cerr << "expected validation diagnostic '" << expected_diagnostic << "':\n" << output << '\n';
        return false;
    }

    [[nodiscard]] ::std::filesystem::path llvm_jit_fixture_path(::std::filesystem::path const& executable_dir,
                                                                 ::std::string_view file_name)
    {
        return executable_dir / "test-artifacts" / "0014.llvm_jit" / ::std::string{file_name};
    }

    template <::std::size_t N>
    [[nodiscard]] bool run_fixture(::std::filesystem::path const& uwvm_path,
                                   ::std::filesystem::path const& executable_dir,
                                   ::std::string_view file_name,
                                   ::std::array<unsigned char, N> const& wasm_bytes,
                                   ::std::string_view extra_args = {})
    {
        auto const wasm_path{llvm_jit_fixture_path(executable_dir, file_name)};
        if(!write_fixture(wasm_path, wasm_bytes)) [[unlikely]] { return false; }

        if(!run_aot_shortcut(uwvm_path, wasm_path, extra_args)) [[unlikely]] { return false; }
        return true;
    }

    template <::std::size_t N>
    [[nodiscard]] bool run_unsupported_aot_fixture(::std::filesystem::path const& uwvm_path,
                                                   ::std::filesystem::path const& executable_dir,
                                                   ::std::string_view file_name,
                                                   ::std::array<unsigned char, N> const& wasm_bytes,
                                                   ::std::string_view expected_reason,
                                                   ::std::string_view expected_detail = {},
                                                   ::std::string_view extra_args = {})
    {
        auto const wasm_path{llvm_jit_fixture_path(executable_dir, file_name)};
        if(!write_fixture(wasm_path, wasm_bytes)) [[unlikely]] { return false; }
        return run_aot_compile_failure(uwvm_path, wasm_path, expected_reason, expected_detail, extra_args);
    }

    template <::std::size_t N>
    [[nodiscard]] bool run_invalid_aot_fixture(::std::filesystem::path const& uwvm_path,
                                               ::std::filesystem::path const& executable_dir,
                                               ::std::string_view file_name,
                                               ::std::array<unsigned char, N> const& wasm_bytes,
                                               ::std::string_view expected_diagnostic,
                                               ::std::string_view extra_args = {})
    {
        auto const wasm_path{llvm_jit_fixture_path(executable_dir, file_name)};
        if(!write_fixture(wasm_path, wasm_bytes)) [[unlikely]] { return false; }
        return run_aot_validation_failure(uwvm_path, wasm_path, expected_diagnostic, extra_args);
    }

    [[nodiscard]] bool run_wasi_alignment_trap_command(::std::filesystem::path const& uwvm_path,
                                                       ::std::filesystem::path const& wasm_path,
                                                       ::std::string_view runtime_args,
                                                       ::std::string_view output_suffix,
                                                       char const* label)
    {
        auto command{quote_argument(uwvm_path)};
        append_extra_args(command, runtime_args);
        command += " --wasm-feature-mvp --run " + quote_argument(wasm_path);

        auto output_path{wasm_path};
        output_path += output_suffix;
        if(!run_trap_command(command, output_path, label)) [[unlikely]] { return false; }

        ::std::string output{};
        if(!read_text_file(output_path, output)) [[unlikely]] { return false; }
        auto const plain_output{strip_ansi_codes(output)};
        if(plain_output.find("WASI Preview 1 guest pointer alignment trap") != ::std::string::npos &&
           plain_output.find("fd_write.iovs") != ::std::string::npos &&
           plain_output.find("required alignment = 4 bytes") != ::std::string::npos) [[likely]]
        {
            return true;
        }

        ::std::cerr << "expected a precise WASI guest-pointer alignment diagnostic for " << label << ":\n" << output << '\n';
        return false;
    }

    [[nodiscard]] bool run_wasi_alignment_trap_fixture(::std::filesystem::path const& uwvm_path,
                                                       ::std::filesystem::path const& executable_dir)
    {
        auto const wasm_path{llvm_jit_fixture_path(executable_dir, "wasi_misaligned_pointer_start.wasm")};
        if(!write_fixture(wasm_path, wasi_misaligned_pointer_start_wasm)) [[unlikely]] { return false; }

#ifndef UWVM_DISABLE_INT
        if(!run_wasi_alignment_trap_command(uwvm_path, wasm_path, "-Rint", ".int-alignment-trap.out", "uwvm-int WASI alignment")) [[unlikely]]
        {
            return false;
        }
#endif
        return run_wasi_alignment_trap_command(uwvm_path,
                                               wasm_path,
                                               "-Raot -Rllvm-cache-path disable",
                                               ".aot-alignment-trap.out",
                                               "LLVM AOT WASI alignment");
    }

}  // namespace

int main(int argc, char** argv)
{
    if(!test_runtime_table_resolution_guards()) [[unlikely]] { return 1; }
    if(argc <= 0 || argv == nullptr || argv[0] == nullptr) [[unlikely]]
    {
        ::std::cerr << "missing argv[0]\n";
        return 1;
    }

    auto const executable{::std::filesystem::absolute(argv[0])};
    auto const executable_dir{executable.parent_path()};
    auto const uwvm_path{find_uwvm_binary(executable_dir)};
    if(uwvm_path.empty()) [[unlikely]]
    {
        ::std::cerr << "failed to locate uwvm next to test executable: " << executable << '\n';
        return 1;
    }

    if(!run_fixture(uwvm_path, executable_dir, "nontrivial_start.wasm", nontrivial_start_wasm)) [[unlikely]] { return 1; }
    if(!run_fixture(uwvm_path, executable_dir, "select_start.wasm", select_start_wasm)) [[unlikely]] { return 1; }
    if(!run_invalid_aot_fixture(uwvm_path,
                                executable_dir,
                                "overlong_i32_blocktype.wasm",
                                overlong_i32_blocktype_wasm,
                                "Illegal block type byte",
                                wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_fixture(uwvm_path, executable_dir, "unaligned_memory_start.wasm", unaligned_memory_start_wasm)) [[unlikely]] { return 1; }
    if(!run_wasi_alignment_trap_fixture(uwvm_path, executable_dir)) [[unlikely]] { return 1; }
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_scalar_start.wasm",
                    wasm1p1_scalar_start_wasm,
                    wasm1p1_scalar_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_scalar_edges_start.wasm",
                    wasm1p1_scalar_edges_start_wasm,
                    wasm1p1_scalar_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_invalid_aot_fixture(uwvm_path,
                                executable_dir,
                                "select_t_empty_result_types.wasm",
                                select_t_empty_result_types_wasm,
                                "Invalid immediate encoding for select.result_types",
                                wasm1p1_scalar_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_bulk_memory_start.wasm",
                                    wasm1p1_bulk_memory_start_wasm,
                                    "memory.init has no LLVM lowering",
                                    "function=0, byte-offset=15, opcode=252, subopcode=8",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_data_drop_start.wasm",
                                    wasm1p1_data_drop_start_wasm,
                                    "data.drop has no LLVM lowering",
                                    "function=0, byte-offset=0, opcode=252, subopcode=9",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_ref_null_start.wasm",
                                    wasm1p1_ref_null_start_wasm,
                                    "ref.null has no LLVM lowering",
                                    "function=0, byte-offset=0, opcode=208",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_export_declared_ref_func_start.wasm",
                                    wasm1p1_export_declared_ref_func_start_wasm,
                                    "ref.func has no LLVM lowering",
                                    "function=1, byte-offset=0, opcode=210",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_table_get_start.wasm",
                                    wasm1p1_table_get_start_wasm,
                                    "table.get has no LLVM lowering",
                                    "function=0, byte-offset=2, opcode=37",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_externref_local_start.wasm",
                                    wasm1p1_externref_local_start_wasm,
                                    "local type is not an LLVM scalar",
                                    "function=0, detail=111",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_table_ref_bulk_start.wasm",
                                    wasm1p1_table_ref_bulk_start_wasm,
                                    "table.size has no LLVM lowering",
                                    "function=2, byte-offset=0, opcode=252, subopcode=16",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_simd_basic_start.wasm",
                                    wasm1p1_simd_basic_start_wasm,
                                    "SIMD instruction has no LLVM lowering",
                                    "function=0, byte-offset=2, opcode=253, subopcode=12",
                                    wasm1p1_all_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_multivalue_start.wasm",
                                    wasm1p1_multivalue_start_wasm,
                                    "function signature has multiple results",
                                    "function=0, detail=2",
                                    wasm1p1_multivalue_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_unsupported_aot_fixture(uwvm_path,
                                    executable_dir,
                                    "wasm1p1_type_index_block_start.wasm",
                                    wasm1p1_type_index_block_start_wasm,
                                    "block uses a type-index/reference/SIMD signature",
                                    "function=0, byte-offset=0, opcode=2, detail=0",
                                    wasm1p1_multivalue_runtime_args)) [[unlikely]]
    {
        return 1;
    }
    if(!run_native_unwind_trap_fixture(uwvm_path, executable_dir)) [[unlikely]] { return 1; }

    return 0;
}
