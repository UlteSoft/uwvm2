#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_minimal_split_opt{
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

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_for_store() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        curr.i32_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);

        if constexpr(::std::same_as<ValType, wasm_i64>)
        {
            curr.i64_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.i64_stack_top_curr_pos, Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_end_pos);
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType, typename ByteStorage>
    [[nodiscard]] bool contains_fill1(ByteStorage const& bc) noexcept
    {
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, ValType>();
            start_pos < optable::details::stacktop_range_end_pos<Opt, ValType>();
            ++start_pos)
        {
            optable::uwvm_interpreter_stacktop_currpos_t curr{};
            optable::uwvm_interpreter_stacktop_remain_size_t remain{};

            if constexpr(::std::same_as<ValType, wasm_i32>)
            {
                curr.i32_stack_top_curr_pos = start_pos;
                remain.i32_stack_top_remain_size = 1uz;
            }
            else if constexpr(::std::same_as<ValType, wasm_i64>)
            {
                curr.i64_stack_top_curr_pos = start_pos;
                remain.i64_stack_top_remain_size = 1uz;
            }
            else
            {
                continue;
            }

            auto const fptr = optable::translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
            if(bytecode_contains_fptr(bc, fptr)) { return true; }
        }

        return false;
    }

    [[nodiscard]] byte_vec build_memory_narrow_store_pop_fill_module()
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

        // f0: i32 survivor must refill after i32.store8 pop2.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 11);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_const); i32(c, 0xab);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32 survivor must refill after i32.store16 pop2.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 13);
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_const); i32(c, 0xcafe);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i64 survivor must refill after i64.store8 pop2.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 5);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i64_const); i64(c, 0xfe);
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i64_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i64 survivor must refill after i64.store16 pop2.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i64_const); i64(c, 0xbeef);
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i64_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i64 survivor must refill after i64.store32 pop2.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const); i64(c, 9);
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i64_const); i64(c, 0x80000001ll);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_memory_narrow_store_pop_fill_suite(runtime_module_t const& rt) noexcept
    {
        static_assert(Opt.is_tail_call);

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == 182);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr1.results) == 51979);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr2.results) == 259);

        auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                               rt.local_defined_function_vec_storage.index_unchecked(3),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr3.results) == 48886);

        auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                               rt.local_defined_function_vec_storage.index_unchecked(4),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr4.results) == 2147483658ll);

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_i32 = make_curr_for_store<Opt, wasm_i32>();
        constexpr auto curr_i64 = make_curr_for_store<Opt, wasm_i64>();

        auto const exp_i32_store8 = optable::translate::get_uwvmint_i32_store8_fptr_from_tuple<Opt>(curr_i32, mem, tuple);
        auto const exp_i32_store16 = optable::translate::get_uwvmint_i32_store16_fptr_from_tuple<Opt>(curr_i32, mem, tuple);
        auto const exp_i64_store8 = optable::translate::get_uwvmint_i64_store8_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
        auto const exp_i64_store16 = optable::translate::get_uwvmint_i64_store16_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
        auto const exp_i64_store32 = optable::translate::get_uwvmint_i64_store32_fptr_from_tuple<Opt>(curr_i64, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
        auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_store16));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i64_store8));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_i64_store16));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc4, exp_i64_store32));

        UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_i32>(bc0)));
        UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_i32>(bc1)));
        UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_i64>(bc2)));
        UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_i64>(bc3)));
        UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_i64>(bc4)));

        return 0;
    }

    [[nodiscard]] int test_translate_memory_narrow_store_pop_fill() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_narrow_store_pop_fill_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_narrow_store_pop_fill");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr auto opt{k_test_tail_minimal_split_opt};
        static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
        UWVM2TEST_REQUIRE(run_memory_narrow_store_pop_fill_suite<opt>(rt) == 0);
        return 0;
    }
}

int main()
{
    return test_translate_memory_narrow_store_pop_fill();
}
