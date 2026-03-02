#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_table_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: br_table switch returning {10,20,30,40} based on selector.
        // (param i32) (result i32)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            // outer: (block (result i32) ...)
            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            // b1..b4: empty blocks for br_table targets.
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b1 (depth 3 from inside b4)
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b2
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b3
            op(c, wasm_op::block);
            append_u8(c, k_block_empty);  // b4

            // selector
            op(c, wasm_op::local_get);
            u32(c, 0u);

            // br_table [0,1,2,3] default=3
            op(c, wasm_op::br_table);
            u32(c, 4u);  // vec length
            u32(c, 0u);
            u32(c, 1u);
            u32(c, 2u);
            u32(c, 3u);
            u32(c, 3u);  // default

            op(c, wasm_op::end);  // end b4 (idx=0 falls here)
            op(c, wasm_op::i32_const);
            i32(c, 10);
            op(c, wasm_op::br);
            u32(c, 3u);  // break to outer (result i32)

            op(c, wasm_op::end);  // end b3 (idx=1 falls here)
            op(c, wasm_op::i32_const);
            i32(c, 20);
            op(c, wasm_op::br);
            u32(c, 2u);  // break to outer

            op(c, wasm_op::end);  // end b2 (idx=2 falls here)
            op(c, wasm_op::i32_const);
            i32(c, 30);
            op(c, wasm_op::br);
            u32(c, 1u);  // break to outer

            op(c, wasm_op::end);  // end b1 (idx>=3 falls here)
            op(c, wasm_op::i32_const);
            i32(c, 40);

            op(c, wasm_op::end);  // end outer
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_br_table() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_table_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_table");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: tailcall (no stacktop caching) + strict br_table opfunc present.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_br_table = optable::translate::get_uwvmint_br_table_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_br_table));

            using Runner = interpreter_runner<opt>;
            auto run_sel = [&](::std::int32_t selector) noexcept
            {
                return load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                            pack_i32(selector),
                                            nullptr,
                                            nullptr)
                                    .results);
            };
            UWVM2TEST_REQUIRE(run_sel(0) == 10);
            UWVM2TEST_REQUIRE(run_sel(1) == 20);
            UWVM2TEST_REQUIRE(run_sel(2) == 30);
            UWVM2TEST_REQUIRE(run_sel(3) == 40);
            UWVM2TEST_REQUIRE(run_sel(4) == 40);
            UWVM2TEST_REQUIRE(run_sel(100) == 40);
        }

        // Mode B: tailcall + stacktop caching (exercise cached-index specialization selection).
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

            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            bool any_match{};
            for(::std::size_t pos = opt.i32_stack_top_begin_pos; pos != opt.i32_stack_top_end_pos; ++pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{
                    .i32_stack_top_curr_pos = pos,
                };
                auto const fptr = optable::translate::get_uwvmint_br_table_fptr_from_tuple<opt>(curr, tuple);
                if(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, fptr))
                {
                    any_match = true;
                    break;
                }
            }
            UWVM2TEST_REQUIRE(any_match);

            using Runner = interpreter_runner<opt>;
            auto run_sel = [&](::std::int32_t selector) noexcept
            {
                return load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                            pack_i32(selector),
                                            nullptr,
                                            nullptr)
                                    .results);
            };
            UWVM2TEST_REQUIRE(run_sel(0) == 10);
            UWVM2TEST_REQUIRE(run_sel(1) == 20);
            UWVM2TEST_REQUIRE(run_sel(2) == 30);
            UWVM2TEST_REQUIRE(run_sel(3) == 40);
            UWVM2TEST_REQUIRE(run_sel(4) == 40);
        }

        // Mode C: byref mode instruments hitting `br_table`.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
            constexpr auto exp_br_table = optable::translate::get_uwvmint_br_table_fptr_from_tuple<opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_br_table));

            using Runner = interpreter_runner<opt>;
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i32(2),
                                   exp_br_table,
                                   nullptr);
            UWVM2TEST_REQUIRE(rr0.hit_expected0);
            UWVM2TEST_REQUIRE(load_i32(rr0.results) == 30);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_table();
}
