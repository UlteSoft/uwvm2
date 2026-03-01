#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_abi_reg_ring_matrix_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: mixed-typed operand stack stress.
        // ((10+20) + trunc_f32((1.5+2.25)*2) + trunc_f64((6.5+8.25)/3) + wrap_i64(10000000000+7)) == 1410065456
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // i32 sum = 10 + 20
            op(c, wasm_op::i32_const); i32(c, 10);
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::i32_add);  // [i32]

            // + trunc_f32(((1.5 + 2.25) * 2.0))
            op(c, wasm_op::f32_const); f32(c, 1.5f);
            op(c, wasm_op::f32_const); f32(c, 2.25f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::f32_const); f32(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);  // [i32]

            // + trunc_f64(((6.5 + 8.25) / 3.0))
            op(c, wasm_op::f64_const); f64(c, 6.5);
            op(c, wasm_op::f64_const); f64(c, 8.25);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::f64_const); f64(c, 3.0);
            op(c, wasm_op::f64_div);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);  // [i32]

            // + wrap_i64(10000000000 + 7)
            op(c, wasm_op::i64_const); i64(c, 10000000000ll);
            op(c, wasm_op::i64_const); i64(c, 7ll);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::i32_wrap_i64);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_one(runtime_module_t const& rt, ::std::int32_t expected) noexcept
    {
        static_assert(!Opt.is_tail_call || compiler::details::interpreter_tuple_has_no_holes<Opt>());

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;
        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0), rt.local_defined_function_vec_storage.index_unchecked(0), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == expected);
        return 0;
    }

    [[nodiscard]] int test_translate_abi_reg_ring_matrix() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_abi_reg_ring_matrix_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_abi_reg_ring_matrix");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr ::std::int32_t expected = static_cast<::std::int32_t>(1410065456);

        // Tailcall (no cache).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        // Byref (no cache).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        // ABI/reg-ring variants (tailcall + stacktop caching).
        {
            // scalar4 fully merged, small ring [3,5)
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
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        {
            // i32/i64 merged, f32 split, f64 split.
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 7uz,
                .f64_stack_top_end_pos = 9uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        {
            // i32/f32 merged (shared slots), i64 split, f64 split.
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 7uz,
                .f64_stack_top_end_pos = 9uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        {
            // Fully split per-type rings.
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 7uz,
                .f32_stack_top_end_pos = 9uz,
                .f64_stack_top_begin_pos = 9uz,
                .f64_stack_top_end_pos = 11uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        {
            // f32/f64 merged range carried by wasm_f64 slots.
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 7uz,
                .f32_stack_top_end_pos = 11uz,
                .f64_stack_top_begin_pos = 7uz,
                .f64_stack_top_end_pos = 11uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        {
            // int/float split: i32/i64 merged, f32/f64 merged (hard-float style).
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 7uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 7uz,
                .f32_stack_top_end_pos = 11uz,
                .f64_stack_top_begin_pos = 7uz,
                .f64_stack_top_end_pos = 11uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            UWVM2TEST_REQUIRE(run_one<opt>(rt, expected) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_abi_reg_ring_matrix();
}

