#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_f32x3(float a, float b, float c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f32x4(float a, float b, float c, float d)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        ::std::memcpy(out.data() + 12, ::std::addressof(d), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x3(double a, double b, double c)
    {
        byte_vec out(24);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x4(double a, double b, double c, double d)
    {
        byte_vec out(32);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        ::std::memcpy(out.data() + 24, ::std::addressof(d), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_conbine_float_mul_add_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: f32 a*b + c
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: f32 a*b - c
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::f32_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32 a*b + c*d
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 3u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f32 a*b - c*d
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 3u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: f64 a*b + c
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: f64 a*b - c
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::f64_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: f64 a*b + c*d
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 3u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: f64 a*b - c*d
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 3u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_conbine_float_mul_add_semantics(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        using Runner = interpreter_runner<Opt>;

        // Choose values that are exactly representable to avoid FP noise.
        float const fa = 1.5f;
        float const fb = 2.0f;
        float const fc = 0.25f;
        float const fd = 4.0f;

        double const da = 1.5;
        double const db = 2.0;
        double const dc = 0.25;
        double const dd = 4.0;

        // f0: 1.5*2 + 0.25 = 3.25
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_f32x3(fa, fb, fc),
                                              nullptr,
                                              nullptr)
                                       .results) == 3.25f);

        // f1: 1.5*2 - 0.25 = 2.75
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32x3(fa, fb, fc),
                                              nullptr,
                                              nullptr)
                                       .results) == 2.75f);

        // f2: (1.5*2) + (0.25*4) = 4.0
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_f32x4(fa, fb, fc, fd),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.0f);

        // f3: (1.5*2) - (0.25*4) = 2.0
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f32x4(fa, fb, fc, fd),
                                              nullptr,
                                              nullptr)
                                       .results) == 2.0f);

        // f4: 1.5*2 + 0.25 = 3.25
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f64x3(da, db, dc),
                                              nullptr,
                                              nullptr)
                                       .results) == 3.25);

        // f5: 1.5*2 - 0.25 = 2.75
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f64x3(da, db, dc),
                                              nullptr,
                                              nullptr)
                                       .results) == 2.75);

        // f6: (1.5*2) + (0.25*4) = 4.0
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f64x4(da, db, dc, dd),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.0);

        // f7: (1.5*2) - (0.25*4) = 2.0
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f64x4(da, db, dc, dd),
                                              nullptr,
                                              nullptr)
                                       .results) == 2.0);

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_float_mul_add() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_float_mul_add_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_float_mul_add");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: strict assertions on float mul-add combined opfuncs.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr auto exp_f32_mul_add_3 =
                optable::translate::get_uwvmint_f32_mul_add_3localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f32_mul_sub_3 =
                optable::translate::get_uwvmint_f32_mul_sub_3localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f32_mul_add_2mul_4 =
                optable::translate::get_uwvmint_f32_mul_add_2mul_4localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f32_mul_sub_2mul_4 =
                optable::translate::get_uwvmint_f32_mul_sub_2mul_4localget_fptr_from_tuple<opt>(curr, tuple);

            constexpr auto exp_f64_mul_add_3 =
                optable::translate::get_uwvmint_f64_mul_add_3localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_mul_sub_3 =
                optable::translate::get_uwvmint_f64_mul_sub_3localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_mul_add_2mul_4 =
                optable::translate::get_uwvmint_f64_mul_add_2mul_4localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_mul_sub_2mul_4 =
                optable::translate::get_uwvmint_f64_mul_sub_2mul_4localget_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_f32_mul_add_3));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_f32_mul_sub_3));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_mul_add_2mul_4));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f32_mul_sub_2mul_4));

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_f64_mul_add_3));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_f64_mul_sub_3));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f64_mul_add_2mul_4));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f64_mul_sub_2mul_4));
#endif

            UWVM2TEST_REQUIRE(run_conbine_float_mul_add_semantics<opt>(cm, rt) == 0);
        }

        // Byref mode: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_conbine_float_mul_add_semantics<opt>(cm, rt) == 0);
        }

        // Tailcall + stacktop caching: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_conbine_float_mul_add_semantics<opt>(cm, rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_conbine_float_mul_add();
}

