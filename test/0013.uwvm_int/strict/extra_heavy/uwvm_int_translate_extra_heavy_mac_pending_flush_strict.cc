#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] inline byte_vec pack_i32x3(::std::int32_t a, ::std::int32_t b, ::std::int32_t c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_i64x3(::std::int64_t a, ::std::int64_t b, ::std::int64_t c)
    {
        byte_vec out(24);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        return out;
    }

    [[nodiscard]] inline byte_vec pack_f32x3(float a, float b, float c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
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

    [[nodiscard]] byte_vec build_mac_pending_flush_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: flush `mac_after_mul` (f32 branch): `acc, x, y; mul; drop; drop`.
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: flush `mac_after_mul` (f64 branch).
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: flush `mac_after_add` (i32 branch): `acc + x*y`, then drop.
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: flush `mac_after_add` (i64 branch).
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i64}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_mul);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: flush `mac_after_add` (f32 branch).
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: flush `mac_after_add` (f64 branch).
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::drop);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_mac_pending_flush() noexcept
    {
        auto wasm = build_mac_pending_flush_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_mac_pending_flush");
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

        // mac_after_mul flush (f32,f64)
        (void)Runner::run(cm.local_funcs.index_unchecked(0uz),
                          rt.local_defined_function_vec_storage.index_unchecked(0uz),
                          pack_f32x3(10.0f, 2.0f, 3.0f),
                          nullptr,
                          nullptr);
        (void)Runner::run(cm.local_funcs.index_unchecked(1uz),
                          rt.local_defined_function_vec_storage.index_unchecked(1uz),
                          pack_f64x3(10.0, 2.0, 3.0),
                          nullptr,
                          nullptr);

        // mac_after_add flush (i32,i64,f32,f64)
        (void)Runner::run(cm.local_funcs.index_unchecked(2uz),
                          rt.local_defined_function_vec_storage.index_unchecked(2uz),
                          pack_i32x3(10, 2, 3),
                          nullptr,
                          nullptr);
        (void)Runner::run(cm.local_funcs.index_unchecked(3uz),
                          rt.local_defined_function_vec_storage.index_unchecked(3uz),
                          pack_i64x3(10, 2, 3),
                          nullptr,
                          nullptr);
        (void)Runner::run(cm.local_funcs.index_unchecked(4uz),
                          rt.local_defined_function_vec_storage.index_unchecked(4uz),
                          pack_f32x3(10.0f, 2.0f, 3.0f),
                          nullptr,
                          nullptr);
        (void)Runner::run(cm.local_funcs.index_unchecked(5uz),
                          rt.local_defined_function_vec_storage.index_unchecked(5uz),
                          pack_f64x3(10.0, 2.0, 3.0),
                          nullptr,
                          nullptr);

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_mac_pending_flush();
}

