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

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_memory_scalar4_merged_u16_copy_scaled_index_module()
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
            constexpr ::std::uint32_t dst_off = 0u;

            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 0u), 0x1234);
            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 2u), 0xBEEF);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, dst_off);
            op(c, wasm_op::nop);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, src_off);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, dst_off);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, dst_off);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            constexpr ::std::uint32_t src_off = 220u;
            constexpr ::std::uint32_t dst_off = 6u;

            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 0u), 0x00AA);
            emit_store16_const(c, static_cast<::std::int32_t>(src_off + 2u), 0x55CC);

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, dst_off);
            op(c, wasm_op::nop);

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::i32_shl);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, src_off);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, dst_off);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, dst_off);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm,
                                     ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;

        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc0, optable::translate::get_uwvmint_u16_copy_scaled_index_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));
        UWVM2TEST_REQUIRE(search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc1, optable::translate::get_uwvmint_u16_copy_scaled_index_fptr_from_tuple<Opt>(curr, mem, tuple));
        }));

        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc0, optable::translate::get_uwvmint_i32_shl_fptr_from_tuple<Opt>(curr, tuple));
        }));
        UWVM2TEST_REQUIRE(!search_curr_i32<Opt>([&](auto curr) noexcept {
            return bytecode_contains_fptr(bc1, optable::translate::get_uwvmint_i32_shl_fptr_from_tuple<Opt>(curr, tuple));
        }));
        return 0;
    }
#endif

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);
        using Runner = interpreter_runner<Opt>;

        for(auto const dst : ::std::array{0, 16, 128, 1024})
        {
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x2(dst, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 0x1234);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x2(dst, 1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 0xBEEF);

            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_i32x2(dst, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 0x00AA + 7);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_i32x2(dst, 1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 0x55CC + 7);
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, mem) == 0);
#endif
        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_scalar4_merged_u16_copy_scaled_index() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_scalar4_merged_u16_copy_scaled_index_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_scalar4_merged_u16_copy_scaled_index");
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
        return test_translate_memory_scalar4_merged_u16_copy_scaled_index();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
