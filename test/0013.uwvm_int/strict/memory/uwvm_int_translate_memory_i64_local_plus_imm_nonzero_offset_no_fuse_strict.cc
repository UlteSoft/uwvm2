#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32_i64(::std::int32_t addr, ::std::int64_t value)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(addr), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(value), 8);
        return out;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_i32_pushes(::std::size_t push_count) noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t i{}; i != push_count; ++i)
            {
                curr.i32_stack_top_curr_pos =
                    optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
            }
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_i32_i64_pushes(::std::size_t i32_push_count,
                                                                                                          ::std::size_t i64_push_count) noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t i{}; i != i32_push_count; ++i)
            {
                curr.i32_stack_top_curr_pos =
                    optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
            }
        }
        if constexpr(Opt.i64_stack_top_begin_pos != SIZE_MAX && Opt.i64_stack_top_begin_pos != Opt.i64_stack_top_end_pos)
        {
            for(::std::size_t i{}; i != i64_push_count; ++i)
            {
                curr.i64_stack_top_curr_pos =
                    optable::details::ring_prev_pos(curr.i64_stack_top_curr_pos, Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_end_pos);
            }
        }
        return curr;
    }

    [[nodiscard]] byte_vec build_memory_i64_local_plus_imm_nonzero_offset_no_fuse_module()
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

        // f0: local.get base ; i32.const 4 ; i32.add ; i64.load offset=12
        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 80);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x1122334455667788ull));
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 12u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get base ; i32.const 8 ; i32.add ; local.get v ; i64.store offset=16 ; reload same addr
        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 16u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 16u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get base ; i32.const 12 ; i32.add ; local.get v ; i64.store32 offset=20 ; reload with i64.load32_u offset=20
        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 12);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 20u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 12);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 20u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 3uz);
        using Runner = interpreter_runner<Opt>;

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(64),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x1122334455667788ull);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32_i64(96, static_cast<::std::int64_t>(0x0102030405060708ull)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x0102030405060708ull);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32_i64(128, static_cast<::std::int64_t>(0x123456789abcdef0ull)),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr.results) == static_cast<::std::int64_t>(0x9abcdef0u));
        }

        return 0;
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto curr_after_i32_1 = make_curr_after_i32_pushes<Opt>(1uz);
        constexpr auto curr_after_i32_2 = make_curr_after_i32_pushes<Opt>(2uz);
        constexpr auto curr_after_i32_i64 = make_curr_after_i32_i64_pushes<Opt>(1uz, 1uz);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_i32_add = optable::translate::get_uwvmint_i32_add_fptr_from_tuple<Opt>(curr_after_i32_2, tuple);
        auto const exp_i32_add_imm_localget = optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_i64_load_plain = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);
        auto const exp_i64_load_localget = optable::translate::get_uwvmint_i64_load_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_store_plain = optable::translate::get_uwvmint_i64_store_fptr_from_tuple<Opt>(curr_after_i32_i64, mem, tuple);
        auto const exp_i64_store_localget = optable::translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_store32_plain = optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr_after_i32_i64, mem, tuple);
        auto const exp_i64_store32_localget = optable::translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_i64_load32_u_plain = optable::translate::get_uwvmint_i64_load32_u_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_add_imm_localget));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i64_load_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_i64_load_localget));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_add));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_add_imm_localget));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i64_store_plain));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i64_load_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_i64_store_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_i64_load_localget));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i32_add));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i32_add_imm_localget));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i64_store32_plain));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i64_load32_u_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_i64_store32_localget));
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto curr = make_initial_stacktop_currpos<Opt>();
        constexpr auto curr_after_i32_1 = make_curr_after_i32_pushes<Opt>(1uz);
        constexpr auto curr_after_i32_2 = make_curr_after_i32_pushes<Opt>(2uz);
        constexpr auto curr_after_i32_i64 = make_curr_after_i32_i64_pushes<Opt>(1uz, 1uz);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_i32_add = optable::translate::get_uwvmint_i32_add_fptr_from_tuple<Opt>(curr_after_i32_2, tuple);
        auto const exp_i32_add_imm_localget = optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_i64_load_plain = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr_after_i32_1, tuple);
        auto const exp_i64_store_plain = optable::translate::get_uwvmint_i64_store_fptr_from_tuple<Opt>(curr_after_i32_i64, tuple);
        auto const exp_i64_store32_plain = optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr_after_i32_i64, tuple);
        auto const exp_i64_load32_u_plain = optable::translate::get_uwvmint_i64_load32_u_fptr_from_tuple<Opt>(curr_after_i32_1, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_add_imm_localget));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i64_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_add));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_add_imm_localget));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i64_store_plain));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i64_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i32_add));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i32_add_imm_localget));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i64_store32_plain));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i64_load32_u_plain));
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

    [[nodiscard]] int test_translate_memory_i64_local_plus_imm_nonzero_offset_no_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_i64_local_plus_imm_nonzero_offset_no_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_i64_local_plus_imm_nonzero_offset_no_fuse");
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
        return test_translate_memory_i64_local_plus_imm_nonzero_offset_no_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
