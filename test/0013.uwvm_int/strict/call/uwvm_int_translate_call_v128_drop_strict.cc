#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr ::std::uint8_t k_val_v128{0x7bu};

    [[nodiscard]] byte_vec build_call_v128_drop_module()
    {
        module_builder mb{};

        // 0: () -> v128
        {
            func_type ty{{}, {k_val_v128}};
            func_body fb{};
            append_u8(fb.code, u8(wasm1p1_op::simd_prefix));
            append_u32_leb(fb.code, u32(wasm1p1_simd_op::v128_const));
            for(unsigned i{}; i != 16u; ++i) { append_u8(fb.code, 0u); }
            append_u8(fb.code, u8(wasm_op::end));
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 1: () -> (), with a non-scalar call result consumed by a real drop instruction.
        {
            func_type ty{{}, {}};
            func_body fb{};
            append_u8(fb.code, u8(wasm_op::call));
            append_u32_leb(fb.code, 0u);
            append_u8(fb.code, u8(wasm_op::drop));
            append_u8(fb.code, u8(wasm_op::end));
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_call_v128_drop() noexcept
    {
        install_unexpected_traps();
        auto const features{make_wasm1p1_feature_parameter()};
        auto const wasm{build_call_v128_drop_module()};
        auto prep{prepare_runtime_from_wasm(wasm, u8"uwvm2test_call_v128_drop", {}, features)};
        UWVM2TEST_REQUIRE(prep.mod != nullptr);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto compiled{compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(
            *prep.mod, cop, err, ::std::addressof(features))};
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(compiled.local_funcs.size() == 2uz);
        return 0;
    }
}

int main()
{
    return test_call_v128_drop();
}
