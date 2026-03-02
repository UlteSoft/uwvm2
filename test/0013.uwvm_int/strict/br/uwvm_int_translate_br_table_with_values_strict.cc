#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_table_with_values_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto emit_br_table_012_default2 = [&](byte_vec& c)
        {
            op(c, wasm_op::br_table);
            // vec length=3: labels 0,1,2; default 2
            u32(c, 3u);
            u32(c, 0u);
            u32(c, 1u);
            u32(c, 2u);
            u32(c, 2u);
        };

        // f0: i32 br_table with result values (and extra junk to force unwind).
        // selector=0 => (10 + 1 + 2)=13; selector=1 => (10 + 2)=12; else => 10
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);  // B0
            op(c, wasm_op::block);
            append_u8(c, k_val_i32);  // B1
            op(c, wasm_op::block);
            append_u8(c, k_val_i32);  // B2

            op(c, wasm_op::i32_const);
            i32(c, 0x11111111);  // junk: must be dropped by branch unwind
            op(c, wasm_op::i32_const);
            i32(c, 10);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            emit_br_table_012_default2(c);

            op(c, wasm_op::end);  // end B2
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);  // end B1
            op(c, wasm_op::i32_const);
            i32(c, 2);
            op(c, wasm_op::i32_add);

            op(c, wasm_op::end);  // end B0
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64 br_table with result values (and extra junk to force unwind).
        // selector=0 => (10 + 1 + 2)=13; selector=1 => 12; else => 10
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i64);  // B0
            op(c, wasm_op::block);
            append_u8(c, k_val_i64);  // B1
            op(c, wasm_op::block);
            append_u8(c, k_val_i64);  // B2

            op(c, wasm_op::i32_const);
            i32(c, 0x22222222);  // junk: must be dropped by branch unwind
            op(c, wasm_op::i64_const);
            i64(c, 10ll);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            emit_br_table_012_default2(c);

            op(c, wasm_op::end);  // end B2
            op(c, wasm_op::i64_const);
            i64(c, 1ll);
            op(c, wasm_op::i64_add);

            op(c, wasm_op::end);  // end B1
            op(c, wasm_op::i64_const);
            i64(c, 2ll);
            op(c, wasm_op::i64_add);

            op(c, wasm_op::end);  // end B0
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32 br_table with result values (and extra junk to force unwind).
        // selector=0 => 13.0; selector=1 => 12.0; else => 10.0
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_f32);  // B0
            op(c, wasm_op::block);
            append_u8(c, k_val_f32);  // B1
            op(c, wasm_op::block);
            append_u8(c, k_val_f32);  // B2

            op(c, wasm_op::i32_const);
            i32(c, 0x33333333);  // junk: must be dropped by branch unwind
            op(c, wasm_op::f32_const);
            f32(c, 10.0f);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            emit_br_table_012_default2(c);

            op(c, wasm_op::end);  // end B2
            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::f32_add);

            op(c, wasm_op::end);  // end B1
            op(c, wasm_op::f32_const);
            f32(c, 2.0f);
            op(c, wasm_op::f32_add);

            op(c, wasm_op::end);  // end B0
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f64 br_table with result values (and extra junk to force unwind).
        // selector=0 => 13.0; selector=1 => 12.0; else => 10.0
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_f64);  // B0
            op(c, wasm_op::block);
            append_u8(c, k_val_f64);  // B1
            op(c, wasm_op::block);
            append_u8(c, k_val_f64);  // B2

            op(c, wasm_op::i32_const);
            i32(c, 0x44444444);  // junk: must be dropped by branch unwind
            op(c, wasm_op::f64_const);
            f64(c, 10.0);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            emit_br_table_012_default2(c);

            op(c, wasm_op::end);  // end B2
            op(c, wasm_op::f64_const);
            f64(c, 1.0);
            op(c, wasm_op::f64_add);

            op(c, wasm_op::end);  // end B1
            op(c, wasm_op::f64_const);
            f64(c, 2.0);
            op(c, wasm_op::f64_add);

            op(c, wasm_op::end);  // end B0
            op(c, wasm_op::end);  // end func

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_br_table_with_values_suite(runtime_module_t const& rt) noexcept
    {
        if constexpr(Opt.is_tail_call)
        {
            static_assert(compiler::details::interpreter_tuple_has_no_holes<Opt>());
        }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        auto run_i32 = [&](::std::size_t fi, ::std::int32_t selector) noexcept
        {
            return load_i32(Runner::run(cm.local_funcs.index_unchecked(fi),
                                        rt.local_defined_function_vec_storage.index_unchecked(fi),
                                        pack_i32(selector),
                                        nullptr,
                                        nullptr)
                                .results);
        };
        auto run_i64 = [&](::std::size_t fi, ::std::int32_t selector) noexcept
        {
            return load_i64(Runner::run(cm.local_funcs.index_unchecked(fi),
                                        rt.local_defined_function_vec_storage.index_unchecked(fi),
                                        pack_i32(selector),
                                        nullptr,
                                        nullptr)
                                .results);
        };
        auto run_f32 = [&](::std::size_t fi, ::std::int32_t selector) noexcept
        {
            return load_f32(Runner::run(cm.local_funcs.index_unchecked(fi),
                                        rt.local_defined_function_vec_storage.index_unchecked(fi),
                                        pack_i32(selector),
                                        nullptr,
                                        nullptr)
                                .results);
        };
        auto run_f64 = [&](::std::size_t fi, ::std::int32_t selector) noexcept
        {
            return load_f64(Runner::run(cm.local_funcs.index_unchecked(fi),
                                        rt.local_defined_function_vec_storage.index_unchecked(fi),
                                        pack_i32(selector),
                                        nullptr,
                                        nullptr)
                                .results);
        };

        // f0 i32
        UWVM2TEST_REQUIRE(run_i32(0uz, 0) == 13);
        UWVM2TEST_REQUIRE(run_i32(0uz, 1) == 12);
        UWVM2TEST_REQUIRE(run_i32(0uz, 2) == 10);
        UWVM2TEST_REQUIRE(run_i32(0uz, 3) == 10);
        UWVM2TEST_REQUIRE(run_i32(0uz, 100) == 10);

        // f1 i64
        UWVM2TEST_REQUIRE(run_i64(1uz, 0) == 13);
        UWVM2TEST_REQUIRE(run_i64(1uz, 1) == 12);
        UWVM2TEST_REQUIRE(run_i64(1uz, 2) == 10);
        UWVM2TEST_REQUIRE(run_i64(1uz, 100) == 10);

        // f2 f32
        UWVM2TEST_REQUIRE(run_f32(2uz, 0) == 13.0f);
        UWVM2TEST_REQUIRE(run_f32(2uz, 1) == 12.0f);
        UWVM2TEST_REQUIRE(run_f32(2uz, 2) == 10.0f);
        UWVM2TEST_REQUIRE(run_f32(2uz, 100) == 10.0f);

        // f3 f64
        UWVM2TEST_REQUIRE(run_f64(3uz, 0) == 13.0);
        UWVM2TEST_REQUIRE(run_f64(3uz, 1) == 12.0);
        UWVM2TEST_REQUIRE(run_f64(3uz, 2) == 10.0);
        UWVM2TEST_REQUIRE(run_f64(3uz, 100) == 10.0);

        return 0;
    }

    [[nodiscard]] int test_translate_br_table_with_values() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_table_with_values_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_table_with_values");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref (also assert br_table opfunc is present and executed).
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
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(1),
                                  exp_br_table,
                                  nullptr);
            UWVM2TEST_REQUIRE(rr.hit_expected0);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 12);

            UWVM2TEST_REQUIRE(run_br_table_with_values_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching) + strict br_table opfunc present.
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
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_br_table));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_br_table));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_br_table));

            UWVM2TEST_REQUIRE(run_br_table_with_values_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching (exercise cached-index selection and unwind with values).
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

            UWVM2TEST_REQUIRE(run_br_table_with_values_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_br_table_with_values();
}

