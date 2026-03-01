#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_float_numeric_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: f32 ops pipeline (result i32) -> 33
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);

            // abs(-3.0) => 3
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, -3.0f);
            op(c, wasm_op::f32_abs);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // nearest(2.5) => 2 (ties-to-even)
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 2.5f);
            op(c, wasm_op::f32_nearest);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // ceil(1.25) => 2
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.25f);
            op(c, wasm_op::f32_ceil);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // floor(1.25) => 1
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.25f);
            op(c, wasm_op::f32_floor);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // trunc(1.25) => 1
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.25f);
            op(c, wasm_op::f32_trunc);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // sqrt(9.0) => 3
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 9.0f);
            op(c, wasm_op::f32_sqrt);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // div(6.0/2.0) => 3
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 6.0f);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 2.0f);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // min(4.0, 5.0) => 4
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 4.0f);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 5.0f);
            op(c, wasm_op::f32_min);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // max(4.0, 5.0) => 5
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 4.0f);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 5.0f);
            op(c, wasm_op::f32_max);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // copysign(3.0, -1.0) => -3
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 3.0f);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, -1.0f);
            op(c, wasm_op::f32_copysign);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // add(1.0 + 2.0) => 3
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.0f);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 2.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // sub(5.0 - 2.0) => 3
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 5.0f);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 2.0f);
            op(c, wasm_op::f32_sub);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // mul(1.5 * 2.0) => 3
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.5f);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // abs(neg(3.0)) => 3 (covers f32.neg)
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 3.0f);
            op(c, wasm_op::f32_neg);
            op(c, wasm_op::f32_abs);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: f32 comparisons vs 0.0 weighted sum.
        // (param f32) (result i32)
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // sum = eq*1 + ne*2 + lt*4 + le*8 + gt*16 + ge*32
            op(c, wasm_op::i32_const);
            i32(c, 0);

            auto add_cmp = [&](wasm_op cmp, ::std::int32_t w)
            {
                op(c, wasm_op::local_get);
                u32(c, 0u);
                op(c, wasm_op::f32_const);
                append_f32_ieee(c, 0.0f);
                op(c, cmp);
                op(c, wasm_op::i32_const);
                i32(c, w);
                op(c, wasm_op::i32_mul);
                op(c, wasm_op::i32_add);
            };

            add_cmp(wasm_op::f32_eq, 1);
            add_cmp(wasm_op::f32_ne, 2);
            add_cmp(wasm_op::f32_lt, 4);
            add_cmp(wasm_op::f32_le, 8);
            add_cmp(wasm_op::f32_gt, 16);
            add_cmp(wasm_op::f32_ge, 32);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f64 ops pipeline (result i32) -> 36
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);

            // abs(-3.0) => 3
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, -3.0);
            op(c, wasm_op::f64_abs);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // nearest(2.5) => 2
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.5);
            op(c, wasm_op::f64_nearest);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // ceil(1.25) => 2
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 1.25);
            op(c, wasm_op::f64_ceil);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // floor(1.25) => 1
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 1.25);
            op(c, wasm_op::f64_floor);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // trunc(1.25) => 1
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 1.25);
            op(c, wasm_op::f64_trunc);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // sqrt(16.0) => 4
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 16.0);
            op(c, wasm_op::f64_sqrt);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // div(10.0/2.0) => 5
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 10.0);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.0);
            op(c, wasm_op::f64_div);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // min(4.0, 5.0) => 4
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 4.0);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 5.0);
            op(c, wasm_op::f64_min);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // max(4.0, 5.0) => 5
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 4.0);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 5.0);
            op(c, wasm_op::f64_max);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // copysign(3.0, -1.0) => -3
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 3.0);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, -1.0);
            op(c, wasm_op::f64_copysign);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // add(1.0 + 2.0) => 3
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 1.0);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // sub(5.0 - 2.0) => 3
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 5.0);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.0);
            op(c, wasm_op::f64_sub);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // mul(1.5 * 2.0) => 3
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 1.5);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.0);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // abs(neg(3.0)) => 3 (covers f64.neg)
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 3.0);
            op(c, wasm_op::f64_neg);
            op(c, wasm_op::f64_abs);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f64 comparisons vs 0.0 weighted sum.
        // (param f64) (result i32)
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 0);

            auto add_cmp = [&](wasm_op cmp, ::std::int32_t w)
            {
                op(c, wasm_op::local_get);
                u32(c, 0u);
                op(c, wasm_op::f64_const);
                append_f64_ieee(c, 0.0);
                op(c, cmp);
                op(c, wasm_op::i32_const);
                i32(c, w);
                op(c, wasm_op::i32_mul);
                op(c, wasm_op::i32_add);
            };

            add_cmp(wasm_op::f64_eq, 1);
            add_cmp(wasm_op::f64_ne, 2);
            add_cmp(wasm_op::f64_lt, 4);
            add_cmp(wasm_op::f64_le, 8);
            add_cmp(wasm_op::f64_gt, 16);
            add_cmp(wasm_op::f64_ge, 32);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_float_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: f32 pipeline
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 33);

        // f1: f32 cmp weights
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32(-1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 14);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32(0.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 41);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32(1.0f),
                                              nullptr,
                                              nullptr)
                                       .results) == 50);

        // f2: f64 pipeline
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 36);

        // f3: f64 cmp weights
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f64(-1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 14);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f64(0.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 41);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f64(1.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 50);

        return 0;
    }

    [[nodiscard]] int test_translate_float_numeric() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_float_numeric_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_float_numeric");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_float_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_float_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching (separate int/float rings).
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
            UWVM2TEST_REQUIRE(run_float_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_float_numeric();
}

