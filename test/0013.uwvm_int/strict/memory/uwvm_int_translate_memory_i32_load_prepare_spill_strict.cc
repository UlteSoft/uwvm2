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

    [[nodiscard]] byte_vec build_memory_i32_load_prepare_spill_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: i32 ring full (survivor + addr) => i32.load must spill one cached i32 before push result.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_const); i32(c, static_cast<::std::int32_t>(0x11223344u));
            op(c, wasm_op::i32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32.load8_s with full i32 ring.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_const); i32(c, 0x80);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i32_load8_s); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i32.load8_u with full i32 ring.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 12);
            op(c, wasm_op::i32_const); i32(c, 0x80);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::i32_const); i32(c, 12);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i32.load16_s with full i32 ring.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i32_load16_s); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i32.load16_u with full i32 ring.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 4);
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm, ::uwvm2::object::memory::linear::native_memory_t const& mem) noexcept
    {
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr = make_curr_after_i32_pushes<Opt>(2uz);

        auto const exp_i32_load = [&]() constexpr
        {
            if constexpr(Opt.is_tail_call)
            {
                return optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr, mem, tuple);
            }
            else
            {
                return optable::translate::get_uwvmint_i32_load_fptr_from_tuple<Opt>(curr, tuple);
            }
        }();
        auto const exp_i32_load8_s = [&]() constexpr
        {
            if constexpr(Opt.is_tail_call)
            {
                return optable::translate::get_uwvmint_i32_load8_s_fptr_from_tuple<Opt>(curr, mem, tuple);
            }
            else
            {
                return optable::translate::get_uwvmint_i32_load8_s_fptr_from_tuple<Opt>(curr, tuple);
            }
        }();
        auto const exp_i32_load8_u = [&]() constexpr
        {
            if constexpr(Opt.is_tail_call)
            {
                return optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr, mem, tuple);
            }
            else
            {
                return optable::translate::get_uwvmint_i32_load8_u_fptr_from_tuple<Opt>(curr, tuple);
            }
        }();
        auto const exp_i32_load16_s = [&]() constexpr
        {
            if constexpr(Opt.is_tail_call)
            {
                return optable::translate::get_uwvmint_i32_load16_s_fptr_from_tuple<Opt>(curr, mem, tuple);
            }
            else
            {
                return optable::translate::get_uwvmint_i32_load16_s_fptr_from_tuple<Opt>(curr, tuple);
            }
        }();
        auto const exp_i32_load16_u = [&]() constexpr
        {
            if constexpr(Opt.is_tail_call)
            {
                return optable::translate::get_uwvmint_i32_load16_u_fptr_from_tuple<Opt>(curr, mem, tuple);
            }
            else
            {
                return optable::translate::get_uwvmint_i32_load16_u_fptr_from_tuple<Opt>(curr, tuple);
            }
        }();

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
        auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i32_load));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i32_load8_s));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i32_load8_u));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_i32_load16_s));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc4, exp_i32_load16_u));

        if constexpr(Opt.is_tail_call && Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_end_pos != Opt.i32_stack_top_begin_pos)
        {
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(bc0));
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(bc1));
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(bc2));
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(bc3));
            UWVM2TEST_REQUIRE(contains_i32_spill1<Opt>(bc4));
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_once(byte_vec const& wasm, ::uwvm2::utils::container::u8string_view name, bool verify_bytecode) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, name);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

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
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(static_cast<::std::uint32_t>(load_i32(rr0.results)) == 0x11223349u);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr1.results) == -121);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr2.results) == 135);

        auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                               rt.local_defined_function_vec_storage.index_unchecked(3),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr3.results) == -32763);

        auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                               rt.local_defined_function_vec_storage.index_unchecked(4),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i32(rr4.results) == 32773);

        return 0;
    }

    [[nodiscard]] int test_translate_memory_i32_load_prepare_spill() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_i32_load_prepare_spill_module();

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_i32_load_prepare_spill_byref", true) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_i32_load_prepare_spill_tailmin", false) == 0);
        }

        {
            constexpr auto opt{k_test_tail_i32_ring2_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_i32_load_prepare_spill_ring2", true) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_i32_load_prepare_spill_tail_sysv", false) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_once<opt>(wasm, u8"uwvm2test_memory_i32_load_prepare_spill_tail_aapcs64", false) == 0);
        }

        return 0;
    }
}

int main()
{
    return test_translate_memory_i32_load_prepare_spill();
}
