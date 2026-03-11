#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i64_i32(::std::int64_t x, ::std::int32_t y)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(x), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(y), 4);
        return out;
    }

    [[nodiscard]] constexpr ::std::uint64_t xor_extend_i32(::std::uint64_t x, ::std::int32_t y, bool sign_extend) noexcept
    {
        if(sign_extend)
        {
            return x ^ static_cast<::std::uint64_t>(static_cast<::std::int64_t>(y));
        }
        return x ^ static_cast<::std::uint64_t>(static_cast<::std::uint32_t>(y));
    }

    [[nodiscard]] byte_vec build_conbine_extend_xor_local_i64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: x = x ^ i64.extend_i32_s(y) ; return x
        {
            func_type ty{{k_val_i64, k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: return (local.tee x = x ^ i64.extend_i32_s(y))
        {
            func_type ty{{k_val_i64, k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: x = x ^ i64.extend_i32_u(y) ; return x
        {
            func_type ty{{k_val_i64, k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: return (local.tee x = x ^ i64.extend_i32_u(y))
        {
            func_type ty{{k_val_i64, k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::local_tee); u32(c, 0u);
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

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
            constexpr auto curr{make_entry_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_s_set = optable::translate::get_uwvmint_i64_extend_i32_s_xor_local_set_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_s_tee = optable::translate::get_uwvmint_i64_extend_i32_s_xor_local_tee_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_u_set = optable::translate::get_uwvmint_i64_extend_i32_u_xor_local_set_fptr_from_tuple<Opt>(curr, tuple);
            constexpr auto exp_u_tee = optable::translate::get_uwvmint_i64_extend_i32_u_xor_local_tee_fptr_from_tuple<Opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_s_set));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_s_tee));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_u_set));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_u_tee));
#endif

        constexpr ::std::uint64_t xs[]{
            0ull,
            1ull,
            0x0123456789abcdefull,
            0xffffffffffffffffull,
        };
        constexpr ::std::int32_t ys[]{
            0,
            1,
            -1,
            static_cast<::std::int32_t>(0x81234567u),
        };

        for(auto const x: xs)
        {
            for(auto const y: ys)
            {
                auto const in = pack_i64_i32(static_cast<::std::int64_t>(x), y);

                auto const r0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                            in,
                                            nullptr,
                                            nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(r0.results)) == xor_extend_i32(x, y, true));

                auto const r1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                            rt.local_defined_function_vec_storage.index_unchecked(1),
                                            in,
                                            nullptr,
                                            nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(r1.results)) == xor_extend_i32(x, y, true));

                auto const r2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                            rt.local_defined_function_vec_storage.index_unchecked(2),
                                            in,
                                            nullptr,
                                            nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(r2.results)) == xor_extend_i32(x, y, false));

                auto const r3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                            rt.local_defined_function_vec_storage.index_unchecked(3),
                                            in,
                                            nullptr,
                                            nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(r3.results)) == xor_extend_i32(x, y, false));
            }
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_extend_xor_local_i64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_extend_xor_local_i64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_extend_xor_local_i64");
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
    return test_translate_conbine_extend_xor_local_i64();
}
