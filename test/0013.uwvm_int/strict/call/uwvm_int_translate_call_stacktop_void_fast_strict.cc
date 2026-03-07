#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_call_stacktop_void_fast_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // === Callees: (T x N) -> () ===

        // i32 callees: 0..3
        {
            func_type ty{{k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32, k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f32 callees: 4..7
        {
            func_type ty{{k_val_f32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_f32, k_val_f32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32, k_val_f32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f64 callees: 8..11
        {
            func_type ty{{k_val_f64}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_f64, k_val_f64}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64, k_val_f64}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // === Callers: push args then call ===

        // i32 callers: 12..15
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::call);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 2);
            op(fb.code, wasm_op::call);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 2);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 3);
            op(fb.code, wasm_op::call);
            u32(fb.code, 2u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 2);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 3);
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 4);
            op(fb.code, wasm_op::call);
            u32(fb.code, 3u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f32 callers: 16..19
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 1.0f);
            op(fb.code, wasm_op::call);
            u32(fb.code, 4u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 1.0f);
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 2.0f);
            op(fb.code, wasm_op::call);
            u32(fb.code, 5u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 1.0f);
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 2.0f);
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 3.0f);
            op(fb.code, wasm_op::call);
            u32(fb.code, 6u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 1.0f);
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 2.0f);
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 3.0f);
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 4.0f);
            op(fb.code, wasm_op::call);
            u32(fb.code, 7u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f64 callers: 20..23
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 1.0);
            op(fb.code, wasm_op::call);
            u32(fb.code, 8u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 1.0);
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 2.0);
            op(fb.code, wasm_op::call);
            u32(fb.code, 9u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 1.0);
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 2.0);
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 3.0);
            op(fb.code, wasm_op::call);
            u32(fb.code, 10u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 1.0);
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 2.0);
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 3.0);
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 4.0);
            op(fb.code, wasm_op::call);
            u32(fb.code, 11u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] bool contains_any_fptr(ByteStorage const& bc, Fptr f3, Fptr f4, Fptr f5, Fptr f6) noexcept
    {
        return bytecode_contains_fptr(bc, f3) || bytecode_contains_fptr(bc, f4) || bytecode_contains_fptr(bc, f5) || bytecode_contains_fptr(bc, f6);
    }

    [[nodiscard]] int test_translate_call_stacktop_void_fast() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_call_stacktop_void_fast_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_call_stacktop_void_fast");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall + stacktop caching (fully merged scalar4 range).
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
            UWVM2TEST_REQUIRE(cm.local_funcs.size() == 24uz);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr optable::uwvm_interpreter_stacktop_currpos_t c3{.i32_stack_top_curr_pos = 3uz,
                                                                      .i64_stack_top_curr_pos = 3uz,
                                                                      .f32_stack_top_curr_pos = 3uz,
                                                                      .f64_stack_top_curr_pos = 3uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c4{.i32_stack_top_curr_pos = 4uz,
                                                                      .i64_stack_top_curr_pos = 4uz,
                                                                      .f32_stack_top_curr_pos = 4uz,
                                                                      .f64_stack_top_curr_pos = 4uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c5{.i32_stack_top_curr_pos = 5uz,
                                                                      .i64_stack_top_curr_pos = 5uz,
                                                                      .f32_stack_top_curr_pos = 5uz,
                                                                      .f64_stack_top_curr_pos = 5uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c6{.i32_stack_top_curr_pos = 6uz,
                                                                      .i64_stack_top_curr_pos = 6uz,
                                                                      .f32_stack_top_curr_pos = 6uz,
                                                                      .f64_stack_top_curr_pos = 6uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};

            // i32 void: ParamCount 1..4 (callers 12..15)
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 1uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(12).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 2uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 2uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 2uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 2uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(13).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 3uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(14).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 4uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 4uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 4uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_i32_fptr_from_tuple<opt, 4uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(15).op.operands, f3, f4, f5, f6));
            }

            // f32 void: ParamCount 1..4 (callers 16..19)
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(16).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(17).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 3uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 3uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 3uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 3uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(18).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 4uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 4uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 4uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 4uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(19).op.operands, f3, f4, f5, f6));
            }

            // f64 void: ParamCount 1..4 (callers 20..23)
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(20).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(21).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 3uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 3uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 3uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 3uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(22).op.operands, f3, f4, f5, f6));
            }
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 4uz, void>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 4uz, void>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 4uz, void>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 4uz, void>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(23).op.operands, f3, f4, f5, f6));
            }
#endif
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_call_stacktop_void_fast();
}

