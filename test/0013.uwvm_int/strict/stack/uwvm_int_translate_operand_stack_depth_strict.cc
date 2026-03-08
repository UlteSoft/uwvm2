#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_operand_stack_depth_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: deep i32 operand stack (push 256 consts, then add reduce).
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            constexpr ::std::int32_t N = 256;
            for(::std::int32_t v = 1; v <= N; ++v)
            {
                op(c, wasm_op::i32_const);
                i32(c, v);
            }
            for(::std::int32_t i = 0; i < (N - 1); ++i)
            {
                op(c, wasm_op::i32_add);
            }

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: deep i64 operand stack (push 128 consts, then add reduce).
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            constexpr ::std::int64_t N = 128;
            for(::std::int64_t v = 1; v <= N; ++v)
            {
                op(c, wasm_op::i64_const);
                i64(c, v);
            }
            for(::std::int64_t i = 0; i < (N - 1); ++i)
            {
                op(c, wasm_op::i64_add);
            }

            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_operand_stack_depth_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0: sum 1..256 = 32896
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_no_params(),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 32896);
        }

        // f1: sum 1..128 = 8256
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_no_params(),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr.results) == 8256);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_operand_stack_depth() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_operand_stack_depth_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_operand_stack_depth");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_operand_stack_depth_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_operand_stack_depth_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_operand_stack_depth_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_operand_stack_depth_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
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
            UWVM2TEST_REQUIRE(run_operand_stack_depth_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_operand_stack_depth();
}
