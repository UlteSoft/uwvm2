#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_memory_narrow_load_byref_no_fuse_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: local.get addr ; i32.load8_s
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_const); i32(c, 0x80);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load8_s); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get addr ; i32.load8_u
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_const); i32(c, 0x80);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get addr ; i32.load16_s
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load16_s); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get addr ; i32.load16_u
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 4uz);
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == -128);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(8),
                                              nullptr,
                                              nullptr)
                                       .results) == 128);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(16),
                                              nullptr,
                                              nullptr)
                                       .results) == -32767);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(24),
                                              nullptr,
                                              nullptr)
                                       .results) == 32769);

        return 0;
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load8_s_fused = optable::translate::get_uwvmint_i32_load8_s_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_load8_u_fused = optable::translate::get_uwvmint_i32_load8_u_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_load16_s_fused = optable::translate::get_uwvmint_i32_load16_s_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_load16_u_fused = optable::translate::get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);

        auto const exp_load8_s_plain = optable::translate::get_uwvmint_i32_load8_s_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_load8_u_plain = optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_load16_s_plain = optable::translate::get_uwvmint_i32_load16_s_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_load16_u_plain = optable::translate::get_uwvmint_i32_load16_u_fptr_from_tuple<Opt>(curr, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_load8_s_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_local_get_i32));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_load8_s_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_load8_u_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_local_get_i32));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_load8_u_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_load16_s_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_local_get_i32));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_load16_s_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_load16_u_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_local_get_i32));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_load16_u_plain));
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load8_s_plain = optable::translate::get_uwvmint_i32_load8_s_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load8_u_plain = optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load16_s_plain = optable::translate::get_uwvmint_i32_load16_s_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_load16_u_plain = optable::translate::get_uwvmint_i32_load16_u_fptr_from_tuple<Opt>(curr, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_load8_s_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_load8_u_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_load16_s_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_load16_u_plain));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt, bool verify_bytecode) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        if(verify_bytecode)
        {
            if constexpr(Opt.is_tail_call)
            {
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
                UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
                auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
                UWVM2TEST_REQUIRE(check_tail_bytecode<Opt>(cm, mem) == 0);
#endif
            }
            else
            {
                UWVM2TEST_REQUIRE(check_byref_bytecode<Opt>(cm) == 0);
            }
        }

        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_narrow_load_byref_no_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_narrow_load_byref_no_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_narrow_load_byref_no_fuse");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_narrow_load_byref_no_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
