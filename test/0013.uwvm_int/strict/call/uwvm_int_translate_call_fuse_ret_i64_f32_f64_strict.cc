#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

    template <optable::uwvm_interpreter_translate_option_t CompileOption>
    static void call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        using info_t = optable::compiled_defined_call_info;

        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        if(wasm_module_id != SIZE_MAX) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        auto const* const info = reinterpret_cast<info_t const*>(call_function);
        if(info == nullptr || info->compiled_func == nullptr || info->runtime_func == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        auto* const top = *stack_top_ptr;
        auto const top_addr = reinterpret_cast<::std::uintptr_t>(top);
        if(top_addr < info->param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::std::byte* const base = reinterpret_cast<::std::byte*>(top_addr - info->param_bytes);

        byte_vec packed_params(info->param_bytes);
        if(info->param_bytes != 0uz) { ::std::memcpy(packed_params.data(), base, info->param_bytes); }

        using Runner = interpreter_runner<CompileOption>;
        auto rr = Runner::run(*info->compiled_func,
                              *reinterpret_cast<runtime_local_func_t const*>(info->runtime_func),
                              packed_params,
                              nullptr,
                              nullptr);

        if(rr.results.size() != info->result_bytes) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        if(info->result_bytes != 0uz)
        {
            ::std::memcpy(base, rr.results.data(), info->result_bytes);
        }
        *stack_top_ptr = base + info->result_bytes;
    }

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

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        optable::call_func = +call_bridge<Opt>;
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 12uz);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos == SIZE_MAX && Opt.i64_stack_top_begin_pos == SIZE_MAX &&
                     Opt.f32_stack_top_begin_pos == SIZE_MAX && Opt.f64_stack_top_begin_pos == SIZE_MAX && Opt.v128_stack_top_begin_pos == SIZE_MAX)
        {
            constexpr auto curr{make_initial_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto i64_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<Opt, wasm_i64>(curr, tuple);
            constexpr auto i64_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<Opt, wasm_i64>(curr, tuple);
            constexpr auto i64_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<Opt, wasm_i64>(curr, tuple);

            constexpr auto f32_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<Opt, wasm_f32>(curr, tuple);
            constexpr auto f32_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<Opt, wasm_f32>(curr, tuple);
            constexpr auto f32_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<Opt, wasm_f32>(curr, tuple);

            constexpr auto f64_drop = optable::translate::get_uwvmint_call_drop_fptr_from_tuple<Opt, wasm_f64>(curr, tuple);
            constexpr auto f64_lset = optable::translate::get_uwvmint_call_local_set_fptr_from_tuple<Opt, wasm_f64>(curr, tuple);
            constexpr auto f64_ltee = optable::translate::get_uwvmint_call_local_tee_fptr_from_tuple<Opt, wasm_f64>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, i64_drop));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, i64_lset));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, i64_ltee));

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, f32_drop));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, f32_lset));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(8).op.operands, f32_ltee));

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(9).op.operands, f64_drop));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(10).op.operands, f64_lset));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(11).op.operands, f64_ltee));
        }
#endif

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0), rt.local_defined_function_vec_storage.index_unchecked(0), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr0.results) == 123);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1), rt.local_defined_function_vec_storage.index_unchecked(1), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_f32(rr1.results) == 1.25f);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2), rt.local_defined_function_vec_storage.index_unchecked(2), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_f64(rr2.results) == 2.5);

        auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3), rt.local_defined_function_vec_storage.index_unchecked(3), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(rr3.results.empty());

        auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4), rt.local_defined_function_vec_storage.index_unchecked(4), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(rr4.results.empty());

        auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5), rt.local_defined_function_vec_storage.index_unchecked(5), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr5.results) == 123);

        auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6), rt.local_defined_function_vec_storage.index_unchecked(6), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(rr6.results.empty());

        auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7), rt.local_defined_function_vec_storage.index_unchecked(7), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(rr7.results.empty());

        auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8), rt.local_defined_function_vec_storage.index_unchecked(8), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_f32(rr8.results) == 1.25f);

        auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9), rt.local_defined_function_vec_storage.index_unchecked(9), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(rr9.results.empty());

        auto rr10 = Runner::run(cm.local_funcs.index_unchecked(10), rt.local_defined_function_vec_storage.index_unchecked(10), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(rr10.results.empty());

        auto rr11 = Runner::run(cm.local_funcs.index_unchecked(11), rt.local_defined_function_vec_storage.index_unchecked(11), pack_no_params(), nullptr, nullptr);
        UWVM2TEST_REQUIRE(load_f64(rr11.results) == 2.5);

        return 0;
    }

    [[nodiscard]] int test_translate_call_fuse_ret_types() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        auto const wasm = build_call_fuse_ret_types_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_call_fuse_ret_types");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_call_fuse_ret_types();
}
