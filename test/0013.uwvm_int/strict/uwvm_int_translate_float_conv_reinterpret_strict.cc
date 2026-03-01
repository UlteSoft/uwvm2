#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_float_conv_reinterpret_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: float pipeline (result i32) -> 31
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // abs(nearest(abs(-3.5))) => 4 ; +10 => 14
            op(c, wasm_op::f32_const); append_f32_ieee(c, -3.5f);
            op(c, wasm_op::f32_abs);
            op(c, wasm_op::f32_nearest);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_const); i32(c, 10);
            op(c, wasm_op::i32_add);

            // sqrt(9.0) => 3 ; add => 17
            op(c, wasm_op::f32_const); append_f32_ieee(c, 9.0f);
            op(c, wasm_op::f32_sqrt);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            // floor(6.5) => 6 ; add => 23
            op(c, wasm_op::f64_const); append_f64_ieee(c, 6.5);
            op(c, wasm_op::f64_floor);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            // max(2.0, 8.0) => 8 ; add => 31
            op(c, wasm_op::f64_const); append_f64_ieee(c, 2.0);
            op(c, wasm_op::f64_const); append_f64_ieee(c, 8.0);
            op(c, wasm_op::f64_max);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: f32 compare + select (param f32) (result i32) -> (x<0 ? 111 : 222)
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 111);
            op(c, wasm_op::i32_const); i32(c, 222);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_const); append_f32_ieee(c, 0.0f);
            op(c, wasm_op::f32_lt);
            op(c, wasm_op::select);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: reinterpret i32 -> f32 -> i32 (param i32) (result i32) bit-identity
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_reinterpret_i32);
            op(c, wasm_op::i32_reinterpret_f32);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: reinterpret i64 -> f64 -> i64 (param i64) (result i64) bit-identity
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_reinterpret_i64);
            op(c, wasm_op::i64_reinterpret_f64);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: demote/promote roundtrip (result i32) -> 1234
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f64_const); append_f64_ieee(c, 1234.25);
            op(c, wasm_op::f32_demote_f64);
            op(c, wasm_op::f64_promote_f32);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i32 -> f32 -> i64 (param i32) (result i64) -> (i64)(float(a)*2)
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_convert_i32_s);
            op(c, wasm_op::f32_const); append_f32_ieee(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::i64_trunc_f32_s);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_float_conv_reinterpret() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_float_conv_reinterpret_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_float_conv");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 31);

            auto rr1a = Runner::run(cm.local_funcs.index_unchecked(1),
                                    rt.local_defined_function_vec_storage.index_unchecked(1),
                                    pack_f32(-1.0f),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1a.results) == 111);

            auto rr1b = Runner::run(cm.local_funcs.index_unchecked(1),
                                    rt.local_defined_function_vec_storage.index_unchecked(1),
                                    pack_f32(1.0f),
                                    nullptr,
                                    nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1b.results) == 222);

            for(::std::uint32_t bits : {0u, 0x3f800000u, 0x7fc00000u, 0xffffffffu})
            {
                auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                                       rt.local_defined_function_vec_storage.index_unchecked(2),
                                       pack_i32(static_cast<::std::int32_t>(bits)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr2.results)) == bits);
            }

            for(::std::uint64_t bits : {0ull, 0x3ff0000000000000ull, 0x7ff8000000000000ull, 0xffffffffffffffffull})
            {
                auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                                       rt.local_defined_function_vec_storage.index_unchecked(3),
                                       pack_i64(static_cast<::std::int64_t>(bits)),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr3.results)) == bits);
            }

            auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                                   rt.local_defined_function_vec_storage.index_unchecked(4),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr4.results) == 1234);

            for(::std::int32_t a : {0, 1, -1, 7, -7, 1234})
            {
                auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                                       rt.local_defined_function_vec_storage.index_unchecked(5),
                                       pack_i32(a),
                                       nullptr,
                                       nullptr);
                UWVM2TEST_REQUIRE(load_i64(rr5.results) == static_cast<::std::int64_t>(a) * 2ll);
            }
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 31);
        }

        // Mode C: tailcall + stacktop caching (merged rings)
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

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 31);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_float_conv_reinterpret();
}
