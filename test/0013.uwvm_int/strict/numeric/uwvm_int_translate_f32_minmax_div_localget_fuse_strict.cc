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

    [[nodiscard]] byte_vec build_f32_minmax_div_localget_fuse_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };

        // f0: local.get0 ; local.get1 ; f32.min ; end
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_min);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get0 ; local.get1 ; f32.max ; end
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_max);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get0 ; local.get1 ; f32.div ; end
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get0 ; f32.const 5.0 ; f32.min ; end
        // Intended to trigger heavy-combine `local_get_const_f32` -> `f32_min_imm_localget`.
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 5.0f);
            op(c, wasm_op::f32_min);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: local.get0 ; f32.const 5.0 ; f32.max ; end
        // Intended to trigger heavy-combine `local_get_const_f32` -> `f32_max_imm_localget`.
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 5.0f);
            op(c, wasm_op::f32_max);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local.get0 ; nop ; local.get1 ; f32.min ; end
        // Intended to trigger delay-local heavy `local_get(rhs)` -> `f32_binop_localget_rhs(min)`.
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_min);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: local.get0 ; nop ; local.get1 ; f32.max ; end
        // Intended to trigger delay-local heavy `local_get(rhs)` -> `f32_binop_localget_rhs(max)`.
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_max);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto curr{make_initial_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_min_imm_localget = optable::translate::get_uwvmint_f32_min_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_max_imm_localget = optable::translate::get_uwvmint_f32_max_imm_localget_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_min_imm_localget));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_max_imm_localget));

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr auto exp_min_2localget = optable::translate::get_uwvmint_f32_min_2localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_max_2localget = optable::translate::get_uwvmint_f32_max_2localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_div_2localget = optable::translate::get_uwvmint_f32_div_2localget_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_min_2localget));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_max_2localget));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_div_2localget));
# endif
        }
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY)
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos == SIZE_MAX && Opt.i64_stack_top_begin_pos == SIZE_MAX &&
                     Opt.f32_stack_top_begin_pos == SIZE_MAX && Opt.f64_stack_top_begin_pos == SIZE_MAX && Opt.v128_stack_top_begin_pos == SIZE_MAX)
        {
            constexpr auto curr{make_initial_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_min_rhs = optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                Opt,
                optable::numeric_details::float_binop::min>(curr, tuple);
            constexpr auto exp_max_rhs = optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                Opt,
                optable::numeric_details::float_binop::max>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_min_rhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_max_rhs));
        }
#endif

        // f0: min(1.0, 2.0) => 1.0
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_f32x2(1.0f, 2.0f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 1.0f);
        }

        // f1: max(1.0, 2.0) => 2.0
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_f32x2(1.0f, 2.0f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 2.0f);
        }

        // f2: div(8.0, 2.0) => 4.0
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_f32x2(8.0f, 2.0f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 4.0f);
        }

        // f3: min(a, 5.0)
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(3),
                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                   pack_f32(7.0f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr0.results) == 5.0f);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(3),
                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                   pack_f32(3.0f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr1.results) == 3.0f);
        }

        // f4: max(a, 5.0)
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(4),
                                   rt.local_defined_function_vec_storage.index_unchecked(4),
                                   pack_f32(7.0f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr0.results) == 7.0f);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(4),
                                   rt.local_defined_function_vec_storage.index_unchecked(4),
                                   pack_f32(3.0f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr1.results) == 5.0f);
        }

        // f5/f6: delay-local rhs forms (min/max)
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(5),
                                   rt.local_defined_function_vec_storage.index_unchecked(5),
                                   pack_f32x2(3.0f, 5.0f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr0.results) == 3.0f);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(6),
                                   rt.local_defined_function_vec_storage.index_unchecked(6),
                                   pack_f32x2(3.0f, 5.0f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr1.results) == 5.0f);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_f32_minmax_div_localget_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_f32_minmax_div_localget_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_f32_minmax_div_localget_fuse");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_f32_minmax_div_localget_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
