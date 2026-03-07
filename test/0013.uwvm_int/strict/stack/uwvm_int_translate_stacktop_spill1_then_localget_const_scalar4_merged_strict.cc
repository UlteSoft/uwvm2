#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

    [[nodiscard]] byte_vec build_stacktop_spill1_then_localget_const_scalar4_merged_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: (param i64 x) -> (result i64)
        // Fill scalar4 merged ring (size 2) with {f32,i32}; then local.get(i64) forces spill1 of f32 and should be fused.
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local#1: i64 tmp
            auto& c = fb.code;

            op(c, wasm_op::f32_const); f32(c, 1.25f);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::nop);  // break pending windows

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);  // prevent local.get being fused away
            op(c, wasm_op::local_set); u32(c, 1u);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i64 x) -> (result i64)
        // Fill scalar4 merged ring (size 2) with {f64,i32}; then local.get(i64) forces spill1 of f64 and should be fused.
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local#1: i64 tmp
            auto& c = fb.code;

            op(c, wasm_op::f64_const); f64(c, 2.5);
            op(c, wasm_op::i32_const); i32(c, -3);
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_set); u32(c, 1u);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (param f32 x) -> (result f32)
        // Fill scalar4 merged ring (size 2) with {i64,i32}; then local.get(f32) forces spill1 of i64 and should be fused.
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local#1: f32 tmp
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 0x1234'5678'9abc'def0ll);
            op(c, wasm_op::i32_const); i32(c, 99);
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_set); u32(c, 1u);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (param f64 x) -> (result f64)
        // Fill scalar4 merged ring (size 2) with {i64,i32}; then local.get(f64) forces spill1 of i64 and should be fused.
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local#1: f64 tmp
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, -42);
            op(c, wasm_op::i32_const); i32(c, 17);
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_set); u32(c, 1u);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: () -> (result f32)
        // Fill scalar4 merged ring (size 2) with {i64,i32}; then f32.const forces spill1 of i64 and should be fused.
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});  // local#0: f32 tmp
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 123);
            op(c, wasm_op::i32_const); i32(c, 456);
            op(c, wasm_op::nop);

            op(c, wasm_op::f32_const); f32(c, 3.25f);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: () -> (result f64)
        // Fill scalar4 merged ring (size 2) with {i64,i32}; then f64.const forces spill1 of i64 and should be fused.
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local#0: f64 tmp
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, -7);
            op(c, wasm_op::i32_const); i32(c, 0x7fffffff);
            op(c, wasm_op::nop);

            op(c, wasm_op::f64_const); f64(c, 6.5);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: () -> (result i64)
        // Fill scalar4 merged ring (size 2) with {f32,i32}; then i64.const forces spill1 of f32 and should be fused.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local#0: i64 tmp
            auto& c = fb.code;

            op(c, wasm_op::f32_const); f32(c, 0.75f);
            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::nop);

            op(c, wasm_op::i64_const); i64(c, 0x0123'4567'89ab'cdefll);
            op(c, wasm_op::nop);  // prevent i64.const being fused into local.set
            op(c, wasm_op::local_set); u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: () -> (result i64)
        // Fill scalar4 merged ring (size 2) with {f64,i32}; then i64.const forces spill1 of f64 and should be fused.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});  // local#0: i64 tmp
            auto& c = fb.code;

            op(c, wasm_op::f64_const); f64(c, -0.25);
            op(c, wasm_op::i32_const); i32(c, -9);
            op(c, wasm_op::nop);

            op(c, wasm_op::i64_const); i64(c, -123456789);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_set); u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: () -> (result i32)
        // Fill scalar4 merged ring (size 2) with {f64,i32}; then i32.const forces spill1 of f64 and should be fused.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local#0: i32 tmp
            auto& c = fb.code;

            op(c, wasm_op::f64_const); f64(c, 1.0);
            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::nop);

            op(c, wasm_op::i32_const); i32(c, 42);
            op(c, wasm_op::nop);  // prevent i32.const being fused into local.set
            op(c, wasm_op::local_set); u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 9uz);

        using Runner = interpreter_runner<Opt>;

        // f0/f1: i64 -> i64
        for(::std::int64_t x : {0ll, 1ll, -1ll, 0x7fff'ffff'ffff'ffffll, static_cast<::std::int64_t>(0x8000'0000'0000'0000ull)})
        {
            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i64(x),
                                                  nullptr,
                                                  nullptr)
                                           .results) == x);
            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_i64(x),
                                                  nullptr,
                                                  nullptr)
                                           .results) == x);
        }

        // f2: f32 -> f32
        for(float x : {0.0f, 1.0f, -2.0f, 3.5f, 123.25f})
        {
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_f32(x),
                                                  nullptr,
                                                  nullptr)
                                           .results) == x);
        }

        // f3: f64 -> f64
        for(double x : {0.0, 1.0, -2.0, 3.5, 123.25})
        {
            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(3),
                                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                                  pack_f64(x),
                                                  nullptr,
                                                  nullptr)
                                           .results) == x);
        }

        // f4: 3.25f
        {
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(4),
                                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 3.25f);
        }

        // f5: 6.5
        {
            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(5),
                                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 6.5);
        }

        // f6: const i64
        {
            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(6),
                                                  rt.local_defined_function_vec_storage.index_unchecked(6),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 0x0123'4567'89ab'cdefll);
        }

        // f7: const i64
        {
            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(7),
                                                  rt.local_defined_function_vec_storage.index_unchecked(7),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == -123456789);
        }

        // f8: 42
        {
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(8),
                                                  rt.local_defined_function_vec_storage.index_unchecked(8),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 42);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_stacktop_spill1_then_localget_const_scalar4_merged() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_spill1_then_localget_const_scalar4_merged_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_spill1_then_localget_const_scalar4_merged");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref semantics smoke
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // tailcall + stacktop caching: scalar4 fully merged ring (tiny => force spill).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
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

            auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
            auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
            auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
            auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
            auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;
            auto const& bc5 = cm.local_funcs.index_unchecked(5).op.operands;
            auto const& bc6 = cm.local_funcs.index_unchecked(6).op.operands;
            auto const& bc7 = cm.local_funcs.index_unchecked(7).op.operands;
            auto const& bc8 = cm.local_funcs.index_unchecked(8).op.operands;

            bool ok0{};
            bool ok1{};
            bool ok2{};
            bool ok3{};
            bool ok4{};
            bool ok5{};
            bool ok6{};
            bool ok7{};
            bool ok8{};
            for(::std::size_t pos = opt.i32_stack_top_begin_pos; pos < opt.i32_stack_top_end_pos; ++pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{
                    .i32_stack_top_curr_pos = pos,
                    .i64_stack_top_curr_pos = pos,
                    .f32_stack_top_curr_pos = pos,
                    .f64_stack_top_curr_pos = pos,
                    .v128_stack_top_curr_pos = SIZE_MAX,
                };

                auto const expect0 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_f32, wasm_i64>(curr, tuple);
                auto const expect1 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_f64, wasm_i64>(curr, tuple);
                auto const expect2 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_i64, wasm_f32>(curr, tuple);
                auto const expect3 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_i64, wasm_f64>(curr, tuple);
                auto const expect4 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_i64, wasm_f32>(curr, tuple);
                auto const expect5 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_i64, wasm_f64>(curr, tuple);
                auto const expect6 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_f32, wasm_i64>(curr, tuple);
                auto const expect7 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_f64, wasm_i64>(curr, tuple);
                auto const expect8 =
                    optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_f64, wasm_i32>(curr, tuple);

                if(bytecode_contains_fptr(bc0, expect0)) { ok0 = true; }
                if(bytecode_contains_fptr(bc1, expect1)) { ok1 = true; }
                if(bytecode_contains_fptr(bc2, expect2)) { ok2 = true; }
                if(bytecode_contains_fptr(bc3, expect3)) { ok3 = true; }
                if(bytecode_contains_fptr(bc4, expect4)) { ok4 = true; }
                if(bytecode_contains_fptr(bc5, expect5)) { ok5 = true; }
                if(bytecode_contains_fptr(bc6, expect6)) { ok6 = true; }
                if(bytecode_contains_fptr(bc7, expect7)) { ok7 = true; }
                if(bytecode_contains_fptr(bc8, expect8)) { ok8 = true; }
            }

            UWVM2TEST_REQUIRE(ok0);
            UWVM2TEST_REQUIRE(ok1);
            UWVM2TEST_REQUIRE(ok2);
            UWVM2TEST_REQUIRE(ok3);
            UWVM2TEST_REQUIRE(ok4);
            UWVM2TEST_REQUIRE(ok5);
            UWVM2TEST_REQUIRE(ok6);
            UWVM2TEST_REQUIRE(ok7);
            UWVM2TEST_REQUIRE(ok8);
#endif

            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_spill1_then_localget_const_scalar4_merged();
}
