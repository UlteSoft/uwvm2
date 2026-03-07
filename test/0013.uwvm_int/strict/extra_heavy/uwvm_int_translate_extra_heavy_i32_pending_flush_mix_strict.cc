#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32_i32(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] constexpr ::std::uint32_t rotl32(::std::uint32_t v, ::std::uint32_t r) noexcept
    {
        return ::std::rotl(v, static_cast<int>(r & 31u));
    }

    [[nodiscard]] byte_vec build_i32_pending_flush_mix_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: rot_xor_add pending flush at `after_rotl` (barrier: i32.const)
        // (x: i32) -> i32 : rotl(x, 5) + 1
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: rot_xor_add pending flush at `after_gety` (barrier: i32.add)
        // (x: i32, y: i32) -> i32 : rotl(x, 7) + y
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: rot_xor_add pending flush at `after_xor` (barrier: i32.eqz)
        // (x: i32, y: i32) -> i32 : i32.eqz(rotl(x, 13) ^ y)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 13);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: rot_xor_add pending flush at `after_xor_constc` (barrier: i32.sub; must NOT be i32.add)
        // (x: i32, y: i32) -> i32 : (rotl(x, 9) ^ y) - 123
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 9);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_const);
            i32(c, 123);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: rotl_xor_local_set pending flush at `after_xor` (barrier: i32.eqz)
        // (x: i32, y: i32) -> i32 : i32.eqz(y ^ rotl(x, 5))
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 1u);  // y
            op(c, wasm_op::local_get);
            u32(c, 0u);  // x
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_rotl);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: xorshift pending flush at `after_shr` (barrier: i32.add)
        // (x: i32) -> i32 : x + (x >> 7)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: xorshift pending flush at `after_xor1` (barrier: i32.eqz)
        // (x: i32) -> i32 : i32.eqz(x ^ (x >> 3))
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: xorshift pending flush at `after_xor1_getx` (barrier: i32.add)
        // (x: i32) -> i32 : (x ^ (x >> 5)) + x
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: xorshift pending flush at `after_xor1_getx_constb` (barrier: i32.add; NOT i32.shl)
        // (x: i32) -> i32 : (x ^ (x >> 5)) + x + 9
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 9);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: xorshift pending flush at `after_shl` (barrier: i32.add)
        // (x: i32) -> i32 : (x ^ (x >> 5)) + (x << 1)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 5);
            op(c, wasm_op::i32_shr_u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_i32_pending_flush_mix_suite(runtime_module_t const& rt, ::std::uint32_t x, ::std::uint32_t y) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        auto run_i32_1 = [&](::std::size_t fn_index, ::std::int32_t a) noexcept -> ::std::int32_t
        {
            return load_i32(
                Runner::run(cm.local_funcs.index_unchecked(fn_index),
                            rt.local_defined_function_vec_storage.index_unchecked(fn_index),
                            pack_i32(a),
                            nullptr,
                            nullptr)
                    .results);
        };

        auto run_i32_2 = [&](::std::size_t fn_index, ::std::int32_t a, ::std::int32_t b) noexcept -> ::std::int32_t
        {
            return load_i32(
                Runner::run(cm.local_funcs.index_unchecked(fn_index),
                            rt.local_defined_function_vec_storage.index_unchecked(fn_index),
                            pack_i32_i32(a, b),
                            nullptr,
                            nullptr)
                    .results);
        };

        // rot_xor_add pending flush
        {
            ::std::uint32_t const e0_u = static_cast<::std::uint32_t>(rotl32(x, 5u) + 1u);
            UWVM2TEST_REQUIRE(run_i32_1(0uz, static_cast<::std::int32_t>(x)) == static_cast<::std::int32_t>(e0_u));

            ::std::uint32_t const e1_u = static_cast<::std::uint32_t>(rotl32(x, 7u) + y);
            UWVM2TEST_REQUIRE(run_i32_2(1uz, static_cast<::std::int32_t>(x), static_cast<::std::int32_t>(y)) == static_cast<::std::int32_t>(e1_u));

            ::std::uint32_t const v2_u = rotl32(x, 13u) ^ y;
            ::std::int32_t const e2 = (v2_u == 0u) ? 1 : 0;
            UWVM2TEST_REQUIRE(run_i32_2(2uz, static_cast<::std::int32_t>(x), static_cast<::std::int32_t>(y)) == e2);

            ::std::uint32_t const e3_u = static_cast<::std::uint32_t>((rotl32(x, 9u) ^ y) - 123u);
            UWVM2TEST_REQUIRE(run_i32_2(3uz, static_cast<::std::int32_t>(x), static_cast<::std::int32_t>(y)) == static_cast<::std::int32_t>(e3_u));

            ::std::uint32_t const v4_u = y ^ rotl32(x, 5u);
            ::std::int32_t const e4 = (v4_u == 0u) ? 1 : 0;
            UWVM2TEST_REQUIRE(run_i32_2(4uz, static_cast<::std::int32_t>(x), static_cast<::std::int32_t>(y)) == e4);
        }

        // xorshift pending flush
        {
            ::std::uint32_t const e5_u = static_cast<::std::uint32_t>(x + (x >> 7u));
            UWVM2TEST_REQUIRE(run_i32_1(5uz, static_cast<::std::int32_t>(x)) == static_cast<::std::int32_t>(e5_u));

            ::std::uint32_t const v6_u = x ^ (x >> 3u);
            ::std::int32_t const e6 = (v6_u == 0u) ? 1 : 0;
            UWVM2TEST_REQUIRE(run_i32_1(6uz, static_cast<::std::int32_t>(x)) == e6);

            ::std::uint32_t const t7 = x ^ (x >> 5u);
            ::std::uint32_t const e7_u = static_cast<::std::uint32_t>(t7 + x);
            UWVM2TEST_REQUIRE(run_i32_1(7uz, static_cast<::std::int32_t>(x)) == static_cast<::std::int32_t>(e7_u));

            ::std::uint32_t const e8_u = static_cast<::std::uint32_t>(t7 + x + 9u);
            UWVM2TEST_REQUIRE(run_i32_1(8uz, static_cast<::std::int32_t>(x)) == static_cast<::std::int32_t>(e8_u));

            ::std::uint32_t const e9_u = static_cast<::std::uint32_t>(t7 + (x << 1u));
            UWVM2TEST_REQUIRE(run_i32_1(9uz, static_cast<::std::int32_t>(x)) == static_cast<::std::int32_t>(e9_u));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_extra_heavy_i32_pending_flush_mix() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_i32_pending_flush_mix_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_i32_pending_flush_mix");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr ::std::uint32_t x = 0x12345678u;
        constexpr ::std::uint32_t y = 0x9abcdef0u;

        // Tailcall (no stacktop caching).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_i32_pending_flush_mix_suite<opt>(rt, x, y) == 0);
        }

        // Byref mode.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_i32_pending_flush_mix_suite<opt>(rt, x, y) == 0);
        }

        // Tailcall + stacktop caching (exercise stacktop_enabled paths inside flush_conbine_pending()).
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
            UWVM2TEST_REQUIRE(run_i32_pending_flush_mix_suite<opt>(rt, x, y) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_extra_heavy_i32_pending_flush_mix();
}

