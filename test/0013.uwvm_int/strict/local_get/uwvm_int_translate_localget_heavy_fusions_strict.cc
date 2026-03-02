#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x8(::std::array<::std::uint32_t, 8uz> const& a) noexcept
    {
        byte_vec out(8uz * 4uz);
        for(::std::size_t i{}; i != a.size(); ++i)
        {
            ::std::int32_t v = static_cast<::std::int32_t>(a[i]);
            ::std::memcpy(out.data() + i * 4uz, ::std::addressof(v), 4uz);
        }
        return out;
    }

    [[nodiscard]] byte_vec pack_i64x8(::std::array<::std::uint64_t, 8uz> const& a) noexcept
    {
        byte_vec out(8uz * 8uz);
        for(::std::size_t i{}; i != a.size(); ++i)
        {
            ::std::int64_t v = static_cast<::std::int64_t>(a[i]);
            ::std::memcpy(out.data() + i * 8uz, ::std::addressof(v), 8uz);
        }
        return out;
    }

    template <::std::size_t N>
    [[nodiscard]] byte_vec pack_f64x(::std::array<double, N> const& a) noexcept
    {
        byte_vec out(N * 8uz);
        for(::std::size_t i{}; i != N; ++i)
        {
            ::std::memcpy(out.data() + i * 8uz, ::std::addressof(a[i]), 8uz);
        }
        return out;
    }

    [[nodiscard]] constexpr ::std::uint32_t f32_bits(float v) noexcept
    {
        return ::std::bit_cast<::std::uint32_t>(v);
    }

    [[nodiscard]] constexpr ::std::uint64_t f64_bits(double v) noexcept
    {
        return ::std::bit_cast<::std::uint64_t>(v);
    }

    template <typename UInt, ::std::size_t N>
    [[nodiscard]] constexpr UInt sum_mod(::std::array<UInt, N> const& a) noexcept
    {
        UInt s{};
        for(auto v : a) { s += v; }
        return s;
    }

    template <::std::size_t N>
    [[nodiscard]] double add_reduce_expected(::std::array<double, N> const& a) noexcept
    {
        static_assert(N >= 2uz);
        // Mirrors the stack evaluation order of:
        //   local.get 0..N-1; f64.add x(N-1)
        double acc = a[N - 2uz] + a[N - 1uz];
        for(::std::size_t i{N - 2uz}; i-- > 0uz;)
        {
            acc = a[i] + acc;
        }
        return acc;
    }

    [[nodiscard]] byte_vec build_localget_heavy_fusions_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: i32 add-reduce (8 local.get + 7 add).
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::uint32_t i{}; i != 8u; ++i)
            {
                op(c, wasm_op::local_get);
                u32(c, i);
            }
            for(::std::uint32_t i{}; i != 7u; ++i) { op(c, wasm_op::i32_add); }
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 add-reduce (8 local.get + 7 add).
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::uint32_t i{}; i != 8u; ++i)
            {
                op(c, wasm_op::local_get);
                u32(c, i);
            }
            for(::std::uint32_t i{}; i != 7u; ++i) { op(c, wasm_op::i64_add); }
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f64 add-reduce (7 local.get + 6 add) => f64_add_reduce_7localget.
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::uint32_t i{}; i != 7u; ++i)
            {
                op(c, wasm_op::local_get);
                u32(c, i);
            }
            for(::std::uint32_t i{}; i != 6u; ++i) { op(c, wasm_op::f64_add); }
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f64 add-reduce (12 local.get + 11 add) => f64_add_reduce_12localget.
        {
            func_type ty{
                {k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64, k_val_f64},
                {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            for(::std::uint32_t i{}; i != 12u; ++i)
            {
                op(c, wasm_op::local_get);
                u32(c, i);
            }
            for(::std::uint32_t i{}; i != 11u; ++i) { op(c, wasm_op::f64_add); }
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: f32 affine update: x = x*2 + 1 (local.set same local).
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            f32(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: f64 affine update: x = x*1.25 + (-0.5) (local.set same local).
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 1.25);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_const);
            f64(c, -0.5);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: extra-heavy f64 chain: local.get src; (mul+add); local.tee d1 (1 stage).
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local1 = d1
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_tee);
            u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: extra-heavy f64 chain: 4 stages of (mul+add) + local.tee (same imm bits).
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({4u, k_val_f64});  // local1..4 = d1..d4
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            for(::std::uint32_t dst = 1u; dst != 5u; ++dst)
            {
                op(c, wasm_op::f64_const);
                f64(c, 2.0);
                op(c, wasm_op::f64_mul);
                op(c, wasm_op::f64_const);
                f64(c, 1.0);
                op(c, wasm_op::f64_add);
                op(c, wasm_op::local_tee);
                u32(c, dst);
            }
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_localget_heavy_fusions_semantics(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        using Runner = interpreter_runner<Opt>;

        // f0: i32 add-reduce.
        {
            ::std::array<::std::uint32_t, 8uz> a{0u, 1u, 2u, 3u, 0x7fffffffu, 0x80000000u, 0xffffffffu, 0x12345678u};
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32x8(a),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == sum_mod(a));
        }

        // f1: i64 add-reduce.
        {
            ::std::array<::std::uint64_t, 8uz> a{
                0ull, 1ull, 2ull, 3ull, 0x7fff'ffff'ffff'ffffull, 0x8000'0000'0000'0000ull, 0xffff'ffff'ffff'ffffull, 0x0123'4567'89ab'cdefull};
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i64x8(a),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == sum_mod(a));
        }

        // f2: f64 add-reduce 7 (order-sensitive).
        {
            ::std::array<double, 7uz> a{1e16, -1e16, 1.0, 1.0, 1.0, 1.0, 1.0};
            double const exp = add_reduce_expected(a);
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_f64x(a),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(f64_bits(load_f64(rr.results)) == f64_bits(exp));
        }

        // f3: f64 add-reduce 12 (order-sensitive).
        {
            ::std::array<double, 12uz> a{1e16, -1e16, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
            double const exp = add_reduce_expected(a);
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_f64x(a),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(f64_bits(load_f64(rr.results)) == f64_bits(exp));
        }

        // f4: f32 affine update x = x*2 + 1
        {
            float const x = 3.5f;
            float const exp = x * 2.0f + 1.0f;
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  pack_f32(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(f32_bits(load_f32(rr.results)) == f32_bits(exp));
        }

        // f5: f64 affine update x = x*1.25 - 0.5
        {
            double const x = 3.5;
            double const exp = x * 1.25 + (-0.5);
            auto rr = Runner::run(cm.local_funcs.index_unchecked(5),
                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                  pack_f64(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(f64_bits(load_f64(rr.results)) == f64_bits(exp));
        }

        // f6: extra-heavy f64 chain (1 stage) or placeholder.
        {
            double const x = 1.25;
            double const exp = x * 2.0 + 1.0;
            auto rr = Runner::run(cm.local_funcs.index_unchecked(6),
                                  rt.local_defined_function_vec_storage.index_unchecked(6),
                                  pack_f64(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(f64_bits(load_f64(rr.results)) == f64_bits(exp));
        }

        // f7: extra-heavy f64 chain (4 stages) or placeholder.
        {
            double const x = 1.25;
            double exp = x;
            for(int i{}; i != 4; ++i) { exp = exp * 2.0 + 1.0; }
            auto rr = Runner::run(cm.local_funcs.index_unchecked(7),
                                  rt.local_defined_function_vec_storage.index_unchecked(7),
                                  pack_f64(x),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(f64_bits(load_f64(rr.results)) == f64_bits(exp));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_localget_heavy_fusions() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_localget_heavy_fusions_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_localget_heavy_fusions");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: strict fptr assertions when heavy combine is enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr auto exp_i32_add_reduce =
                optable::translate::get_uwvmint_i32_add_reduce_8localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i64_add_reduce =
                optable::translate::get_uwvmint_i64_add_reduce_8localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_add_reduce_7 =
                optable::translate::get_uwvmint_f64_add_reduce_7localget_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_add_reduce_12 =
                optable::translate::get_uwvmint_f64_add_reduce_12localget_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_add_reduce));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i64_add_reduce));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f64_add_reduce_7));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f64_add_reduce_12));

            constexpr auto exp_f32_affine =
                optable::translate::get_uwvmint_f32_mul_add_2imm_local_set_same_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_affine =
                optable::translate::get_uwvmint_f64_mul_add_2imm_local_set_same_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_f32_affine));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_f64_affine));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            {
                constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
                constexpr auto tuple =
                    compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

                constexpr auto exp_chain_1 =
                    optable::translate::get_uwvmint_f64_mul_add_2imm_localget_local_tee_fptr_from_tuple<opt>(curr, tuple);
                constexpr auto exp_chain_4 =
                    optable::translate::get_uwvmint_f64_mul_add_2imm_localget_local_tee_4x_fptr_from_tuple<opt>(curr, tuple);

                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_chain_1));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_chain_4));
            }
#endif

            UWVM2TEST_REQUIRE(run_localget_heavy_fusions_semantics<opt>(cm, rt) == 0);
        }

        // Byref mode: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_localget_heavy_fusions_semantics<opt>(cm, rt) == 0);
        }

        // Tailcall + stacktop caching: semantics.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(run_localget_heavy_fusions_semantics<opt>(cm, rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_localget_heavy_fusions();
}
