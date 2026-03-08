#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_i32_ring2_opt{
        .is_tail_call = true,
        .i32_stack_top_begin_pos = 3uz,
        .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 5uz,
        .i64_stack_top_end_pos = 6uz,
        .f32_stack_top_begin_pos = 6uz,
        .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 7uz,
        .f64_stack_top_end_pos = 8uz,
        .v128_stack_top_begin_pos = SIZE_MAX,
        .v128_stack_top_end_pos = SIZE_MAX,
    };

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

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage>
    [[nodiscard]] bool contains_i32_spill1(ByteStorage const& bc) noexcept
    {
        if constexpr(Opt.i32_stack_top_begin_pos == SIZE_MAX || Opt.i32_stack_top_begin_pos == Opt.i32_stack_top_end_pos)
        {
            return false;
        }
        else
        {
            using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, wasm_i32>();
                start_pos < optable::details::stacktop_range_end_pos<Opt, wasm_i32>();
                ++start_pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                optable::uwvm_interpreter_stacktop_remain_size_t remain{};
                curr.i32_stack_top_curr_pos = start_pos;
                remain.i32_stack_top_remain_size = 1uz;

                auto const fptr = optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<Opt, wasm_i32>(curr, remain, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }
            return false;
        }
    }

    [[nodiscard]] byte_vec build_memory_size_grow_stacktop_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 2u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: full 2-slot i32 ring + memory.size => push1 must spill one cached value.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::memory_size); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: full 2-slot i32 ring + delta=2 => memory.grow fail (-1), deeper survivors must remain intact.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::memory_grow); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: full 2-slot i32 ring + delta=1 => memory.grow succeeds, returns old size 1.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::memory_grow); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: after f2 succeeds, memory.size should observe the grown size (2 pages).
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_const); i32(c, 1);
            op(c, wasm_op::memory_size); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto const curr_size = make_curr_after_i32_pushes<Opt>(2uz);
        auto const curr_grow = make_curr_after_i32_pushes<Opt>(3uz);

        auto const exp_memory_size = optable::translate::get_uwvmint_memory_size_fptr_from_tuple<Opt>(curr_size, tuple);
        auto const exp_memory_grow = optable::translate::get_uwvmint_memory_grow_fptr_from_tuple<Opt>(curr_grow, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_memory_size));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_memory_grow));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_memory_grow));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_memory_size));

        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(cm.local_funcs.index_unchecked(0).op.operands));
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(cm.local_funcs.index_unchecked(1).op.operands));
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(cm.local_funcs.index_unchecked(2).op.operands));
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(cm.local_funcs.index_unchecked(3).op.operands));
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_once(byte_vec const& wasm, ::uwvm2::utils::container::u8string_view name) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, name);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm) == 0);

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == 14);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr1.results) == 8);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr2.results) == 9);

        auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                               rt.local_defined_function_vec_storage.index_unchecked(3),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr3.results) == 7);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_size_grow_stacktop() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_size_grow_stacktop_module();

        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_size_grow_stacktop_byref") == 0);
        }
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_size_grow_stacktop_tailmin") == 0);
        }
        {
            constexpr auto opt{k_test_tail_i32_ring2_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_size_grow_stacktop_tailring2") == 0);
        }

        return 0;
    }
}

int main()
{
    return test_translate_memory_size_grow_stacktop();
}
