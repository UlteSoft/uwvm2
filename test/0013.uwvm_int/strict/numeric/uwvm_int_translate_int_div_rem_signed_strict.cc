#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i64x2(::std::int64_t a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_int_div_rem_signed_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i32.div_s
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_div_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32.rem_s
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_rem_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i64.div_s
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_div_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i64.rem_s
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_rem_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i32.shr_s (tests shift-count masking)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_shr_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i64.shl (tests shift-count masking)
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_shl);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: i64.shr_s
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_shr_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: i64.shr_u
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_shr_u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: i64.rotr
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_rotr);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: i64.sub
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_sub);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] constexpr ::std::uint64_t rotr64(::std::uint64_t x, unsigned r) noexcept
    {
        r &= 63u;
        return (x >> r) | (x << ((64u - r) & 63u));
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0/f1: i32 div/rem signed.
        for(auto const [a, b] : ::std::array{
                ::std::pair{7, 3},
                ::std::pair{-7, 3},
                ::std::pair{7, -3},
                ::std::pair{-7, -3},
                ::std::pair{static_cast<::std::int32_t>(0x80000000u), 2},
                ::std::pair{123456, -7},
            })
        {
            // Avoid overflow trap: INT_MIN / -1 is invalid in Wasm.
            UWVM2TEST_REQUIRE(!(a == static_cast<::std::int32_t>(0x80000000u) && b == -1));
            UWVM2TEST_REQUIRE(b != 0);

            auto rr_div = Runner::run(cm.local_funcs.index_unchecked(0),
                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                      pack_i32x2(a, b),
                                      nullptr,
                                      nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr_div.results) == (a / b));

            auto rr_rem = Runner::run(cm.local_funcs.index_unchecked(1),
                                      rt.local_defined_function_vec_storage.index_unchecked(1),
                                      pack_i32x2(a, b),
                                      nullptr,
                                      nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr_rem.results) == (a % b));
        }

        // f2/f3: i64 div/rem signed.
        for(auto const [a, b] : ::std::array{
                ::std::pair{7ll, 3ll},
                ::std::pair{-7ll, 3ll},
                ::std::pair{7ll, -3ll},
                ::std::pair{-7ll, -3ll},
                ::std::pair{static_cast<::std::int64_t>(0x8000000000000000ull), 2ll},
                ::std::pair{0x123456789abcdefll, -7ll},
            })
        {
            UWVM2TEST_REQUIRE(!(a == static_cast<::std::int64_t>(0x8000000000000000ull) && b == -1ll));
            UWVM2TEST_REQUIRE(b != 0);

            auto rr_div = Runner::run(cm.local_funcs.index_unchecked(2),
                                      rt.local_defined_function_vec_storage.index_unchecked(2),
                                      pack_i64x2(a, b),
                                      nullptr,
                                      nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr_div.results) == (a / b));

            auto rr_rem = Runner::run(cm.local_funcs.index_unchecked(3),
                                      rt.local_defined_function_vec_storage.index_unchecked(3),
                                      pack_i64x2(a, b),
                                      nullptr,
                                      nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr_rem.results) == (a % b));
        }

        // f4: i32.shr_s masking (shift count mod 32).
        for(auto const [x, s] : ::std::array{
                ::std::pair{static_cast<::std::int32_t>(-8), 1},
                ::std::pair{static_cast<::std::int32_t>(-8), 33},  // 33 -> 1
                ::std::pair{static_cast<::std::int32_t>(0x80000000u), 40},  // 40 -> 8
                ::std::pair{static_cast<::std::int32_t>(0x7fffffffu), 35},  // 35 -> 3
            })
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  pack_i32x2(x, s),
                                  nullptr,
                                  nullptr);
            auto const sh = static_cast<unsigned>(s) & 31u;
            UWVM2TEST_REQUIRE(load_i32(rr.results) == (x >> sh));
        }

        // f5-f8: i64 shifts/rotates masking (mod 64); f9: sub.
        for(auto const [x, s] : ::std::array{
                ::std::pair{0x0123456789abcdefll, 0ll},
                ::std::pair{0x0123456789abcdefll, 4ll},
                ::std::pair{static_cast<::std::int64_t>(0x8000000000000000ull), 1ll},
                ::std::pair{static_cast<::std::int64_t>(-1ll), 63ll},
                ::std::pair{static_cast<::std::int64_t>(-1ll), 65ll},  // 65 -> 1
            })
        {
            auto const sh = static_cast<unsigned long long>(s) & 63ull;

            auto rr_shl = Runner::run(cm.local_funcs.index_unchecked(5),
                                      rt.local_defined_function_vec_storage.index_unchecked(5),
                                      pack_i64x2(x, s),
                                      nullptr,
                                      nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr_shl.results)) ==
                              (static_cast<::std::uint64_t>(x) << sh));

            auto rr_shrs = Runner::run(cm.local_funcs.index_unchecked(6),
                                       rt.local_defined_function_vec_storage.index_unchecked(6),
                                       pack_i64x2(x, s),
                                       nullptr,
                                       nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr_shrs.results) == (x >> sh));

            auto rr_shru = Runner::run(cm.local_funcs.index_unchecked(7),
                                       rt.local_defined_function_vec_storage.index_unchecked(7),
                                       pack_i64x2(x, s),
                                       nullptr,
                                       nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr_shru.results)) ==
                              (static_cast<::std::uint64_t>(x) >> sh));

            auto rr_rotr = Runner::run(cm.local_funcs.index_unchecked(8),
                                       rt.local_defined_function_vec_storage.index_unchecked(8),
                                       pack_i64x2(x, s),
                                       nullptr,
                                       nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr_rotr.results)) ==
                              rotr64(static_cast<::std::uint64_t>(x), static_cast<unsigned>(sh)));

            auto rr_sub = Runner::run(cm.local_funcs.index_unchecked(9),
                                      rt.local_defined_function_vec_storage.index_unchecked(9),
                                      pack_i64x2(x, 7ll),
                                      nullptr,
                                      nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr_sub.results)) ==
                              (static_cast<::std::uint64_t>(x) - 7ull));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_int_div_rem_signed() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_int_div_rem_signed_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_int_div_rem_signed");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching (merged rings) smoke.
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
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_int_div_rem_signed();
}
