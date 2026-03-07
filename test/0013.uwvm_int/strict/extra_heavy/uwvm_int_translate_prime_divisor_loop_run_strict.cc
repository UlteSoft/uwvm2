#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_prime_divisor_loop_run_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_case = [&](::std::int32_t n, double sqrt, ::std::int32_t step) -> void
        {
            // (result i32) (local i32 n, f64 sqrt, i32 i)
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local0: n
            fb.locals.push_back({1u, k_val_f64});  // local1: sqrt
            fb.locals.push_back({1u, k_val_i32});  // local2: i
            auto& c = fb.code;

            // n = const
            op(c, wasm_op::i32_const);
            i32(c, n);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // sqrt = const
            op(c, wasm_op::f64_const);
            f64(c, sqrt);
            op(c, wasm_op::local_set);
            u32(c, 1u);

            // i = 3 (avoid div-by-zero, match odd-divisor loop style)
            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::local_set);
            u32(c, 2u);

            // block/loop
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);
            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // Canonical extra-heavy divisor loop (for prime_divisor_loop_run mega-fuse):
            // br_if (n % i == 0) -> break out of the block
            op(c, wasm_op::local_get);
            u32(c, 0u);  // n
            op(c, wasm_op::local_get);
            u32(c, 2u);  // i
            op(c, wasm_op::i32_rem_u);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 1u);  // break (block label)

            // i += step; if !(sqrt < f64(i)) continue loop
            op(c, wasm_op::local_get);
            u32(c, 1u);  // sqrt
            op(c, wasm_op::local_get);
            u32(c, 2u);  // i
            op(c, wasm_op::i32_const);
            i32(c, step);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 2u);  // i = i + step
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::f64_lt);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);  // continue (loop label)

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            // return i
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0: composite: breaks on i=3
        add_case(21, 5.0, 2);
        // f1: prime-ish: ends on i=7 (>sqrt=5)
        add_case(29, 5.0, 2);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto exp_prime =
                optable::translate::get_uwvmint_prime_divisor_loop_run_fptr_from_tuple<Opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_prime));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_prime));
        }
#endif

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == 3);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr1.results) == 7);

        return 0;
    }

    [[nodiscard]] int test_translate_prime_divisor_loop_run() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        // No calls expected in this test.
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_prime_divisor_loop_run_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_prime_divisor_loop_run");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: extra-heavy mega fuse lives here when enabled.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Byref mode: semantics smoke (combine state machine is intentionally disabled).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_prime_divisor_loop_run();
}

