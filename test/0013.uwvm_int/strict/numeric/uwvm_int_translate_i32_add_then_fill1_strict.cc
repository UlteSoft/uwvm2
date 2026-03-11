#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_i32_add_then_fill1_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: ((1 + 2) + 3) but arranged to:
        // - fill a small i32 stacktop ring (2 slots),
        // - spill 1 value to operand-stack memory,
        // - then trigger i32.add_then_fill1 patching at the first i32.add.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_const);
            i32(c, 2);
            op(c, wasm_op::i32_const);
            i32(c, 3);

            // Flush const pending so all 3 values are materialized, forcing a stacktop spill with a 2-slot ring.
            op(c, wasm_op::nop);

            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_i32_add_then_fill1() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_i32_add_then_fill1_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_i32_add_then_fill1");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall + stacktop caching (small rings). Same layout as translate_strict Mode C.
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

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        constexpr auto tuple = compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr_a{
            .i32_stack_top_curr_pos = 3uz,
            .i64_stack_top_curr_pos = 3uz,
            .f32_stack_top_curr_pos = 5uz,
            .f64_stack_top_curr_pos = 5uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr_b{
            .i32_stack_top_curr_pos = 4uz,
            .i64_stack_top_curr_pos = 4uz,
            .f32_stack_top_curr_pos = 6uz,
            .f64_stack_top_curr_pos = 6uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };

        constexpr auto f_a = optable::translate::get_uwvmint_i32_add_then_fill1_fptr_from_tuple<opt>(curr_a, tuple);
        constexpr auto f_b = optable::translate::get_uwvmint_i32_add_then_fill1_fptr_from_tuple<opt>(curr_b, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, f_a) || bytecode_contains_fptr(bc0, f_b));
#endif

        using Runner = interpreter_runner<opt>;
        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == 6);

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_i32_add_then_fill1();
}

