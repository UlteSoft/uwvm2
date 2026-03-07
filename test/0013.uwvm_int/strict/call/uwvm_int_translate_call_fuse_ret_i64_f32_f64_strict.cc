#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

    [[nodiscard]] byte_vec build_call_fuse_ret_types_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // 0: callee_i64: () -> (i64) : i64.const 123 ; end
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            op(fb.code, wasm_op::i64_const);
            i64(fb.code, 123);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 1: callee_f32: () -> (f32) : f32.const 1.25 ; end
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::f32_const);
            f32(fb.code, 1.25f);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 2: callee_f64: () -> (f64) : f64.const 2.5 ; end
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            op(fb.code, wasm_op::f64_const);
            f64(fb.code, 2.5);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // i64 callers (call+drop/local.set/local.tee)
        {
            // 3: call_drop_i64: () -> () : call 0 ; drop ; end
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::call);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::drop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            // 4: call_local_set_i64: () -> () : (local i64) call 0 ; local.set 0 ; end
            func_type ty{{}, {}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            op(fb.code, wasm_op::call);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            // 5: call_local_tee_i64: () -> (i64) : (local i64) call 0 ; local.tee 0 ; end
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            op(fb.code, wasm_op::call);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f32 callers
        {
            // 6: call_drop_f32: () -> () : call 1 ; drop ; end
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::call);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::drop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            // 7: call_local_set_f32: () -> () : (local f32) call 1 ; local.set 0 ; end
            func_type ty{{}, {}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});
            op(fb.code, wasm_op::call);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            // 8: call_local_tee_f32: () -> (f32) : (local f32) call 1 ; local.tee 0 ; end
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});
            op(fb.code, wasm_op::call);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f64 callers
        {
            // 9: call_drop_f64: () -> () : call 2 ; drop ; end
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::call);
            u32(fb.code, 2u);
            op(fb.code, wasm_op::drop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            // 10: call_local_set_f64: () -> () : (local f64) call 2 ; local.set 0 ; end
            func_type ty{{}, {}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});
            op(fb.code, wasm_op::call);
            u32(fb.code, 2u);
            op(fb.code, wasm_op::local_set);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        {
            // 11: call_local_tee_f64: () -> (f64) : (local f64) call 2 ; local.tee 0 ; end
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});
            op(fb.code, wasm_op::call);
            u32(fb.code, 2u);
            op(fb.code, wasm_op::local_tee);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_call_fuse_ret_types() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_call_fuse_ret_types_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_call_fuse_ret_types");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 12uz);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

        constexpr auto i64_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<opt, wasm_i64>(curr, tuple);
        constexpr auto i64_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<opt, wasm_i64>(curr, tuple);
        constexpr auto i64_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<opt, wasm_i64>(curr, tuple);

        constexpr auto f32_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<opt, wasm_f32>(curr, tuple);
        constexpr auto f32_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<opt, wasm_f32>(curr, tuple);
        constexpr auto f32_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<opt, wasm_f32>(curr, tuple);

        constexpr auto f64_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<opt, wasm_f64>(curr, tuple);
        constexpr auto f64_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<opt, wasm_f64>(curr, tuple);
        constexpr auto f64_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<opt, wasm_f64>(curr, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, i64_drop));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, i64_lset));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, i64_ltee));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, f32_drop));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, f32_lset));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, f32_ltee));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, f64_drop));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, f64_lset));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, f64_ltee));
#endif

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_call_fuse_ret_types();
}

