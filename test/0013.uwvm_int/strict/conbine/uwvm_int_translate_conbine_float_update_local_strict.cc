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

    [[nodiscard]] byte_vec build_float_update_local_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (param f32) (result f32)  x = x + 1.5 ; return x
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.5f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param f32) (result f32)  x = x * 2.0 ; return x
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (param f32) (result f32)  x = x - 0.25 ; return x
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 0.25f);
            op(c, wasm_op::f32_sub);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (param f64) (result f64)  x = x + 1.125 ; return x
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 1.125);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: (param f64) (result f64)  x = x * 3.0 ; return x
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 3.0);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: (param f64) (result f64)  x = x - 0.5 ; return x
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 0.5);
            op(c, wasm_op::f64_sub);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: (param f32 f32) (result f32)  tmp = a + b ; return tmp
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: (param f64 f64) (result f64)  tmp = a + b ; return tmp
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
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
        constexpr auto curr{make_entry_stacktop_currpos<Opt>()};
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const f0 = optable::translate::get_uwvmint_f32_add_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple);
        auto const f1 = optable::translate::get_uwvmint_f32_mul_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple);
        auto const f2 = optable::translate::get_uwvmint_f32_sub_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple);
        auto const f3 = optable::translate::get_uwvmint_f64_add_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple);
        auto const f4 = optable::translate::get_uwvmint_f64_mul_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple);
        auto const f5 = optable::translate::get_uwvmint_f64_sub_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple);
# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        auto const f6 = optable::translate::get_uwvmint_f32_add_2localget_local_set_fptr_from_tuple<Opt>(curr, tuple);
        auto const f7 = optable::translate::get_uwvmint_f64_add_2localget_local_set_fptr_from_tuple<Opt>(curr, tuple);
# else
        auto const f6 = optable::translate::get_uwvmint_f32_add_2localget_local_set_common_fptr_from_tuple<Opt>(curr, tuple);
        auto const f7 = optable::translate::get_uwvmint_f64_add_2localget_local_set_common_fptr_from_tuple<Opt>(curr, tuple);
# endif

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, f0));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, f1));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, f2));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, f3));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, f4));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, f5));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, f6));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, f7));
#endif

        // f32
        {
            float const x = 7.0f;

            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_f32(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr0.results) == (x + 1.5f));

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_f32(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr1.results) == (x * 2.0f));

            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_f32(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr2.results) == (x - 0.25f));
        }

        // f64
        {
            double const x = 9.0;

            auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                   rt.local_defined_function_vec_storage.index_unchecked(3),
                                   pack_f64(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr3.results) == (x + 1.125));

            auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                                   rt.local_defined_function_vec_storage.index_unchecked(4),
                                   pack_f64(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr4.results) == (x * 3.0));

            auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                                   rt.local_defined_function_vec_storage.index_unchecked(5),
                                   pack_f64(x),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr5.results) == (x - 0.5));

            auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6),
                                   rt.local_defined_function_vec_storage.index_unchecked(6),
                                   pack_f32x2(1.25f, 2.5f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr6.results) == 3.75f);

            auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7),
                                   rt.local_defined_function_vec_storage.index_unchecked(7),
                                   pack_f64x2(1.25, 2.75),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr7.results) == 4.0);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_float_update_local() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_float_update_local_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_float_update_local");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_float_update_local();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
