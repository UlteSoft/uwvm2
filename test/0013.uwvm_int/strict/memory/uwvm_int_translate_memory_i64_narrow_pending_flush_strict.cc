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

    [[nodiscard]] byte_vec pack_i64_i32_i64(::std::int64_t a, ::std::int32_t b, ::std::int64_t c)
    {
        byte_vec out(20);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 12, ::std::addressof(c), 8);
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

    [[nodiscard]] byte_vec build_memory_i64_narrow_pending_flush_module()
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

        // f0: local.get seed ; local.get addr ; i64.load8_s (must flush local_get2, no fuse)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 64);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x80ull));
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load8_s); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get seed ; local.get addr ; i64.load8_u
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 80);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0xABull));
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get seed ; local.get addr ; i64.load16_s
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 96);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x8001ull));
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load16_s); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get seed ; local.get addr ; i64.load16_u
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 112);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0xCDEFull));
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: local.get seed ; local.get addr ; i64.load32_s
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 128);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x80000001ull));
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load32_s); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: local.get seed ; local.get addr ; i64.load32_u
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 144);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x89abcdefull));
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::i64_extend_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: local.get addr ; local.get value ; i64.store8 (must flush delayed mixed-type local.get chain)
        {
            func_type ty{{k_val_i64, k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 160);
            op(c, wasm_op::i64_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: local.get addr ; local.get value ; i64.store16
        {
            func_type ty{{k_val_i64, k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 176);
            op(c, wasm_op::i64_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 8uz);
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32x2(11, 64),
                                              nullptr,
                                              nullptr)
                                       .results) == -117ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32x2(22, 80),
                                              nullptr,
                                              nullptr)
                                       .results) == 193ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(33, 96),
                                              nullptr,
                                              nullptr)
                                       .results) == -32734ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2(44, 112),
                                              nullptr,
                                              nullptr)
                                       .results) == 52763ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32x2(55, 128),
                                              nullptr,
                                              nullptr)
                                       .results) == -2147483592ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i32x2(66, 144),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int64_t>(2309738033ull));

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i64_i32_i64(9, 160, static_cast<::std::int64_t>(0x1234abull)),
                                              nullptr,
                                              nullptr)
                                       .results) == 180ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_i64_i32_i64(10, 176, static_cast<::std::int64_t>(0x1234cdefull)),
                                              nullptr,
                                              nullptr)
                                       .results) == 52729ll);

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename MemoryRef>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm, MemoryRef const& mem) noexcept
    {
        constexpr auto curr_load = make_curr_after_i32_pushes<Opt>(2);
        constexpr auto curr_store = make_curr_after_i32_i64_pushes<Opt>(1, 1);
        constexpr auto curr_initial = make_initial_stacktop_currpos<Opt>();

        if constexpr(Opt.is_tail_call)
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            auto const exp_load8_s = optable::translate::get_uwvmint_i64_load8_s_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_load8_u = optable::translate::get_uwvmint_i64_load8_u_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_load16_s = optable::translate::get_uwvmint_i64_load16_s_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_load16_u = optable::translate::get_uwvmint_i64_load16_u_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_load32_s = optable::translate::get_uwvmint_i64_load32_s_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_load32_u = optable::translate::get_uwvmint_i64_load32_u_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_store8 = optable::translate::get_uwvmint_i64_store8_fptr_from_tuple<Opt>(curr_store, mem, tuple);
            auto const exp_store16 = optable::translate::get_uwvmint_i64_store16_fptr_from_tuple<Opt>(curr_store, mem, tuple);
            auto const exp_store_localget = optable::translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<Opt>(curr_initial, mem, tuple);
            auto const exp_store32_localget = optable::translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<Opt>(curr_initial, mem, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_load8_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_load8_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_load16_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_load16_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_load32_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_load32_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_store8));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_store16));
            UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_store_localget));
            UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_store32_localget));
            UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_store_localget));
            UWVM2TEST_REQUIRE(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_store32_localget));
        }
        else
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            auto const exp_load8_s = optable::translate::get_uwvmint_i64_load8_s_fptr_from_tuple<Opt>(curr_initial, tuple);
            auto const exp_load8_u = optable::translate::get_uwvmint_i64_load8_u_fptr_from_tuple<Opt>(curr_initial, tuple);
            auto const exp_load16_s = optable::translate::get_uwvmint_i64_load16_s_fptr_from_tuple<Opt>(curr_initial, tuple);
            auto const exp_load16_u = optable::translate::get_uwvmint_i64_load16_u_fptr_from_tuple<Opt>(curr_initial, tuple);
            auto const exp_load32_s = optable::translate::get_uwvmint_i64_load32_s_fptr_from_tuple<Opt>(curr_initial, tuple);
            auto const exp_load32_u = optable::translate::get_uwvmint_i64_load32_u_fptr_from_tuple<Opt>(curr_initial, tuple);
            auto const exp_store8 = optable::translate::get_uwvmint_i64_store8_fptr_from_tuple<Opt>(curr_initial, tuple);
            auto const exp_store16 = optable::translate::get_uwvmint_i64_store16_fptr_from_tuple<Opt>(curr_initial, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_load8_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_load8_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_load16_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_load16_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_load32_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_load32_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_store8));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_store16));
        }

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
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, mem) == 0);
        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_i64_narrow_pending_flush() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_i64_narrow_pending_flush_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_i64_narrow_pending_flush");
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
        return test_translate_memory_i64_narrow_pending_flush();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
