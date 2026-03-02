#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_value_types_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: nested br carries i64, with extra junk on stack to force unwind.
        // (result i64) -> 0x0102030405060708 + 1
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i64);
            op(c, wasm_op::block);
            append_u8(c, k_val_i64);

            op(c, wasm_op::i32_const);
            i32(c, 0x12345678);
            op(c, wasm_op::i64_const);
            i64(c, 0x0102030405060708ll);
            op(c, wasm_op::br);
            u32(c, 1u);

            op(c, wasm_op::i64_const);
            i64(c, 0);  // unreachable
            op(c, wasm_op::end);  // end inner
            // Even though the inner block never completes normally, the type system still treats it as producing an i64.
            // Keep the outer fallthrough path stack-correct.
            op(c, wasm_op::i64_const);
            i64(c, 0);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);  // end outer
            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: nested br carries f32, with extra junk on stack to force unwind.
        // (result f32) -> 3.5 + 2.0
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_f32);
            op(c, wasm_op::block);
            append_u8(c, k_val_f32);

            op(c, wasm_op::i32_const);
            i32(c, 0x23456789);
            op(c, wasm_op::f32_const);
            f32(c, 3.5f);
            op(c, wasm_op::br);
            u32(c, 1u);

            op(c, wasm_op::f32_const);
            f32(c, 0.0f);  // unreachable
            op(c, wasm_op::end);  // end inner
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);  // end outer
            op(c, wasm_op::f32_const);
            f32(c, 2.0f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: nested br carries f64, with extra junk on stack to force unwind.
        // (result f64) -> 9.0 + 1.0
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_f64);
            op(c, wasm_op::block);
            append_u8(c, k_val_f64);

            op(c, wasm_op::i32_const);
            i32(c, 0x3456789a);
            op(c, wasm_op::f64_const);
            f64(c, 9.0);
            op(c, wasm_op::br);
            u32(c, 1u);

            op(c, wasm_op::f64_const);
            f64(c, 0.0);  // unreachable
            op(c, wasm_op::end);  // end inner
            op(c, wasm_op::f64_const);
            f64(c, 0.0);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);  // end outer
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_br_value_types_suite(runtime_module_t const& rt) noexcept
    {
        if constexpr(Opt.is_tail_call) { static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>()); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == (0x0102030405060708ll + 1ll));

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 5.5f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 10.0);

        return 0;
    }

    [[nodiscard]] int test_translate_br_value_types() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_value_types_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_value_types");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_br_value_types_suite<opt>(rt) == 0);
        }

        // tailcall
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_br_value_types_suite<opt>(rt) == 0);
        }

        // tailcall + stacktop caching (int/float merged rings)
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
            UWVM2TEST_REQUIRE(run_br_value_types_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_value_types();
}
