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

    [[nodiscard]] byte_vec build_memory_localget2_load_nonzero_offset_fuse_module()
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

        // f0: local.get seed ; local.get addr ; i32.load offset=12 ; i32.const 7 ; i32.add ; i32.add
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 212);
            op(c, wasm_op::i32_const); i32(c, 100);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: local.get seed ; local.get addr ; i32.load offset=20 ; i32.const 0x00ff00ff ; i32.and ; i32.add
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 320);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x1234abcdu));
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 20u);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x00ff00ffu));
            op(c, wasm_op::i32_and);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: local.get seed ; local.get addr ; f32.load offset=4 ; local.set tmp ; drop ; local.get seed ; convert ; local.get tmp ; add
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 404);
            op(c, wasm_op::f32_const); f32(c, 3.75f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 4u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_convert_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: local.get seed ; local.get addr ; f64.load offset=8 ; local.set tmp ; drop ; local.get seed ; convert ; local.get tmp ; add
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 520);
            op(c, wasm_op::f64_const); f64(c, -6.25);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 8u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_convert_i32_s);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f64_add);
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
        constexpr auto curr_initial = make_initial_stacktop_currpos<Opt>();
        constexpr auto curr_after_i32_1 = make_curr_after_i32_pushes<Opt>(1uz);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_initial, tuple);
        auto const exp_i32_load_add_imm = optable::translate::get_uwvmint_i32_load_add_imm_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);
        auto const exp_i32_load_and_imm = optable::translate::get_uwvmint_i32_load_and_imm_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);
        auto const exp_f32_load_localget = optable::translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);
        auto const exp_f64_load_localget = optable::translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);
        auto const exp_i32_load_plain = optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);
        auto const exp_f32_load_plain = optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);
        auto const exp_f64_load_plain = optable::translate::get_uwvmint_f64_load_fptr_from_tuple<Opt>(curr_after_i32_1, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_load_add_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_i32_load_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc0, exp_local_get_i32) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_load_and_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_i32_load_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc1, exp_local_get_i32) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_f32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_f32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_f64_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_f64_load_plain));
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto curr_initial = make_initial_stacktop_currpos<Opt>();
        constexpr auto curr_after_i32_1 = make_curr_after_i32_pushes<Opt>(1uz);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr_initial, tuple);
        auto const exp_i32_load_add_imm =
            optable::translate::get_uwvmint_i32_load_add_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_after_i32_1);
        auto const exp_i32_load_and_imm =
            optable::translate::get_uwvmint_i32_load_and_imm_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_after_i32_1);
        auto const exp_f32_load_localget =
            optable::translate::get_uwvmint_f32_load_localget_off_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_after_i32_1);
        auto const exp_f64_load_localget =
            optable::translate::get_uwvmint_f64_load_localget_off_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_after_i32_1);
        auto const exp_i32_load_plain = optable::translate::get_uwvmint_i32_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_after_i32_1);
        auto const exp_f32_load_plain = optable::translate::get_uwvmint_f32_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_after_i32_1);
        auto const exp_f64_load_plain = optable::translate::get_uwvmint_f64_load_fptr<Opt, ::std::byte const*, ::std::byte*, ::std::byte*>(curr_after_i32_1);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_load_add_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_i32_load_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc0, exp_local_get_i32) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_load_and_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_i32_load_plain));
        UWVM2TEST_REQUIRE(bytecode_count_fptr(bc1, exp_local_get_i32) == 1uz);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_f32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_f32_load_plain));

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_local_get_i32));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_f64_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_f64_load_plain));
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
                                              pack_i32x2(11, 200),
                                              nullptr,
                                              nullptr)
                                       .results) == 118);

        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(1),
                                                                            pack_i32x2(0, 300),
                                                                            nullptr,
                                                                            nullptr)
                                                                 .results)) == 0x003400cdu);

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(11, 400),
                                              nullptr,
                                              nullptr)
                                       .results) == 14.75f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2(10, 512),
                                              nullptr,
                                              nullptr)
                                       .results) == 3.75);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_localget2_load_nonzero_offset_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_localget2_load_nonzero_offset_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_localget2_load_nonzero_offset_fuse");
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
        return test_translate_memory_localget2_load_nonzero_offset_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
