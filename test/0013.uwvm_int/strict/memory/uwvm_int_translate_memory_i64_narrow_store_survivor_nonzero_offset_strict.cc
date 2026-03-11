#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_i64_store_split_opt{
        .is_tail_call = true,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 4uz,
        .i64_stack_top_begin_pos = 4uz,
        .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz,
        .f32_stack_top_end_pos = 6uz,
        .f64_stack_top_begin_pos = 6uz,
        .f64_stack_top_end_pos = 7uz,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX,
    };

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

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_fill1(ByteStorage const& bc) noexcept
    {
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, wasm_i64>();
            start_pos < optable::details::stacktop_range_end_pos<Opt, wasm_i64>();
            ++start_pos)
        {
            optable::uwvm_interpreter_stacktop_currpos_t curr{};
            optable::uwvm_interpreter_stacktop_remain_size_t remain{};
            curr.i64_stack_top_curr_pos = start_pos;
            remain.i64_stack_top_remain_size = 1uz;

            auto const fptr = optable::translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<Opt, wasm_i64>(curr, remain, tuple);
            if(bytecode_contains_fptr(bc, fptr)) { return true; }
        }
        return false;
    }

    [[nodiscard]] byte_vec build_memory_i64_narrow_store_survivor_nonzero_offset_module()
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
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 5);
            op(c, wasm_op::i32_const); i32(c, 96);
            op(c, wasm_op::i64_const); i64(c, 0xfe);
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 9u);

            op(c, wasm_op::i32_const); i32(c, 105);
            op(c, wasm_op::i64_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 128);
            op(c, wasm_op::i64_const); i64(c, 0xbeef);
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 10u);

            op(c, wasm_op::i32_const); i32(c, 138);
            op(c, wasm_op::i64_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 9);
            op(c, wasm_op::i32_const); i32(c, 160);
            op(c, wasm_op::i64_const); i64(c, 0x80000001ll);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 12u);

            op(c, wasm_op::i32_const); i32(c, 172);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i64_add);
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

        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 259ll);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 48886ll);
        UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_no_params(),
                                              nullptr,
                                              nullptr)
                                       .results) == 2147483658ll);
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem,
                                          bool expect_fill) noexcept
    {
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr = make_curr_after_i32_i64_pushes<Opt>(1uz, 2uz);

        auto const exp_store8 = optable::translate::get_uwvmint_i64_store8_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_store16 = optable::translate::get_uwvmint_i64_store16_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_store32 = optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr, mem, tuple);
        auto const exp_store32_localget = optable::translate::get_uwvmint_i64_store32_localget_off_fptr_from_tuple<Opt>(curr, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_store16));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_store32));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_store32_localget));

        if(expect_fill)
        {
            UWVM2TEST_REQUIRE((contains_fill1<Opt>(bc0)));
            UWVM2TEST_REQUIRE((contains_fill1<Opt>(bc1)));
            UWVM2TEST_REQUIRE((contains_fill1<Opt>(bc2)));
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr = make_curr_after_i32_i64_pushes<Opt>(1uz, 2uz);

        auto const exp_store8 = optable::translate::get_uwvmint_i64_store8_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_store16 = optable::translate::get_uwvmint_i64_store16_fptr_from_tuple<Opt>(curr, tuple);
        auto const exp_store32 = optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_store16));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_store32));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt, bool verify_bytecode, bool expect_fill) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        if(verify_bytecode)
        {
            if constexpr(Opt.is_tail_call)
            {
                UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
                auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
                UWVM2TEST_REQUIRE(check_tail_bytecode<Opt>(cm, mem, expect_fill) == 0);
            }
            else
            {
                UWVM2TEST_REQUIRE(check_byref_bytecode<Opt>(cm) == 0);
            }
        }

        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_i64_narrow_store_survivor_nonzero_offset() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_i64_narrow_store_survivor_nonzero_offset_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_i64_narrow_store_survivor_nonzero_offset");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true, false) == 0);
        }

        {
            constexpr auto opt{k_test_tail_i64_store_split_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true, true) == 0);
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

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_i64_narrow_store_survivor_nonzero_offset();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
