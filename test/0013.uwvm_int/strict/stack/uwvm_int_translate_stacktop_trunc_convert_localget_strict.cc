#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_stacktop_trunc_convert_localget_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_f32_to_i32 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_to_i32 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f32_to_i64 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f32}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_to_i64 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i32_to_f32 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i64_to_f32 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_i64}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i32_to_f64 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i64_to_f64 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_i64}, {k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_to_f32 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f64}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f32_to_f64 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f32}, {k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // i32 truncations (f32/f64, s/u)
        add_f32_to_i32(wasm_op::i32_trunc_f32_s);  // f0
        add_f32_to_i32(wasm_op::i32_trunc_f32_u);  // f1
        add_f64_to_i32(wasm_op::i32_trunc_f64_s);  // f2
        add_f64_to_i32(wasm_op::i32_trunc_f64_u);  // f3

        // i64 truncations (f32/f64, s/u)
        add_f32_to_i64(wasm_op::i64_trunc_f32_s);  // f4
        add_f32_to_i64(wasm_op::i64_trunc_f32_u);  // f5
        add_f64_to_i64(wasm_op::i64_trunc_f64_s);  // f6
        add_f64_to_i64(wasm_op::i64_trunc_f64_u);  // f7

        // f32 conversions + demote
        add_i32_to_f32(wasm_op::f32_convert_i32_s);  // f8
        add_i32_to_f32(wasm_op::f32_convert_i32_u);  // f9
        add_i64_to_f32(wasm_op::f32_convert_i64_s);  // f10
        add_i64_to_f32(wasm_op::f32_convert_i64_u);  // f11
        add_f64_to_f32(wasm_op::f32_demote_f64);     // f12

        // f64 conversions + promote
        add_i32_to_f64(wasm_op::f64_convert_i32_s);  // f13
        add_i32_to_f64(wasm_op::f64_convert_i32_u);  // f14
        add_i64_to_f64(wasm_op::f64_convert_i64_s);  // f15
        add_i64_to_f64(wasm_op::f64_convert_i64_u);  // f16
        add_f32_to_f64(wasm_op::f64_promote_f32);    // f17

        // Non-fuse variants: use const operands so the translator must take the stacktop caching path
        // in the non-localget (non-heavy-combine) code path.
        //
        // i32.trunc_* from const f32/f64
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, -3.75f);
            op(fb.code, wasm_op::i32_trunc_f32_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f18
        }
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 3.75f);
            op(fb.code, wasm_op::i32_trunc_f32_u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f19
        }
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, -5.25);
            op(fb.code, wasm_op::i32_trunc_f64_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f20
        }
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 7.9);
            op(fb.code, wasm_op::i32_trunc_f64_u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f21
        }

        // f{32,64}.convert_i32_* from const i32
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, -1234567);
            op(fb.code, wasm_op::f32_convert_i32_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f22
        }
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1234567);
            op(fb.code, wasm_op::f32_convert_i32_u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f23
        }
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, -1234567);
            op(fb.code, wasm_op::f64_convert_i32_s);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f24
        }
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1234567);
            op(fb.code, wasm_op::f64_convert_i32_u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));  // f25
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_stacktop_trunc_convert_localget_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 26uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            // For stacktop-caching ABIs the expected fptr depends on the current ring cursor. All our test functions
            // start with an empty stack, so the cursor begins at each range's begin position.
            [[maybe_unused]] constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{
                .i32_stack_top_curr_pos = Opt.i32_stack_top_begin_pos,
                .i64_stack_top_curr_pos = Opt.i64_stack_top_begin_pos,
                .f32_stack_top_curr_pos = Opt.f32_stack_top_begin_pos,
                .f64_stack_top_curr_pos = Opt.f64_stack_top_begin_pos,
                .v128_stack_top_curr_pos = Opt.v128_stack_top_begin_pos,
            };
            [[maybe_unused]] constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            // i32.trunc_* fused with local.get (heavy combine).
            constexpr auto exp_i32_trunc_f32_s = optable::translate::get_uwvmint_i32_from_f32_trunc_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_i32_trunc_f32_u = optable::translate::get_uwvmint_i32_from_f32_trunc_u_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_i32_trunc_f64_s = optable::translate::get_uwvmint_i32_from_f64_trunc_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_i32_trunc_f64_u = optable::translate::get_uwvmint_i32_from_f64_trunc_u_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_trunc_f32_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_trunc_f32_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_trunc_f64_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i32_trunc_f64_u));

            // f{32,64}.convert_i32_* fused with local.get (heavy combine).
            constexpr auto exp_f32_from_i32_s = optable::translate::get_uwvmint_f32_from_i32_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f32_from_i32_u = optable::translate::get_uwvmint_f32_from_i32_u_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_from_i32_s = optable::translate::get_uwvmint_f64_from_i32_s_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_f64_from_i32_u = optable::translate::get_uwvmint_f64_from_i32_u_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_f32_from_i32_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_f32_from_i32_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(13).op.operands, exp_f64_from_i32_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(14).op.operands, exp_f64_from_i32_u));
        }
