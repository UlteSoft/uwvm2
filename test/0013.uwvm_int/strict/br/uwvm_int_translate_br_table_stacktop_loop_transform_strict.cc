#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_table_loop_transform_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param i32 n) -> (result i32)
        //
        // Keep a cached f32 value on the operand stack across a loop, and use `br_table` as a loop back-edge.
        // With stacktop caching enabled, this triggers:
        //   target_frame.type == loop && stacktop_cache_count != 0
        //
        // Always returns 42.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // localidx=1: saved selector
            auto& c = fb.code;

            // Keep one fp-cached value across loop re-entry so stacktop_cache_count != 0 after popping the i32 selector.
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.0f);

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::loop);
            append_u8(c, k_block_empty);

            // selector = (n == 0) ? 1 : 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::local_set);
            u32(c, 1u);

            // n = n - 1
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_sub);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            // br_table [0(loop), 1(block)] default=0
            op(c, wasm_op::local_get);
            u32(c, 1u);  // selector

            op(c, wasm_op::br_table);
            u32(c, 2u);  // vec length
            u32(c, 0u);  // continue loop
            u32(c, 1u);  // break outer block
            u32(c, 0u);  // default continue

            op(c, wasm_op::end);  // end loop
            op(c, wasm_op::end);  // end block

            // drop carried f32; return 42
            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 42);
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_br_table_stacktop_loop_transform() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_table_loop_transform_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_table_loop_transform");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: tailcall (no caching) semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;
            for(::std::int32_t n : {0, 1, 2, 3, 7})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                      pack_i32(n),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);
            }
        }

        // Mode B: tailcall + stacktop caching (i32 ring). Cover br_table's loop-target transform thunk path.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                // translate.h requires all scalar ranges enabled together.
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,  // int ring size=2: enough for (value,selector)
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,  // fp ring size=2 (unused here, but required by contract)
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

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

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

            constexpr auto f35 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<opt>(c35, tuple);
            constexpr auto f36 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<opt>(c36, tuple);
            constexpr auto f45 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<opt>(c45, tuple);
            constexpr auto f46 = optable::translate::get_uwvmint_br_stacktop_transform_to_begin_fptr_from_tuple<opt>(c46, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, f35) ||
                              bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, f36) ||
                              bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, f45) ||
                              bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, f46));
#endif

            using Runner = interpreter_runner<opt>;
            for(::std::int32_t n : {0, 1, 2, 3, 7})
            {
                auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                      rt.local_defined_function_vec_storage.index_unchecked(0),
                                      pack_i32(n),
                                      nullptr,
                                      nullptr);
                UWVM2TEST_REQUIRE(load_i32(rr.results) == 42);
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_table_stacktop_loop_transform();
}
