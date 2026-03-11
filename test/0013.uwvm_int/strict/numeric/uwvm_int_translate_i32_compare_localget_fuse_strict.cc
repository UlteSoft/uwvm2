#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_i32_compare_localget_fuse_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        auto add_cmp_func = [&](wasm_op cmp) -> void
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, cmp);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_cmp_brif_func = [&](wasm_op cmp, ::std::int32_t true_v, ::std::int32_t false_v) -> void
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, 0x7f);
            op(c, wasm_op::i32_const);
            i32(c, true_v);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, cmp);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, false_v);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_cmp_func(wasm_op::i32_eq);    // f0
        add_cmp_func(wasm_op::i32_ne);    // f1
        add_cmp_func(wasm_op::i32_lt_s);  // f2
        add_cmp_func(wasm_op::i32_lt_u);  // f3
        add_cmp_func(wasm_op::i32_ge_s);  // f4
        add_cmp_func(wasm_op::i32_ge_u);  // f5
        add_cmp_func(wasm_op::i32_gt_s);  // f6
        add_cmp_func(wasm_op::i32_gt_u);  // f7
        add_cmp_func(wasm_op::i32_le_s);  // f8
        add_cmp_func(wasm_op::i32_le_u);  // f9

        // f10: local.get ; i32.const ; i32.ne ; br_if
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, 0x7f);
            op(c, wasm_op::i32_const);
            i32(c, 11);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_ne);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 22);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: local.get ; i32.const ; i32.lt_u ; br_if
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, 0x7f);
            op(c, wasm_op::i32_const);
            i32(c, 33);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_lt_u);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 44);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f12: local.get ; i32.eqz
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f13: local.get ; i32.eqz ; br_if
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, 0x7f);
            op(c, wasm_op::i32_const);
            i32(c, 55);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 66);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        add_cmp_brif_func(wasm_op::i32_eq, 77, 88);      // f14
        add_cmp_brif_func(wasm_op::i32_lt_s, 99, 111);   // f15
        add_cmp_brif_func(wasm_op::i32_ge_s, 123, 124);  // f16
        add_cmp_brif_func(wasm_op::i32_ge_u, 135, 136);  // f17
        add_cmp_brif_func(wasm_op::i32_gt_s, 147, 148);  // f18
        add_cmp_brif_func(wasm_op::i32_gt_u, 159, 160);  // f19
        add_cmp_brif_func(wasm_op::i32_le_s, 171, 172);  // f20
        add_cmp_brif_func(wasm_op::i32_le_u, 183, 184);  // f21

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

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto curr{make_initial_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_eq = optable::translate::get_uwvmint_i32_eq_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ne = optable::translate::get_uwvmint_i32_ne_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_lt_s = optable::translate::get_uwvmint_i32_lt_s_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_lt_u = optable::translate::get_uwvmint_i32_lt_u_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ge_s = optable::translate::get_uwvmint_i32_ge_s_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ge_u = optable::translate::get_uwvmint_i32_ge_u_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_gt_s = optable::translate::get_uwvmint_i32_gt_s_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_gt_u = optable::translate::get_uwvmint_i32_gt_u_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_le_s = optable::translate::get_uwvmint_i32_le_s_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_le_u = optable::translate::get_uwvmint_i32_le_u_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_eqz = optable::translate::get_uwvmint_i32_eqz_localget_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_eq = optable::translate::get_uwvmint_br_if_i32_eq_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_ne = optable::translate::get_uwvmint_br_if_i32_ne_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_lt_s = optable::translate::get_uwvmint_br_if_i32_lt_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_lt_u = optable::translate::get_uwvmint_br_if_i32_lt_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_ge_s = optable::translate::get_uwvmint_br_if_i32_ge_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_ge_u = optable::translate::get_uwvmint_br_if_i32_ge_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_gt_s = optable::translate::get_uwvmint_br_if_i32_gt_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_gt_u = optable::translate::get_uwvmint_br_if_i32_gt_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_le_s = optable::translate::get_uwvmint_br_if_i32_le_s_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_le_u = optable::translate::get_uwvmint_br_if_i32_le_u_imm_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_brif_eqz = optable::translate::get_uwvmint_br_if_local_eqz_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_ne));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_lt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_lt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_ge_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_ge_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_gt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_gt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_le_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_le_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_brif_ne));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_brif_lt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(12).op.operands, exp_eqz));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(13).op.operands, exp_brif_eqz));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_brif_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(15).op.operands, exp_brif_lt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(16).op.operands, exp_brif_ge_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(17).op.operands, exp_brif_ge_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(18).op.operands, exp_brif_gt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(19).op.operands, exp_brif_gt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(20).op.operands, exp_brif_le_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(21).op.operands, exp_brif_le_u));
        }
