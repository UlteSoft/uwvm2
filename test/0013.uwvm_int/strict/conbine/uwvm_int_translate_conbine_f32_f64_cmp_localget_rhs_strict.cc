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

    [[nodiscard]] byte_vec build_f32_f64_cmp_localget_rhs_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        auto add_f32_cmp = [&](wasm_op cmp) -> void
        {
            // (param f32 a, f32 b) -> i32
            // local.get0 ; nop ; local.get1 ; <cmp> ; end
            // `nop` blocks local_get2 combine so heavy-conbine can fuse rhs local.get into the opfunc.
            func_type ty{{k_val_f32, k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, cmp);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_cmp = [&](wasm_op cmp) -> void
        {
            // (param f64 a, f64 b) -> i32
            // local.get0 ; nop ; local.get1 ; <cmp> ; end
            func_type ty{{k_val_f64, k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, cmp);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0..f5 (f32)
        add_f32_cmp(wasm_op::f32_eq);
        add_f32_cmp(wasm_op::f32_ne);
        add_f32_cmp(wasm_op::f32_lt);
        add_f32_cmp(wasm_op::f32_gt);
        add_f32_cmp(wasm_op::f32_le);
        add_f32_cmp(wasm_op::f32_ge);

        // f6..f11 (f64)
        add_f64_cmp(wasm_op::f64_eq);
        add_f64_cmp(wasm_op::f64_ne);
        add_f64_cmp(wasm_op::f64_lt);
        add_f64_cmp(wasm_op::f64_gt);
        add_f64_cmp(wasm_op::f64_le);
        add_f64_cmp(wasm_op::f64_ge);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 12uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            // f32
            constexpr auto exp_f32_eq = optable::translate::get_uwvmint_f32_eq_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_ne = optable::translate::get_uwvmint_f32_ne_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_lt = optable::translate::get_uwvmint_f32_lt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_gt = optable::translate::get_uwvmint_f32_gt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_le = optable::translate::get_uwvmint_f32_le_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_ge = optable::translate::get_uwvmint_f32_ge_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_f32_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_f32_ne));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_lt));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f32_gt));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_f32_le));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_f32_ge));

            // f64
            constexpr auto exp_f64_eq = optable::translate::get_uwvmint_f64_eq_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_ne = optable::translate::get_uwvmint_f64_ne_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_lt = optable::translate::get_uwvmint_f64_lt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_gt = optable::translate::get_uwvmint_f64_gt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_le = optable::translate::get_uwvmint_f64_le_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_ge = optable::translate::get_uwvmint_f64_ge_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f64_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f64_ne));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_lt));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_f64_gt));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_f64_le));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_f64_ge));
        }
#endif

        auto run_f32 = [&](::std::size_t fidx, float a, float b) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        auto run_f64 = [&](::std::size_t fidx, double a, double b) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        // f32 eq/ne/lt/gt/le/ge
        UWVM2TEST_REQUIRE(run_f32(0, 1.0f, 1.0f) == 1);
        UWVM2TEST_REQUIRE(run_f32(0, 1.0f, 2.0f) == 0);

        UWVM2TEST_REQUIRE(run_f32(1, 1.0f, 2.0f) == 1);
        UWVM2TEST_REQUIRE(run_f32(1, 2.0f, 2.0f) == 0);

        UWVM2TEST_REQUIRE(run_f32(2, 1.0f, 2.0f) == 1);
        UWVM2TEST_REQUIRE(run_f32(2, 2.0f, 1.0f) == 0);

        UWVM2TEST_REQUIRE(run_f32(3, 2.0f, 1.0f) == 1);
        UWVM2TEST_REQUIRE(run_f32(3, 1.0f, 2.0f) == 0);

        UWVM2TEST_REQUIRE(run_f32(4, 1.0f, 1.0f) == 1);
        UWVM2TEST_REQUIRE(run_f32(4, 2.0f, 1.0f) == 0);

        UWVM2TEST_REQUIRE(run_f32(5, 2.0f, 1.0f) == 1);
        UWVM2TEST_REQUIRE(run_f32(5, 1.0f, 2.0f) == 0);

        // f64 eq/ne/lt/gt/le/ge
        UWVM2TEST_REQUIRE(run_f64(6, 1.0, 1.0) == 1);
        UWVM2TEST_REQUIRE(run_f64(6, 1.0, 2.0) == 0);

        UWVM2TEST_REQUIRE(run_f64(7, 1.0, 2.0) == 1);
        UWVM2TEST_REQUIRE(run_f64(7, 2.0, 2.0) == 0);

        UWVM2TEST_REQUIRE(run_f64(8, 1.0, 2.0) == 1);
        UWVM2TEST_REQUIRE(run_f64(8, 2.0, 1.0) == 0);

        UWVM2TEST_REQUIRE(run_f64(9, 2.0, 1.0) == 1);
        UWVM2TEST_REQUIRE(run_f64(9, 1.0, 2.0) == 0);

        UWVM2TEST_REQUIRE(run_f64(10, 1.0, 1.0) == 1);
        UWVM2TEST_REQUIRE(run_f64(10, 2.0, 1.0) == 0);

        UWVM2TEST_REQUIRE(run_f64(11, 2.0, 1.0) == 1);
        UWVM2TEST_REQUIRE(run_f64(11, 1.0, 2.0) == 0);

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_f32_f64_cmp_localget_rhs() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_f32_f64_cmp_localget_rhs_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_f32_f64_cmp_localget_rhs");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // tailcall: strict fptr assertions when enabled + semantics.
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
        return test_translate_conbine_f32_f64_cmp_localget_rhs();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

