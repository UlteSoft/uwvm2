#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_conbine_heavy_unary_float_localget_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        auto add_f32_unary = [&](wasm_op unary_op) -> void
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, unary_op);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_unary = [&](wasm_op unary_op) -> void
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, unary_op);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f32 unary localget fusions (only a subset has dedicated localget opfuncs)
        add_f32_unary(wasm_op::f32_abs);   // f0
        add_f32_unary(wasm_op::f32_neg);   // f1
        add_f32_unary(wasm_op::f32_sqrt);  // f2

        // f64 unary localget fusions
        add_f64_unary(wasm_op::f64_ceil);     // f3
        add_f64_unary(wasm_op::f64_floor);    // f4
        add_f64_unary(wasm_op::f64_trunc);    // f5
        add_f64_unary(wasm_op::f64_nearest);  // f6
        add_f64_unary(wasm_op::f64_abs);      // f7
        add_f64_unary(wasm_op::f64_neg);      // f8
        add_f64_unary(wasm_op::f64_sqrt);     // f9

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 10uz);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands,
                                                    optable::translate::get_uwvmint_f32_abs_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands,
                                                    optable::translate::get_uwvmint_f32_neg_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands,
                                                    optable::translate::get_uwvmint_f32_sqrt_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands,
                                                    optable::translate::get_uwvmint_f64_ceil_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands,
                                                    optable::translate::get_uwvmint_f64_floor_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands,
                                                    optable::translate::get_uwvmint_f64_trunc_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands,
                                                    optable::translate::get_uwvmint_f64_nearest_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands,
                                                    optable::translate::get_uwvmint_f64_abs_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands,
                                                    optable::translate::get_uwvmint_f64_neg_localget_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands,
                                                    optable::translate::get_uwvmint_f64_sqrt_localget_fptr_from_tuple<Opt>(curr, tuple)));
        }
#endif

        using Runner = interpreter_runner<Opt>;

        auto run_f32 = [&](::std::size_t fidx, float x) noexcept -> float
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32(x),
                                  nullptr,
                                  nullptr);
            return load_f32(rr.results);
        };

        auto run_f64 = [&](::std::size_t fidx, double x) noexcept -> double
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64(x),
                                  nullptr,
                                  nullptr);
            return load_f64(rr.results);
        };

        // f32 cases
        UWVM2TEST_REQUIRE(run_f32(0, -3.0f) == 3.0f);   // abs
        UWVM2TEST_REQUIRE(run_f32(1, 3.0f) == -3.0f);   // neg
        UWVM2TEST_REQUIRE(run_f32(2, 16.0f) == 4.0f);   // sqrt

        // f64 cases
        UWVM2TEST_REQUIRE(run_f64(3, 1.25) == 2.0);      // ceil
        UWVM2TEST_REQUIRE(run_f64(4, -1.25) == -2.0);    // floor
        UWVM2TEST_REQUIRE(run_f64(5, -1.25) == -1.0);    // trunc
        UWVM2TEST_REQUIRE(run_f64(6, 1.4) == 1.0);       // nearest
        UWVM2TEST_REQUIRE(run_f64(7, -3.0) == 3.0);      // abs
        UWVM2TEST_REQUIRE(run_f64(8, 3.0) == -3.0);      // neg
        UWVM2TEST_REQUIRE(run_f64(9, 9.0) == 3.0);       // sqrt

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_heavy_unary_float_localget() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_heavy_unary_float_localget_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_heavy_unary_float_localget");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode triggers combine state machine.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        // byref smoke
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_heavy_unary_float_localget();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
