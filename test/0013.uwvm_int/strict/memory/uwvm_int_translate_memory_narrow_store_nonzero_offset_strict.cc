#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] ::std::size_t bytecode_count_fptr(ByteStorage const& bytes, Fptr fptr) noexcept
    {
        if(fptr == nullptr) { return 0uz; }
        ::std::array<::std::byte, sizeof(Fptr)> needle{};
        ::std::memcpy(needle.data(), ::std::addressof(fptr), sizeof(Fptr));
        if(bytes.size() < needle.size()) { return 0uz; }

        ::std::size_t count{};
        for(::std::size_t i{}; i + needle.size() <= bytes.size(); ++i)
        {
            if(::std::memcmp(bytes.data() + i, needle.data(), needle.size()) == 0) { ++count; }
        }
        return count;
    }

    [[nodiscard]] byte_vec build_memory_narrow_store_nonzero_offset_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: local.get addr ; local.get v ; i32.store8 offset=5 ; reload absolute 105
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 5u);

            op(c, wasm_op::i32_const); i32(c, 105);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get addr ; i32.const imm ; i32.store8 offset=9 ; reload absolute 209
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x1234abu));
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 9u);

            op(c, wasm_op::i32_const); i32(c, 209);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get addr ; local.get v ; i32.store16 offset=10 ; reload absolute 310
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 10u);

            op(c, wasm_op::i32_const); i32(c, 310);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get addr ; i32.const imm ; i32.store16 offset=14 ; reload absolute 414
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x1234beefu));
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 14u);

            op(c, wasm_op::i32_const); i32(c, 414);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_store8_plain = optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_store16_plain = optable::translate::get_uwvmint_i32_store16_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_store8_imm = optable::translate::get_uwvmint_i32_store8_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_store16_imm = optable::translate::get_uwvmint_i32_store16_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        auto const exp_store8_localget = optable::translate::get_uwvmint_i32_store8_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_store16_localget = optable::translate::get_uwvmint_i32_store16_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_store8_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_store8_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_local_get_i32));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_store16_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_store16_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_local_get_i32));
# else
        (void)bc0;
        (void)bc2;
# endif

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_store8_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_store8_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_local_get_i32));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_store16_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_store16_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_local_get_i32));

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
        auto const exp_i32_const = optable::translate::get_uwvmint_i32_const_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_store8_plain = optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_store16_plain = optable::translate::get_uwvmint_i32_store16_fptr_from_tuple<Opt>(curr, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_store8_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc0, exp_local_get_i32) == 2uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_store8_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc1, exp_local_get_i32) == 1uz);
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_const));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_store16_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc2, exp_local_get_i32) == 2uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_store16_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc3, exp_local_get_i32) == 1uz);
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_i32_const));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt,
                                            ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 4uz);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            UWVM2TEST_REQUIRE(check_tail_bytecode<Opt>(cm, mem) == 0);
        }
        else
#endif
        {
            UWVM2TEST_REQUIRE(check_byref_bytecode<Opt>(cm) == 0);
        }

        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32x2(100, static_cast<::std::int32_t>(0x1234abu)),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xabu);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(200),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xabu);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(300, static_cast<::std::int32_t>(0xcafeu)),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0xcafeu));

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(400),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0xbeefu));

        return 0;
    }

    [[nodiscard]] int test_translate_memory_narrow_store_nonzero_offset() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_narrow_store_nonzero_offset_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_narrow_store_nonzero_offset");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, mem) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_narrow_store_nonzero_offset();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
