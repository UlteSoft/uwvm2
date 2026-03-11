#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr auto k_test_tail_scalar4_merged_opt{make_tailcall_scalar4_merged_opt<2uz>()};

    [[nodiscard]] byte_vec pack_i32x3(::std::int32_t a, ::std::int32_t b, ::std::int32_t c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i32x2_i64(::std::int32_t a, ::std::int32_t b, ::std::int64_t c)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 8);
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

    [[nodiscard]] byte_vec build_memory_scalar4_merged_localget2_flush_then_store_plain_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 3u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 3u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 6u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 6u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 8u);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 8u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 4u);
            op(c, wasm_op::i64_extend_i32_u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 4u);
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

        auto contains_local_get_i32 = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<Opt>(curr, tuple));
            });
        };
        auto contains_i32_store_plain = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store_imm_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store8_plain = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store8_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store8_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store8_imm_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store8_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store16_plain = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store16_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store16_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store16_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i32_store16_imm_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_store16_imm_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto contains_u16_copy_scaled_index = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_u16_copy_scaled_index_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };
# endif

        auto contains_i64_store_plain = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i64_store_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i64_store_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i64_store_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i64_store32_plain = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto contains_i64_store32_localget = [&](auto const& bc) noexcept
        {
            return search_curr_i32_i64<Opt>([&](auto curr) noexcept {
                return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
            });
        };

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
        auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;

        UWVM2TEST_REQUIRE(contains_local_get_i32(bc0));
        UWVM2TEST_REQUIRE(contains_i32_store_plain(bc0));
        UWVM2TEST_REQUIRE(!contains_i32_store_localget(bc0));
        UWVM2TEST_REQUIRE(!contains_i32_store_imm_localget(bc0));

        UWVM2TEST_REQUIRE(contains_local_get_i32(bc1));
        UWVM2TEST_REQUIRE(contains_i32_store8_plain(bc1));
        UWVM2TEST_REQUIRE(!contains_i32_store8_localget(bc1));
        UWVM2TEST_REQUIRE(!contains_i32_store8_imm_localget(bc1));

        UWVM2TEST_REQUIRE(contains_local_get_i32(bc2));
        UWVM2TEST_REQUIRE(contains_i32_store16_plain(bc2));
        UWVM2TEST_REQUIRE(!contains_i32_store16_localget(bc2));
        UWVM2TEST_REQUIRE(!contains_i32_store16_imm_localget(bc2));
# if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(!contains_u16_copy_scaled_index(bc2));
# endif

        UWVM2TEST_REQUIRE(contains_local_get_i32(bc3));
        UWVM2TEST_REQUIRE(contains_i64_store_plain(bc3));
        UWVM2TEST_REQUIRE(!contains_i64_store_localget(bc3));

        UWVM2TEST_REQUIRE(contains_local_get_i32(bc4));
        UWVM2TEST_REQUIRE(contains_i64_store32_plain(bc4));
        UWVM2TEST_REQUIRE(!contains_i64_store32_localget(bc4));

        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 5uz);
        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32x3(11, 64, static_cast<::std::int32_t>(0x12345678u)),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int32_t>(0x12345683u));

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32x3(7, 96, 0xAB),
                                              nullptr,
                                              nullptr)
                                       .results) == 178);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x3(9, 128, 0xBEEF),
                                              nullptr,
                                              nullptr)
                                       .results) == 48888);

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2_i64(5, 160, static_cast<::std::int64_t>(0x1122334455667788ull)),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int64_t>(0x112233445566778dull));

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_i32x2_i64(6, 192, static_cast<::std::int64_t>(0x1234567880000001ull)),
                                              nullptr,
                                              nullptr)
                                       .results) == static_cast<::std::int64_t>(0x80000007ull));
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
#else
        (void)mem;
#endif
        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_scalar4_merged_localget2_flush_then_store_plain() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_scalar4_merged_localget2_flush_then_store_plain_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_scalar4_merged_localget2_flush_then_store_plain");
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
        return test_translate_memory_scalar4_merged_localget2_flush_then_store_plain();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
