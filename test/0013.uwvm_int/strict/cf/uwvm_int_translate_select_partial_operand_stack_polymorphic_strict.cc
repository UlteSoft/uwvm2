#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    // Covers wasm1.h `select` stack modeling branch:
    //   if(!v1_from_stack && v2_from_stack) { operand_stack_push(v2_type); }
    // by placing `select` in a polymorphic (unreachable) region after an unconditional `br 0`
    // inside a block whose base stack height is 2.
    [[nodiscard]] byte_vec build_select_partial_operand_stack_polymorphic_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: () -> i32
        //   i32.const 11; i32.const 1;
        //   block
        //     br 0
        //     select       ;; polymorphic; only 2 stack values available (v2, cond)
        //   end
        //   drop; drop; i32.const 7
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 11);
            op(c, wasm_op::i32_const);
            i32(c, 1);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::br);
            append_u32_leb(c, 0u);

            op(c, wasm_op::select);

            op(c, wasm_op::end);  // end block

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 7);

            op(c, wasm_op::end);  // end func
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_select_partial_operand_stack_polymorphic_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);

        using Runner = interpreter_runner<Opt>;
        ::std::int32_t const got =
            load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                 rt.local_defined_function_vec_storage.index_unchecked(0),
                                 pack_no_params(),
                                 nullptr,
                                 nullptr)
                         .results);
        UWVM2TEST_REQUIRE(got == 7);
        return 0;
    }

    [[nodiscard]] int test_translate_select_partial_operand_stack_polymorphic() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_select_partial_operand_stack_polymorphic_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_select_partial_operand_stack_polymorphic");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall + stacktop caching: required so `end` can restore the reachable stack snapshot after the `br 0`
        // (the unreachable `select` intentionally pops below the block base).
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
            UWVM2TEST_REQUIRE(run_select_partial_operand_stack_polymorphic_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_select_partial_operand_stack_polymorphic();
}

