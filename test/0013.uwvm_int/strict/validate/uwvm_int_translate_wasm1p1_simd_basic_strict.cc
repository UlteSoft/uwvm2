#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline void simd(byte_vec& c, wasm1p1_simd_op o)
    {
        append_u8(c, u8(wasm1p1_op::simd_prefix));
        append_u32_leb(c, u32(o));
    }

    inline void memarg(byte_vec& c, ::std::uint32_t align, ::std::uint32_t offset)
    {
        append_u32_leb(c, align);
        append_u32_leb(c, offset);
    }

    inline void append_u32_le(byte_vec& c, ::std::uint32_t v)
    {
        append_u8(c, static_cast<::std::uint8_t>(v & 0xffu));
        append_u8(c, static_cast<::std::uint8_t>((v >> 8u) & 0xffu));
        append_u8(c, static_cast<::std::uint8_t>((v >> 16u) & 0xffu));
        append_u8(c, static_cast<::std::uint8_t>((v >> 24u) & 0xffu));
    }

    inline void v128_i32x4(byte_vec& c, ::std::uint32_t a, ::std::uint32_t b, ::std::uint32_t d, ::std::uint32_t e)
    {
        simd(c, wasm1p1_simd_op::v128_const);
        append_u32_le(c, a);
        append_u32_le(c, b);
        append_u32_le(c, d);
        append_u32_le(c, e);
    }

    inline void v128_f32x4(byte_vec& c, float a, float b, float d, float e)
    {
        simd(c, wasm1p1_simd_op::v128_const);
        append_f32_ieee(c, a);
        append_f32_ieee(c, b);
        append_f32_ieee(c, d);
        append_f32_ieee(c, e);
    }

    [[nodiscard]] byte_vec build_wasm1p1_simd_basic_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;

        // i32 lanes: ((1,2,3,4) + (5,6,7,8)) == (6,8,10,12).
        v128_i32x4(c, 1u, 2u, 3u, 4u);
        v128_i32x4(c, 5u, 6u, 7u, 8u);
        simd(c, wasm1p1_simd_op::i32x4_add);
        v128_i32x4(c, 6u, 8u, 10u, 12u);
        simd(c, wasm1p1_simd_op::i32x4_eq);
        simd(c, wasm1p1_simd_op::i32x4_all_true);

        // f32 lanes: ((1.5,2.0,-3.0,4.0) * (2.0,-3.0,-2.0,0.5)) == (3,-6,6,2).
        v128_f32x4(c, 1.5f, 2.0f, -3.0f, 4.0f);
        v128_f32x4(c, 2.0f, -3.0f, -2.0f, 0.5f);
        simd(c, wasm1p1_simd_op::f32x4_mul);
        v128_f32x4(c, 3.0f, -6.0f, 6.0f, 2.0f);
        simd(c, wasm1p1_simd_op::f32x4_eq);
        simd(c, wasm1p1_simd_op::i32x4_all_true);
        op(c, wasm_op::i32_and);

        // splat/extract lane stays in the f32/v128 stack-top class.
        op(c, wasm_op::f32_const);
        append_f32_ieee(c, 2.5f);
        simd(c, wasm1p1_simd_op::f32x4_splat);
        simd(c, wasm1p1_simd_op::f32x4_extract_lane);
        append_u8(c, 3u);
        op(c, wasm_op::f32_const);
        append_f32_ieee(c, 2.5f);
        op(c, wasm_op::f32_eq);
        op(c, wasm_op::i32_and);

        // v128.store/load, bitwise and v128.any_true.
        op(c, wasm_op::i32_const);
        i32(c, 0);
        v128_i32x4(c, 0x12345678u, 0u, 0xFFFFFFFFu, 0x80808080u);
        simd(c, wasm1p1_simd_op::v128_store);
        memarg(c, 4u, 0u);
        op(c, wasm_op::i32_const);
        i32(c, 0);
        simd(c, wasm1p1_simd_op::v128_load);
        memarg(c, 4u, 0u);
        v128_i32x4(c, 0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000000u, 0xFFFFFFFFu);
        simd(c, wasm1p1_simd_op::v128_and);
        simd(c, wasm1p1_simd_op::v128_any_true);
        op(c, wasm_op::i32_and);

        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_and_run(byte_vec const& wasm, wasm_feature_parameter_t const& features) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_wasm1p1_simd_basic", {}, features);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err, ::std::addressof(features));
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                              rt.local_defined_function_vec_storage.index_unchecked(0),
                              pack_no_params(),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr.results) == 1);
        return 0;
    }

    [[nodiscard]] int test_wasm1p1_simd_basic() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto const wasm = build_wasm1p1_simd_basic_module();
        auto const features = make_wasm1p1_feature_parameter();

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_and_run<opt>(wasm, features) == 0);
        }

        if(abi_mode_enabled("tail-sysv-v128"))
        {
            constexpr auto opt{k_test_tail_sysv_v128_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_and_run<opt>(wasm, features) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64-v128"))
        {
            constexpr auto opt{k_test_tail_aapcs64_v128_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_and_run<opt>(wasm, features) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_wasm1p1_simd_basic();
}
