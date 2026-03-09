#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_i64_ring1_opt{
        .is_tail_call = true,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 5uz,
        .i64_stack_top_end_pos = 6uz,
        .f32_stack_top_begin_pos = 6uz,
        .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 7uz,
        .f64_stack_top_end_pos = 8uz,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX,
    };

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_i64_pushes(::std::size_t push_count) noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.i64_stack_top_begin_pos != SIZE_MAX && Opt.i64_stack_top_begin_pos != Opt.i64_stack_top_end_pos)
        {
            for(::std::size_t i{}; i != push_count; ++i)
            {
                curr.i64_stack_top_curr_pos =
                    optable::details::ring_prev_pos(curr.i64_stack_top_curr_pos, Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_end_pos);
            }
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_i64_spill1(ByteStorage const& bc) noexcept
    {
        if constexpr(Opt.i64_stack_top_begin_pos == SIZE_MAX || Opt.i64_stack_top_begin_pos == Opt.i64_stack_top_end_pos)
        {
            return false;
        }
        else
        {
            using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, wasm_i64>();
                start_pos < optable::details::stacktop_range_end_pos<Opt, wasm_i64>();
                ++start_pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                optable::uwvm_interpreter_stacktop_remain_size_t remain{};
                curr.i64_stack_top_curr_pos = start_pos;
                remain.i64_stack_top_remain_size = 1uz;

                auto const fptr = optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<Opt, wasm_i64>(curr, remain, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }
            return false;
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_any_i64_local_set(ByteStorage const& bc) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        if constexpr(Opt.i64_stack_top_begin_pos == SIZE_MAX || Opt.i64_stack_top_begin_pos == Opt.i64_stack_top_end_pos)
        {
            constexpr auto curr = make_initial_stacktop_currpos<Opt>();
            auto const fptr = optable::translate::get_uwvmint_local_set_i64_fptr_from_tuple<Opt>(curr, tuple);
            return bytecode_contains_fptr(bc, fptr);
        }
        else
        {
            for(::std::size_t pos = Opt.i64_stack_top_begin_pos; pos < Opt.i64_stack_top_end_pos; ++pos)
            {
                auto curr = make_initial_stacktop_currpos<Opt>();
                curr.i64_stack_top_curr_pos = pos;
                auto const fptr = optable::translate::get_uwvmint_local_set_i64_fptr_from_tuple<Opt>(curr, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }
            return false;
        }
    }

    [[nodiscard]] byte_vec build_memory_i64_load_localget_survivor_nonzero_offset_module()
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
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 224);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x1122334455667788ull));
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 16u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 336);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x0123456789abcdefull));
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 24u);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 448);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x0102030405060708ull));
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 5);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 40u);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 560);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x1021324354657687ull));
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 48u);
            op(c, wasm_op::i64_add);
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

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(208),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x1122334455667788ull);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32(312),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x0123456789abcdefull);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32(408),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x010203040506070dull);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_i32(512),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr.results)) == 0x102132435465768eull);
        }

        return 0;
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem,
                                          bool expect_spill_in_survivor_funcs) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_initial = make_initial_stacktop_currpos<Opt>();
        constexpr auto curr_after_i64_1 = make_curr_after_i64_pushes<Opt>(1uz);

        auto const exp_initial_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_initial, tuple);
        auto const exp_survivor_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_after_i64_1, tuple);

        auto const exp_f0_fused = optable::translate::get_uwvmint_i64_load_localget_off_fptr_from_tuple<Opt>(curr_initial, mem, tuple);
        auto const exp_f1_fused = optable::translate::get_uwvmint_i64_load_localget_set_local_fptr_from_tuple<Opt>(curr_initial, mem, tuple);
        auto const exp_f2_fused = optable::translate::get_uwvmint_i64_load_localget_set_local_fptr_from_tuple<Opt>(curr_after_i64_1, mem, tuple);
        auto const exp_f3_fused = optable::translate::get_uwvmint_i64_load_localget_off_fptr_from_tuple<Opt>(curr_after_i64_1, mem, tuple);

        auto const exp_f0_plain = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr_initial, mem, tuple);
        auto const exp_f1_plain = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr_initial, mem, tuple);
        auto const exp_f2_plain = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr_after_i64_1, mem, tuple);
        auto const exp_f3_plain = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr_after_i64_1, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_f0_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_f0_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_initial_local_get_i32));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_f1_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_f1_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_initial_local_get_i32));
        UWVM2TEST_REQUIRE(!contains_any_i64_local_set<Opt>(bc1));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_f2_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_f2_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_survivor_local_get_i32));
        UWVM2TEST_REQUIRE(!contains_any_i64_local_set<Opt>(bc2));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_f3_fused));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_f3_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_survivor_local_get_i32));

        if(expect_spill_in_survivor_funcs)
        {
            UWVM2TEST_REQUIRE((contains_i64_spill1<Opt>(bc2)));
            UWVM2TEST_REQUIRE((contains_i64_spill1<Opt>(bc3)));
        }
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

        auto const exp_plain_load = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_local_set_i64 = optable::translate::get_uwvmint_local_set_i64_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_local_get_i64 = optable::translate::get_uwvmint_local_get_i64_fptr_from_tuple<Opt>(curr, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_plain_load));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_plain_load));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_local_set_i64));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_local_get_i64));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_plain_load));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_set_i64));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_get_i64));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_plain_load));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt,
                                            bool verify_bytecode,
                                            bool expect_spill_in_survivor_funcs) noexcept
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
                UWVM2TEST_REQUIRE(check_tail_bytecode<Opt>(cm, mem, expect_spill_in_survivor_funcs) == 0);
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

    [[nodiscard]] int test_translate_memory_i64_load_localget_survivor_nonzero_offset() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_i64_load_localget_survivor_nonzero_offset_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_i64_load_localget_survivor_nonzero_offset");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true, false) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true, false) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true, false) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true, false) == 0);
        }

        UWVM2TEST_REQUIRE(compile_check_and_run<k_test_tail_i64_ring1_opt>(rt, true, true) == 0);
        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_i64_load_localget_survivor_nonzero_offset();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
