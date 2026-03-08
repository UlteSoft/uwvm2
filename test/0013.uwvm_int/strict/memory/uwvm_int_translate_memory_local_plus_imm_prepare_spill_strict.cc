#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_local_plus_imm_spill_opt{
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

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_f32_pushes(::std::size_t push_count) noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos)
        {
            for(::std::size_t i{}; i != push_count; ++i)
            {
                curr.f32_stack_top_curr_pos =
                    optable::details::ring_prev_pos(curr.f32_stack_top_curr_pos, Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_end_pos);
            }
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_f64_pushes(::std::size_t push_count) noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos)
        {
            for(::std::size_t i{}; i != push_count; ++i)
            {
                curr.f64_stack_top_curr_pos =
                    optable::details::ring_prev_pos(curr.f64_stack_top_curr_pos, Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_end_pos);
            }
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType, typename ByteStorage>
    [[nodiscard]] bool contains_spill1(ByteStorage const& bc) noexcept
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
            else if constexpr(::std::same_as<ValType, wasm_f32>)
            {
                curr.f32_stack_top_curr_pos = start_pos;
                remain.f32_stack_top_remain_size = 1uz;
            }
            else if constexpr(::std::same_as<ValType, wasm_f64>)
            {
                curr.f64_stack_top_curr_pos = start_pos;
                remain.f64_stack_top_remain_size = 1uz;
            }
            else
            {
                continue;
            }

            auto const fptr = optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
            if(bytecode_contains_fptr(bc, fptr)) { return true; }
        }

        return false;
    }

    [[nodiscard]] byte_vec build_memory_local_plus_imm_prepare_spill_module()
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

        // f0: full i32 ring + local.get base ; i32.const 4 ; i32.add ; i32.load
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 20);
            op(c, wasm_op::i32_const); i32(c, 55);
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 2);
            op(c, wasm_op::i32_const); i32(c, 3);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: full f32 ring + local.get base ; i32.const 4 ; i32.add ; f32.load
        {
            func_type ty{{k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 40);
            op(c, wasm_op::f32_const); f32(c, 3.5f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::f32_const); f32(c, 1.25f);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: full f64 ring + local.get base ; i32.const 8 ; i32.add ; f64.load
        {
            func_type ty{{k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 56);
            op(c, wasm_op::f64_const); f64(c, 6.25);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::f64_const); f64(c, 1.5);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm, ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_i32 = make_curr_after_i32_pushes<Opt>(2uz);
        constexpr auto curr_f32 = make_curr_after_f32_pushes<Opt>(1uz);
        constexpr auto curr_f64 = make_curr_after_f64_pushes<Opt>(1uz);

        auto const exp_i32_load_local_plus_imm =
            optable::translate::get_uwvmint_i32_load_local_plus_imm_fptr_from_tuple<Opt>(curr_i32, mem, tuple);
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_load_local_plus_imm));
        UWVM2TEST_REQUIRE((contains_spill1<Opt, wasm_i32>(cm.local_funcs.index_unchecked(0).op.operands)));

#if defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        auto const exp_f32_load_local_plus_imm =
            optable::translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<Opt>(curr_f32, mem, tuple);
        auto const exp_f64_load_local_plus_imm =
            optable::translate::get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple<Opt>(curr_f64, mem, tuple);
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_f32_load_local_plus_imm));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f64_load_local_plus_imm));
        UWVM2TEST_REQUIRE((contains_spill1<Opt, wasm_f32>(cm.local_funcs.index_unchecked(1).op.operands)));
        UWVM2TEST_REQUIRE((contains_spill1<Opt, wasm_f64>(cm.local_funcs.index_unchecked(2).op.operands)));
#endif

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_local_plus_imm_prepare_spill_suite(runtime_module_t const& rt, bool verify_bytecode) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        if(verify_bytecode)
        {
            UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
            auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
            UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, mem) == 0);
        }

        using Runner = interpreter_runner<Opt>;

        auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                               rt.local_defined_function_vec_storage.index_unchecked(0),
                               pack_i32(16),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr0.results) == 60);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_i32(36),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_f32(rr1.results) == 4.75f);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_i32(48),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_f64(rr2.results) == 7.75);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_local_plus_imm_prepare_spill() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_local_plus_imm_prepare_spill_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_local_plus_imm_prepare_spill");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr auto opt{k_test_tail_local_plus_imm_spill_opt};
        static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
        UWVM2TEST_REQUIRE(run_local_plus_imm_prepare_spill_suite<opt>(rt, true) == 0);
        return 0;
    }
}

int main()
{
    return test_translate_memory_local_plus_imm_prepare_spill();
}
