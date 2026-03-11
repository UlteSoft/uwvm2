#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

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

    [[nodiscard]] byte_vec build_br_if_float_cmp_generic_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_f32_brif = [&](wasm_op cmp)
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, cmp);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_brif = [&](wasm_op cmp)
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, cmp);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_f32_brif(wasm_op::f32_lt);  // f0
        add_f32_brif(wasm_op::f32_gt);  // f1
        add_f32_brif(wasm_op::f32_le);  // f2
        add_f32_brif(wasm_op::f32_ge);  // f3
        add_f64_brif(wasm_op::f64_lt);  // f4
        add_f64_brif(wasm_op::f64_gt);  // f5
        add_f64_brif(wasm_op::f64_le);  // f6
        add_f64_brif(wasm_op::f64_ge);  // f7

        return mb.build();
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

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        UWVM2TEST_REQUIRE(bytecode_contains_f32_variant<Opt>(
            cm.local_funcs.index_unchecked(0).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f32_lt_fptr_from_tuple<Opt>(curr_variant, tuple); }));
        UWVM2TEST_REQUIRE(bytecode_contains_f32_variant<Opt>(
            cm.local_funcs.index_unchecked(1).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f32_gt_fptr_from_tuple<Opt>(curr_variant, tuple); }));
        UWVM2TEST_REQUIRE(bytecode_contains_f32_variant<Opt>(
            cm.local_funcs.index_unchecked(2).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f32_le_fptr_from_tuple<Opt>(curr_variant, tuple); }));
        UWVM2TEST_REQUIRE(bytecode_contains_f32_variant<Opt>(
            cm.local_funcs.index_unchecked(3).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f32_ge_fptr_from_tuple<Opt>(curr_variant, tuple); }));

        UWVM2TEST_REQUIRE(bytecode_contains_f64_variant<Opt>(
            cm.local_funcs.index_unchecked(4).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f64_lt_fptr_from_tuple<Opt>(curr_variant, tuple); }));
        UWVM2TEST_REQUIRE(bytecode_contains_f64_variant<Opt>(
            cm.local_funcs.index_unchecked(5).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f64_gt_fptr_from_tuple<Opt>(curr_variant, tuple); }));
        UWVM2TEST_REQUIRE(bytecode_contains_f64_variant<Opt>(
            cm.local_funcs.index_unchecked(6).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f64_le_fptr_from_tuple<Opt>(curr_variant, tuple); }));
        UWVM2TEST_REQUIRE(bytecode_contains_f64_variant<Opt>(
            cm.local_funcs.index_unchecked(7).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            { return optable::translate::get_uwvmint_br_if_f64_ge_fptr_from_tuple<Opt>(curr_variant, tuple); }));
#endif

        auto run_f32 = [&](::std::size_t idx, float x) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(idx),
                                  rt.local_defined_function_vec_storage.index_unchecked(idx),
                                  pack_f32(x),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        auto run_f64 = [&](::std::size_t idx, double x) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(idx),
                                  rt.local_defined_function_vec_storage.index_unchecked(idx),
                                  pack_f64(x),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        for(float x : {-1.0f, 0.0f, 1.0f})
        {
            UWVM2TEST_REQUIRE(run_f32(0uz, x) == (x < 0.0f ? 111 : 222));
            UWVM2TEST_REQUIRE(run_f32(1uz, x) == (x > 0.0f ? 111 : 222));
            UWVM2TEST_REQUIRE(run_f32(2uz, x) == (x <= 0.0f ? 111 : 222));
            UWVM2TEST_REQUIRE(run_f32(3uz, x) == (x >= 0.0f ? 111 : 222));
        }

        for(double x : {-1.0, 0.0, 1.0})
        {
            UWVM2TEST_REQUIRE(run_f64(4uz, x) == (x < 0.0 ? 111 : 222));
            UWVM2TEST_REQUIRE(run_f64(5uz, x) == (x > 0.0 ? 111 : 222));
            UWVM2TEST_REQUIRE(run_f64(6uz, x) == (x <= 0.0 ? 111 : 222));
            UWVM2TEST_REQUIRE(run_f64(7uz, x) == (x >= 0.0 ? 111 : 222));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_float_cmp_generic() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_float_cmp_generic_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_float_cmp_generic");
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
        return test_translate_br_if_float_cmp_generic();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
