#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_f32x2(float a, float b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x2(double a, double b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] constexpr ::std::uint32_t f32_bits(float v) noexcept
    {
        return ::std::bit_cast<::std::uint32_t>(v);
    }

    [[nodiscard]] constexpr ::std::uint64_t f64_bits(double v) noexcept
    {
        return ::std::bit_cast<::std::uint64_t>(v);
    }

    [[nodiscard]] byte_vec build_conbine_heavy_float_acc_more_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: acc += ceil(x) (f32)
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_ceil);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: acc += trunc(x) (f32)
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_trunc);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: acc += nearest(x) (f32) (ties-to-even)
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_nearest);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: acc += abs(x) (f32)
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_abs);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: acc += -abs(x) (f32) via copysign(x, -1.0)
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_const);
            f32(c, -1.0f);
            op(c, wasm_op::f32_copysign);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: acc += ceil(x) (f64)
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_ceil);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: acc += trunc(x) (f64)
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_trunc);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: acc += nearest(x) (f64) (ties-to-even)
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_nearest);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: acc += abs(x) (f64)
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_abs);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: acc += -abs(x) (f64) via copysign(x, -1.0)
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_const);
            f64(c, -1.0);
            op(c, wasm_op::f64_copysign);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_conbine_heavy_float_acc_more_semantics(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        using Runner = interpreter_runner<Opt>;

        // f0: 10 + ceil(1.25) = 12
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_f32x2(10.0f, 1.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == 12.0f);

        // f1: 10 + trunc(-1.75) = 9
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32x2(10.0f, -1.75f),
                                              nullptr,
                                              nullptr)
                                       .results) == 9.0f);

        // f2: 10 + nearest(2.5) (ties-to-even) = 12
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_f32x2(10.0f, 2.5f),
                                              nullptr,
                                              nullptr)
                                       .results) == 12.0f);

        // f3: 1 + abs(-3.25) = 4.25
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f32x2(1.0f, -3.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.25f);

        // f4: 1 + (-abs(3.25)) = -2.25
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f32x2(1.0f, 3.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == -2.25f);
        // f4: signbit on -0 + (-abs(0)) = -0
        UWVM2TEST_REQUIRE(f32_bits(load_f32(Runner::run(cm.local_funcs.index_unchecked(4),
                                                       rt.local_defined_function_vec_storage.index_unchecked(4),
                                                       pack_f32x2(-0.0f, 0.0f),
                                                       nullptr,
                                                       nullptr)
                                                .results)) == 0x80000000u);

        // f5: 20 + ceil(1.25) = 22
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f64x2(20.0, 1.25),
                                              nullptr,
                                              nullptr)
                                       .results) == 22.0);

        // f6: 20 + trunc(-1.75) = 19
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f64x2(20.0, -1.75),
                                              nullptr,
                                              nullptr)
                                       .results) == 19.0);

        // f7: 20 + nearest(3.5) (ties-to-even) = 24
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f64x2(20.0, 3.5),
                                              nullptr,
                                              nullptr)
                                       .results) == 24.0);

        // f8: 2 + abs(-5.5) = 7.5
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_f64x2(2.0, -5.5),
                                              nullptr,
                                              nullptr)
                                       .results) == 7.5);

        // f9: 2 + (-abs(5.5)) = -3.5
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_f64x2(2.0, 5.5),
                                              nullptr,
                                              nullptr)
                                       .results) == -3.5);
        // f9: signbit on -0 + (-abs(0)) = -0
        UWVM2TEST_REQUIRE(f64_bits(load_f64(Runner::run(cm.local_funcs.index_unchecked(9),
                                                       rt.local_defined_function_vec_storage.index_unchecked(9),
                                                       pack_f64x2(-0.0, 0.0),
                                                       nullptr,
                                                       nullptr)
                                                .results)) == 0x8000000000000000ull);

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_conbine_heavy_float_acc_more_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto curr{make_entry_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_f32_ceil =
                optable::translate::get_uwvmint_f32_acc_add_ceil_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_trunc =
                optable::translate::get_uwvmint_f32_acc_add_trunc_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_nearest =
                optable::translate::get_uwvmint_f32_acc_add_nearest_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_abs =
                optable::translate::get_uwvmint_f32_acc_add_abs_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_negabs =
                optable::translate::get_uwvmint_f32_acc_add_negabs_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);

            constexpr auto exp_f64_ceil =
                optable::translate::get_uwvmint_f64_acc_add_ceil_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_trunc =
                optable::translate::get_uwvmint_f64_acc_add_trunc_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_nearest =
                optable::translate::get_uwvmint_f64_acc_add_nearest_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_abs =
                optable::translate::get_uwvmint_f64_acc_add_abs_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_negabs =
                optable::translate::get_uwvmint_f64_acc_add_negabs_localget_set_acc_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_f32_ceil));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_f32_trunc));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_nearest));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f32_abs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_f32_negabs));

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_f64_ceil));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f64_trunc));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f64_nearest));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_abs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_f64_negabs));
        }
#endif

        UWVM2TEST_REQUIRE(run_conbine_heavy_float_acc_more_semantics<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_conbine_heavy_float_acc_more() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_heavy_float_acc_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_heavy_float_acc_more");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_conbine_heavy_float_acc_more_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_conbine_heavy_float_acc_more_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_heavy_float_acc_more_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_heavy_float_acc_more_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
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
            UWVM2TEST_REQUIRE(run_conbine_heavy_float_acc_more_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_conbine_heavy_float_acc_more();
}
