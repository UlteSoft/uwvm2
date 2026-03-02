#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] constexpr ::std::uint32_t rotl32(::std::uint32_t x, unsigned r) noexcept
    {
        r &= 31u;
        return (x << r) | (x >> ((32u - r) & 31u));
    }

    [[nodiscard]] constexpr ::std::uint32_t rotr32(::std::uint32_t x, unsigned r) noexcept
    {
        r &= 31u;
        return (x >> r) | (x << ((32u - r) & 31u));
    }

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_i32_numeric_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: i32 bitmix (param i32) (result i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0x11111111);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::i32_const); i32(c, 0x0f0f0f0f);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::i32_const); i32(c, 0x10000000);
            op(c, wasm_op::i32_or);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32 clz/ctz/popcnt sum (param i32) (result i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_clz);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_ctz);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_popcnt);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i32 (x/7)+(x%7) unsigned (param i32) (result i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_div_u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_rem_u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i32.wrap_i64 (param i64) (result i32) -> wrap(x)+100
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_wrap_i64);
            op(c, wasm_op::i32_const); i32(c, 100);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i32 shift/rotate mix (param i32 i32) (result i32)
        // (x << 1) ^ (x >>u 3) ^ rotr(x, y)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_shl);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::i32_shr_u);

            op(c, wasm_op::i32_xor);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_rotr);

            op(c, wasm_op::i32_xor);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local.get ; i32.const ; i32.add (param i32) (result i32) -> x+7
        // Intended to trigger i32_binop_imm_stack combine in tailcall mode.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_i32_numeric() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_i32_numeric_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_i32_numeric");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        auto expect_f0 = [](::std::uint32_t x) -> ::std::uint32_t
        {
            x ^= 0x11111111u;
            x += 7u;
            x *= 3u;
            x = rotl32(x, 5u);
            x &= 0x0f0f0f0fu;
            x |= 0x10000000u;
            return x;
        };

        auto expect_f1 = [](::std::uint32_t x) -> ::std::uint32_t
        {
            ::std::uint32_t clz = (x == 0u) ? 32u : static_cast<::std::uint32_t>(__builtin_clz(x));
            ::std::uint32_t ctz = (x == 0u) ? 32u : static_cast<::std::uint32_t>(__builtin_ctz(x));
            ::std::uint32_t pop = static_cast<::std::uint32_t>(__builtin_popcount(x));
            return clz + ctz + pop;
        };

        auto expect_f2 = [](::std::uint32_t x) -> ::std::uint32_t
        {
            return (x / 7u) + (x % 7u);
        };

        auto expect_f4 = [](::std::uint32_t x, ::std::uint32_t y) -> ::std::uint32_t
        {
            ::std::uint32_t a = x << 1u;
            ::std::uint32_t b = x >> 3u;
            ::std::uint32_t c = rotr32(x, static_cast<unsigned>(y));
            return (a ^ b) ^ c;
        };

        // Mode A: byref (broad semantics coverage)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            for(::std::uint32_t x : {0u, 1u, 0x12345678u, 0xffffffffu, 0x80000000u})
            {
                auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                       rt.local_defined_function_vec_storage.index_unchecked(0),
                                       pack_i32(static_cast<::std::int32_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr0.results)) == expect_f0(x));

                auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                       rt.local_defined_function_vec_storage.index_unchecked(1),
                                       pack_i32(static_cast<::std::int32_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr1.results)) == expect_f1(x));

                auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                       rt.local_defined_function_vec_storage.index_unchecked(2),
                                       pack_i32(static_cast<::std::int32_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr2.results)) == expect_f2(x));

                auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                                       rt.local_defined_function_vec_storage.index_unchecked(5),
                                       pack_i32(static_cast<::std::int32_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr5.results)) == (x + 7u));
            }

            for(auto [x, y] : {::std::pair{0x12345678u, 0u}, ::std::pair{0x12345678u, 7u}, ::std::pair{0xffffffffu, 13u}})
            {
                auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                                       rt.local_defined_function_vec_storage.index_unchecked(4),
                                       pack_i32x2(static_cast<::std::int32_t>(x), static_cast<::std::int32_t>(y)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr4.results)) == expect_f4(x, y));
            }

            for(::std::uint64_t x : {0ull, 1ull, 0x123456789abcdef0ull, 0xffffffffffffffffull})
            {
                auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                       rt.local_defined_function_vec_storage.index_unchecked(3),
                                       pack_i64(static_cast<::std::int64_t>(x)),
                                       nullptr,
                                       nullptr);
                auto const wrap = static_cast<::std::uint32_t>(x);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr3.results)) == (wrap + 100u));
            }
        }

        // Mode B: tailcall (combine assertions)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_imm_stack =
                optable::translate::get_uwvmint_i32_binop_imm_stack_fptr_from_tuple<opt, optable::numeric_details::int_binop::add>(curr, tuple);
            constexpr auto exp_imm_localget =
                optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<opt>(curr, tuple);
            auto const& bc = cm.local_funcs.index_unchecked(5).op.operands;
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc, exp_imm_stack) || bytecode_contains_fptr(bc, exp_imm_localget));
#endif

            using Runner = interpreter_runner<opt>;
            for(::std::uint32_t x : {0u, 1u, 0x12345678u, 0xffffffffu, 0x80000000u})
            {
                auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                                       rt.local_defined_function_vec_storage.index_unchecked(5),
                                       pack_i32(static_cast<::std::int32_t>(x)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr5.results)) == (x + 7u));
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_i32_numeric();
}
