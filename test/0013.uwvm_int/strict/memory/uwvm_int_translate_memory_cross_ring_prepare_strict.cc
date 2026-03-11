#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

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

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_for_i64_cross_load() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        curr.i32_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
        curr.i64_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.i64_stack_top_curr_pos, Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_end_pos);
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_for_f32_cross_load() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        curr.i32_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
        curr.f32_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.f32_stack_top_curr_pos, Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_end_pos);
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_for_f64_cross_load() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        curr.i32_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
        curr.f64_stack_top_curr_pos =
            optable::details::ring_prev_pos(curr.f64_stack_top_curr_pos, Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_end_pos);
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType>
    [[nodiscard]] auto expected_spill1_fptr() noexcept
    {
        constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        optable::uwvm_interpreter_stacktop_currpos_t curr{};
        optable::uwvm_interpreter_stacktop_remain_size_t remain{};

        if constexpr(::std::same_as<ValType, wasm_i64>)
        {
            curr.i64_stack_top_curr_pos =
                optable::details::ring_prev_pos(Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_begin_pos, Opt.i64_stack_top_end_pos);
            remain.i64_stack_top_remain_size = 1uz;
        }
        else if constexpr(::std::same_as<ValType, wasm_f32>)
        {
            curr.f32_stack_top_curr_pos =
                optable::details::ring_prev_pos(Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_end_pos);
            remain.f32_stack_top_remain_size = 1uz;
        }
        else if constexpr(::std::same_as<ValType, wasm_f64>)
        {
            curr.f64_stack_top_curr_pos =
                optable::details::ring_prev_pos(Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_end_pos);
            remain.f64_stack_top_remain_size = 1uz;
        }

        return optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
    }

    [[nodiscard]] byte_vec build_memory_cross_ring_prepare_module()
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
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        // f0: i64 ring full + i32 addr => i64.load must spill target ring first.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i64_const); i64(c, 0x0123456789abcdefll);
            op(c, wasm_op::i64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 5);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::i64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i64.load8_s with full i64 ring.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i64_const); i64(c, 0x80);
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 8);
            op(c, wasm_op::i64_load8_s); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: i64.load8_u with full i64 ring.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::i64_const); i64(c, 0x80);
            op(c, wasm_op::i64_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::i64_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i64.load16_s with full i64 ring.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i64_const); i64(c, 0x8001);
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 16);
            op(c, wasm_op::i64_load16_s); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i64.load16_u with full i64 ring.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 18);
            op(c, wasm_op::i64_const); i64(c, 0x8001);
            op(c, wasm_op::i64_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 18);
            op(c, wasm_op::i64_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i64.load32_s with full i64 ring.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i64_const); i64(c, 0x80000001ll);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 24);
            op(c, wasm_op::i64_load32_s); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: i64.load32_u with full i64 ring.
        {
            func_type ty{{}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 28);
            op(c, wasm_op::i64_const); i64(c, 0x80000001ll);
            op(c, wasm_op::i64_store32); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i32_const); i32(c, 28);
            op(c, wasm_op::i64_load32_u); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: f32 ring full + i32 addr => f32.load must spill target ring first.
        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 32);
            op(c, wasm_op::f32_const); f32(c, 3.25f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::f32_const); f32(c, 1.5f);
            op(c, wasm_op::i32_const); i32(c, 32);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f8: f64 ring full + i32 addr => f64.load must spill target ring first.
        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 40);
            op(c, wasm_op::f64_const); f64(c, 6.5);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::f64_const); f64(c, 1.25);
            op(c, wasm_op::i32_const); i32(c, 40);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_memory_cross_ring_prepare_suite(runtime_module_t const& rt, bool verify_spill_bytecode) noexcept
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
        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(rr0.results)) == 0x0123456789abcdefull + 5ull);

        auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                               rt.local_defined_function_vec_storage.index_unchecked(1),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr1.results) == -121);

        auto rr2 = Runner::run(cm.local_funcs.index_unchecked(2),
                               rt.local_defined_function_vec_storage.index_unchecked(2),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr2.results) == 135);

        auto rr3 = Runner::run(cm.local_funcs.index_unchecked(3),
                               rt.local_defined_function_vec_storage.index_unchecked(3),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr3.results) == -32760);

        auto rr4 = Runner::run(cm.local_funcs.index_unchecked(4),
                               rt.local_defined_function_vec_storage.index_unchecked(4),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr4.results) == 32776);

        auto rr5 = Runner::run(cm.local_funcs.index_unchecked(5),
                               rt.local_defined_function_vec_storage.index_unchecked(5),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr5.results) == -2147483640ll);

        auto rr6 = Runner::run(cm.local_funcs.index_unchecked(6),
                               rt.local_defined_function_vec_storage.index_unchecked(6),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_i64(rr6.results) == 2147483656ll);

        auto rr7 = Runner::run(cm.local_funcs.index_unchecked(7),
                               rt.local_defined_function_vec_storage.index_unchecked(7),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_f32(rr7.results) == 4.75f);

        auto rr8 = Runner::run(cm.local_funcs.index_unchecked(8),
                               rt.local_defined_function_vec_storage.index_unchecked(8),
                               pack_no_params(),
                               nullptr,
                               nullptr);
        UWVM2TEST_REQUIRE(load_f64(rr8.results) == 7.75);

        if(verify_spill_bytecode)
        {
            UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
            auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;

            constexpr auto tuple = compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
            constexpr auto curr_i64 = make_curr_for_i64_cross_load<Opt>();
            constexpr auto curr_f32 = make_curr_for_f32_cross_load<Opt>();
            constexpr auto curr_f64 = make_curr_for_f64_cross_load<Opt>();

            auto const spill_i64 = expected_spill1_fptr<Opt, wasm_i64>();
            auto const spill_f32 = expected_spill1_fptr<Opt, wasm_f32>();
            auto const spill_f64 = expected_spill1_fptr<Opt, wasm_f64>();

            auto const exp_i64_load = optable::translate::get_uwvmint_i64_load_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
            auto const exp_i64_load8_s = optable::translate::get_uwvmint_i64_load8_s_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
            auto const exp_i64_load8_u = optable::translate::get_uwvmint_i64_load8_u_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
            auto const exp_i64_load16_s = optable::translate::get_uwvmint_i64_load16_s_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
            auto const exp_i64_load16_u = optable::translate::get_uwvmint_i64_load16_u_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
            auto const exp_i64_load32_s = optable::translate::get_uwvmint_i64_load32_s_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
            auto const exp_i64_load32_u = optable::translate::get_uwvmint_i64_load32_u_fptr_from_tuple<Opt>(curr_i64, mem, tuple);
            auto const exp_f32_load = optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr_f32, mem, tuple);
            auto const exp_f64_load = optable::translate::get_uwvmint_f64_load_fptr_from_tuple<Opt>(curr_f64, mem, tuple);

            auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
            auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
            auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
            auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;
            auto const& bc4 = cm.local_funcs.index_unchecked(4).op.operands;
            auto const& bc5 = cm.local_funcs.index_unchecked(5).op.operands;
            auto const& bc6 = cm.local_funcs.index_unchecked(6).op.operands;
            auto const& bc7 = cm.local_funcs.index_unchecked(7).op.operands;
            auto const& bc8 = cm.local_funcs.index_unchecked(8).op.operands;

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, spill_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, spill_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, spill_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, spill_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc4, spill_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc5, spill_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc6, spill_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc7, spill_f32));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc8, spill_f64));

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_i64_load));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_i64_load8_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_i64_load8_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_i64_load16_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc4, exp_i64_load16_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc5, exp_i64_load32_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc6, exp_i64_load32_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc7, exp_f32_load));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc8, exp_f64_load));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_memory_cross_ring_prepare() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_cross_ring_prepare_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_cross_ring_prepare");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        {
            constexpr auto opt{k_test_tail_minimal_split_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_memory_cross_ring_prepare_suite<opt>(rt, true) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_memory_cross_ring_prepare_suite<opt>(rt, false) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_memory_cross_ring_prepare_suite<opt>(rt, false) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_memory_cross_ring_prepare();
}
