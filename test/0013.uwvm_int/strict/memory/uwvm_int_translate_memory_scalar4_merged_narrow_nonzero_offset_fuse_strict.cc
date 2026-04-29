#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr auto k_test_tail_scalar4_merged_opt{make_tailcall_scalar4_merged_opt<2uz>()};

    template <optable::uwvm_interpreter_translate_option_t Opt, typename Fn>
    [[nodiscard]] bool search_curr_i32(Fn&& fn) noexcept
    {
        for(::std::size_t i32_pos{Opt.i32_stack_top_begin_pos}; i32_pos != Opt.i32_stack_top_end_pos; ++i32_pos)
        {
            auto curr = make_initial_stacktop_currpos<Opt>();
            curr.i32_stack_top_curr_pos = i32_pos;
            if(fn(curr)) { return true; }
        }
        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename Fn>
    [[nodiscard]] bool search_curr_i32_i64(Fn&& fn) noexcept
    {
        for(::std::size_t i32_pos{Opt.i32_stack_top_begin_pos}; i32_pos != Opt.i32_stack_top_end_pos; ++i32_pos)
        {
            for(::std::size_t i64_pos{Opt.i64_stack_top_begin_pos}; i64_pos != Opt.i64_stack_top_end_pos; ++i64_pos)
            {
                auto curr = make_initial_stacktop_currpos<Opt>();
                curr.i32_stack_top_curr_pos = i32_pos;
                curr.i64_stack_top_curr_pos = i64_pos;
                if(fn(curr)) { return true; }
            }
        }
        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_any_local_get_i32(ByteStorage const& bc) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        for(::std::size_t i32_pos{Opt.i32_stack_top_begin_pos}; i32_pos != Opt.i32_stack_top_end_pos; ++i32_pos)
        {
            auto curr = make_initial_stacktop_currpos<Opt>();
            curr.i32_stack_top_curr_pos = i32_pos;
            auto const fptr = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple);
            if(bytecode_contains_fptr(bc, fptr)) { return true; }
        }
        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_any_local_get_i64(ByteStorage const& bc) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        for(::std::size_t i64_pos{Opt.i64_stack_top_begin_pos}; i64_pos != Opt.i64_stack_top_end_pos; ++i64_pos)
        {
            auto curr = make_initial_stacktop_currpos<Opt>();
            curr.i64_stack_top_curr_pos = i64_pos;
            auto const fptr = optable::translate::get_uwvmint_local_get_i64_fptr_from_tuple<Opt>(curr, tuple);
            if(bytecode_contains_fptr(bc, fptr)) { return true; }
        }
        return false;
    }

    [[nodiscard]] byte_vec pack_i32(::std::int32_t v)
    {
        byte_vec out(4);
        ::std::memcpy(out.data(), ::std::addressof(v), 4);
        return out;
    }

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

    [[nodiscard]] byte_vec build_memory_scalar4_merged_narrow_nonzero_offset_fuse_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 105);
            op(c, wasm_op::i32_const); i32(c, 0x85);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load8_s); u32(c, 0u); u32(c, 5u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 209);
            op(c, wasm_op::i32_const); i32(c, 0xab);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 9u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 310);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x8001u));
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load16_s); u32(c, 1u); u32(c, 10u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::i32_const); i32(c, 414);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0xbeefu));
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 14u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

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

        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::i32_const); i32(c, 44);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 20u);
            op(c, wasm_op::i32_const); i32(c, 60);
            op(c, wasm_op::i64_load32_s); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm,
                                     ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
        [[maybe_unused]] auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;
        auto const& bc5 = cm.local_funcs.index_unchecked(5).op.operands;
        [[maybe_unused]] auto const& bc6 = cm.local_funcs.index_unchecked(6).op.operands;
        auto const& bc7 = cm.local_funcs.index_unchecked(7).op.operands;
        auto const& bc8 = cm.local_funcs.index_unchecked(8).op.operands;
        auto const& bc9 = cm.local_funcs.index_unchecked(9).op.operands;

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc0, optable::translate::get_uwvmint_i32_load8_s_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc0, optable::translate::get_uwvmint_i32_load8_s_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc0));

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc1, optable::translate::get_uwvmint_i32_load8_u_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc1, optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc1));

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc2, optable::translate::get_uwvmint_i32_load16_s_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc2, optable::translate::get_uwvmint_i32_load16_s_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc2));

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc3, optable::translate::get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc3, optable::translate::get_uwvmint_i32_load16_u_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc3));

# if defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc4, optable::translate::get_uwvmint_i32_store8_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc4, optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc4));

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc6, optable::translate::get_uwvmint_i32_store16_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc6, optable::translate::get_uwvmint_i32_store16_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc6));
# endif

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc5, optable::translate::get_uwvmint_i32_store8_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc5, optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc5));

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc7, optable::translate::get_uwvmint_i32_store16_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc7, optable::translate::get_uwvmint_i32_store16_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i32<Opt>(bc7));

        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc8, optable::translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc8, optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i64<Opt>(bc8));

        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc9, optable::translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc9, optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!contains_any_local_get_i64<Opt>(bc9));

        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 10uz);
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(100),
                                              nullptr,
                                              nullptr)
                                       .results) == -123);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(200),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xab);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(300),
                                              nullptr,
                                              nullptr)
                                       .results) == -32767);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(400),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0xbeefu));

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32x2(100, static_cast<::std::int32_t>(0x1234abu)),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xab);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_i32(200),
                                              nullptr,
                                              nullptr)
                                       .results) == 0xab);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(6),
                                              rt.local_defined_function_vec_storage.index_unchecked(6),
                                              pack_i32x2(300, static_cast<::std::int32_t>(0xcafeu)),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0xcafeu));

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(7),
                                              rt.local_defined_function_vec_storage.index_unchecked(7),
                                              pack_i32(400),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0xbeefu));

        constexpr auto trunc_zero_extend = static_cast<::std::int64_t>(0x1122334455667788ull);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(8),
                                              rt.local_defined_function_vec_storage.index_unchecked(8),
                                              pack_i32_i64(32, trunc_zero_extend),
                                              nullptr,
                                              nullptr)
                                       .results) == 0x55667788ll);

        constexpr auto trunc_sign_extend = static_cast<::std::int64_t>(0x80000001ull);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(9),
                                              rt.local_defined_function_vec_storage.index_unchecked(9),
                                              pack_i32_i64(40, trunc_sign_extend),
                                              nullptr,
                                              nullptr)
                                       .results) == -2147483647ll);

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
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, mem) == 0);
#endif
        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_scalar4_merged_narrow_nonzero_offset_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_scalar4_merged_narrow_nonzero_offset_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_scalar4_merged_narrow_nonzero_offset_fuse");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        static_assert(compiler::details::interpreter_tuple_has_no_holes<k_test_tail_scalar4_merged_opt>());
        UWVM2TEST_REQUIRE(compile_check_and_run<k_test_tail_scalar4_merged_opt>(rt, mem) == 0);
        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_scalar4_merged_narrow_nonzero_offset_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
