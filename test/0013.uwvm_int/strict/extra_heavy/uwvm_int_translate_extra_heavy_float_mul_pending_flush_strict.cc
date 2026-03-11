#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] inline float load_f32(byte_vec const& bytes) noexcept
    {
        if(bytes.size() != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
        float v{};
        ::std::memcpy(::std::addressof(v), bytes.data(), 4);
        return v;
    }

    [[nodiscard]] inline double load_f64(byte_vec const& bytes) noexcept
    {
        if(bytes.size() != 8uz) [[unlikely]] { ::fast_io::fast_terminate(); }
        double v{};
        ::std::memcpy(::std::addressof(v), bytes.data(), 8);
        return v;
    }

    [[nodiscard]] inline byte_vec pack_f32x3(float a, float b, float c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_f32x4(float a, float b, float c, float d)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        ::std::memcpy(out.data() + 12, ::std::addressof(d), 4);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_f64x3(double a, double b, double c)
    {
        byte_vec out(24);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_f64x4(double a, double b, double c, double d)
    {
        byte_vec out(32);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        ::std::memcpy(out.data() + 24, ::std::addressof(d), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_float_mul_pending_flush_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: f32 mul + local3 pending flush: `a*b ; local.get c ; drop` => flush kind `float_mul_2localget_local3`.
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: f32 2mul wait-second-mul pending flush:
        // `a*b ; local.get c ; local.get d ; drop ; drop` => flush kind `float_2mul_wait_second_mul`.
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::local_get); u32(c, 3u);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32 2mul after-second-mul pending flush:
        // `a*b ; c*d ; drop` => flush kind `float_2mul_after_second_mul`.
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::local_get); u32(c, 3u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f64 mul + local3 pending flush.
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: f64 2mul wait-second-mul pending flush.
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::local_get); u32(c, 3u);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: f64 2mul after-second-mul pending flush.
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::local_get); u32(c, 3u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_float_mul_pending_flush() noexcept
    {
        auto wasm = build_float_mul_pending_flush_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_float_mul_pending_flush");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr optable::uwvm_interpreter_translate_option_t opt{
            .is_tail_call = true,
            .i32_stack_top_begin_pos = 3uz,
            .i32_stack_top_end_pos = 7uz,
            .i64_stack_top_begin_pos = 3uz,
            .i64_stack_top_end_pos = 7uz,
            .f32_stack_top_begin_pos = 3uz,
            .f32_stack_top_end_pos = 7uz,
            .f64_stack_top_begin_pos = 3uz,
            .f64_stack_top_end_pos = 7uz,
            .v128_stack_top_begin_pos = SIZE_MAX,
            .v128_stack_top_end_pos = SIZE_MAX,
        };
        static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        cop.curr_wasm_id = 0uz;
        auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<opt>;

        // f32 cases
        {
            auto r0 = Runner::run(cm.local_funcs.index_unchecked(0uz),
                                  rt.local_defined_function_vec_storage.index_unchecked(0uz),
                                  pack_f32x3(2.0f, 3.0f, 4.0f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(r0.results) == 6.0f);

            auto r1 = Runner::run(cm.local_funcs.index_unchecked(1uz),
                                  rt.local_defined_function_vec_storage.index_unchecked(1uz),
                                  pack_f32x4(2.0f, 3.0f, 4.0f, 5.0f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(r1.results) == 6.0f);

            auto r2 = Runner::run(cm.local_funcs.index_unchecked(2uz),
                                  rt.local_defined_function_vec_storage.index_unchecked(2uz),
                                  pack_f32x4(2.0f, 3.0f, 4.0f, 5.0f),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(r2.results) == 6.0f);
        }

        // f64 cases
        {
            auto r3 = Runner::run(cm.local_funcs.index_unchecked(3uz),
                                  rt.local_defined_function_vec_storage.index_unchecked(3uz),
                                  pack_f64x3(2.0, 3.0, 4.0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(r3.results) == 6.0);

            auto r4 = Runner::run(cm.local_funcs.index_unchecked(4uz),
                                  rt.local_defined_function_vec_storage.index_unchecked(4uz),
                                  pack_f64x4(2.0, 3.0, 4.0, 5.0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(r4.results) == 6.0);

            auto r5 = Runner::run(cm.local_funcs.index_unchecked(5uz),
                                  rt.local_defined_function_vec_storage.index_unchecked(5uz),
                                  pack_f64x4(2.0, 3.0, 4.0, 5.0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(r5.results) == 6.0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_float_mul_pending_flush();
}
