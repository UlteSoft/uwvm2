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

    [[nodiscard]] byte_vec build_int_compare_more_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: i32 comparisons bit-pack: bit0=le_s, bit1=gt_u
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);  // acc

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_le_s);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_or);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_gt_u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_or);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 comparisons bit-pack:
        // bit0=eq, bit1=ne, bit2=lt_s, bit3=lt_u, bit4=gt_s, bit5=le_s, bit6=le_u, bit7=ge_s, bit8=ge_u
        {
            func_type ty{{k_val_i64, k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);  // acc

            auto emit_bit = [&](wasm_op cmp, ::std::int32_t bit)
            {
                op(c, wasm_op::local_get); u32(c, 0u);
                op(c, wasm_op::local_get); u32(c, 1u);
                op(c, cmp);
                op(c, wasm_op::i32_const); i32(c, bit);
                op(c, wasm_op::i32_shl);
                op(c, wasm_op::i32_or);
            };

            emit_bit(wasm_op::i64_eq, 0);
            emit_bit(wasm_op::i64_ne, 1);
            emit_bit(wasm_op::i64_lt_s, 2);
            emit_bit(wasm_op::i64_lt_u, 3);
            emit_bit(wasm_op::i64_gt_s, 4);
            emit_bit(wasm_op::i64_le_s, 5);
            emit_bit(wasm_op::i64_le_u, 6);
            emit_bit(wasm_op::i64_ge_s, 7);
            emit_bit(wasm_op::i64_ge_u, 8);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] constexpr ::std::uint32_t expect_i32_pack(::std::int32_t a, ::std::int32_t b) noexcept
    {
        ::std::uint32_t acc{};
        acc |= (a <= b) ? 1u : 0u;
        acc |= (static_cast<::std::uint32_t>(a) > static_cast<::std::uint32_t>(b)) ? 2u : 0u;
        return acc;
    }

    [[nodiscard]] constexpr ::std::uint32_t expect_i64_pack(::std::int64_t a, ::std::int64_t b) noexcept
    {
        ::std::uint32_t acc{};
        acc |= (a == b) ? (1u << 0) : 0u;
        acc |= (a != b) ? (1u << 1) : 0u;
        acc |= (a < b) ? (1u << 2) : 0u;
        acc |= (static_cast<::std::uint64_t>(a) < static_cast<::std::uint64_t>(b)) ? (1u << 3) : 0u;
        acc |= (a > b) ? (1u << 4) : 0u;
        acc |= (a <= b) ? (1u << 5) : 0u;
        acc |= (static_cast<::std::uint64_t>(a) <= static_cast<::std::uint64_t>(b)) ? (1u << 6) : 0u;
        acc |= (a >= b) ? (1u << 7) : 0u;
        acc |= (static_cast<::std::uint64_t>(a) >= static_cast<::std::uint64_t>(b)) ? (1u << 8) : 0u;
        return acc;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: i32 pack
        for(auto const [a, b] : ::std::array{
                ::std::pair{0, 0},
                ::std::pair{1, 2},
                ::std::pair{2, 1},
                ::std::pair{-1, 0},
                ::std::pair{static_cast<::std::int32_t>(0x80000000u), 0},
                ::std::pair{static_cast<::std::int32_t>(0xffffffffu), 1},
            })
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32x2(a, b),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == expect_i32_pack(a, b));
        }

        // f1: i64 pack
        for(auto const [a, b] : ::std::array{
                ::std::pair{0ll, 0ll},
                ::std::pair{1ll, 2ll},
                ::std::pair{2ll, 1ll},
                ::std::pair{-1ll, 0ll},
                ::std::pair{static_cast<::std::int64_t>(0x8000000000000000ull), 0ll},
                ::std::pair{static_cast<::std::int64_t>(0xffffffffffffffffull), 1ll},
            })
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i64x2(a, b),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr.results)) == expect_i64_pack(a, b));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_int_compare_more() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_int_compare_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_int_cmp_more");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_hardfloat_abi_opt<2uz, 2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_int_compare_more();
}
