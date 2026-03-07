#include "../uwvm_int_translate_strict_common.h"

#include <cstdint>

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_conbine_heavy_unary_localget_bitops_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (i32)->i32: local.get 0; i32.popcnt
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_popcnt);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (i32)->i32: local.get 0; i32.clz
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_clz);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (i32)->i32: local.get 0; i32.ctz
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_ctz);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (i64)->i64: local.get 0; i64.popcnt
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i64_popcnt);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: (i64)->i64: local.get 0; i64.clz
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i64_clz);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: (i64)->i64: local.get 0; i64.ctz
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i64_ctz);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] constexpr ::std::int32_t popcnt32(::std::uint32_t x) noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<::std::int32_t>(__builtin_popcount(x));
#else
        // fallback: simple loop (should never be used on supported toolchains)
        ::std::uint32_t c{};
        while(x) { c += (x & 1u); x >>= 1u; }
        return static_cast<::std::int32_t>(c);
#endif
    }

    [[nodiscard]] constexpr ::std::int32_t clz32(::std::uint32_t x) noexcept
    {
        if(x == 0u) { return 32; }
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<::std::int32_t>(__builtin_clz(x));
#else
        ::std::int32_t n{};
        for(int i = 31; i >= 0; --i)
        {
            if((x >> i) & 1u) { break; }
            ++n;
        }
        return n;
#endif
    }

    [[nodiscard]] constexpr ::std::int32_t ctz32(::std::uint32_t x) noexcept
    {
        if(x == 0u) { return 32; }
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<::std::int32_t>(__builtin_ctz(x));
#else
        ::std::int32_t n{};
        for(int i = 0; i < 32; ++i)
        {
            if((x >> i) & 1u) { break; }
            ++n;
        }
        return n;
#endif
    }

    [[nodiscard]] constexpr ::std::int64_t popcnt64(::std::uint64_t x) noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<::std::int64_t>(__builtin_popcountll(x));
#else
        ::std::uint64_t c{};
        while(x) { c += (x & 1u); x >>= 1u; }
        return static_cast<::std::int64_t>(c);
#endif
    }

    [[nodiscard]] constexpr ::std::int64_t clz64(::std::uint64_t x) noexcept
    {
        if(x == 0ull) { return 64; }
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<::std::int64_t>(__builtin_clzll(x));
#else
        ::std::int64_t n{};
        for(int i = 63; i >= 0; --i)
        {
            if((x >> i) & 1ull) { break; }
            ++n;
        }
        return n;
#endif
    }

    [[nodiscard]] constexpr ::std::int64_t ctz64(::std::uint64_t x) noexcept
    {
        if(x == 0ull) { return 64; }
#if defined(__GNUC__) || defined(__clang__)
        return static_cast<::std::int64_t>(__builtin_ctzll(x));
#else
        ::std::int64_t n{};
        for(int i = 0; i < 64; ++i)
        {
            if((x >> i) & 1ull) { break; }
            ++n;
        }
        return n;
#endif
    }

    [[nodiscard]] int test_translate_conbine_heavy_unary_localget_bitops() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_heavy_unary_localget_bitops_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_heavy_unary_localget_bitops");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Combine state machine only exists in tailcall mode.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
            UWVM2TEST_REQUIRE(cm.local_funcs.size() == 6uz);

            using Runner = interpreter_runner<opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            [[maybe_unused]] constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            [[maybe_unused]] constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands,
                                                    optable::translate::get_uwvmint_i32_popcnt_localget_fptr_from_tuple<opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands,
                                                    optable::translate::get_uwvmint_i32_clz_localget_fptr_from_tuple<opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands,
                                                    optable::translate::get_uwvmint_i32_ctz_localget_fptr_from_tuple<opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands,
                                                    optable::translate::get_uwvmint_i64_popcnt_localget_fptr_from_tuple<opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands,
                                                    optable::translate::get_uwvmint_i64_clz_localget_fptr_from_tuple<opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands,
                                                    optable::translate::get_uwvmint_i64_ctz_localget_fptr_from_tuple<opt>(curr, tuple)));
#endif

            for(::std::int32_t v : {0, 1, 2, 3, 7, 8, -1, static_cast<::std::int32_t>(0x80000000u), 0x12345678})
            {
                ::std::uint32_t const u = static_cast<::std::uint32_t>(v);

                UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                                      pack_i32(v),
                                                      nullptr,
                                                      nullptr)
                                               .results) == popcnt32(u));
                UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                      rt.local_defined_function_vec_storage.index_unchecked(1),
                                                      pack_i32(v),
                                                      nullptr,
                                                      nullptr)
                                               .results) == clz32(u));
                UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                      rt.local_defined_function_vec_storage.index_unchecked(2),
                                                      pack_i32(v),
                                                      nullptr,
                                                      nullptr)
                                               .results) == ctz32(u));
            }

            for(::std::int64_t v : {0ll, 1ll, 2ll, 3ll, 7ll, 8ll, -1ll, static_cast<::std::int64_t>(0x8000000000000000ull), 0x0123456789abcdefll})
            {
                ::std::uint64_t const u = static_cast<::std::uint64_t>(v);

                UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                                      rt.local_defined_function_vec_storage.index_unchecked(3),
                                                      pack_i64(v),
                                                      nullptr,
                                                      nullptr)
                                               .results) == popcnt64(u));
                UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                                      rt.local_defined_function_vec_storage.index_unchecked(4),
                                                      pack_i64(v),
                                                      nullptr,
                                                      nullptr)
                                               .results) == clz64(u));
                UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(5),
                                                      rt.local_defined_function_vec_storage.index_unchecked(5),
                                                      pack_i64(v),
                                                      nullptr,
                                                      nullptr)
                                               .results) == ctz64(u));
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_conbine_heavy_unary_localget_bitops();
}

