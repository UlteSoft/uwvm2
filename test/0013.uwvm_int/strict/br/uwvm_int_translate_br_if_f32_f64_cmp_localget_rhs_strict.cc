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

    [[nodiscard]] byte_vec build_br_if_float_cmp_localget_rhs_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_brif_f32_localget_rhs = [&](wasm_op cmp_op, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_neg);
            op(c, wasm_op::local_get);
            u32(c, 0u);
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

        auto add_brif_f32_generic = [&](wasm_op cmp_op, float rhs, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, rhs);
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

        auto add_brif_f32_localget2_flush = [&](wasm_op cmp_op, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_i32}};
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

        auto add_ret_f32_localget_rhs = [&](wasm_op cmp_op) -> void
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_neg);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, cmp_op);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_brif_f64_localget_rhs = [&](wasm_op cmp_op, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_neg);
            op(c, wasm_op::local_get);
            u32(c, 0u);
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

        auto add_brif_f64_generic = [&](wasm_op cmp_op, double rhs, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, rhs);
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

        auto add_brif_f64_localget2_flush = [&](wasm_op cmp_op, ::std::int32_t taken_ret, ::std::int32_t fall_ret) -> void
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_i32}};
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

        auto add_ret_f64_localget_rhs = [&](wasm_op cmp_op) -> void
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_neg);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, cmp_op);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0: f32.eq localget-rhs + br_if (localget-rhs fused compare, then br_if fusion)
        add_brif_f32_localget_rhs(wasm_op::f32_eq, 111, 112);
        // f1: f32.eq generic + br_if (non-localget-rhs br_if fuse path)
        add_brif_f32_generic(wasm_op::f32_eq, 1.25f, 121, 122);
        // f2: f32.eq localget-rhs (no br_if; covers non-br_if localget-rhs stacktop update)
        add_ret_f32_localget_rhs(wasm_op::f32_eq);
        // f3: f32.ne with local.get2 (forces flush local_get2 path) + br_if
        add_brif_f32_localget2_flush(wasm_op::f32_ne, 131, 132);

        // f4..f7: f32.{lt,gt,le,ge} localget-rhs + br_if (hit localget-rhs + br_if-adjacent stacktop paths)
        add_brif_f32_localget_rhs(wasm_op::f32_lt, 141, 142);
        add_brif_f32_localget_rhs(wasm_op::f32_gt, 151, 152);
        add_brif_f32_localget_rhs(wasm_op::f32_le, 161, 162);
        add_brif_f32_localget_rhs(wasm_op::f32_ge, 171, 172);

        // f8: f64.eq localget-rhs + br_if
        add_brif_f64_localget_rhs(wasm_op::f64_eq, 211, 212);
        // f9: f64.eq generic + br_if
        add_brif_f64_generic(wasm_op::f64_eq, 2.5, 221, 222);
        // f10: f64.eq localget-rhs (no br_if)
        add_ret_f64_localget_rhs(wasm_op::f64_eq);
        // f11: f64.ne with local.get2 flush + br_if
        add_brif_f64_localget2_flush(wasm_op::f64_ne, 231, 232);

        // f12..f15: f64.{lt,gt,le,ge} localget-rhs + br_if
        add_brif_f64_localget_rhs(wasm_op::f64_lt, 241, 242);
        add_brif_f64_localget_rhs(wasm_op::f64_gt, 251, 252);
        add_brif_f64_localget_rhs(wasm_op::f64_le, 261, 262);
        add_brif_f64_localget_rhs(wasm_op::f64_ge, 271, 272);

        // f16: f32.ne localget-rhs + br_if (covers br_if_f32_ne_localget_rhs)
        add_brif_f32_localget_rhs(wasm_op::f32_ne, 181, 182);

        // f17: f64.ne localget-rhs + br_if (covers br_if_f64_ne_localget_rhs)
        add_brif_f64_localget_rhs(wasm_op::f64_ne, 281, 282);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 18uz);

        using Runner = interpreter_runner<Opt>;

        auto run_f32_1 = [&](::std::size_t fidx, float x) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32(x),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        auto run_f32_2 = [&](::std::size_t fidx, float a, float b) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        auto run_f64_1 = [&](::std::size_t fidx, double x) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64(x),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        auto run_f64_2 = [&](::std::size_t fidx, double a, double b) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        // f0: f32.eq localget-rhs + br_if (-x == x) => x==0
        UWVM2TEST_REQUIRE(run_f32_1(0, 0.0f) == 111);
        UWVM2TEST_REQUIRE(run_f32_1(0, 1.25f) == 112);

        // f1: f32.eq generic + br_if
        UWVM2TEST_REQUIRE(run_f32_1(1, 1.25f) == 121);
        UWVM2TEST_REQUIRE(run_f32_1(1, 0.0f) == 122);

        // f2: f32.eq localget-rhs (no br_if) => i32 bool (-x == x) => x==0
        UWVM2TEST_REQUIRE(run_f32_1(2, 0.0f) == 1);
        UWVM2TEST_REQUIRE(run_f32_1(2, 2.25f) == 0);

        // f3: f32.ne local.get2 flush + br_if
        UWVM2TEST_REQUIRE(run_f32_2(3, 1.0f, 2.0f) == 131);
        UWVM2TEST_REQUIRE(run_f32_2(3, 2.0f, 2.0f) == 132);

        // f4..f7: f32 comparisons localget-rhs + br_if (lhs=-x, rhs=x)
        UWVM2TEST_REQUIRE(run_f32_1(4, 2.0f) == 141);    // -2.0 < 2.0
        UWVM2TEST_REQUIRE(run_f32_1(4, -2.0f) == 142);   // 2.0 < -2.0 is false

        UWVM2TEST_REQUIRE(run_f32_1(5, -2.0f) == 151);   // 2.0 > -2.0
        UWVM2TEST_REQUIRE(run_f32_1(5, 2.0f) == 152);    // -2.0 > 2.0 is false

        UWVM2TEST_REQUIRE(run_f32_1(6, 2.0f) == 161);    // -2.0 <= 2.0
        UWVM2TEST_REQUIRE(run_f32_1(6, -2.0f) == 162);   // 2.0 <= -2.0 is false

        UWVM2TEST_REQUIRE(run_f32_1(7, -2.0f) == 171);   // 2.0 >= -2.0
        UWVM2TEST_REQUIRE(run_f32_1(7, 2.0f) == 172);    // -2.0 >= 2.0 is false

        // f8: f64.eq localget-rhs + br_if (-x == x) => x==0
        UWVM2TEST_REQUIRE(run_f64_1(8, 0.0) == 211);
        UWVM2TEST_REQUIRE(run_f64_1(8, 2.5) == 212);

        // f9: f64.eq generic + br_if
        UWVM2TEST_REQUIRE(run_f64_1(9, 2.5) == 221);
        UWVM2TEST_REQUIRE(run_f64_1(9, 0.0) == 222);

        // f10: f64.eq localget-rhs (no br_if) => i32 bool (-x == x) => x==0
        UWVM2TEST_REQUIRE(run_f64_1(10, 0.0) == 1);
        UWVM2TEST_REQUIRE(run_f64_1(10, 3.5) == 0);

        // f11: f64.ne local.get2 flush + br_if
        UWVM2TEST_REQUIRE(run_f64_2(11, 1.0, 2.0) == 231);
        UWVM2TEST_REQUIRE(run_f64_2(11, 2.0, 2.0) == 232);

        // f12..f15: f64 comparisons localget-rhs + br_if (lhs=-x, rhs=x)
        UWVM2TEST_REQUIRE(run_f64_1(12, 4.0) == 241);    // -4.0 < 4.0
        UWVM2TEST_REQUIRE(run_f64_1(12, -4.0) == 242);   // 4.0 < -4.0 is false

        UWVM2TEST_REQUIRE(run_f64_1(13, -4.0) == 251);   // 4.0 > -4.0
        UWVM2TEST_REQUIRE(run_f64_1(13, 4.0) == 252);    // -4.0 > 4.0 is false

        UWVM2TEST_REQUIRE(run_f64_1(14, 4.0) == 261);    // -4.0 <= 4.0
        UWVM2TEST_REQUIRE(run_f64_1(14, -4.0) == 262);   // 4.0 <= -4.0 is false

        UWVM2TEST_REQUIRE(run_f64_1(15, -4.0) == 271);   // 4.0 >= -4.0
        UWVM2TEST_REQUIRE(run_f64_1(15, 4.0) == 272);    // -4.0 >= 4.0 is false

        // f16: f32.ne localget-rhs + br_if (-x != x) => x!=0
        UWVM2TEST_REQUIRE(run_f32_1(16, 2.0f) == 181);
        UWVM2TEST_REQUIRE(run_f32_1(16, 0.0f) == 182);

        // f17: f64.ne localget-rhs + br_if (-x != x) => x!=0
        UWVM2TEST_REQUIRE(run_f64_1(17, 2.0) == 281);
        UWVM2TEST_REQUIRE(run_f64_1(17, 0.0) == 282);

        // Tailcall mode (no stacktop): strict fptr assertions on fused br_if opfuncs when heavy-combine is enabled.
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos == Opt.i32_stack_top_end_pos)
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_f32_eq = optable::translate::get_uwvmint_br_if_f32_eq_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_eq_lgrhs = optable::translate::get_uwvmint_br_if_f32_eq_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_ne = optable::translate::get_uwvmint_br_if_f32_ne_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_ne_lgrhs = optable::translate::get_uwvmint_br_if_f32_ne_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_lt_lgrhs = optable::translate::get_uwvmint_br_if_f32_lt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_gt_lgrhs = optable::translate::get_uwvmint_br_if_f32_gt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_le_lgrhs = optable::translate::get_uwvmint_br_if_f32_le_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_ge_lgrhs = optable::translate::get_uwvmint_br_if_f32_ge_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);

            constexpr auto exp_f64_eq = optable::translate::get_uwvmint_br_if_f64_eq_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_eq_lgrhs = optable::translate::get_uwvmint_br_if_f64_eq_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_ne = optable::translate::get_uwvmint_br_if_f64_ne_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_ne_lgrhs = optable::translate::get_uwvmint_br_if_f64_ne_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_lt_lgrhs = optable::translate::get_uwvmint_br_if_f64_lt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_gt_lgrhs = optable::translate::get_uwvmint_br_if_f64_gt_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_le_lgrhs = optable::translate::get_uwvmint_br_if_f64_le_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_ge_lgrhs = optable::translate::get_uwvmint_br_if_f64_ge_localget_rhs_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_f32_eq_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_f32_eq));
            // In heavy combine, `local.get a; local.get b; f32.ne; br_if` may use either the generic f32.ne br_if
            // or the "localget-rhs" variant (rhs in local, lhs from stack). Accept either.
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f32_ne) ||
                              bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f32_ne_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_f32_lt_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_f32_gt_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f32_le_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f32_ge_lgrhs));

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f64_eq_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_f64_eq));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_f64_ne) ||
                              bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_f64_ne_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(12).op.operands, exp_f64_lt_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(13).op.operands, exp_f64_gt_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_f64_le_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(15).op.operands, exp_f64_ge_lgrhs));

            // New: f32/f64 ne localget-rhs + br_if must select the dedicated br_if-localget-rhs opfunc.
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(16).op.operands, exp_f32_ne_lgrhs));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(17).op.operands, exp_f64_ne_lgrhs));

            // Ensure we don't accidentally match these fptrs in the non-br_if localget-rhs return-bool functions.
            UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_eq_lgrhs));
            UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_f64_eq_lgrhs));
        }
#endif

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_float_cmp_localget_rhs() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_float_cmp_localget_rhs_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_float_cmp_localget_rhs");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching). Also performs fused br_if opfunc assertions when heavy-combine is enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching with fully split rings.
        // Covers `float.cmp -> i32` across disjoint f32/f64 vs i32 rings and both br_if-adjacent and non-br_if uses.
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

        // Mode D: tailcall + stacktop caching with scalar-all-merged ring.
        // Covers the `stacktop_ranges_merged_for(f32/i32)` and `stacktop_ranges_merged_for(f64/i32)` compare->i32 update paths.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
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
        return test_translate_br_if_float_cmp_localget_rhs();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
