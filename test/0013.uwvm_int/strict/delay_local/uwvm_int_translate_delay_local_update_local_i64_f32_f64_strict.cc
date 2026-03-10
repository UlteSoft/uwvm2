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

    [[nodiscard]] byte_vec build_delay_local_update_local_i64_f32_f64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i64 add local_set variant: b = a + b ; return b
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 add local_tee variant: tmp = a + b ; drop ; return tmp
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32 add local_set variant: b = a + b ; return b
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f32 sub local_tee variant: tmp = a - b ; drop ; return tmp
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_sub);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: f32 mul local_set variant: b = a * b ; return b
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: f64 add local_set variant: b = a + b ; return b
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: f64 sub local_tee variant: tmp = a - b ; drop ; return tmp
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_sub);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: f64 mul local_set variant: b = a * b ; return b
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: i64 xor local_set variant: b = a xor b ; return b
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: i64 mul local_tee variant: tmp = a * b ; drop ; return tmp
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_mul);
            op(c, wasm_op::local_tee); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
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
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 10uz);

        using Runner = interpreter_runner<Opt>;
        [[maybe_unused]] auto const curr{make_entry_stacktop_currpos<Opt>()};
        [[maybe_unused]] constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        auto const contains_i64_variant{
            [&](auto const& bc, auto make_fptr) constexpr noexcept
            {
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

                if constexpr(Opt.i64_stack_top_begin_pos != SIZE_MAX && Opt.i64_stack_top_begin_pos != Opt.i64_stack_top_end_pos)
                {
                    auto curr_variant{curr};
                    for(::std::size_t pos{Opt.i64_stack_top_begin_pos}; pos < Opt.i64_stack_top_end_pos; ++pos)
                    {
                        curr_variant.i64_stack_top_curr_pos = pos;
                        if(bytecode_contains_fptr(bc, make_fptr(curr_variant))) { return true; }
                    }
                }

                return false;
            }};
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

        if constexpr(Opt.is_tail_call)
        {
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT)
            auto const exp_i64_add_set{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::add>(curr_variant, tuple);
                }};
            auto const exp_i64_add_tee{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::add>(curr_variant, tuple);
                }};
            auto const exp_f32_add_set{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::add>(curr_variant, tuple);
                }};
            auto const exp_f32_mul_set{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::mul>(curr_variant, tuple);
                }};
            auto const exp_f64_add_set{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::add>(curr_variant, tuple);
                }};
            auto const exp_f64_mul_set{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::mul>(curr_variant, tuple);
                }};
            auto const exp_i64_xor_set{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::xor_>(curr_variant, tuple);
                }};
# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            auto const exp_i64_add_2localget_local_set{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_i64_add_2localget_local_set_fptr_from_tuple<Opt>(curr_variant, tuple); }};
            auto const exp_i64_add_2localget_local_tee{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_i64_add_2localget_local_tee_fptr_from_tuple<Opt>(curr_variant, tuple); }};
            auto const exp_f32_add_2localget_local_set{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_f32_add_2localget_local_set_fptr_from_tuple<Opt>(curr_variant, tuple); }};
            auto const exp_f64_add_2localget_local_set{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_f64_add_2localget_local_set_fptr_from_tuple<Opt>(curr_variant, tuple); }};
