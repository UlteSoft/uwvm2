#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr auto k_test_tail_scalar4_merged_opt{make_tailcall_scalar4_merged_opt<2uz>()};

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename Matcher>
    [[nodiscard]] bool bytecode_contains_any_i32_curr([[maybe_unused]] ByteStorage const& bc, Matcher&& matcher) noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if(matcher(curr)) { return true; }
        if constexpr(Opt.i32_stack_top_begin_pos == SIZE_MAX || Opt.i32_stack_top_begin_pos == Opt.i32_stack_top_end_pos)
        {
            return false;
        }
        else
        {
            for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos != Opt.i32_stack_top_end_pos; ++pos)
            {
                curr = make_initial_stacktop_currpos<Opt>();
                curr.i32_stack_top_curr_pos = pos;
                if(matcher(curr)) { return true; }
            }
            return false;
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_any_i32_shl(ByteStorage const& bc) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        return bytecode_contains_any_i32_curr<Opt>(bc,
                                                   [&](optable::uwvm_interpreter_stacktop_currpos_t curr) noexcept
                                                   { return bytecode_contains_fptr(bc, optable::translate::get_uwvmint_i32_shl_fptr_from_tuple<Opt>(curr, tuple)); });
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_any_plain_i32_load16_u(ByteStorage const& bc,
                                                       ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        return bytecode_contains_any_i32_curr<Opt>(bc,
                                                   [&](optable::uwvm_interpreter_stacktop_currpos_t curr) noexcept
                                                   {
                                                       return bytecode_contains_fptr(
                                                           bc,
                                                           optable::translate::get_uwvmint_i32_load16_u_fptr_from_tuple<Opt>(curr, mem, tuple));
                                                   });
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_any_i32_load16_u_localget_off(ByteStorage const& bc,
                                                              ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        return bytecode_contains_any_i32_curr<Opt>(bc,
                                                   [&](optable::uwvm_interpreter_stacktop_currpos_t curr) noexcept
                                                   {
                                                       return bytecode_contains_fptr(
                                                           bc,
                                                           optable::translate::get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple));
                                                   });
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_any_u16_copy_scaled_index(ByteStorage const& bc,
                                                          ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        return bytecode_contains_any_i32_curr<Opt>(bc,
                                                   [&](optable::uwvm_interpreter_stacktop_currpos_t curr) noexcept
                                                   {
                                                       return bytecode_contains_fptr(
                                                           bc,
                                                           optable::translate::get_uwvmint_u16_copy_scaled_index_fptr_from_tuple<Opt>(curr, mem, tuple));
                                                   });
    }

    [[nodiscard]] byte_vec build_memory_scalar4_merged_u16_copy_scaled_index_pending_flush_bytecode_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        auto emit_store16_const = [&](byte_vec& c, ::std::int32_t addr, ::std::int32_t v)
        {
            op(c, wasm_op::i32_const); i32(c, addr);
            op(c, wasm_op::i32_const); i32(c, v);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);
        };

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            constexpr ::std::uint32_t src_off = 200u;

            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 0u), 0x1234);
            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 2u), 0xBEEF);
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, src_off);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            constexpr ::std::uint32_t src_off = 240u;

            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 0u), 0x00AA);
            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 2u), 0x55CC);
            op(c, wasm_op::nop);

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, src_off);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;
            constexpr ::std::uint32_t src_off = 280u;

            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 0u), 0x1111);
            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 2u), 0x2222);
            op(c, wasm_op::nop);

            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, src_off);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
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
                                  pack_i32x2(100, 0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 100 + 0x1234);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32x2(100, 1),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 100 + 0xBEEF);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32x2(200, 0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 7 + 200 + 0x00AA);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32x2(200, 1),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 7 + 200 + 0x55CC);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32x2(300, 0),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 5 + 9 + 300 + 0x1111);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32x2(300, 1),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 5 + 9 + 300 + 0x2222);
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm,
                                     ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;

        UWVM2TEST_REQUIRE((contains_any_i32_shl<Opt>(bc0)));
        UWVM2TEST_REQUIRE((contains_any_i32_shl<Opt>(bc1)));
        UWVM2TEST_REQUIRE((contains_any_i32_shl<Opt>(bc2)));
        UWVM2TEST_REQUIRE((contains_any_plain_i32_load16_u<Opt>(bc0, mem)));
        UWVM2TEST_REQUIRE((contains_any_plain_i32_load16_u<Opt>(bc1, mem)));
        UWVM2TEST_REQUIRE((contains_any_plain_i32_load16_u<Opt>(bc2, mem)));
        UWVM2TEST_REQUIRE(!(contains_any_i32_load16_u_localget_off<Opt>(bc0, mem)));
        UWVM2TEST_REQUIRE(!(contains_any_i32_load16_u_localget_off<Opt>(bc1, mem)));
        UWVM2TEST_REQUIRE(!(contains_any_i32_load16_u_localget_off<Opt>(bc2, mem)));
        UWVM2TEST_REQUIRE(!(contains_any_u16_copy_scaled_index<Opt>(bc0, mem)));
        UWVM2TEST_REQUIRE(!(contains_any_u16_copy_scaled_index<Opt>(bc1, mem)));
        UWVM2TEST_REQUIRE(!(contains_any_u16_copy_scaled_index<Opt>(bc2, mem)));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, mem) == 0);
        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_scalar4_merged_u16_copy_scaled_index_pending_flush_bytecode() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_scalar4_merged_u16_copy_scaled_index_pending_flush_bytecode_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_scalar4_merged_u16_copy_scaled_index_pending_flush_bytecode");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        static_assert(compiler::details::interpreter_tuple_has_no_holes<k_test_tail_scalar4_merged_opt>());
        UWVM2TEST_REQUIRE(compile_check_and_run<k_test_tail_scalar4_merged_opt>(rt) == 0);
        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_scalar4_merged_u16_copy_scaled_index_pending_flush_bytecode();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
