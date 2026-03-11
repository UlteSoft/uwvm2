#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_unreachable_polymorphic_stack_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: (param i32) -> i32 : if (cond) 111 else { unreachable; <mixed stack ops>; 222 }
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i32);
            op(c, wasm_op::i32_const);
            i32(c, 111);
            op(c, wasm_op::else_);
            op(c, wasm_op::unreachable);
            op(c, wasm_op::i64_const);
            i64(c, 7);
            op(c, wasm_op::drop);
            op(c, wasm_op::f32_const);
            f32(c, 1.5f);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);
            op(c, wasm_op::end);  // end if
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i32) -> i64
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_val_i64);
            op(c, wasm_op::i64_const);
            i64(c, 0x1122334455667788ll);
            op(c, wasm_op::else_);
            op(c, wasm_op::unreachable);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::drop);
            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            op(c, wasm_op::drop);
            op(c, wasm_op::i64_const);
            i64(c, 0x99aabbccddeeff00ll);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (param i32) -> f32
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_val_f32);
            op(c, wasm_op::f32_const);
            f32(c, 1.25f);
            op(c, wasm_op::else_);
            op(c, wasm_op::unreachable);
            op(c, wasm_op::i64_const);
            i64(c, 0);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::drop);
            op(c, wasm_op::f32_const);
            f32(c, -1.0f);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (param i32) -> f64
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::if_);
            append_u8(c, k_val_f64);
            op(c, wasm_op::f64_const);
            f64(c, 3.5);
            op(c, wasm_op::else_);
            op(c, wasm_op::unreachable);
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::drop);
            op(c, wasm_op::f32_const);
            f32(c, 0.0f);
            op(c, wasm_op::drop);
            op(c, wasm_op::f64_const);
            f64(c, -1.0);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_unreachable_polymorphic_stack_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 4uz);

        using Runner = interpreter_runner<Opt>;

        // Run only the non-trapping path (cond=1).
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 0x1122334455667788ll);

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 1.25f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 3.5);

        return 0;
    }

    [[nodiscard]] int test_translate_unreachable_polymorphic_stack() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_unreachable_polymorphic_stack_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_unreachable_polymorphic_stack");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_unreachable_polymorphic_stack_suite<opt>(rt) == 0);
        }

        // tailcall (no cache)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_unreachable_polymorphic_stack_suite<opt>(rt) == 0);
        }

        // tailcall + stacktop caching (merged scalar4 rings)
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
            UWVM2TEST_REQUIRE(run_unreachable_polymorphic_stack_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_unreachable_polymorphic_stack();
}

