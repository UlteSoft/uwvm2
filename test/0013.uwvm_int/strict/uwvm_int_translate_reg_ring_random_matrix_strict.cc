#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_mixed_params(::std::int32_t a, ::std::int64_t b, float c, double d)
    {
        byte_vec out(4 + 8 + 4 + 8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 12, ::std::addressof(c), 4);
        ::std::memcpy(out.data() + 16, ::std::addressof(d), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_reg_ring_random_matrix_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: mixed-type operand stack stress -> deterministic i32 result.
        // ((10+20) + trunc_f32((1.5+2.25)*2) + trunc_f64((6.5+8.25)/3) + wrap_i64(10000000000+7)) == 1410065456
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 10);
            op(c, wasm_op::i32_const);
            i32(c, 20);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::f32_const);
            f32(c, 1.5f);
            op(c, wasm_op::f32_const);
            f32(c, 2.25f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::f32_const);
            f32(c, 2.0f);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::f64_const);
            f64(c, 6.5);
            op(c, wasm_op::f64_const);
            f64(c, 8.25);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::f64_const);
            f64(c, 3.0);
            op(c, wasm_op::f64_div);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::i64_const);
            i64(c, 10000000000ll);
            op(c, wasm_op::i64_const);
            i64(c, 7ll);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::i32_wrap_i64);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: mixed ABI params (i32,i64,f32,f64) -> i32
        // return a + wrap_i64(b) + trunc_f32(c) + trunc_f64(d)
        {
            func_type ty{{k_val_i32, k_val_i64, k_val_f32, k_val_f64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_wrap_i64);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::i32_trunc_f32_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::local_get);
            u32(c, 3u);
            op(c, wasm_op::i32_trunc_f64_s);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_reg_ring_random_matrix_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0
        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == static_cast<::std::int32_t>(1410065456));

        // f1
        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_mixed_params(10, 20, 1.5f, 2.25),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr1.results) == 33);

        return 0;
    }

    [[nodiscard]] int test_translate_reg_ring_random_matrix() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_reg_ring_random_matrix_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_reg_ring_random_matrix");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // No caching.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_reg_ring_random_matrix_suite<opt>(rt) == 0);
        }
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_reg_ring_random_matrix_suite<opt>(rt) == 0);
        }

        // Random-ish ring layouts (hole-free), exercise both tailcall and byref ABIs.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 4uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 4uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 4uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 4uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_reg_ring_random_matrix_suite<opt>(rt) == 0);
        }

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
            UWVM2TEST_REQUIRE(run_reg_ring_random_matrix_suite<opt>(rt) == 0);
        }

        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 4uz,
                .i64_stack_top_begin_pos = 4uz,
                .i64_stack_top_end_pos = 6uz,
                .f32_stack_top_begin_pos = 6uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 7uz,
                .f64_stack_top_end_pos = 9uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_reg_ring_random_matrix_suite<opt>(rt) == 0);
        }

        {
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
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_reg_ring_random_matrix_suite<opt>(rt) == 0);
        }

        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 9uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 9uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_reg_ring_random_matrix_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_reg_ring_random_matrix();
}