# else
            auto const exp_i64_add_2localget_local_set{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_i64_add_2localget_local_set_common_fptr_from_tuple<Opt>(curr_variant, tuple); }};
            auto const exp_i64_add_2localget_local_tee{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_i64_add_2localget_local_tee_common_fptr_from_tuple<Opt>(curr_variant, tuple); }};
            auto const exp_f32_add_2localget_local_set{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_f32_add_2localget_local_set_common_fptr_from_tuple<Opt>(curr_variant, tuple); }};
            auto const exp_f64_add_2localget_local_set{
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_f64_add_2localget_local_set_common_fptr_from_tuple<Opt>(curr_variant, tuple); }};
# endif

            bool ok_i64_add_set{};
            ok_i64_add_set = ok_i64_add_set || contains_i64_variant(cm.local_funcs.index_unchecked(0).op.operands, exp_i64_add_2localget_local_set);
            ok_i64_add_set = ok_i64_add_set || contains_i64_variant(cm.local_funcs.index_unchecked(0).op.operands, exp_i64_add_set);
            UWVM2TEST_REQUIRE(ok_i64_add_set);

            bool ok_i64_add_tee{};
            ok_i64_add_tee = ok_i64_add_tee || contains_i64_variant(cm.local_funcs.index_unchecked(1).op.operands, exp_i64_add_2localget_local_tee);
            ok_i64_add_tee = ok_i64_add_tee || contains_i64_variant(cm.local_funcs.index_unchecked(1).op.operands, exp_i64_add_tee);
            UWVM2TEST_REQUIRE(ok_i64_add_tee);

            bool ok_f32_add_set{};
            ok_f32_add_set = ok_f32_add_set || contains_f32_variant(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_add_2localget_local_set);
            ok_f32_add_set = ok_f32_add_set || contains_f32_variant(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_add_set);
            UWVM2TEST_REQUIRE(ok_f32_add_set);
            UWVM2TEST_REQUIRE(contains_f32_variant(cm.local_funcs.index_unchecked(4).op.operands, exp_f32_mul_set));
            bool ok_f64_add_set{};
            ok_f64_add_set = ok_f64_add_set || contains_f64_variant(cm.local_funcs.index_unchecked(5).op.operands, exp_f64_add_2localget_local_set);
            ok_f64_add_set = ok_f64_add_set || contains_f64_variant(cm.local_funcs.index_unchecked(5).op.operands, exp_f64_add_set);
            UWVM2TEST_REQUIRE(ok_f64_add_set);
            UWVM2TEST_REQUIRE(contains_f64_variant(cm.local_funcs.index_unchecked(7).op.operands, exp_f64_mul_set));
            UWVM2TEST_REQUIRE(contains_i64_variant(cm.local_funcs.index_unchecked(8).op.operands, exp_i64_xor_set));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY)
            auto const exp_i64_mul_tee{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i64_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::mul>(curr_variant, tuple);
                }};
            auto const exp_f32_sub_tee{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f32_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::sub>(curr_variant, tuple);
                }};
            auto const exp_f64_sub_tee{
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_f64_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::float_binop::sub>(curr_variant, tuple);
                }};

            UWVM2TEST_REQUIRE(contains_f32_variant(cm.local_funcs.index_unchecked(3).op.operands, exp_f32_sub_tee));
            UWVM2TEST_REQUIRE(contains_f64_variant(cm.local_funcs.index_unchecked(6).op.operands, exp_f64_sub_tee));
            UWVM2TEST_REQUIRE(contains_i64_variant(cm.local_funcs.index_unchecked(9).op.operands, exp_i64_mul_tee));
#endif
        }

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i64x2(10, -3),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i64x2(20, 22),
                                              nullptr,
                                              nullptr)
                                       .results) == 42);

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_f32x2(1.5f, 2.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == 3.75f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f32x2(9.5f, 4.25f),
                                              nullptr,
                                              nullptr)
                                       .results) == 5.25f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f32x2(3.0f, 2.5f),
                                              nullptr,
                                              nullptr)
                                       .results) == 7.5f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f64x2(1.25, 2.75),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_f64x2(11.5, 4.5),
                                              nullptr,
                                              nullptr)
                                       .results) == 7.0);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_f64x2(3.0, 2.5),
                                              nullptr,
                                              nullptr)
                                       .results) == 7.5);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i64x2(0x123456789abcdef0ll, 0x0f0f0f0f0f0f0f0fll),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int64_t>(0x1d3b597795b3d1ffull));
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_i64x2(6, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 42);

        return 0;
    }

    [[nodiscard]] int test_translate_delay_local_update_local_i64_f32_f64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_delay_local_update_local_i64_f32_f64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_delay_local_update_local_i64_f32_f64");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
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
    return test_translate_delay_local_update_local_i64_f32_f64();
}
