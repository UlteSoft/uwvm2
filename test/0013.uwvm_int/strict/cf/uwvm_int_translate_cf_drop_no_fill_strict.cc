#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_cf_drop_no_fill_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: stacktop disabled -> br(arity=0) forces `emit_drop_to_stack_size_no_fill()` fallback loop.
        // (result i32) -> 42
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i64_const);
            i64(c, 2);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 3.0f);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 4.0);

            op(c, wasm_op::br);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block

            op(c, wasm_op::i32_const);
            i32(c, 42);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: stacktop enabled + polymorphic (unreachable) -> br(arity=1) forces `emit_drop_to_stack_size_no_fill()` polymorphic path.
        // (result i32) -> 111
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::br);
            u32(c, 0u);

            // Unreachable-at-runtime region: still must compile, and must perform stack-shape repair in polymorphic mode.
            op(c, wasm_op::i64_const);
            i64(c, 2);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 3.0f);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 4.0);
            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::br);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_cf_drop_no_fill() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_cf_drop_no_fill_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_cf_drop_no_fill");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode: tailcall, stacktop disabled
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
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
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 42);
        }

        // Mode: tailcall, stacktop enabled
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

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_no_params(),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr1.results) == 111);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_cf_drop_no_fill();
}

