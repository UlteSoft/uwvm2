#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i64x2(::std::int64_t a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

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

    [[nodiscard]] byte_vec build_delay_local_heavy_update_local_i64_f32_f64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i64 div_u local_set variant: b = a /u b ; return b
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_div_u);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 div_s local_tee variant: tmp = a /s b ; drop ; return tmp
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_div_s);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i64 rem_u local_set variant: b = a %u b ; return b
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_rem_u);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i64 rem_s local_tee variant: tmp = a %s b ; drop ; return tmp
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_rem_s);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: f32 div local_set variant: b = a / b ; return b
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: f32 copysign local_tee variant: tmp = copysign(a, b) ; drop ; return tmp
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_copysign);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: f64 div local_set variant: b = a / b ; return b
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_div);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: f64 copysign local_tee variant: tmp = copysign(a, b) ; drop ; return tmp
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_copysign);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i64_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.i64_stack_top_begin_pos != SIZE_MAX && Opt.i64_stack_top_begin_pos != Opt.i64_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i64_stack_top_begin_pos}; pos < Opt.i64_stack_top_end_pos; ++pos)
            {
                curr.i64_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_f32_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.f32_stack_top_begin_pos}; pos < Opt.f32_stack_top_end_pos; ++pos)
            {
                curr.f32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_f64_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.f64_stack_top_begin_pos}; pos < Opt.f64_stack_top_end_pos; ++pos)
            {
                curr.f64_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 8uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(0).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::div_u>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(0).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::div_u>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(1).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::div_s>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(1).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::div_s>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(2).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::rem_u>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(2).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::rem_u>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(3).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::rem_s>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_i64_variant<Opt>(
                cm.local_funcs.index_unchecked(3).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::rem_s>(curr, tuple);
                }));

            UWVM2TEST_REQUIRE(bytecode_contains_f32_variant<Opt>(
                cm.local_funcs.index_unchecked(4).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::div>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_f32_variant<Opt>(
                cm.local_funcs.index_unchecked(4).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::div>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(bytecode_contains_f32_variant<Opt>(
                cm.local_funcs.index_unchecked(5).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::copysign>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_f32_variant<Opt>(
                cm.local_funcs.index_unchecked(5).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::copysign>(curr, tuple);
                }));

            UWVM2TEST_REQUIRE(bytecode_contains_f64_variant<Opt>(
                cm.local_funcs.index_unchecked(6).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::div>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_f64_variant<Opt>(
                cm.local_funcs.index_unchecked(6).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::div>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(bytecode_contains_f64_variant<Opt>(
                cm.local_funcs.index_unchecked(7).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::copysign>(curr, tuple);
                }));
            UWVM2TEST_REQUIRE(!bytecode_contains_f64_variant<Opt>(
                cm.local_funcs.index_unchecked(7).op.operands,
                [&](auto const& curr) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::copysign>(curr, tuple);
                }));
        }
#endif

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                                                            pack_i64x2(10, 3),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 3ull);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i64x2(-7, 3),
                                              nullptr,
                                              nullptr)
                                       .results) == -2);
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(2),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(2),
                                                                            pack_i64x2(10, 3),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 1ull);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i64x2(-7, 3),
                                              nullptr,
                                              nullptr)
                                       .results) == -1);

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f32x2(8.0f, 2.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.0f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f32x2(3.0f, -1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == -3.0f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f64x2(9.0, 4.5),
                                              nullptr,
                                              nullptr)
                                       .results) == 2.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f64x2(3.0, -1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == -3.0);

        return 0;
    }

    [[nodiscard]] int test_translate_delay_local_heavy_update_local_i64_f32_f64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_delay_local_heavy_update_local_i64_f32_f64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_delay_local_heavy_update_local_i64_f32_f64");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}

int main()
{
    try
    {
        return test_translate_delay_local_heavy_update_local_i64_f32_f64();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
