#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i32_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_initial_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos < Opt.i32_stack_top_end_pos; ++pos)
            {
                curr.i32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i64_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_initial_stacktop_currpos<Opt>()};
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
        auto curr{make_initial_stacktop_currpos<Opt>()};
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
        auto curr{make_initial_stacktop_currpos<Opt>()};
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

    [[nodiscard]] byte_vec build_conbine_imm_localget_more_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: x then const, to hit i32_sub_imm_localget
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: x then const, to hit i32_mul_imm_localget
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 6);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: x then const, to hit i32_and_imm_localget
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0x0ff00ff0);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: x then const, to hit i64_and_imm_localget
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x00ff00ff00ff00ffull));
            op(c, wasm_op::i64_and);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: 8.0f / x
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f32_const); f32(c, 8.0f);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: 9.0 / x
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f64_const); f64(c, 9.0);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_div);
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
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 6uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
            cm.local_funcs.index_unchecked(0).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_i32_sub_imm_localget_fptr_from_tuple<Opt>(curr_variant, tuple); }));

        UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
            cm.local_funcs.index_unchecked(1).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_i32_mul_imm_localget_fptr_from_tuple<Opt>(curr_variant, tuple); }));

        UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
            cm.local_funcs.index_unchecked(2).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_i32_and_imm_localget_fptr_from_tuple<Opt>(curr_variant, tuple); }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(3).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_i64_and_imm_localget_fptr_from_tuple<Opt>(curr_variant, tuple); }));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(bytecode_contains_f32_variant<Opt>(
            cm.local_funcs.index_unchecked(4).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_f32_div_from_imm_localget_fptr_from_tuple<Opt>(curr_variant, tuple); }));

        UWVM2TEST_REQUIRE(bytecode_contains_f64_variant<Opt>(
            cm.local_funcs.index_unchecked(5).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_f64_div_from_imm_localget_fptr_from_tuple<Opt>(curr_variant, tuple); }));
#endif

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(4),
                                              nullptr,
                                              nullptr)
                                       .results) == (4 - 9));

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(-7),
                                              nullptr,
                                              nullptr)
                                       .results) == -42);

        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(2),
                                                                            pack_i32(static_cast<::std::int32_t>(0x12345678u)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (0x12345678u & 0x0ff00ff0u));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(3),
                                                                            pack_i64(static_cast<::std::int64_t>(0x123456789abcdef0ull)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (0x123456789abcdef0ull & 0x00ff00ff00ff00ffull));

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f32(2.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.0f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f64(2.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.5);

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_imm_localget_more() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_imm_localget_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_imm_localget_more");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
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
        return test_translate_conbine_imm_localget_more();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
