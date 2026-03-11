#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i64x2(::std::int64_t a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x2(double a, double b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_stacktop_spill1_then_add_2localget_scalar4_merged_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // Group A: i32.add localget2 (result i32). We keep the function void and drop everything.

        auto add_i32_case = [&](auto emit_const) -> void
        {
            func_type ty{{k_val_i32, k_val_i32}, {}};
            func_body fb{};
            auto& c = fb.code;

            emit_const(c);
            // Flush pending-const state before forming the local.get2 window.
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::drop);  // drop add result
            op(c, wasm_op::drop);  // drop spilled const
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f0: spill(i32) + i32_add_2localget
        add_i32_case([&](byte_vec& c) {
            op(c, wasm_op::i32_const);
            i32(c, 0x1234);
        });

        // f1: spill(i64) + i32_add_2localget
        add_i32_case([&](byte_vec& c) {
            op(c, wasm_op::i64_const);
            i64(c, 0x1234'5678'9abc'def0ll);
        });

        // f2: spill(f32) + i32_add_2localget
        add_i32_case([&](byte_vec& c) {
            op(c, wasm_op::f32_const);
            f32(c, 3.25f);
        });

        // f3: spill(f64) + i32_add_2localget
        add_i32_case([&](byte_vec& c) {
            op(c, wasm_op::f64_const);
            f64(c, 6.5);
        });

        // Group B: i64.add localget2 (result i64), with cross-type spills.
        auto add_i64_case = [&](auto emit_const) -> void
        {
            func_type ty{{k_val_i64, k_val_i64}, {}};
            func_body fb{};
            auto& c = fb.code;

            emit_const(c);
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i64_add);

            op(c, wasm_op::drop);  // drop add result
            op(c, wasm_op::drop);  // drop spilled const
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f4: spill(i64) + i64_add_2localget
        add_i64_case([&](byte_vec& c) {
            op(c, wasm_op::i64_const);
            i64(c, 7);
        });

        // f5: spill(i32) + i64_add_2localget
        add_i64_case([&](byte_vec& c) {
            op(c, wasm_op::i32_const);
            i32(c, -9);
        });

        // f6: spill(f32) + i64_add_2localget
        add_i64_case([&](byte_vec& c) {
            op(c, wasm_op::f32_const);
            f32(c, -0.25f);
        });

        // f7: spill(f64) + i64_add_2localget
        add_i64_case([&](byte_vec& c) {
            op(c, wasm_op::f64_const);
            f64(c, -1.0);
        });

        // Group C: f64.add localget2 (result f64), with cross-type spills.
        auto add_f64_case = [&](auto emit_const) -> void
        {
            func_type ty{{k_val_f64, k_val_f64}, {}};
            func_body fb{};
            auto& c = fb.code;

            emit_const(c);
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::f64_add);

            op(c, wasm_op::drop);  // drop add result
            op(c, wasm_op::drop);  // drop spilled const
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f8: spill(f64) + f64_add_2localget
        add_f64_case([&](byte_vec& c) {
            op(c, wasm_op::f64_const);
            f64(c, 1.25);
        });

        // f9: spill(f32) + f64_add_2localget
        add_f64_case([&](byte_vec& c) {
            op(c, wasm_op::f32_const);
            f32(c, 2.5f);
        });

        // f10: spill(i32) + f64_add_2localget
        add_f64_case([&](byte_vec& c) {
            op(c, wasm_op::i32_const);
            i32(c, 123);
        });

        // f11: spill(i64) + f64_add_2localget
        add_f64_case([&](byte_vec& c) {
            op(c, wasm_op::i64_const);
            i64(c, -42);
        });

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 12uz);

        using Runner = interpreter_runner<Opt>;

        // i32.add cases
        for(::std::size_t i = 0; i < 4; ++i)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(i),
                                  rt.local_defined_function_vec_storage.index_unchecked(i),
                                  pack_i32x2(11, 22),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(rr.results.empty());
        }

        // i64.add cases
        for(::std::size_t i = 4; i < 8; ++i)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(i),
                                  rt.local_defined_function_vec_storage.index_unchecked(i),
                                  pack_i64x2(10, 20),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(rr.results.empty());
        }

        // f64.add cases
        for(::std::size_t i = 8; i < 12; ++i)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(i),
                                  rt.local_defined_function_vec_storage.index_unchecked(i),
                                  pack_f64x2(1.25, 2.5),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(rr.results.empty());
        }

        return 0;
    }

    [[nodiscard]] int test_translate_stacktop_spill1_then_add_2localget_scalar4_merged() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_spill1_then_add_2localget_scalar4_merged_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_spill1_then_add_2localget_scalar4_merged");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: scalar4 fully merged ring with size=1 (force spill1).
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

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{
                .i32_stack_top_curr_pos = 3uz,
                .i64_stack_top_curr_pos = 3uz,
                .f32_stack_top_curr_pos = 3uz,
                .f64_stack_top_curr_pos = 3uz,
                .v128_stack_top_curr_pos = SIZE_MAX,
            };

            // i32.add localget2 spill patch (spilled_vt in {i32,i64,f32,f64})
            {
                constexpr auto exp_spill_i32 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<opt, wasm_i32>(curr, tuple);
                constexpr auto exp_spill_i64 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<opt, wasm_i64>(curr, tuple);
                constexpr auto exp_spill_f32 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<opt, wasm_f32>(curr, tuple);
                constexpr auto exp_spill_f64 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i32_add_2localget_typed_fptr_from_tuple<opt, wasm_f64>(curr, tuple);

                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_spill_i32));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_spill_i64));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_spill_f32));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_spill_f64));
            }

            // i64.add localget2 spill patch (spilled_vt in {i64,i32,f32,f64})
            {
                constexpr auto exp_spill_i64 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<opt, wasm_i64>(curr, tuple);
                constexpr auto exp_spill_i32 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<opt, wasm_i32>(curr, tuple);
                constexpr auto exp_spill_f32 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<opt, wasm_f32>(curr, tuple);
                constexpr auto exp_spill_f64 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_i64_add_2localget_typed_fptr_from_tuple<opt, wasm_f64>(curr, tuple);

                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_spill_i64));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_spill_i32));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_spill_f32));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_spill_f64));
            }

            // f64.add localget2 spill patch (spilled_vt in {f64,f32,i32,i64})
            {
                constexpr auto exp_spill_f64 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<opt, wasm_f64>(curr, tuple);
                constexpr auto exp_spill_f32 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<opt, wasm_f32>(curr, tuple);
                constexpr auto exp_spill_i32 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<opt, wasm_i32>(curr, tuple);
                constexpr auto exp_spill_i64 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_f64_add_2localget_typed_fptr_from_tuple<opt, wasm_i64>(curr, tuple);

                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, exp_spill_f64));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, exp_spill_f32));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, exp_spill_i32));
                UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, exp_spill_i64));
            }
#endif

            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_spill1_then_add_2localget_scalar4_merged();
}
