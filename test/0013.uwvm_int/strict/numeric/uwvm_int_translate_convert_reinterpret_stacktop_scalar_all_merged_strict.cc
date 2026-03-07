#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_convert_reinterpret_stacktop_scalar_all_merged_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // Goals:
        // - Cover i32->f32/f64 convert paths under both:
        //   - tailcall conbine local.get fusion (for f32.convert_i32_{s,u})
        //   - fully merged scalar stacktop rings (codegen_stack_set_top branches)
        // - Cover i64<->f64 reinterpret under fully merged scalar rings.

        // f0: (param i32) -> (i32+1) via f32.convert_i32_s
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_convert_i32_s);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i32) -> (i32+1) via f32.convert_i32_u (use only non-negative inputs)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (param i32) -> (i32+1) via f64.convert_i32_s
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_convert_i32_s);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (param i32) -> (i32+1) via f64.convert_i32_u (use only non-negative inputs)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: (param i64) -> (i64*2) via f32.convert_i64_s
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_convert_i64_s);
            op(c, wasm_op::f32_const);
            f32(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::i64_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: (param i64) -> (i64*2) via f32.convert_i64_u (use only non-negative inputs)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_convert_i64_u);
            op(c, wasm_op::f32_const);
            f32(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::i64_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: (param i64) -> (i64*2) via f64.convert_i64_s
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_convert_i64_s);
            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::i64_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: (param i64) -> (i64*2) via f64.convert_i64_u (use only non-negative inputs)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_convert_i64_u);
            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::i64_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: const i32 -> f32.convert_i32_s path (avoid local.get fusion; cover merged scalar ring codegen_stack_set_top)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::f32_convert_i32_s);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f9: const i32 -> f32.convert_i32_u path (avoid local.get fusion; cover merged scalar ring codegen_stack_set_top)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::f32_convert_i32_u);
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f10: const i32 -> f64.convert_i32_s path (also used for merged scalar ring coverage; expected -6)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, -7);
            op(c, wasm_op::f64_convert_i32_s);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f11: const i32 -> f64.convert_i32_u path (expected 8)
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f12: reinterpret i64 -> f64 -> i64 (param i64) bit-identity (covers merged scalar ring retype paths for i64<->f64)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_reinterpret_i64);
            op(c, wasm_op::i64_reinterpret_f64);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_convert_reinterpret_stacktop_scalar_all_merged_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        auto run_i32 = [&](::std::size_t idx, byte_vec const& params) noexcept -> ::std::int32_t
        {
            return load_i32(Runner::run(cm.local_funcs.index_unchecked(idx),
                                        rt.local_defined_function_vec_storage.index_unchecked(idx),
                                        params,
                                        nullptr,
                                        nullptr)
                                .results);
        };

        auto run_i64 = [&](::std::size_t idx, byte_vec const& params) noexcept -> ::std::int64_t
        {
            return load_i64(Runner::run(cm.local_funcs.index_unchecked(idx),
                                        rt.local_defined_function_vec_storage.index_unchecked(idx),
                                        params,
                                        nullptr,
                                        nullptr)
                                .results);
        };

        // f0: f32.convert_i32_s
        for(::std::int32_t a : {-7, 0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i32(0uz, pack_i32(a)) == a + 1);
        }

        // f1: f32.convert_i32_u (avoid negative inputs)
        for(::std::int32_t a : {0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i32(1uz, pack_i32(a)) == a + 1);
        }

        // f2: f64.convert_i32_s
        for(::std::int32_t a : {-7, 0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i32(2uz, pack_i32(a)) == a + 1);
        }

        // f3: f64.convert_i32_u (avoid negative inputs)
        for(::std::int32_t a : {0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i32(3uz, pack_i32(a)) == a + 1);
        }

        // f4: f32.convert_i64_s
        for(::std::int64_t a : {-7, 0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i64(4uz, pack_i64(a)) == a * 2);
        }

        // f5: f32.convert_i64_u (avoid negative inputs)
        for(::std::int64_t a : {0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i64(5uz, pack_i64(a)) == a * 2);
        }

        // f6: f64.convert_i64_s
        for(::std::int64_t a : {-7, 0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i64(6uz, pack_i64(a)) == a * 2);
        }

        // f7: f64.convert_i64_u (avoid negative inputs)
        for(::std::int64_t a : {0, 1, 7, 1234})
        {
            UWVM2TEST_REQUIRE(run_i64(7uz, pack_i64(a)) == a * 2);
        }

        // f8-f11: const-based smoke (used to force non-fused convert paths)
        UWVM2TEST_REQUIRE(run_i32(8uz, pack_no_params()) == 8);
        UWVM2TEST_REQUIRE(run_i32(9uz, pack_no_params()) == 8);
        UWVM2TEST_REQUIRE(run_i32(10uz, pack_no_params()) == -6);
        UWVM2TEST_REQUIRE(run_i32(11uz, pack_no_params()) == 8);

        // f12: i64<->f64 reinterpret bit-identity
        for(::std::uint64_t bits : {0ull, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0xffffffffffffffffull})
        {
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(run_i64(12uz, pack_i64(static_cast<::std::int64_t>(bits)))) == bits);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_convert_reinterpret_stacktop_scalar_all_merged() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_convert_reinterpret_stacktop_scalar_all_merged_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_convert_reinterpret_stacktop_scalar_all_merged");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref: minimal conbine window (no local.get->convert fusion for i32->f32) but should still execute correctly.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_convert_reinterpret_stacktop_scalar_all_merged_suite<opt>(rt) == 0);
        }

        // Tailcall: enables the full conbine state machine (covers local.get->f32.convert_i32_{s,u} fused emission).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_convert_reinterpret_stacktop_scalar_all_merged_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: fully merged scalar ring (i32/i64/f32/f64).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_convert_reinterpret_stacktop_scalar_all_merged_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_convert_reinterpret_stacktop_scalar_all_merged();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
