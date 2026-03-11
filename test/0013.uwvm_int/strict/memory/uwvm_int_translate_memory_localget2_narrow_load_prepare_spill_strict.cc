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

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

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

    template <typename ByteStorage, typename Fptr>
    [[nodiscard]] ::std::size_t bytecode_count_fptr(ByteStorage const& bytes, Fptr fptr) noexcept
    {
        if(fptr == nullptr) { return 0uz; }
        ::std::array<::std::byte, sizeof(Fptr)> needle{};
        ::std::memcpy(needle.data(), ::std::addressof(fptr), sizeof(Fptr));
        if(bytes.size() < needle.size()) { return 0uz; }

        ::std::size_t count{};
        for(::std::size_t i{}; i + needle.size() <= bytes.size(); ++i)
        {
            if(::std::memcmp(bytes.data() + i, needle.data(), needle.size()) == 0) { ++count; }
        }
        return count;
    }


    [[nodiscard]] byte_vec build_memory_localget2_narrow_load_prepare_spill_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;
        mb.memory_has_max = true;
        mb.memory_max = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 64);
            op(c, wasm_op::i32_const); i32(c, 0x80);
            op(c, wasm_op::i32_store8); u32(c, 0u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load8_u); u32(c, 0u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 96);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 7);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load16_s); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 112);
            op(c, wasm_op::i32_const); i32(c, 0x8001);
            op(c, wasm_op::i32_store16); u32(c, 1u); u32(c, 0u);

            op(c, wasm_op::i32_const); i32(c, 9);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::i32_load16_u); u32(c, 1u); u32(c, 0u);
            op(c, wasm_op::local_set); u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get); u32(c, 2u);
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
                                  pack_i32x2(11, 64),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 133);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i32x2(22, 96),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == -32760);
        }

        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_i32x2(33, 112),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 32778);
        }

        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename MemoryRef>
    [[nodiscard]] int check_bytecode(compiled_module_t const& cm, MemoryRef const& mem, bool expect_spill) noexcept
    {
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr auto curr_load = make_curr_after_i32_pushes<Opt>(2uz);
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        if constexpr(Opt.is_tail_call)
        {
            auto const exp_load8_u = optable::translate::get_uwvmint_i32_load8_u_localget_off_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_load16_s = optable::translate::get_uwvmint_i32_load16_s_localget_off_fptr_from_tuple<Opt>(curr_load, mem, tuple);
            auto const exp_load16_u = optable::translate::get_uwvmint_i32_load16_u_localget_off_fptr_from_tuple<Opt>(curr_load, mem, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_load8_u));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_load16_s));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_load16_u));


            if(expect_spill)
            {
                UWVM2TEST_REQUIRE((contains_i32_spill1<Opt>(cm.local_funcs.index_unchecked(0).op.operands)));
                UWVM2TEST_REQUIRE((contains_i32_spill1<Opt>(cm.local_funcs.index_unchecked(1).op.operands)));
                UWVM2TEST_REQUIRE((contains_i32_spill1<Opt>(cm.local_funcs.index_unchecked(2).op.operands)));
            }
        }
#else
        (void)cm;
        (void)mem;
        (void)expect_spill;
#endif
        return 0;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_check_and_run(runtime_module_t const& rt, bool expect_spill) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        UWVM2TEST_REQUIRE(!rt.local_defined_memory_vec_storage.empty());
        auto const& mem = rt.local_defined_memory_vec_storage.index_unchecked(0).memory;
        UWVM2TEST_REQUIRE(check_bytecode<Opt>(cm, mem, expect_spill) == 0);
        UWVM2TEST_REQUIRE((run_suite<Opt>(cm, rt)) == 0);
        return 0;
    }

    [[nodiscard]] int test_translate_memory_localget2_narrow_load_prepare_spill() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_memory_localget2_narrow_load_prepare_spill_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_memory_localget2_narrow_load_prepare_spill");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 5uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_check_and_run<opt>(rt, false) == 0);
        }

        static_assert(compiler::details::interpreter_tuple_has_no_holes<k_test_tail_i32_ring2_opt>());
        UWVM2TEST_REQUIRE(compile_check_and_run<k_test_tail_i32_ring2_opt>(rt, true) == 0);

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_memory_localget2_narrow_load_prepare_spill();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
