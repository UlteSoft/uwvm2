#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] constexpr ::std::uint64_t rotl64(::std::uint64_t x, unsigned r) noexcept
    {
        r &= 63u;
        return (x << r) | (x >> ((64u - r) & 63u));
    }

    [[nodiscard]] byte_vec build_i64_numeric_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: i64 bitmix (param i64) (result i64)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, 0x1111111111111111ll);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::i64_const); i64(c, 3);
            op(c, wasm_op::i64_mul);
            op(c, wasm_op::i64_const); i64(c, 5);
            op(c, wasm_op::i64_rotl);
            op(c, wasm_op::i64_const); i64(c, 0x0f0f0f0f0f0f0f0fll);
            op(c, wasm_op::i64_and);
            op(c, wasm_op::i64_const); i64(c, 0x1000000000000000ll);
            op(c, wasm_op::i64_or);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 clz/ctz/popcnt sum (param i64) (result i64)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_clz);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_ctz);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_popcnt);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i64 (x/7)+(x%7) unsigned (param i64) (result i64)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i64_div_u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i64_rem_u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i64.extend_i32_s (param i32) (result i64) -> (i64)a + 100
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::i64_const); i64(c, 100);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i64.extend_i32_u (param i32) (result i64) -> (u64)a + 200
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::i64_const); i64(c, 200);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local.get ; i64.const ; i64.add (param i64) (result i64) -> x+7
        // Intended to trigger i64_binop_imm_stack combine in tailcall mode.
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_i64_numeric() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_i64_numeric_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_i64_numeric");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        auto expect_f0 = [](::std::uint64_t x) -> ::std::uint64_t
        {
            x ^= 0x1111111111111111ull;
            x += 7ull;
            x *= 3ull;
            x = rotl64(x, 5u);
            x &= 0x0f0f0f0f0f0f0f0full;
            x |= 0x1000000000000000ull;
            return x;
        };

        auto expect_f1 = [](::std::uint64_t x) -> ::std::uint64_t
        {
            ::std::uint64_t clz = (x == 0u) ? 64u : static_cast<::std::uint64_t>(__builtin_clzll(x));
            ::std::uint64_t ctz = (x == 0u) ? 64u : static_cast<::std::uint64_t>(__builtin_ctzll(x));
            ::std::uint64_t pop = static_cast<::std::uint64_t>(__builtin_popcountll(x));
            return clz + ctz + pop;
        };

        auto expect_f2 = [](::std::uint64_t x) -> ::std::uint64_t
        {
            return (x / 7ull) + (x % 7ull);
        };

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            for(::std::uint64_t x : {0ull, 1ull, 0x123456789ull, 0xffffffffffffffffull, 0x8000000000000000ull})
            {
                auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                       rt.local_defined_function_vec_storage.index_unchecked(0),
                                       pack_i64(static_cast<::std::int64_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr0.results)) == expect_f0(x));

                auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                       rt.local_defined_function_vec_storage.index_unchecked(1),
                                       pack_i64(static_cast<::std::int64_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr1.results)) == expect_f1(x));

                auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                       rt.local_defined_function_vec_storage.index_unchecked(2),
                                       pack_i64(static_cast<::std::int64_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr2.results)) == expect_f2(x));

                auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                                       rt.local_defined_function_vec_storage.index_unchecked(5),
                                       pack_i64(static_cast<::std::int64_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr5.results)) == (x + 7ull));
            }

            for(::std::int32_t a : {0, 1, -1, 12345, static_cast<::std::int32_t>(0x80000000u)})
            {
                auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                       rt.local_defined_function_vec_storage.index_unchecked(3),
                                       pack_i32(a),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(load_i64(rr3.results) == (static_cast<::std::int64_t>(a) + 100ll));

                auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                                       rt.local_defined_function_vec_storage.index_unchecked(4),
                                       pack_i32(a),
                                       nullptr,
                                       nullptr);
                auto const ua = static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(a));
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr4.results)) == (ua + 200ull));
            }
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            // f5: ensure i64_binop_imm_stack(add) is present in emitted bytecode.
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple = compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_imm_stack =
                optable::translate::get_uwvmint_i64_binop_imm_stack_fptr_from_tuple<opt, optable::numeric_details::int_binop::add>(curr, tuple);
            constexpr auto exp_imm_localget =
                optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<opt, optable::numeric_details::int_binop::add>(curr, tuple);
            auto const& bc = cm.local_funcs.index_unchecked(5).op.operands;
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, exp_imm_stack) || bytecode_contains_fptr(bc, exp_imm_localget));
#endif

            using Runner = interpreter_runner<opt>;
            for(::std::uint64_t x : {0ull, 1ull, 0x123456789ull, 0xffffffffffffffffull, 0x8000000000000000ull})
            {
                auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                                       rt.local_defined_function_vec_storage.index_unchecked(5),
                                       pack_i64(static_cast<::std::int64_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr5.results)) == (x + 7ull));
            }
        }

        // Mode C: tailcall + stacktop caching (merged rings)
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

            using Runner = interpreter_runner<opt>;
            for(::std::uint64_t x : {0ull, 1ull, 0x123456789ull, 0xffffffffffffffffull, 0x8000000000000000ull})
            {
                auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                       rt.local_defined_function_vec_storage.index_unchecked(0),
                                       pack_i64(static_cast<::std::int64_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr0.results)) == expect_f0(x));
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_i64_numeric();
}
