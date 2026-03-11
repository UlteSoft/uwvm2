#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_load_localget_survivor_nonzero_offset_opt{
        .is_tail_call = true,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 6uz,
        .i64_stack_top_begin_pos = 6uz,
        .i64_stack_top_end_pos = 7uz,
        .f32_stack_top_begin_pos = 7uz,
        .f32_stack_top_end_pos = 8uz,
        .f64_stack_top_begin_pos = 8uz,
        .f64_stack_top_end_pos = 9uz,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX,
    };

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
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_f32_survivor() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos)
        {
            curr.f32_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.f32_stack_top_curr_pos, Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_end_pos);
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_f64_survivor() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos)
        {
            curr.f64_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.f64_stack_top_curr_pos, Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_end_pos);
        }
        return curr;
    }

    [[nodiscard]] byte_vec build_memory_load_localget_survivor_nonzero_offset_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: i32 survivor + local.get addr ; i32.load offset=12
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 212);
            op(c, wasm_op::i32_const); i32(c, 1000);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32 survivor + local.get addr ; i32.load offset=20 ; i32.const -5 ; i32.add
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 324);
            op(c, wasm_op::i32_const); i32(c, 100);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 20u);
            op(c, wasm_op::i32_const); i32(c, -5);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: two i32 survivors + local.get addr ; i32.load offset=24 ; i32.const 0xff ; i32.and
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 436);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x123456cdu));
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::i32_const); i32(c, 11);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 24u);
            op(c, wasm_op::i32_const); i32(c, 0xff);
            op(c, wasm_op::i32_and);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        // f3: f32 survivor + local.get addr ; f32.load offset=28
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 532);
            op(c, wasm_op::f32_const); f32(c, 3.5f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::f32_const); f32(c, 1.25f);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 28u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: f64 survivor + local.get addr ; f64.load offset=40
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 680);
            op(c, wasm_op::f64_const); f64(c, 6.25);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::f64_const); f64(c, 1.5);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 40u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
#endif

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 5uz);
#else
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 3uz);
#endif
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(200),
                                              nullptr,
                                              nullptr)
                                       .results) == 1007);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(304),
                                              nullptr,
                                              nullptr)
                                       .results) == 102);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(412),
                                              nullptr,
                                              nullptr)
                                       .results) == 225);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(504),
                                              nullptr,
                                              nullptr)
                                       .results) == 4.75f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32(640),
                                              nullptr,
                                              nullptr)
                                       .results) == 7.75);
#endif
        return 0;
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_i32_1 = make_curr_after_i32_pushes<Opt>(1uz);
        constexpr auto curr_i32_2 = make_curr_after_i32_pushes<Opt>(2uz);
        constexpr auto curr_f32 = make_curr_after_f32_survivor<Opt>();
        constexpr auto curr_f64 = make_curr_after_f64_survivor<Opt>();

        auto const exp_local_get_i32_1 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_i32_1, tuple);
        auto const exp_local_get_i32_2 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_i32_2, tuple);
        auto const exp_local_get_i32_f32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_f32, tuple);
        auto const exp_local_get_i32_f64 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_f64, tuple);

        auto const exp_i32_load_localget = optable::translate::get_uwvmint_i32_load_localget_off_fptr_from_tuple<Opt>(curr_i32_1, mem, tuple);
        auto const exp_i32_load_add_imm = optable::translate::get_uwvmint_i32_load_add_imm_fptr_from_tuple<Opt>(curr_i32_1, mem, tuple);
        auto const exp_i32_load_and_imm = optable::translate::get_uwvmint_i32_load_and_imm_fptr_from_tuple<Opt>(curr_i32_2, mem, tuple);
        auto const exp_i32_load_plain_1 = optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr_i32_1, mem, tuple);
        auto const exp_i32_load_plain_2 = optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr_i32_2, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_i32_load_plain_1));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_local_get_i32_1));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc0, exp_i32_load_localget) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_load_add_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_i32_load_plain_1));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_local_get_i32_1));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc1, exp_i32_load_add_imm) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i32_load_and_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_i32_load_plain_2));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_local_get_i32_2));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc2, exp_i32_load_and_imm) == 1uz);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto const exp_f32_load_localget = optable::translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<Opt>(curr_f32, mem, tuple);
        auto const exp_f64_load_localget = optable::translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<Opt>(curr_f64, mem, tuple);
        auto const exp_f32_load_plain = optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr_f32, mem, tuple);
        auto const exp_f64_load_plain = optable::translate::get_uwvmint_f64_load_fptr_from_tuple<Opt>(curr_f64, mem, tuple);
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
        auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_f32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_f32_load_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_local_get_i32_f32));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc3, exp_f32_load_localget) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc4, exp_f64_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc4, exp_f64_load_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc4, exp_local_get_i32_f64));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc4, exp_f64_load_localget) == 1uz);
