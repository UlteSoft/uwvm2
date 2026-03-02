#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_cf_stacktop_transform_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param i32) (result i32) -> 123 (for param!=0)
        // Contains an unreachable-at-runtime `br 0` that should be emitted as `br_stacktop_transform_to_begin`
        // when stacktop caching (merged rings) is enabled and the cache is non-empty at the branch point.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // Keep one cached stack value across the loop so the branch sees stacktop_cache_count != 0.
            op(c, wasm_op::i32_const); i32(c, 7);

            op(c, wasm_op::block); append_u8(c, k_block_empty);
            op(c, wasm_op::loop); append_u8(c, k_block_empty);

            // if (param != 0) break out of loop+block
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::br_if); u32(c, 1u);

            // else path: never execute in test (we pass param=1), but it must compile.
            op(c, wasm_op::nop);
            op(c, wasm_op::br); u32(c, 0u);

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            // Drop carried value; return 123
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const); i32(c, 123);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t CompileOption, typename ByteStorage>
    [[nodiscard]] bool bytecode_contains_any_br_stacktop_transform(ByteStorage const& bc) noexcept
    {
#if !defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        (void)bc;
        return true;  // feature off: skip assertion
#else
        constexpr auto tuple = compiler::details::make_interpreter_tuple<CompileOption>(
            ::std::make_index_sequence<compiler::details::interpreter_tuple_size<CompileOption>()>{});

        // Only the currpos tuple matters for fptr selection. With 2-slot merged rings there are few combinations.
        constexpr optable::uwvm_interpreter_stacktop_currpos_t c35{
            .i32_stack_top_curr_pos = 3uz,
            .i64_stack_top_curr_pos = 3uz,
            .f32_stack_top_curr_pos = 5uz,
            .f64_stack_top_curr_pos = 5uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };
        constexpr optable::uwvm_interpreter_stacktop_currpos_t c36{
            .i32_stack_top_curr_pos = 3uz,
            .i64_stack_top_curr_pos = 3uz,
            .f32_stack_top_curr_pos = 6uz,
            .f64_stack_top_curr_pos = 6uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };
        constexpr optable::uwvm_interpreter_stacktop_currpos_t c45{
            .i32_stack_top_curr_pos = 4uz,
            .i64_stack_top_curr_pos = 4uz,
            .f32_stack_top_curr_pos = 5uz,
            .f64_stack_top_curr_pos = 5uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };
        constexpr optable::uwvm_interpreter_stacktop_currpos_t c46{
            .i32_stack_top_curr_pos = 4uz,
            .i64_stack_top_curr_pos = 4uz,
            .f32_stack_top_curr_pos = 6uz,
            .f64_stack_top_curr_pos = 6uz,
            .v128_stack_top_curr_pos = SIZE_MAX,
        };

        constexpr auto f35 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<CompileOption>(c35, tuple);
        constexpr auto f36 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<CompileOption>(c36, tuple);
        constexpr auto f45 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<CompileOption>(c45, tuple);
        constexpr auto f46 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<CompileOption>(c46, tuple);

        return bytecode_contains_fptr(bc, f35) || bytecode_contains_fptr(bc, f36) || bytecode_contains_fptr(bc, f45) || bytecode_contains_fptr(bc, f46);
#endif
    }

    [[nodiscard]] int test_translate_cf_stacktop_transform() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_cf_stacktop_transform_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_cf_stacktop_transform");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref semantics smoke
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(1),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 123);
        }

        // Mode C: tailcall + stacktop caching (merged rings)
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

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
            UWVM2TEST_REQUIRE(bytecode_contains_any_br_stacktop_transform<opt>(bc0));

            using Runner = interpreter_runner<opt>;
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(1),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 123);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_cf_stacktop_transform();
}
