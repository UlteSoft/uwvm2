#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] constexpr ::std::uint32_t rotr32(::std::uint32_t x, unsigned s) noexcept
    {
        s &= 31u;
        return (x >> s) | (x << ((32u - s) & 31u));
    }

    [[nodiscard]] constexpr ::std::uint32_t rotl32(::std::uint32_t x, unsigned s) noexcept
    {
        s &= 31u;
        return (x << s) | (x >> ((32u - s) & 31u));
    }

    [[nodiscard]] constexpr ::std::uint32_t shr_s32(::std::uint32_t x, unsigned s) noexcept
    {
        s &= 31u;
        if(s == 0u) { return x; }
        if((x >> 31u) == 0u) { return x >> s; }
        return (x >> s) | (~0u << (32u - s));
    }

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_conbine_update_local_i32_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: x = x + 7 ; return x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: return (local.tee x = x + 9)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: tmp = a + b ; return tmp
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: return (local.tee tmp = a + b)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: x = x * 3 ; return x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: return (local.tee x = rotr(x, 7))
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_rotr);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: x = x - 5 ; return x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: return (local.tee x = x ^ 0x55aa55aa)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x55aa55aau));
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: return (local.tee x = rotl(x, 11))
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 11);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: x = x << 5 ; return x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: return (local.tee x = x >>u 9)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: x = x & 0x00ff00ff ; return x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x00ff00ffu));
            op(c, wasm_op::i32_and);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f12: return (local.tee x = x | 0x0f0f0f0f)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x0f0f0f0fu));
            op(c, wasm_op::i32_or);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f13: x = x >>s 13 ; return x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 13);
            op(c, wasm_op::i32_shr_s);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_conbine_update_local_i32_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr auto curr{make_entry_stacktop_currpos<Opt>()};
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        constexpr auto exp_local_set_same =
            optable::translate::get_uwvmint_i32_add_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple);
        constexpr auto exp_local_tee_same =
            optable::translate::get_uwvmint_i32_add_imm_local_tee_same_fptr_from_tuple<Opt>(curr, tuple);
        constexpr auto exp_sub_local_set_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::sub>(curr, tuple);
        constexpr auto exp_mul_local_set_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::mul>(curr, tuple);
        constexpr auto exp_rotr_local_tee_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::rotr>(curr, tuple);
        constexpr auto exp_xor_local_tee_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::xor_>(curr, tuple);
        constexpr auto exp_rotl_local_tee_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::rotl>(curr, tuple);
        constexpr auto exp_shl_local_set_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::shl>(curr, tuple);
        constexpr auto exp_shr_u_local_tee_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::shr_u>(curr, tuple);
        constexpr auto exp_and_local_set_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::and_>(curr, tuple);
        constexpr auto exp_or_local_tee_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_tee_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::or_>(curr, tuple);
        constexpr auto exp_shr_s_local_set_same =
            optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt, optable::numeric_details::int_binop::shr_s>(curr, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_local_set_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_local_tee_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_mul_local_set_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_rotr_local_tee_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_sub_local_set_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_xor_local_tee_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_rotl_local_tee_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_shl_local_set_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_shr_u_local_tee_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_and_local_set_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(12).op.operands, exp_or_local_tee_same));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(13).op.operands, exp_shr_s_local_set_same));

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        constexpr auto exp_add_2localget_local_set =
            optable::translate::get_uwvmint_i32_add_2localget_local_set_fptr_from_tuple<Opt>(curr, tuple);
        constexpr auto exp_add_2localget_local_tee =
            optable::translate::get_uwvmint_i32_add_2localget_local_tee_fptr_from_tuple<Opt>(curr, tuple);
# else
        constexpr auto exp_add_2localget_local_set =
            optable::translate::get_uwvmint_i32_add_2localget_local_set_common_fptr_from_tuple<Opt>(curr, tuple);
        constexpr auto exp_add_2localget_local_tee =
            optable::translate::get_uwvmint_i32_add_2localget_local_tee_common_fptr_from_tuple<Opt>(curr, tuple);
# endif
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_add_2localget_local_set));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_add_2localget_local_tee));
#endif

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x12345678u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x + 7u));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x89abcdefu})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x + 9u));
        }

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(3, 4),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(-5, 12),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2(10, -3),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);

        for(::std::uint32_t x : {0u, 1u, 7u, 0x12345678u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x * 3u));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x01234567u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(5),
                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == rotr32(x, 7u));
        }

        for(::std::uint32_t x : {0u, 1u, 5u, 0x12345678u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(6),
                                  rt.local_defined_function_vec_storage.index_unchecked(6),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x - 5u));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x01234567u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(7),
                                  rt.local_defined_function_vec_storage.index_unchecked(7),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x ^ 0x55aa55aau));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x89abcdefu})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(8),
                                  rt.local_defined_function_vec_storage.index_unchecked(8),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == rotl32(x, 11u));
        }

        for(::std::uint32_t x : {0u, 1u, 7u, 0x12345678u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(9),
                                  rt.local_defined_function_vec_storage.index_unchecked(9),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x << 5u));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x89abcdefu})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(10),
                                  rt.local_defined_function_vec_storage.index_unchecked(10),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x >> 9u));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x12345678u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(11),
                                  rt.local_defined_function_vec_storage.index_unchecked(11),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x & 0x00ff00ffu));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0x01234567u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(12),
                                  rt.local_defined_function_vec_storage.index_unchecked(12),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == (x | 0x0f0f0f0fu));
        }

        for(::std::uint32_t x : {0u, 1u, 0xffffffffu, 0xf1234567u})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(13),
                                  rt.local_defined_function_vec_storage.index_unchecked(13),
                                  pack_i32(static_cast<::std::int32_t>(x)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == shr_s32(x, 13u));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_update_local_i32() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_update_local_i32_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_update_local_i32");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_conbine_update_local_i32_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_conbine_update_local_i32_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_update_local_i32_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_update_local_i32_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
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
            UWVM2TEST_REQUIRE(run_conbine_update_local_i32_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_conbine_update_local_i32();
}
