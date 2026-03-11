#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

    inline constexpr optable::uwvm_interpreter_translate_option_t k_test_tail_float_plain_survivor_split_opt{
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
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_f32_survivor() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos)
        {
            curr.f32_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.f32_stack_top_curr_pos, Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_end_pos);
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_after_f64_survivor() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos)
        {
            curr.f64_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.f64_stack_top_curr_pos, Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_end_pos);
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_for_f32_plain_load() noexcept
    {
        auto curr = make_curr_after_f32_survivor<Opt>();
        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            curr.i32_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_for_f64_plain_load() noexcept
    {
        auto curr = make_curr_after_f64_survivor<Opt>();
        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            curr.i32_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType>
    [[nodiscard]] consteval ::std::size_t make_store_value_push_count() noexcept
    {
        if constexpr(::std::same_as<ValType, wasm_f32>)
        {
            if constexpr(Opt.f32_stack_top_begin_pos == SIZE_MAX || Opt.f32_stack_top_begin_pos == Opt.f32_stack_top_end_pos)
            {
                return 0uz;
            }
            else
            {
                constexpr ::std::size_t cap = Opt.f32_stack_top_end_pos - Opt.f32_stack_top_begin_pos;
                return cap < 2uz ? cap : 2uz;
            }
        }
        else
        {
            if constexpr(Opt.f64_stack_top_begin_pos == SIZE_MAX || Opt.f64_stack_top_begin_pos == Opt.f64_stack_top_end_pos)
            {
                return 0uz;
            }
            else
            {
                constexpr ::std::size_t cap = Opt.f64_stack_top_end_pos - Opt.f64_stack_top_begin_pos;
                return cap < 2uz ? cap : 2uz;
            }
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType>
    [[nodiscard]] consteval optable::uwvm_interpreter_stacktop_currpos_t make_curr_for_plain_store() noexcept
    {
        auto curr = make_initial_stacktop_currpos<Opt>();
        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            curr.i32_stack_top_curr_pos =
                optable::details::ring_prev_pos(curr.i32_stack_top_curr_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos);
        }
        if constexpr(::std::same_as<ValType, wasm_f32>)
        {
            if constexpr(Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos)
            {
                for(::std::size_t i{}; i != make_store_value_push_count<Opt, wasm_f32>(); ++i)
                {
                    curr.f32_stack_top_curr_pos =
                        optable::details::ring_prev_pos(curr.f32_stack_top_curr_pos, Opt.f32_stack_top_begin_pos, Opt.f32_stack_top_end_pos);
                }
            }
        }
        else if constexpr(::std::same_as<ValType, wasm_f64>)
        {
            if constexpr(Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos)
            {
                for(::std::size_t i{}; i != make_store_value_push_count<Opt, wasm_f64>(); ++i)
                {
                    curr.f64_stack_top_curr_pos =
                        optable::details::ring_prev_pos(curr.f64_stack_top_curr_pos, Opt.f64_stack_top_begin_pos, Opt.f64_stack_top_end_pos);
                }
            }
        }
        return curr;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType, typename ByteStorage>
    [[nodiscard]] bool contains_spill1(ByteStorage const& bc) noexcept
    {
        if constexpr(optable::details::stacktop_range_begin_pos<Opt, ValType>() == optable::details::stacktop_range_end_pos<Opt, ValType>())
        {
            return false;
        }
        else
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
            for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, ValType>();
                start_pos < optable::details::stacktop_range_end_pos<Opt, ValType>();
                ++start_pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                optable::uwvm_interpreter_stacktop_remain_size_t remain{};
                if constexpr(::std::same_as<ValType, wasm_f32>)
                {
                    curr.f32_stack_top_curr_pos = start_pos;
                    remain.f32_stack_top_remain_size = 1uz;
                }
                else if constexpr(::std::same_as<ValType, wasm_f64>)
                {
                    curr.f64_stack_top_curr_pos = start_pos;
                    remain.f64_stack_top_remain_size = 1uz;
                }
                auto const fptr = optable::translate::get_uwvmint_stacktop_to_operand_stack_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }
            return false;
        }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ValType, typename ByteStorage>
    [[nodiscard]] bool contains_fill1(ByteStorage const& bc) noexcept
    {
        if constexpr(optable::details::stacktop_range_begin_pos<Opt, ValType>() == optable::details::stacktop_range_end_pos<Opt, ValType>())
        {
            return false;
        }
        else
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
            for(::std::size_t start_pos = optable::details::stacktop_range_begin_pos<Opt, ValType>();
                start_pos < optable::details::stacktop_range_end_pos<Opt, ValType>();
                ++start_pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{};
                optable::uwvm_interpreter_stacktop_remain_size_t remain{};
                if constexpr(::std::same_as<ValType, wasm_f32>)
                {
                    curr.f32_stack_top_curr_pos = start_pos;
                    remain.f32_stack_top_remain_size = 1uz;
                }
                else if constexpr(::std::same_as<ValType, wasm_f64>)
                {
                    curr.f64_stack_top_curr_pos = start_pos;
                    remain.f64_stack_top_remain_size = 1uz;
                }
                auto const fptr = optable::translate::get_uwvmint_operand_stack_to_stacktop_fptr_from_tuple<Opt, ValType>(curr, remain, tuple);
                if(bytecode_contains_fptr(bc, fptr)) { return true; }
            }
            return false;
        }
    }

    [[nodiscard]] byte_vec build_memory_float_plain_survivor_nonzero_offset_module()
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

        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 76);
            op(c, wasm_op::f32_const); f32(c, 3.5f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 0u);

            op(c, wasm_op::f32_const); f32(c, 1.25f);
            op(c, wasm_op::i32_const); i32(c, 64);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 12u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 112);
            op(c, wasm_op::f64_const); f64(c, 6.25);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 0u);

            op(c, wasm_op::f64_const); f64(c, 1.5);
            op(c, wasm_op::i32_const); i32(c, 96);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 16u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f32_const); f32(c, 1.25f);
            op(c, wasm_op::i32_const); i32(c, 60);
            op(c, wasm_op::f32_const); f32(c, 3.5f);
            op(c, wasm_op::f32_store); u32(c, 2u); u32(c, 12u);

            op(c, wasm_op::i32_const); i32(c, 72);
            op(c, wasm_op::f32_load); u32(c, 2u); u32(c, 0u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f64_const); f64(c, 1.5);
            op(c, wasm_op::i32_const); i32(c, 80);
            op(c, wasm_op::f64_const); f64(c, 6.25);
            op(c, wasm_op::f64_store); u32(c, 3u); u32(c, 24u);

            op(c, wasm_op::i32_const); i32(c, 104);
            op(c, wasm_op::f64_load); u32(c, 3u); u32(c, 0u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(compiled_module_t const& cm, runtime_module_t const& rt) noexcept
    {
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 4uz);
        using Runner = interpreter_runner<Opt>;

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_no_params(),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 4.75f);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_no_params(),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 7.75);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_no_params(),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == 4.75f);
        }
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_no_params(),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 7.75);
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_tail_bytecode(compiled_module_t const& cm,
                                          ::uwvm2::object::memory::linear::native_memory_t const& mem,
                                          bool expect_spill_fill) noexcept
    {
        static_assert(Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        constexpr auto curr_f32_load = make_curr_for_f32_plain_load<Opt>();
        constexpr auto curr_f64_load = make_curr_for_f64_plain_load<Opt>();
        constexpr auto curr_f32_store = make_curr_for_plain_store<Opt, wasm_f32>();
        constexpr auto curr_f64_store = make_curr_for_plain_store<Opt, wasm_f64>();
        constexpr auto curr_f32_survivor = make_curr_after_f32_survivor<Opt>();
        constexpr auto curr_f64_survivor = make_curr_after_f64_survivor<Opt>();

        auto const exp_f32_load = optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr_f32_load, mem, tuple);
        auto const exp_f64_load = optable::translate::get_uwvmint_f64_load_fptr_from_tuple<Opt>(curr_f64_load, mem, tuple);
        auto const exp_f32_store = optable::translate::get_uwvmint_f32_store_fptr_from_tuple<Opt>(curr_f32_store, mem, tuple);
        auto const exp_f64_store = optable::translate::get_uwvmint_f64_store_fptr_from_tuple<Opt>(curr_f64_store, mem, tuple);

        auto const exp_f32_load_localget = optable::translate::get_uwvmint_f32_load_localget_off_fptr_from_tuple<Opt>(curr_f32_survivor, mem, tuple);
        auto const exp_f64_load_localget = optable::translate::get_uwvmint_f64_load_localget_off_fptr_from_tuple<Opt>(curr_f64_survivor, mem, tuple);
        auto const exp_f32_load_local_plus_imm =
            optable::translate::get_uwvmint_f32_load_local_plus_imm_fptr_from_tuple<Opt>(curr_f32_survivor, mem, tuple);
        auto const exp_f64_load_local_plus_imm =
            optable::translate::get_uwvmint_f64_load_local_plus_imm_fptr_from_tuple<Opt>(curr_f64_survivor, mem, tuple);
        auto const exp_f32_store_local_plus_imm =
            optable::translate::get_uwvmint_f32_store_local_plus_imm_fptr_from_tuple<Opt>(curr_f32_survivor, mem, tuple);
        auto const exp_f64_store_local_plus_imm =
            optable::translate::get_uwvmint_f64_store_local_plus_imm_fptr_from_tuple<Opt>(curr_f64_survivor, mem, tuple);

        auto const& bc0 = cm.local_funcs.index_unchecked(0).op.operands;
        auto const& bc1 = cm.local_funcs.index_unchecked(1).op.operands;
        auto const& bc2 = cm.local_funcs.index_unchecked(2).op.operands;
        auto const& bc3 = cm.local_funcs.index_unchecked(3).op.operands;

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc0, exp_f32_load));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc1, exp_f64_load));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc2, exp_f32_store));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(bc3, exp_f64_store));

        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_f32_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc0, exp_f32_load_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_f64_load_localget));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc1, exp_f64_load_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc2, exp_f32_store_local_plus_imm));
        UWVM2TEST_REQUIRE(!bytecode_contains_fptr(bc3, exp_f64_store_local_plus_imm));

        if(expect_spill_fill)
        {
            UWVM2TEST_REQUIRE((contains_spill1<Opt, wasm_f32>(bc0)));
            UWVM2TEST_REQUIRE((contains_spill1<Opt, wasm_f64>(bc1)));
            UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_f32>(bc2)));
            UWVM2TEST_REQUIRE((contains_fill1<Opt, wasm_f64>(bc3)));
        }
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int check_byref_bytecode(compiled_module_t const& cm) noexcept
    {
        static_assert(!Opt.is_tail_call);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        constexpr auto curr_f32_load = make_curr_for_f32_plain_load<Opt>();
        constexpr auto curr_f64_load = make_curr_for_f64_plain_load<Opt>();
        constexpr auto curr_f32_store = make_curr_for_plain_store<Opt, wasm_f32>();
        constexpr auto curr_f64_store = make_curr_for_plain_store<Opt, wasm_f64>();

        auto const exp_f32_load = optable::translate::get_uwvmint_f32_load_fptr_from_tuple<Opt>(curr_f32_load, tuple);
        auto const exp_f64_load = optable::translate::get_uwvmint_f64_load_fptr_from_tuple<Opt>(curr_f64_load, tuple);
        auto const exp_f32_store = optable::translate::get_uwvmint_f32_store_fptr_from_tuple<Opt>(curr_f32_store, tuple);
        auto const exp_f64_store = optable::translate::get_uwvmint_f64_store_fptr_from_tuple<Opt>(curr_f64_store, tuple);

        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_f32_load));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_f64_load));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_store));
        UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f64_store));
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt, bool verify_bytecode, bool expect_spill_fill) noexcept
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
                UWVM2TEST_REQUIRE(check_tail_bytecode<Opt>(cm, mem, expect_spill_fill) == 0);
            }
            else
            {
                UWVM2TEST_REQUIRE(check_byref_bytecode<Opt>(cm) == 0);
            }
        }

        UWVM2TEST_REQUIRE(run_suite<Opt>(cm, rt) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_float_plain_survivor_nonzero_offset() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_float_plain_survivor_nonzero_offset_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_float_plain_survivor_nonzero_offset");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, true, false) == 0);
        }

        static_assert(compiler::details::interpreter_tuple_has_no_holes<k_test_tail_float_plain_survivor_split_opt>());
        UWVM2TEST_REQUIRE(compile_check_and_run<k_test_tail_float_plain_survivor_split_opt>(rt, true, true) == 0);

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
        return test_translate_memory_float_plain_survivor_nonzero_offset();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
