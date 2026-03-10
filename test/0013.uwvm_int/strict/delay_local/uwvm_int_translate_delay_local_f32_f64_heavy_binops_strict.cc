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

    [[nodiscard]] byte_vec build_delay_local_f32_f64_heavy_binops_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        auto add_f32_binop = [&](wasm_op binop) -> void
        {
            // (param f32 a, f32 b) -> f32
            // local.get0 ; nop ; local.get1 ; <binop> ; end
            // `nop` blocks local_get2 combine so delay-local heavy can fuse rhs local.get into the opfunc.
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, binop);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_binop = [&](wasm_op binop) -> void
        {
            // (param f64 a, f64 b) -> f64
            // local.get0 ; nop ; local.get1 ; <binop> ; end
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, binop);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0..f2 (f32): cover delay-local heavy allow-list: sub/div/copysign.
        add_f32_binop(wasm_op::f32_sub);
        add_f32_binop(wasm_op::f32_div);
        add_f32_binop(wasm_op::f32_copysign);

        // f3..f5 (f64): cover delay-local heavy allow-list: sub/div/copysign.
        add_f64_binop(wasm_op::f64_sub);
        add_f64_binop(wasm_op::f64_div);
        add_f64_binop(wasm_op::f64_copysign);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 6uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY)
        if constexpr(Opt.is_tail_call)
        {
            auto const curr{make_entry_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
            auto const contains_f32_variant{
                [&](auto const& bc, auto make_fptr) constexpr noexcept
                {
                    if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

                    if constexpr(Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos)
                    {
                        auto curr_variant{curr};
                        for(::std::size_t pos{Opt.f32_stack_top_begin_pos}; pos < Opt.f32_stack_top_end_pos; ++pos)
                        {
                            curr_variant.f32_stack_top_curr_pos = pos;
                            if(bytecode_contains_fptr(bc, make_fptr(curr_variant))) { return true; }
                        }
                    }

                    return false;
                }};
            auto const contains_f64_variant{
                [&](auto const& bc, auto make_fptr) constexpr noexcept
                {
                    if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

                    if constexpr(Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos)
                    {
                        auto curr_variant{curr};
                        for(::std::size_t pos{Opt.f64_stack_top_begin_pos}; pos < Opt.f64_stack_top_end_pos; ++pos)
                        {
                            curr_variant.f64_stack_top_curr_pos = pos;
                            if(bytecode_contains_fptr(bc, make_fptr(curr_variant))) { return true; }
                        }
                    }

                    return false;
                }};

            UWVM2TEST_REQUIRE(contains_f32_variant(
                cm.local_funcs.index_unchecked(0).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::sub>(curr_variant, tuple);
                }));
            UWVM2TEST_REQUIRE(contains_f32_variant(
                cm.local_funcs.index_unchecked(1).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::div>(curr_variant, tuple);
                }));
            UWVM2TEST_REQUIRE(contains_f32_variant(
                cm.local_funcs.index_unchecked(2).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::copysign>(curr_variant, tuple);
                }));

            UWVM2TEST_REQUIRE(contains_f64_variant(
                cm.local_funcs.index_unchecked(3).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::sub>(curr_variant, tuple);
                }));
            UWVM2TEST_REQUIRE(contains_f64_variant(
                cm.local_funcs.index_unchecked(4).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::div>(curr_variant, tuple);
                }));
            UWVM2TEST_REQUIRE(contains_f64_variant(
                cm.local_funcs.index_unchecked(5).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::copysign>(curr_variant, tuple);
                }));
        }
#endif

        auto run_f32 = [&](::std::size_t fidx, float a, float b) noexcept -> float
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_f32(rr.results);
        };

        auto run_f64 = [&](::std::size_t fidx, double a, double b) noexcept -> double
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_f64(rr.results);
        };

        // f32: sub/div/copysign (+local_set/+local_tee variants)
        UWVM2TEST_REQUIRE(run_f32(0, 5.5f, 2.0f) == 3.5f);
        UWVM2TEST_REQUIRE(run_f32(1, 8.0f, 2.0f) == 4.0f);
        UWVM2TEST_REQUIRE(run_f32(2, 3.0f, -1.0f) == -3.0f);

        // f64: sub/div/copysign
        UWVM2TEST_REQUIRE(run_f64(3, 6.5, 2.25) == 4.25);
        UWVM2TEST_REQUIRE(run_f64(4, 9.0, 3.0) == 3.0);
        UWVM2TEST_REQUIRE(run_f64(5, 3.0, -1.0) == -3.0);

        return 0;
    }

    [[nodiscard]] int test_translate_delay_local_f32_f64_heavy_binops() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_delay_local_f32_f64_heavy_binops_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_delay_local_f32_f64_heavy_binops");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_delay_local_f32_f64_heavy_binops();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