#endif

        // i32 truncations
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_f32(-3.75f),
                                              nullptr,
                                              nullptr)
                                       .results) == -3);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32(3.75f),
                                              nullptr,
                                              nullptr)
                                       .results) == 3);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_f64(-5.25),
                                              nullptr,
                                              nullptr)
                                       .results) == -5);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f64(7.9),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);

        // i64 truncations
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f32(-9.5f),
                                              nullptr,
                                              nullptr)
                                       .results) == -9);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f32(9.5f),
                                              nullptr,
                                              nullptr)
                                       .results) == 9);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f64(-11.25),
                                              nullptr,
                                              nullptr)
                                       .results) == -11);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f64(42.125),
                                              nullptr,
                                              nullptr)
                                       .results) == 42);

        // f32 conversions + demote (integers are <= 2^24 so results are exact in f32)
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i32(-1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == -1234567.0f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_i32(1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == 1234567.0f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(10),
                                              rt.local_defined_function_vec_storage.index_unchecked(10),
                                              pack_i64(-1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == -1234567.0f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(11),
                                              rt.local_defined_function_vec_storage.index_unchecked(11),
                                              pack_i64(1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == 1234567.0f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(12),
                                              rt.local_defined_function_vec_storage.index_unchecked(12),
                                              pack_f64(3.25),
                                              nullptr,
                                              nullptr)
                                       .results) == 3.25f);

        // f64 conversions + promote
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(13),
                                              rt.local_defined_function_vec_storage.index_unchecked(13),
                                              pack_i32(-1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == -1234567.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(14),
                                              rt.local_defined_function_vec_storage.index_unchecked(14),
                                              pack_i32(1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == 1234567.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(15),
                                              rt.local_defined_function_vec_storage.index_unchecked(15),
                                              pack_i64(-1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == -1234567.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(16),
                                              rt.local_defined_function_vec_storage.index_unchecked(16),
                                              pack_i64(1234567),
                                              nullptr,
                                              nullptr)
                                       .results) == 1234567.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(17),
                                              rt.local_defined_function_vec_storage.index_unchecked(17),
                                              pack_f32(1.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == 1.25);

        // Non-fuse variants (const operands)
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(18),
                                              rt.local_defined_function_vec_storage.index_unchecked(18),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == -3);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(19),
                                              rt.local_defined_function_vec_storage.index_unchecked(19),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 3);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(20),
                                              rt.local_defined_function_vec_storage.index_unchecked(20),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == -5);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(21),
                                              rt.local_defined_function_vec_storage.index_unchecked(21),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(22),
                                              rt.local_defined_function_vec_storage.index_unchecked(22),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == -1234567.0f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(23),
                                              rt.local_defined_function_vec_storage.index_unchecked(23),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 1234567.0f);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(24),
                                              rt.local_defined_function_vec_storage.index_unchecked(24),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == -1234567.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(25),
                                              rt.local_defined_function_vec_storage.index_unchecked(25),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 1234567.0);

        return 0;
    }

    [[nodiscard]] int test_translate_stacktop_trunc_convert_localget() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        // No calls expected in this test.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_trunc_convert_localget_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_trunc_convert_localget");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref smoke (no stacktop caching).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_stacktop_trunc_convert_localget_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: scalar4 disjoint rings.
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
            UWVM2TEST_REQUIRE(run_stacktop_trunc_convert_localget_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: scalar4 fully merged ring.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 7uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_stacktop_trunc_convert_localget_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_stacktop_trunc_convert_localget();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
