#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

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
        "--runtime-llvm-jit-cache-path disable --wasm-feature-enable-sign-extension --wasm-feature-enable-nontrapping-float-to-int"};

    inline constexpr ::std::string_view wasm1p1_all_runtime_args{
        "--runtime-llvm-jit-cache-path disable --wasm-feature-1p1"};

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

    // WebAssembly 1.1 multi-value fixture.  The helper returns two i32s and
    // `_start` consumes them.  This intentionally exercises the LLVM raw-entry
    // interpreter fallback path because the current typed LLVM ABI is scalar.
    inline constexpr ::std::array<unsigned char, 59uz> wasm1p1_multivalue_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x09u, 0x02u, 0x60u,
        0x00u, 0x02u, 0x7fu, 0x7fu, 0x60u, 0x00u, 0x00u, 0x03u, 0x03u, 0x02u, 0x00u, 0x01u,
        0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x01u,
        0x0au, 0x15u, 0x02u, 0x06u, 0x00u, 0x41u, 0x0au, 0x41u, 0x20u, 0x0bu, 0x0cu, 0x00u,
        0x10u, 0x00u, 0x6au, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    inline constexpr ::std::string_view wasm1p1_multivalue_runtime_args{
        "--runtime-llvm-jit-cache-path disable --wasm-feature-enable-multi-value"};

    // Generated from:
    // (module
    //   (type $t0 (func))
    //   (type $t1 (func (param i32)))
    //   (memory 1)
    //   (func $leaf_trap (type $t1) (param i32)
    //     local.get 0
    //     i32.load
    //     drop)
    //   (func $inline_candidate (type $t1) (param i32) local.get 0 call $leaf_trap)
    //   (func $wrapper (type $t1) (param i32) local.get 0 call $inline_candidate)
    //   (func $_start (type $t0) (export "_start") i32.const -1 call $wrapper))
    //
    // The out-of-bounds load traps at a real Wasm access site. Instruction
    // call-stack mode verifies that the complete logical stack survives full
    // LLVM inlining; explicit unwind mode verifies native frame reporting on
    // the same generated full/lazy JIT code paths.
    inline constexpr ::std::array<unsigned char, 75uz> inline_unwind_trap_wasm{
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

    [[nodiscard]] ::std::array<bool, 4uz> collect_inline_call_stack_func_idx(::std::string_view output) noexcept
    {
        ::std::array<bool, 4uz> seen{};
        constexpr ::std::string_view prefix{" func_idx="};
        ::std::size_t pos{};

        for(;;)
        {
            pos = output.find(prefix, pos);
            if(pos == ::std::string_view::npos) { return seen; }
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

            if(digit_pos != pos && value < seen.size()) { seen[value] = true; }
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

    [[nodiscard]] bool check_inline_call_stack_trap_output(::std::filesystem::path const& output_path, char const* label)
    {
        ::std::string output{};
        if(!read_text_file(output_path, output)) [[unlikely]] { return false; }

        auto const plain_output{strip_ansi_codes(output)};
        auto const has_call_stack_header{plain_output.find("Call stack:") != ::std::string::npos};
        auto const has_module{plain_output.find(" module=") != ::std::string::npos};
        auto const seen{collect_inline_call_stack_func_idx(plain_output)};
        if(has_call_stack_header && has_module && seen[0uz] && seen[1uz] && seen[2uz] && seen[3uz]) [[likely]] { return true; }

        ::std::cerr << "missing expected LLVM JIT inline call-stack frame chain in " << label << " output:\n" << output << '\n';
        return false;
    }

    [[nodiscard]] bool check_unwind_trap_output(::std::filesystem::path const& output_path, char const* label)
    {
        ::std::string output{};
        if(!read_text_file(output_path, output)) [[unlikely]] { return false; }

        auto const plain_output{strip_ansi_codes(output)};
        auto const has_unwind_header{plain_output.find("Call stack:") != ::std::string::npos};
        auto const has_module{plain_output.find(" module=") != ::std::string::npos};
        auto const has_func_idx{plain_output.find(" func_idx=") != ::std::string::npos};
        if(has_unwind_header && has_module && has_func_idx) [[likely]] { return true; }

        ::std::cerr << "missing LLVM JIT unwind frame in " << label << " output:\n" << output << '\n';
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

        if(log.find("call_stack=unwind") != ::std::string::npos)
        {
            default_uses_unwind = true;
            return true;
        }

        if(log.find("call_stack=instruction") != ::std::string::npos || log.find("call_stack=none") != ::std::string::npos)
        {
            default_uses_unwind = false;
            return true;
        }

        ::std::cerr << "unable to determine default LLVM JIT call-stack policy from log:\n" << log << '\n';
        return false;
    }

    [[nodiscard]] bool run_inline_unwind_trap_fixture(::std::filesystem::path const& uwvm_path, ::std::filesystem::path const& executable_dir)
    {
        auto const artifact_dir{executable_dir / "test-artifacts" / "0014.llvm_jit"};
        auto const wasm_path{artifact_dir / "inline_unwind_trap.wasm"};
        if(!write_fixture(wasm_path, inline_unwind_trap_wasm)) [[unlikely]] { return false; }

        auto const run_inline_mode{[&](::std::string_view policy,
                                       ::std::string_view mode_name,
                                       ::std::string_view mode_args,
                                       bool expect_full_inline_chain) -> bool
                                   {
                                       auto const label{::std::string{policy} + "_" + ::std::string{mode_name}};
                                       auto const output_path{artifact_dir / (::std::string{"inline_"} + label + ".out")};
                                       auto const log_path{artifact_dir / (::std::string{"inline_"} + label + ".log")};
                                       auto const command{quote_argument(uwvm_path) + " " + ::std::string{mode_args} +
                                                          " -Rllvm-cache-path disable -Rllvm-call-stack " + ::std::string{policy} + " -Rclog file " +
                                                          quote_argument(log_path) + " --run " + quote_argument(wasm_path)};

                                       if(!run_trap_command(command, output_path, label.c_str())) [[unlikely]] { return false; }
                                       if(expect_full_inline_chain) { return check_inline_call_stack_trap_output(output_path, label.c_str()); }
                                       return check_unwind_trap_output(output_path, label.c_str());
                                   }};

        if(!run_inline_mode("instruction", "full", "-Rcm full -Rcc jit", true)) [[unlikely]] { return false; }
        if(!run_inline_mode("instruction", "aot", "-Raot", true)) [[unlikely]] { return false; }
        if(!run_inline_mode("instruction", "lazy", "-Rjit", true)) [[unlikely]] { return false; }
        if(!run_inline_mode("instruction", "lazy_verification", "-Rcm lazy+verification -Rcc jit", true)) [[unlikely]] { return false; }

        bool default_uses_unwind{};
        if(!probe_default_call_stack_unwind(uwvm_path, executable_dir, default_uses_unwind)) [[unlikely]] { return false; }
        if(!default_uses_unwind)
        {
            ::std::cout << "[llvm_jit] skip explicit unwind trap fixture: default call-stack policy is not unwind\n";
            return true;
        }

        if(!run_inline_mode("unwind", "full", "-Rcm full -Rcc jit", false)) [[unlikely]] { return false; }
        if(!run_inline_mode("unwind", "aot", "-Raot", false)) [[unlikely]] { return false; }
        if(!run_inline_mode("unwind", "lazy", "-Rjit", false)) [[unlikely]] { return false; }
        if(!run_inline_mode("unwind", "lazy_verification", "-Rcm lazy+verification -Rcc jit", false)) [[unlikely]] { return false; }
        if(!run_inline_mode("unwind-uncheck", "full", "-Rcm full -Rcc jit", false)) [[unlikely]] { return false; }
        if(!run_inline_mode("unwind-uncheck", "aot", "-Raot", false)) [[unlikely]] { return false; }
        if(!run_inline_mode("unwind-uncheck", "lazy", "-Rjit", false)) [[unlikely]] { return false; }
        if(!run_inline_mode("unwind-uncheck", "lazy_verification", "-Rcm lazy+verification -Rcc jit", false)) [[unlikely]] { return false; }
        return true;
    }

    [[nodiscard]] bool run_command(::std::string const& command, char const* label)
    {
        ::std::cout << "[llvm_jit] " << command << '\n';

        auto const status{run_system_command(command)};
        if(status == 0) [[likely]] { return true; }

        ::std::cerr << "uwvm returned non-zero status for " << label << ": " << status << '\n';
        return false;
    }

    [[nodiscard]] bool run_full_mode(::std::filesystem::path const& uwvm_path,
                                     ::std::filesystem::path const& wasm_path,
                                     ::std::string_view extra_args = {})
    {
        auto command{quote_argument(uwvm_path) + " -Rcm full -Rcc jit"};
        append_default_llvm_cache_disable_arg(command, extra_args);
        append_extra_args(command, extra_args);
        command += " --run " + quote_argument(wasm_path);
        return run_command(command, "full llvm-jit");
    }

    [[nodiscard]] bool run_full_mode_expect_failure(::std::filesystem::path const& uwvm_path,
                                                    ::std::filesystem::path const& wasm_path,
                                                    char const* label)
    {
        auto const command{quote_argument(uwvm_path) + " -Rcm full -Rcc jit --run " + quote_argument(wasm_path)};
        ::std::cout << "[llvm_jit] " << command << '\n';

        auto const status{run_system_command(command)};
        if(status != 0) [[likely]] { return true; }

        ::std::cerr << "uwvm unexpectedly accepted feature-gated wasm1p1 fixture without flags for " << label << '\n';
        return false;
    }

    [[nodiscard]] bool run_lazy_mode(::std::filesystem::path const& uwvm_path,
                                     ::std::filesystem::path const& wasm_path,
                                     ::std::string_view extra_args = {})
    {
        auto command{quote_argument(uwvm_path) + " -Rjit"};
        append_default_llvm_cache_disable_arg(command, extra_args);
        append_extra_args(command, extra_args);
        command += " --run " + quote_argument(wasm_path);
        return run_command(command, "lazy llvm-jit");
    }

    [[nodiscard]] bool run_lazy_verification_mode(::std::filesystem::path const& uwvm_path,
                                                  ::std::filesystem::path const& wasm_path,
                                                  ::std::string_view extra_args = {})
    {
        auto command{quote_argument(uwvm_path) + " -Rcm lazy+verification -Rcc jit"};
        append_default_llvm_cache_disable_arg(command, extra_args);
        append_extra_args(command, extra_args);
        command += " --run " + quote_argument(wasm_path);
        return run_command(command, "lazy+verification llvm-jit");
    }

    [[nodiscard]] bool run_tiered_mode(::std::filesystem::path const& uwvm_path,
                                       ::std::filesystem::path const& wasm_path,
                                       ::std::string_view tiered_args,
                                       ::std::string_view extra_args = {})
    {
        auto command{quote_argument(uwvm_path) + " " + ::std::string{tiered_args}};
        append_default_llvm_cache_disable_arg(command, extra_args);
        append_extra_args(command, extra_args);
        command += " --run " + quote_argument(wasm_path);
        return run_command(command, "tiered llvm-jit");
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
                                   ::std::string_view extra_args = {},
                                   bool expect_without_extra_args_to_fail = false)
    {
        auto const wasm_path{llvm_jit_fixture_path(executable_dir, file_name)};
        if(!write_fixture(wasm_path, wasm_bytes)) [[unlikely]] { return false; }

        auto const label{::std::string{file_name}};
        if(expect_without_extra_args_to_fail && !run_full_mode_expect_failure(uwvm_path, wasm_path, label.c_str())) [[unlikely]] { return false; }

        if(!run_full_mode(uwvm_path, wasm_path, extra_args)) [[unlikely]] { return false; }
        if(!run_aot_shortcut(uwvm_path, wasm_path, extra_args)) [[unlikely]] { return false; }
        if(!run_lazy_mode(uwvm_path, wasm_path, extra_args)) [[unlikely]] { return false; }
        if(!run_lazy_verification_mode(uwvm_path, wasm_path, extra_args)) [[unlikely]] { return false; }
        return true;
    }

    [[nodiscard]] bool run_wasm1p1_tiered_matrix(::std::filesystem::path const& uwvm_path,
                                                 ::std::filesystem::path const& wasm_path,
                                                 ::std::string_view extra_args)
    {
        for(::std::string_view tiered_args: {::std::string_view{"-Rtiered"},
                                            ::std::string_view{"-Rtiered -Rtiered-disable-t0"},
                                            ::std::string_view{"-Rtiered -Rtiered-disable-t2"},
                                            ::std::string_view{"-Rtiered -Rtiered-disable-t0 -Rtiered-disable-t2"}})
        {
            if(!run_tiered_mode(uwvm_path, wasm_path, tiered_args, extra_args)) [[unlikely]] { return false; }
        }

        for(::std::string_view policy: {::std::string_view{"debug"},
                                        ::std::string_view{"default"},
                                        ::std::string_view{"fast-compile"},
                                        ::std::string_view{"balanced"},
                                        ::std::string_view{"max"}})
        {
            auto policy_args{::std::string{extra_args} + " --runtime-llvm-jit-policy " + ::std::string{policy}};
            if(!run_tiered_mode(uwvm_path, wasm_path, "-Rtiered", policy_args)) [[unlikely]] { return false; }
        }

        return true;
    }

    [[nodiscard]] bool run_wasm1p1_policy_matrix(::std::filesystem::path const& uwvm_path, ::std::filesystem::path const& executable_dir)
    {
        auto const wasm_path{llvm_jit_fixture_path(executable_dir, "wasm1p1_scalar_start.wasm")};

        for(::std::string_view policy: {::std::string_view{"debug"},
                                        ::std::string_view{"pb-o1"},
                                        ::std::string_view{"pb-o2"},
                                        ::std::string_view{"pb-o3"}})
        {
            auto extra_args{::std::string{wasm1p1_scalar_runtime_args} + " --runtime-llvm-jit-full-policy " + ::std::string{policy}};
            if(!run_full_mode(uwvm_path, wasm_path, extra_args)) [[unlikely]] { return false; }
        }

        for(::std::string_view policy: {::std::string_view{"debug"}, ::std::string_view{"light"}, ::std::string_view{"balanced"}})
        {
            auto extra_args{::std::string{wasm1p1_scalar_runtime_args} + " --runtime-llvm-jit-lazy-policy " + ::std::string{policy}};
            if(!run_lazy_mode(uwvm_path, wasm_path, extra_args)) [[unlikely]] { return false; }
        }

        return run_wasm1p1_tiered_matrix(uwvm_path, wasm_path, wasm1p1_scalar_runtime_args);
    }

}  // namespace

int main(int argc, char** argv)
{
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
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_scalar_start.wasm",
                    wasm1p1_scalar_start_wasm,
                    wasm1p1_scalar_runtime_args,
                    true)) [[unlikely]]
    {
        return 1;
    }
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_scalar_edges_start.wasm",
                    wasm1p1_scalar_edges_start_wasm,
                    wasm1p1_scalar_runtime_args,
                    true)) [[unlikely]]
    {
        return 1;
    }
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_bulk_memory_start.wasm",
                    wasm1p1_bulk_memory_start_wasm,
                    wasm1p1_all_runtime_args,
                    true)) [[unlikely]]
    {
        return 1;
    }
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_table_ref_bulk_start.wasm",
                    wasm1p1_table_ref_bulk_start_wasm,
                    wasm1p1_all_runtime_args,
                    true)) [[unlikely]]
    {
        return 1;
    }
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_simd_basic_start.wasm",
                    wasm1p1_simd_basic_start_wasm,
                    wasm1p1_all_runtime_args,
                    true)) [[unlikely]]
    {
        return 1;
    }
    if(!run_fixture(uwvm_path,
                    executable_dir,
                    "wasm1p1_multivalue_start.wasm",
                    wasm1p1_multivalue_start_wasm,
                    wasm1p1_multivalue_runtime_args,
                    true)) [[unlikely]]
    {
        return 1;
    }
    if(!run_wasm1p1_policy_matrix(uwvm_path, executable_dir)) [[unlikely]] { return 1; }
    for(auto const file_name: {::std::string_view{"wasm1p1_scalar_edges_start.wasm"},
                               ::std::string_view{"wasm1p1_bulk_memory_start.wasm"},
                               ::std::string_view{"wasm1p1_table_ref_bulk_start.wasm"},
                               ::std::string_view{"wasm1p1_simd_basic_start.wasm"}})
    {
        if(!run_wasm1p1_tiered_matrix(uwvm_path,
                                      llvm_jit_fixture_path(executable_dir, file_name),
                                      wasm1p1_all_runtime_args)) [[unlikely]]
        {
            return 1;
        }
    }
    if(!run_inline_unwind_trap_fixture(uwvm_path, executable_dir)) [[unlikely]] { return 1; }

    return 0;
}
