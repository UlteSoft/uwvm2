#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr ::std::uint32_t k_loop_end = 97u;
    inline constexpr ::std::uint64_t k_loop_mul = 6364136223846793005ull;

    [[nodiscard]] byte_vec build_loop_i64_loop_run_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: matches the extra-heavy `loop_i64` mega-fusion pattern (when enabled).
        // Returns: x ^ i64.extend_i32_u(i)  (i should equal k_loop_end at exit)
        {
            constexpr ::std::uint32_t k_i = 0u;
            constexpr ::std::uint32_t k_x = 1u;

            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0 i
            fb.locals.push_back({1u, k_val_i64});  // local1 x
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, k_i);

            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::i32_const);
            i32(c, static_cast<::std::int32_t>(k_loop_end));
            op(c, wasm_op::i32_ge_u);
            op(c, wasm_op::br_if);
            u32(c, 1u);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::i64_const);
            i64(c, static_cast<::std::int64_t>(k_loop_mul));
            op(c, wasm_op::i64_mul);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::i64_xor);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::i64_const);
            i64(c, 17);
            op(c, wasm_op::i64_rotr);
            op(c, wasm_op::local_set);
            u32(c, k_x);

            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set);
            u32(c, k_i);

            op(c, wasm_op::br);
            u32(c, 0u);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            op(c, wasm_op::local_get);
            u32(c, k_x);
            op(c, wasm_op::local_get);
            u32(c, k_i);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::i64_xor);

            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] constexpr ::std::uint64_t expected_f0_u64() noexcept
    {
        ::std::uint32_t i = 0u;
        ::std::uint64_t x = 1u;

        while(i < k_loop_end)
        {
            x = static_cast<::std::uint64_t>(x + 1u);
            x = static_cast<::std::uint64_t>(x * k_loop_mul);
            x ^= static_cast<::std::uint64_t>(i);
            x = ::std::rotr(x, 17);
            i = static_cast<::std::uint32_t>(i + 1u);
        }

        return x ^ static_cast<::std::uint64_t>(i);
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_loop_i64_loop_run_suite(runtime_module_t const& rt, ::std::uint64_t expected) noexcept
    {
        if constexpr(Opt.is_tail_call) { static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>()); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                                                            pack_no_params(),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == expected);
        return 0;
    }

    [[nodiscard]] int test_translate_loop_i64_loop_run() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_loop_i64_loop_run_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_loop_i64_loop_run");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::std::uint64_t const expected = expected_f0_u64();

        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_loop_run =
                optable::translate::get_uwvmint_loop_i64_loop_run_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_loop_run));
#endif

            using Runner = interpreter_runner<opt>;
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                                                                rt.local_defined_function_vec_storage.index_unchecked(0),
                                                                                pack_no_params(),
                                                                                nullptr,
                                                                                nullptr)
                                                                     .results)) == expected);
        }

        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_loop_i64_loop_run_suite<opt>(rt, expected) == 0);
        }

        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_loop_i64_loop_run_suite<opt>(rt, expected) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_loop_i64_loop_run();
}
