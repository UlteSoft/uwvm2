#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr auto k_test_tail_scalar4_merged_opt{make_tailcall_scalar4_merged_opt<2uz>()};

    [[nodiscard]] byte_vec pack_i32(::std::int32_t v)
    {
        byte_vec out(4);
        ::std::memcpy(out.data(), ::std::addressof(v), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i64_i32(::std::int64_t a, ::std::int32_t b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 4);
        return out;
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

    [[nodiscard]] byte_vec build_memory_scalar4_merged_i64_narrow_local_plus_imm_nonzero_offset_no_fuse_module()
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

            op(c, wasm_op::i32_const); i32(c, 73);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x80ull));
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load8_s); u32(c, 0u); u32(c, 5u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 95);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0xABull));
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load8_u); u32(c, 0u); u32(c, 7u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 130);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0x8001ull));
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 12);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load16_s); u32(c, 1u); u32(c, 10u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 164);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0xFEDCull));
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load16_u); u32(c, 1u); u32(c, 12u);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i64, k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 185);
            op(c, wasm_op::i64_const); i64(c, static_cast<::std::int64_t>(0xF0ull));
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i64_load8_u); u32(c, 0u); u32(c, 1u);
            op(c, wasm_op::i64_add);
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
        auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;

        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc0, optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc0, optable::translate::get_uwvmint_i64_load8_s_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc0, optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc1, optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc1, optable::translate::get_uwvmint_i64_load8_u_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc1, optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc2, optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc2, optable::translate::get_uwvmint_i64_load16_s_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc2, optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc3, optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc3, optable::translate::get_uwvmint_i64_load16_u_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc3, optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple));
        }));

        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc4, optable::translate::get_uwvmint_local_get_i64_fptr_from_tuple<Opt>(curr, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc4, optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc4, optable::translate::get_uwvmint_i64_load8_u_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32_i64<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc4, optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple));
        }));
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 5uz);
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(64),
                                              nullptr,
                                              nullptr)
                                       .results) == -128ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32(80),
                                              nullptr,
                                              nullptr)
                                       .results) == 171ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32(108),
                                              nullptr,
                                              nullptr)
                                       .results) == -32767ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32(136),
                                              nullptr,
                                              nullptr)
                                       .results) == 65244ll);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i64_i32(9, 164),
                                              nullptr,
                                              nullptr)
                                       .results) == 249ll);
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

    [[nodiscard]] int test_translate_memory_scalar4_merged_i64_narrow_local_plus_imm_nonzero_offset_no_fuse() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_scalar4_merged_i64_narrow_local_plus_imm_nonzero_offset_no_fuse_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_scalar4_merged_i64_narrow_local_plus_imm_nonzero_offset_no_fuse");
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
        return test_translate_memory_scalar4_merged_i64_narrow_local_plus_imm_nonzero_offset_no_fuse();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