#endif
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_i32_1 = make_curr_after_i32_pushes<Opt>(1uz);
        constexpr auto curr_i32_2 = make_curr_after_i32_pushes<Opt>(2uz);
        constexpr auto curr_f32 = make_curr_after_f32_survivor<Opt>();
        constexpr auto curr_f64 = make_curr_after_f64_survivor<Opt>();

        auto const exp_local_get_i32_1 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_i32_1, tuple);
        auto const exp_local_get_i32_2 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_i32_2, tuple);
        auto const exp_local_get_i32_f32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_f32, tuple);
        auto const exp_local_get_i32_f64 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_f64, tuple);

        auto const exp_i32_load_localget =
            optable::translate::get_uwvmint_i32_load_localget_off_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_i32_1);
        auto const exp_i32_load_add_imm =
            optable::translate::get_uwvmint_i32_load_add_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_i32_1);
        auto const exp_i32_load_and_imm =
            optable::translate::get_uwvmint_i32_load_and_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_i32_2);
        auto const exp_i32_load_plain_1 = optable::translate::get_uwvmint_i32_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_i32_1);
        auto const exp_i32_load_plain_2 = optable::translate::get_uwvmint_i32_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_i32_2);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_i32_load_plain_1));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_local_get_i32_1));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc0, exp_i32_load_localget) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_load_add_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_i32_load_plain_1));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_local_get_i32_1));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc1, exp_i32_load_add_imm) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i32_load_and_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_i32_load_plain_2));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_local_get_i32_2));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc2, exp_i32_load_and_imm) == 1uz);

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto const exp_f32_load_localget =
            optable::translate::get_uwvmint_f32_load_localget_off_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_f32);
        auto const exp_f64_load_localget =
            optable::translate::get_uwvmint_f64_load_localget_off_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_f64);
        auto const exp_f32_load_plain = optable::translate::get_uwvmint_f32_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_f32);
        auto const exp_f64_load_plain = optable::translate::get_uwvmint_f64_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_f64);
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
        auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_f32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_f32_load_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_local_get_i32_f32));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc3, exp_f32_load_localget) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc4, exp_f64_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc4, exp_f64_load_plain));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc4, exp_local_get_i32_f64));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc4, exp_f64_load_localget) == 1uz);
#endif
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt, bool verify_bytecode) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if(verify_bytecode)
        {
            if constexpr(Opt.is_tail_call)
            {
                UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
                auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
                UWVM2TEST_REQUIRE(check_tail_bytecode<Opt>(cm, mem) == 0);
            }
            else
            {
                UWVM2TEST_REQUIRE(check_byref_bytecode<Opt>(cm) == 0);
            }
        }
#else
        (void)verify_bytecode;
#endif

        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_load_localget_survivor_nonzero_offset() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_load_localget_survivor_nonzero_offset_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_load_localget_survivor_nonzero_offset");
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

        if(legacy_layouts_enabled())
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true) == 0);
        }

        static_assert(compiler::details::interpreter_tuple_has_no_holes<k_test_tail_load_localget_survivor_nonzero_offset_opt>());
        UWVM2TEST_REQUIRE(compile_check_and_run<k_test_tail_load_localget_survivor_nonzero_offset_opt>(rt, true) == 0);
        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_load_localget_survivor_nonzero_offset();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
