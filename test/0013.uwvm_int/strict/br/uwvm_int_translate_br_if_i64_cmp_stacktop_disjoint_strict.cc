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

    [[nodiscard]] byte_vec build_br_if_i64_cmp_stacktop_disjoint_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        auto add_brif_i64_cmp = [&](wasm_op cmp_op, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, cmp_op);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, fall_ret);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_ret_i64_cmp = [&](wasm_op cmp_op) -> void
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, cmp_op);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_brif_i64_eqz = [&](::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, fall_ret);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_ret_i64_eqz = [&]() -> void
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_eqz);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0/f1: i64.ne (br_if + return-bool)
        add_brif_i64_cmp(wasm_op::i64_ne, 111, 222);
        add_ret_i64_cmp(wasm_op::i64_ne);

        // f2/f3: i64.lt_u (br_if + return-bool)
        add_brif_i64_cmp(wasm_op::i64_lt_u, 333, 444);
        add_ret_i64_cmp(wasm_op::i64_lt_u);

        // f4/f5: i64.gt_u (br_if + return-bool)
        add_brif_i64_cmp(wasm_op::i64_gt_u, 555, 666);
        add_ret_i64_cmp(wasm_op::i64_gt_u);

        // f6/f7: i64.eqz (br_if + return-bool), for disjoint i64->i32 pop+push modeling.
        add_brif_i64_eqz(777, 888);
        add_ret_i64_eqz();

        // f8/f9: i64.eq
        add_brif_i64_cmp(wasm_op::i64_eq, 901, 902);
        add_ret_i64_cmp(wasm_op::i64_eq);

        // f10/f11: i64.lt_s
        add_brif_i64_cmp(wasm_op::i64_lt_s, 903, 904);
        add_ret_i64_cmp(wasm_op::i64_lt_s);

        // f12/f13: i64.gt_s
        add_brif_i64_cmp(wasm_op::i64_gt_s, 905, 906);
        add_ret_i64_cmp(wasm_op::i64_gt_s);

        // f14/f15: i64.le_s
        add_brif_i64_cmp(wasm_op::i64_le_s, 907, 908);
        add_ret_i64_cmp(wasm_op::i64_le_s);

        // f16/f17: i64.le_u
        add_brif_i64_cmp(wasm_op::i64_le_u, 909, 910);
        add_ret_i64_cmp(wasm_op::i64_le_u);

        // f18/f19: i64.ge_s
        add_brif_i64_cmp(wasm_op::i64_ge_s, 911, 912);
        add_ret_i64_cmp(wasm_op::i64_ge_s);

        // f20/f21: i64.ge_u
        add_brif_i64_cmp(wasm_op::i64_ge_u, 913, 914);
        add_ret_i64_cmp(wasm_op::i64_ge_u);

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

        auto expect_eq = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t { return (a == b) ? 1 : 0; };
        auto expect_ne = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t { return (a != b) ? 1 : 0; };
        auto expect_lt_s = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t { return (a < b) ? 1 : 0; };
        auto expect_lt_u = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t
        { return (static_cast<::std::uint64_t>(a) < static_cast<::std::uint64_t>(b)) ? 1 : 0; };
        auto expect_gt_s = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t { return (a > b) ? 1 : 0; };
        auto expect_gt_u = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t
        { return (static_cast<::std::uint64_t>(a) > static_cast<::std::uint64_t>(b)) ? 1 : 0; };
        auto expect_le_s = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t { return (a <= b) ? 1 : 0; };
        auto expect_le_u = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t
        { return (static_cast<::std::uint64_t>(a) <= static_cast<::std::uint64_t>(b)) ? 1 : 0; };
        auto expect_ge_s = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t { return (a >= b) ? 1 : 0; };
        auto expect_ge_u = [](::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t
        { return (static_cast<::std::uint64_t>(a) >= static_cast<::std::uint64_t>(b)) ? 1 : 0; };
        auto expect_eqz = [](::std::int64_t a) noexcept -> ::std::int32_t { return (a == 0) ? 1 : 0; };

        auto run_ret2 = [&](::std::size_t fidx, ::std::int64_t a, ::std::int64_t b) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_i64x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        // f0: i64.ne + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i64x2(7, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i64x2(7, 8),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        // f1: i64.ne -> i32
        UWVM2TEST_REQUIRE(run_ret2(1, 0, 0) == 0);
        UWVM2TEST_REQUIRE(run_ret2(1, -1, 0) == 1);

        // f2: i64.lt_u + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i64x2(1, 2),
                                              nullptr,
                                              nullptr)
                                       .results) == 333);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i64x2(2, 1),
                                              nullptr,
                                              nullptr)
                                       .results) == 444);

        // f3: i64.lt_u -> i32
        UWVM2TEST_REQUIRE(run_ret2(3, 0, 1) == 1);
        UWVM2TEST_REQUIRE(run_ret2(3, -1, 0) == 0);  // 0xffff... < 0 is false (unsigned)

        // f4: i64.gt_u + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i64x2(2, 1),
                                              nullptr,
                                              nullptr)
                                       .results) == 555);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i64x2(1, 2),
                                              nullptr,
                                              nullptr)
                                       .results) == 666);

        // f5: i64.gt_u -> i32
        UWVM2TEST_REQUIRE(run_ret2(5, 1, 0) == 1);
        UWVM2TEST_REQUIRE(run_ret2(5, 0, -1) == 0);

        // f6: i64.eqz + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i64(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 777);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i64(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 888);

        // f7: i64.eqz -> i32
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(7),
                                   rt.local_defined_function_vec_storage.index_unchecked(7),
                                   pack_i64(0),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 1);
            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(7),
                                   rt.local_defined_function_vec_storage.index_unchecked(7),
                                   pack_i64(123),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == 0);
        }

        // f8: i64.eq + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i64x2(7, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 901);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i64x2(7, 8),
                                              nullptr,
                                              nullptr)
                                       .results) == 902);

        // f9: i64.eq -> i32
        UWVM2TEST_REQUIRE(run_ret2(9, 5, 5) == 1);
        UWVM2TEST_REQUIRE(run_ret2(9, 5, 6) == 0);

        // f10: i64.lt_s + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_i64x2(-1, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 903);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_i64x2(1, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 904);

        // f11: i64.lt_s -> i32
        UWVM2TEST_REQUIRE(run_ret2(11, -1, 0) == 1);
        UWVM2TEST_REQUIRE(run_ret2(11, 1, 0) == 0);

        // f12: i64.gt_s + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(12),
                                              rt.local_defined_function_vec_storage.index_unchecked(12),
                                              pack_i64x2(1, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 905);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(12),
                                              rt.local_defined_function_vec_storage.index_unchecked(12),
                                              pack_i64x2(-1, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 906);

        // f13: i64.gt_s -> i32
        UWVM2TEST_REQUIRE(run_ret2(13, 1, 0) == 1);
        UWVM2TEST_REQUIRE(run_ret2(13, -1, 0) == 0);

        // f14: i64.le_s + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(14),
                                              rt.local_defined_function_vec_storage.index_unchecked(14),
                                              pack_i64x2(0, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 907);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(14),
                                              rt.local_defined_function_vec_storage.index_unchecked(14),
                                              pack_i64x2(1, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 908);

        // f15: i64.le_s -> i32
        UWVM2TEST_REQUIRE(run_ret2(15, 0, 0) == 1);
        UWVM2TEST_REQUIRE(run_ret2(15, 1, 0) == 0);

        // f16: i64.le_u + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(16),
                                              rt.local_defined_function_vec_storage.index_unchecked(16),
                                              pack_i64x2(0, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 909);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(16),
                                              rt.local_defined_function_vec_storage.index_unchecked(16),
                                              pack_i64x2(2, 1),
                                              nullptr,
                                              nullptr)
                                       .results) == 910);

        // f17: i64.le_u -> i32
        UWVM2TEST_REQUIRE(run_ret2(17, 0, 0) == 1);
        UWVM2TEST_REQUIRE(run_ret2(17, 2, 1) == 0);

        // f18: i64.ge_s + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(18),
                                              rt.local_defined_function_vec_storage.index_unchecked(18),
                                              pack_i64x2(0, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 911);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(18),
                                              rt.local_defined_function_vec_storage.index_unchecked(18),
                                              pack_i64x2(-1, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 912);

        // f19: i64.ge_s -> i32
        UWVM2TEST_REQUIRE(run_ret2(19, 0, 0) == 1);
        UWVM2TEST_REQUIRE(run_ret2(19, -1, 0) == 0);

        // f20: i64.ge_u + br_if
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(20),
                                              rt.local_defined_function_vec_storage.index_unchecked(20),
                                              pack_i64x2(1, 0),
                                              nullptr,
                                              nullptr)
                                       .results) == 913);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(20),
                                              rt.local_defined_function_vec_storage.index_unchecked(20),
                                              pack_i64x2(0, 1),
                                              nullptr,
                                              nullptr)
                                       .results) == 914);

        // f21: i64.ge_u -> i32
        UWVM2TEST_REQUIRE(run_ret2(21, 1, 0) == 1);
        UWVM2TEST_REQUIRE(run_ret2(21, 0, 1) == 0);

        // Extra sanity: check a few more pairs for return-bool functions.
        UWVM2TEST_REQUIRE(run_ret2(9, 42, 42) == expect_eq(42, 42));
        UWVM2TEST_REQUIRE(run_ret2(1, 42, 42) == expect_ne(42, 42));
        UWVM2TEST_REQUIRE(run_ret2(11, -2, 1) == expect_lt_s(-2, 1));
        UWVM2TEST_REQUIRE(run_ret2(3, 42, 43) == expect_lt_u(42, 43));
        UWVM2TEST_REQUIRE(run_ret2(13, 43, 42) == expect_gt_s(43, 42));
        UWVM2TEST_REQUIRE(run_ret2(5, 43, 42) == expect_gt_u(43, 42));
        UWVM2TEST_REQUIRE(run_ret2(15, -1, -1) == expect_le_s(-1, -1));
        UWVM2TEST_REQUIRE(run_ret2(17, 1, 2) == expect_le_u(1, 2));
        UWVM2TEST_REQUIRE(run_ret2(19, 43, 42) == expect_ge_s(43, 42));
        UWVM2TEST_REQUIRE(run_ret2(21, 43, 42) == expect_ge_u(43, 42));
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_i64(-1),
                                              nullptr,
                                              nullptr)
                                       .results) == expect_eqz(-1));

        // Tailcall mode (no stacktop): strict fptr assertions on fused br_if opfuncs when combine is enabled.
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos == Opt.i32_stack_top_end_pos)
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_eq = optable::translate::get_uwvmint_br_if_i64_eq_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ne = optable::translate::get_uwvmint_br_if_i64_ne_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_lt_s = optable::translate::get_uwvmint_br_if_i64_lt_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_lt_u = optable::translate::get_uwvmint_br_if_i64_lt_u_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_gt_s = optable::translate::get_uwvmint_br_if_i64_gt_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_gt_u = optable::translate::get_uwvmint_br_if_i64_gt_u_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_le_s = optable::translate::get_uwvmint_br_if_i64_le_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_le_u = optable::translate::get_uwvmint_br_if_i64_le_u_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ge_s = optable::translate::get_uwvmint_br_if_i64_ge_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_ge_u = optable::translate::get_uwvmint_br_if_i64_ge_u_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_eqz = optable::translate::get_uwvmint_br_if_i64_eqz_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_local_eqz = optable::translate::get_uwvmint_br_if_i64_local_eqz_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_ne));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_lt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_lt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(12).op.operands, exp_gt_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_gt_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_le_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(16).op.operands, exp_le_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(18).op.operands, exp_ge_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(20).op.operands, exp_ge_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_eqz) ||
                              bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_local_eqz));
        }
#endif

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_i64_cmp_stacktop_disjoint() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_i64_cmp_stacktop_disjoint_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_i64_cmp_stacktop_disjoint");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching). Also performs fused br_if opfunc assertions when combine is enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching (disjoint i64/i32 rings).
        // Covers cross-ring `i64.cmp -> i32` pop+push modeling with both br_if-adjacent and non-br_if uses.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 7uz,
                .f32_stack_top_end_pos = 9uz,
                .f64_stack_top_begin_pos = 9uz,
                .f64_stack_top_end_pos = 11uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
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
        return test_translate_br_if_i64_cmp_stacktop_disjoint();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
