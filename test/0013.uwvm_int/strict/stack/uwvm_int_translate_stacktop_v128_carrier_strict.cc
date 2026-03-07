#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_stacktop_v128_carrier_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (param f32) -> (result f32)
        // (x * 1.5) + 2.25
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.5f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 2.25f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param f64) -> (result f64)
        // (x + 2.0) * 3.0
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 3.0);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (result f32)
        // demote_f64(promote_f32(1.25) + 2.0) == 3.25f
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.25f);
            op(c, wasm_op::f64_promote_f32);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::f32_demote_f64);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_stacktop_v128_carrier_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: (x * 1.5) + 2.25
        {
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_f32(0.0f),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 2.25f);

            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_f32(1.0f),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 3.75f);

            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_f32(-2.0f),
                                                  nullptr,
                                                  nullptr)
                                           .results) == -0.75f);
        }

        // f1: (x + 2.0) * 3.0
        {
            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_f64(1.0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.0);

            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_f64(-2.0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 0.0);

            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_f64(0.5),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 7.5);
        }

        // f2: 3.25f
        {
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  byte_vec{},
                                                  nullptr,
                                                  nullptr)
                                           .results) == 3.25f);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_stacktop_v128_carrier() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_v128_carrier_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_v128_carrier");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_stacktop_v128_carrier_suite<opt>(rt) == 0);
        }

        // tailcall (no cache)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_stacktop_v128_carrier_suite<opt>(rt) == 0);
        }

        // tailcall + stacktop caching: f32/f64/v128 fully merged float ring (v128 used as the carrier).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 4uz,
                .i64_stack_top_begin_pos = 4uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 6uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 6uz,
                .v128_stack_top_begin_pos = 5uz,
                .v128_stack_top_end_pos = 6uz,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_stacktop_v128_carrier_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_v128_carrier();
}
