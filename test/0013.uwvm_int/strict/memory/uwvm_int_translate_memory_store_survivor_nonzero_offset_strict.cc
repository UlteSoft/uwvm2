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

    [[nodiscard]] byte_vec pack_i32_i64(::std::int32_t a, ::std::int64_t b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_i32_survivor() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        curr.i32_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_i64_survivor() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        curr.i64_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.i64_stack_top_curr_pos, Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_end_pos);
        return curr;
    }

    [[nodiscard]] byte_vec build_memory_store_survivor_nonzero_offset_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 4u);
            op(c, wasm_op::i32_const); i32(c, 104);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x12345678u));
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 8u);
            op(c, wasm_op::i32_const); i32(c, 208);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 11);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::i32_const); i32(c, 316);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 9);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 16u);
            op(c, wasm_op::i32_const); i32(c, 448);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 13);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 20u);
            op(c, wasm_op::i32_const); i32(c, 532);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i64_add);
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
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_i32 = make_curr_after_i32_survivor<Opt>();
        constexpr auto curr_i64 = make_curr_after_i64_survivor<Opt>();

        auto const exp_i32_store_localget =
            optable::translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<Opt>(curr_i32, mem, tuple);
        auto const exp_i32_store_imm =
            optable::translate::get_uwvmint_i32_store_imm_localget_off_fptr_from_tuple<Opt>(curr_i32, mem, tuple);
        auto const exp_i32_store_local_plus_imm =
            optable::translate::get_uwvmint_i32_store_local_plus_imm_fptr_from_tuple<Opt>(curr_i32, mem, tuple);
        auto const exp_i64_store_localget =
            optable::translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
        auto const exp_i64_store32_localget =
            optable::translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<Opt>(curr_i64, mem, tuple);

        auto const exp_i32_store_plain = optable::translate::get_uwvmint_i32_store_fptr_from_tuple<Opt>(curr_i32, mem, tuple);
        auto const exp_i64_store_plain = optable::translate::get_uwvmint_i64_store_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
        auto const exp_i64_store32_plain = optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr_i64, mem, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_store_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_store_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_store_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i64_store_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i64_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i64_store32_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i64_store32_plain));
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_i32 = make_curr_after_i32_survivor<Opt>();
        constexpr auto curr_i64 = make_curr_after_i64_survivor<Opt>();

        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_i32, tuple);
        auto const exp_local_get_i64 = optable::translate::get_uwvmint_local_get_i64_fptr_from_tuple<Opt>(curr_i64, tuple);
        auto const exp_i32_const = optable::translate::get_uwvmint_i32_const_fptr_from_tuple<Opt>(curr_i32, tuple);
        auto const exp_i32_store_plain = optable::translate::get_uwvmint_i32_store_fptr_from_tuple<Opt>(curr_i32, tuple);
        auto const exp_i32_store_local_plus_imm =
            optable::translate::get_uwvmint_i32_store_local_plus_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_i32);
        auto const exp_i64_store_plain = optable::translate::get_uwvmint_i64_store_fptr_from_tuple<Opt>(curr_i64, tuple);
        auto const exp_i64_store32_plain = optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr_i64, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_const));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_store_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_i32_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_local_get_i64));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_i64_store_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_local_get_i64));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i64_store32_plain));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 5uz);
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32x2(100, static_cast<::std::int32_t>(0x11223344u)),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0x11223349u));

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(200),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0x1234567fu));

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(300, 90),
                                              nullptr,
                                              nullptr)
                                       .results) == 101);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32_i64(432, 0x0123456789abcdefll),
                                              nullptr,
                                              nullptr)
                                       .results) == 0x0123456789abcdf8ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32_i64(512, 0x1122334455667788ll),
                                              nullptr,
                                              nullptr)
                                       .results) == 0x55667795ll);

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

        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_store_survivor_nonzero_offset() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_store_survivor_nonzero_offset_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_store_survivor_nonzero_offset");
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
        return test_translate_memory_store_survivor_nonzero_offset();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
