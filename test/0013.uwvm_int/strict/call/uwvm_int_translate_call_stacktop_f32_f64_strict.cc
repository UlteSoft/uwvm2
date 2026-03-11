#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

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
            // This strict test only supports local-defined calls.
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

    [[nodiscard]] byte_vec build_call_stacktop_float_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // 0: add_f32(x) => x + 1.25
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_const); f32(c, 1.25f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 1: mul_add_f32(a,b) => a*b + 0.5
        {
            func_type ty{{k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_const); f32(c, 0.5f);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 2: promote_f32_to_f64(x) => f64.promote_f32(x)
        {
            func_type ty{{k_val_f32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_promote_f32);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 3: demote_f64_to_f32(x) => f32.demote_f64(x)
        {
            func_type ty{{k_val_f64}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_demote_f64);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 4: add_f64(x) => x + 2.0
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_const); f64(c, 2.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 5: mul_add_f64(a,b) => a*b - 1.0
        {
            func_type ty{{k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_const); f64(c, -1.0);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 6: caller_call_add_f32 => add_f32(3.0) == 4.25
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f32_const); f32(c, 3.0f);
            op(c, wasm_op::call); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 7: caller_call_mul_add_f32 => mul_add_f32(2.0, 4.0) == 8.5
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f32_const); f32(c, 2.0f);
            op(c, wasm_op::f32_const); f32(c, 4.0f);
            op(c, wasm_op::call); u32(c, 1u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 8: caller_call_promote => promote_f32_to_f64(1.5) == 1.5
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f32_const); f32(c, 1.5f);
            op(c, wasm_op::call); u32(c, 2u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 9: caller_call_demote => demote_f64_to_f32(9.25) == 9.25
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f64_const); f64(c, 9.25);
            op(c, wasm_op::call); u32(c, 3u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 10: caller_call_add_f64 => add_f64(1.0) == 3.0
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f64_const); f64(c, 1.0);
            op(c, wasm_op::call); u32(c, 4u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // 11: caller_call_mul_add_f64 => mul_add_f64(2.0, 4.0) == 7.0
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::f64_const); f64(c, 2.0);
            op(c, wasm_op::f64_const); f64(c, 4.0);
            op(c, wasm_op::call); u32(c, 5u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] bool contains_any_fptr(ByteStorage const& bc, Fptr f3, Fptr f4, Fptr f5, Fptr f6) noexcept
    {
        return bytecode_contains_fptr(bc, f3) || bytecode_contains_fptr(bc, f4) || bytecode_contains_fptr(bc, f5) || bytecode_contains_fptr(bc, f6);
    }

    [[nodiscard]] int test_translate_call_stacktop_f32_f64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        auto wasm = build_call_stacktop_float_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_call_stacktop_float");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            optable::call_func = +call_bridge<opt>;
            optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6), rt.local_defined_function_vec_storage.index_unchecked(6), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr6.results) == 4.25f);

            auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7), rt.local_defined_function_vec_storage.index_unchecked(7), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr7.results) == 8.5f);

            auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8), rt.local_defined_function_vec_storage.index_unchecked(8), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr8.results) == 1.5);

            auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9), rt.local_defined_function_vec_storage.index_unchecked(9), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr9.results) == 9.25f);

            auto rr10 =
                Runner::run(cm.local_funcs.index_unchecked(10), rt.local_defined_function_vec_storage.index_unchecked(10), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr10.results) == 3.0);

            auto rr11 =
                Runner::run(cm.local_funcs.index_unchecked(11), rt.local_defined_function_vec_storage.index_unchecked(11), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr11.results) == 7.0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            optable::call_func = +call_bridge<opt>;
            optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6), rt.local_defined_function_vec_storage.index_unchecked(6), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr6.results) == 4.25f);

            auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7), rt.local_defined_function_vec_storage.index_unchecked(7), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr7.results) == 8.5f);

            auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8), rt.local_defined_function_vec_storage.index_unchecked(8), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr8.results) == 1.5);

            auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9), rt.local_defined_function_vec_storage.index_unchecked(9), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr9.results) == 9.25f);

            auto rr10 =
                Runner::run(cm.local_funcs.index_unchecked(10), rt.local_defined_function_vec_storage.index_unchecked(10), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr10.results) == 3.0);

            auto rr11 =
                Runner::run(cm.local_funcs.index_unchecked(11), rt.local_defined_function_vec_storage.index_unchecked(11), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr11.results) == 7.0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            optable::call_func = +call_bridge<opt>;
            optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6), rt.local_defined_function_vec_storage.index_unchecked(6), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr6.results) == 4.25f);

            auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7), rt.local_defined_function_vec_storage.index_unchecked(7), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr7.results) == 8.5f);

            auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8), rt.local_defined_function_vec_storage.index_unchecked(8), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr8.results) == 1.5);

            auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9), rt.local_defined_function_vec_storage.index_unchecked(9), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr9.results) == 9.25f);

            auto rr10 =
                Runner::run(cm.local_funcs.index_unchecked(10), rt.local_defined_function_vec_storage.index_unchecked(10), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr10.results) == 3.0);

            auto rr11 =
                Runner::run(cm.local_funcs.index_unchecked(11), rt.local_defined_function_vec_storage.index_unchecked(11), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr11.results) == 7.0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            optable::call_func = +call_bridge<opt>;
            optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6), rt.local_defined_function_vec_storage.index_unchecked(6), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr6.results) == 4.25f);

            auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7), rt.local_defined_function_vec_storage.index_unchecked(7), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr7.results) == 8.5f);

            auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8), rt.local_defined_function_vec_storage.index_unchecked(8), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr8.results) == 1.5);

            auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9), rt.local_defined_function_vec_storage.index_unchecked(9), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr9.results) == 9.25f);

            auto rr10 =
                Runner::run(cm.local_funcs.index_unchecked(10), rt.local_defined_function_vec_storage.index_unchecked(10), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr10.results) == 3.0);

            auto rr11 =
                Runner::run(cm.local_funcs.index_unchecked(11), rt.local_defined_function_vec_storage.index_unchecked(11), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr11.results) == 7.0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 7uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

            optable::call_func = +call_bridge<opt>;
            optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr optable::uwvm_interpreter_stacktop_currpos_t c3{.i32_stack_top_curr_pos = 3uz,
                                                                      .i64_stack_top_curr_pos = 3uz,
                                                                      .f32_stack_top_curr_pos = 3uz,
                                                                      .f64_stack_top_curr_pos = 3uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c4{.i32_stack_top_curr_pos = 4uz,
                                                                      .i64_stack_top_curr_pos = 4uz,
                                                                      .f32_stack_top_curr_pos = 4uz,
                                                                      .f64_stack_top_curr_pos = 4uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c5{.i32_stack_top_curr_pos = 5uz,
                                                                      .i64_stack_top_curr_pos = 5uz,
                                                                      .f32_stack_top_curr_pos = 5uz,
                                                                      .f64_stack_top_curr_pos = 5uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};
            constexpr optable::uwvm_interpreter_stacktop_currpos_t c6{.i32_stack_top_curr_pos = 6uz,
                                                                      .i64_stack_top_curr_pos = 6uz,
                                                                      .f32_stack_top_curr_pos = 6uz,
                                                                      .f64_stack_top_curr_pos = 6uz,
                                                                      .v128_stack_top_curr_pos = SIZE_MAX};

            // caller_call_add_f32: call_stacktop_f32 ParamCount=1 Ret=wasm_f32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(6).op.operands, f3, f4, f5, f6));
            }

            // caller_call_mul_add_f32: call_stacktop_f32 ParamCount=2 Ret=wasm_f32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, wasm_f32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, wasm_f32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, wasm_f32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 2uz, wasm_f32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(7).op.operands, f3, f4, f5, f6));
            }

            // caller_call_promote: call_stacktop_f32 ParamCount=1 Ret=wasm_f64
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f64>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f64>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f64>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f32_fptr_from_tuple<opt, 1uz, wasm_f64>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(8).op.operands, f3, f4, f5, f6));
            }

            // caller_call_demote: call_stacktop_f64 ParamCount=1 Ret=wasm_f32
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f32>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f32>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f32>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f32>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(9).op.operands, f3, f4, f5, f6));
            }

            // caller_call_add_f64: call_stacktop_f64 ParamCount=1 Ret=wasm_f64
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f64>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f64>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f64>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 1uz, wasm_f64>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(10).op.operands, f3, f4, f5, f6));
            }

            // caller_call_mul_add_f64: call_stacktop_f64 ParamCount=2 Ret=wasm_f64
            {
                constexpr auto f3 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, wasm_f64>(c3, tuple);
                constexpr auto f4 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, wasm_f64>(c4, tuple);
                constexpr auto f5 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, wasm_f64>(c5, tuple);
                constexpr auto f6 = optable::translate::get_uwvmint_call_stacktop_f64_fptr_from_tuple<opt, 2uz, wasm_f64>(c6, tuple);
                UWVM2TEST_REQUIRE(contains_any_fptr(cm.local_funcs.index_unchecked(11).op.operands, f3, f4, f5, f6));
            }
#endif

            using Runner = interpreter_runner<opt>;

            auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6), rt.local_defined_function_vec_storage.index_unchecked(6), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr6.results) == 4.25f);

            auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7), rt.local_defined_function_vec_storage.index_unchecked(7), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr7.results) == 8.5f);

            auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8), rt.local_defined_function_vec_storage.index_unchecked(8), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr8.results) == 1.5);

            auto rr9 = Runner::run(cm.local_funcs.index_unchecked(9), rt.local_defined_function_vec_storage.index_unchecked(9), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr9.results) == 9.25f);

            auto rr10 =
                Runner::run(cm.local_funcs.index_unchecked(10), rt.local_defined_function_vec_storage.index_unchecked(10), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr10.results) == 3.0);

            auto rr11 =
                Runner::run(cm.local_funcs.index_unchecked(11), rt.local_defined_function_vec_storage.index_unchecked(11), pack_no_params(), nullptr, nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr11.results) == 7.0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_call_stacktop_f32_f64();
}
