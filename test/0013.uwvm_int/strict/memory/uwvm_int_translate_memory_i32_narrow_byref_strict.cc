#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_memory_i32_narrow_byref_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: i32.load8_s => -128
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_const); i32(c, 0x80);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_load8_s); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32.load8_u => 128
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_const); i32(c, 0x80);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i32.load16_s => -32767
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i32_load16_s); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i32.load16_u => 32769
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i32.store8 truncation
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 32);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 32);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i32.store16 truncation
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 40);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 40);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_store8 = optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_store16 = optable::translate::get_uwvmint_i32_store16_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load8_s = optable::translate::get_uwvmint_i32_load8_s_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load8_u = optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load16_s = optable::translate::get_uwvmint_i32_load16_s_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load16_u = optable::translate::get_uwvmint_i32_load16_u_fptr_from_tuple<Opt>(curr, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_load8_s));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_load8_u));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_store16));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_load16_s));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_store16));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_load16_u));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_load8_u));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_store16));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_load16_u));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt, bool verify_bytecode) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 6uz);

        if(verify_bytecode)
        {
            if constexpr(!Opt.is_tail_call)
            {
                UWVM2TEST_REQUIRE(check_byref_bytecode<Opt>(cm) == 0);
            }
        }

        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == -128);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 128);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == -32767);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 32769);

        constexpr auto trunc_value = static_cast<::std::int32_t>(0x12345678u);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32(trunc_value),
                                              nullptr,
                                              nullptr)
                                       .results) == 0x78);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i32(trunc_value),
                                              nullptr,
                                              nullptr)
                                       .results) == 0x5678);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_i32_narrow_byref() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_i32_narrow_byref_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_i32_narrow_byref");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_i32_narrow_byref();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
