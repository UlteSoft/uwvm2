#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_div_from_imm_localtee_polymorphic_flush_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: Put `f32.const imm; local.get; f32.div; local.tee` inside a polymorphic (unreachable) if-then block
        // so local.tee cannot take the fused mega-op, forcing `flush_conbine_pending()` to hit:
        // - conbine_pending_kind::f32_div_from_imm_localtee_wait
        // (function returns 0 via else branch at runtime).
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local0: f32
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::f32_const); f32(c, 2.0f);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_div);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const); i32(c, 1);

            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::end);  // end if
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: Same as f0 but f64.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local0: f64
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::f64_const); f64(c, 2.0);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_div);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const); i32(c, 1);

            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::end);  // end if
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        cop.curr_wasm_id = 0uz;
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        auto r0 = Runner::run(cm.local_funcs.index_unchecked(0uz),
                              rt.local_defined_function_vec_storage.index_unchecked(0uz),
                              pack_no_params(),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(r0.results) == 0);

        auto r1 = Runner::run(cm.local_funcs.index_unchecked(1uz),
                              rt.local_defined_function_vec_storage.index_unchecked(1uz),
                              pack_no_params(),
                              nullptr,
                              nullptr);
        UWVM2TEST_REQUIRE(load_i32(r1.results) == 0);

        return 0;
    }

    [[nodiscard]] int test_translate_div_from_imm_localtee_polymorphic_flush() noexcept
    {
        auto wasm = build_div_from_imm_localtee_polymorphic_flush_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_div_from_imm_localtee_polymorphic_flush");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
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
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_div_from_imm_localtee_polymorphic_flush();
}