#endif

        auto expect_eq = [](::std::int32_t x) constexpr -> ::std::int32_t { return static_cast<::std::int32_t>(x == 7); };
        auto expect_ne = [](::std::int32_t x) constexpr -> ::std::int32_t { return static_cast<::std::int32_t>(x != 7); };
        auto expect_lt_s = [](::std::int32_t x) constexpr -> ::std::int32_t { return static_cast<::std::int32_t>(x < 7); };
        auto expect_lt_u = [](::std::int32_t x) constexpr -> ::std::int32_t
        { return static_cast<::std::int32_t>(static_cast<::std::uint32_t>(x) < 7u); };
        auto expect_gt_s = [](::std::int32_t x) constexpr -> ::std::int32_t { return static_cast<::std::int32_t>(x > 7); };
        auto expect_gt_u = [](::std::int32_t x) constexpr -> ::std::int32_t
        { return static_cast<::std::int32_t>(static_cast<::std::uint32_t>(x) > 7u); };
        auto expect_le_s = [](::std::int32_t x) constexpr -> ::std::int32_t { return static_cast<::std::int32_t>(x <= 7); };
        auto expect_le_u = [](::std::int32_t x) constexpr -> ::std::int32_t
        { return static_cast<::std::int32_t>(static_cast<::std::uint32_t>(x) <= 7u); };
        auto expect_ge_s = [](::std::int32_t x) constexpr -> ::std::int32_t { return static_cast<::std::int32_t>(x >= 7); };
        auto expect_ge_u = [](::std::int32_t x) constexpr -> ::std::int32_t
        { return static_cast<::std::int32_t>(static_cast<::std::uint32_t>(x) >= 7u); };

        for(::std::int32_t x : {0, 6, 7, 8, -1, static_cast<::std::int32_t>(0x80000000u)})
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0), rt.local_defined_function_vec_storage.index_unchecked(0), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == expect_eq(x));

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1), rt.local_defined_function_vec_storage.index_unchecked(1), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == expect_ne(x));

            auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2), rt.local_defined_function_vec_storage.index_unchecked(2), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr2.results) == expect_lt_s(x));

            auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3), rt.local_defined_function_vec_storage.index_unchecked(3), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr3.results) == expect_lt_u(x));

            auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4), rt.local_defined_function_vec_storage.index_unchecked(4), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr4.results) == expect_ge_s(x));

            auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5), rt.local_defined_function_vec_storage.index_unchecked(5), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr5.results) == expect_ge_u(x));

            auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6), rt.local_defined_function_vec_storage.index_unchecked(6), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr6.results) == expect_gt_s(x));

            auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7), rt.local_defined_function_vec_storage.index_unchecked(7), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr7.results) == expect_gt_u(x));

            auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8), rt.local_defined_function_vec_storage.index_unchecked(8), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr8.results) == expect_le_s(x));

            auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9), rt.local_defined_function_vec_storage.index_unchecked(9), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr9.results) == expect_le_u(x));

            auto rr10 = Runner::run(cm.local_funcs.index_unchecked(10), rt.local_defined_function_vec_storage.index_unchecked(10), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr10.results) == (x != 7 ? 11 : 22));

            auto rr11 = Runner::run(cm.local_funcs.index_unchecked(11), rt.local_defined_function_vec_storage.index_unchecked(11), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr11.results) == (static_cast<::std::uint32_t>(x) < 7u ? 33 : 44));

            auto rr12 = Runner::run(cm.local_funcs.index_unchecked(12), rt.local_defined_function_vec_storage.index_unchecked(12), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr12.results) == static_cast<::std::int32_t>(x == 0));

            auto rr13 = Runner::run(cm.local_funcs.index_unchecked(13), rt.local_defined_function_vec_storage.index_unchecked(13), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr13.results) == (x == 0 ? 55 : 66));

            auto rr14 = Runner::run(cm.local_funcs.index_unchecked(14), rt.local_defined_function_vec_storage.index_unchecked(14), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr14.results) == (x == 7 ? 77 : 88));

            auto rr15 = Runner::run(cm.local_funcs.index_unchecked(15), rt.local_defined_function_vec_storage.index_unchecked(15), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr15.results) == (x < 7 ? 99 : 111));

            auto rr16 = Runner::run(cm.local_funcs.index_unchecked(16), rt.local_defined_function_vec_storage.index_unchecked(16), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr16.results) == (x >= 7 ? 123 : 124));

            auto rr17 = Runner::run(cm.local_funcs.index_unchecked(17), rt.local_defined_function_vec_storage.index_unchecked(17), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr17.results) == (static_cast<::std::uint32_t>(x) >= 7u ? 135 : 136));

            auto rr18 = Runner::run(cm.local_funcs.index_unchecked(18), rt.local_defined_function_vec_storage.index_unchecked(18), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr18.results) == (x > 7 ? 147 : 148));

            auto rr19 = Runner::run(cm.local_funcs.index_unchecked(19), rt.local_defined_function_vec_storage.index_unchecked(19), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr19.results) == (static_cast<::std::uint32_t>(x) > 7u ? 159 : 160));

            auto rr20 = Runner::run(cm.local_funcs.index_unchecked(20), rt.local_defined_function_vec_storage.index_unchecked(20), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr20.results) == (x <= 7 ? 171 : 172));

            auto rr21 = Runner::run(cm.local_funcs.index_unchecked(21), rt.local_defined_function_vec_storage.index_unchecked(21), pack_i32(x), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr21.results) == (static_cast<::std::uint32_t>(x) <= 7u ? 183 : 184));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_i32_compare_localget_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_i32_compare_localget_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_i32_compare_localget_fuse");
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
        return test_translate_i32_compare_localget_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
