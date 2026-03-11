#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_f32_i32(float a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64_i64(double a, ::std::int64_t b)
    {
        byte_vec out(16);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_conbine_float_update_local_polymorphic_flush_f32_f64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_f32_flush_case = [&](wasm_op binop, float imm, float fallback)
        {
            func_type ty{{k_val_f32, k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_f32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_const); f32(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set); u32(c, 1u);  // wrong-type/same-offset barrier under polymorphic stack
            op(c, wasm_op::f32_const); f32(c, fallback);

            op(c, wasm_op::else_);
            op(c, wasm_op::f32_const); f32(c, fallback);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_flush_case = [&](wasm_op binop, double imm, double fallback)
        {
            func_type ty{{k_val_f64, k_val_i64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_f64);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_const); f64(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set); u32(c, 1u);  // wrong-type/same-offset barrier under polymorphic stack
            op(c, wasm_op::f64_const); f64(c, fallback);

            op(c, wasm_op::else_);
            op(c, wasm_op::f64_const); f64(c, fallback);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_f32_flush_case(wasm_op::f32_add, 1.5f, 11.25f);
        add_f32_flush_case(wasm_op::f32_mul, 2.0f, 22.5f);
        add_f32_flush_case(wasm_op::f32_sub, 0.25f, 33.75f);

        add_f64_flush_case(wasm_op::f64_add, 1.125, 44.5);
        add_f64_flush_case(wasm_op::f64_mul, 3.0, 55.625);
        add_f64_flush_case(wasm_op::f64_sub, 0.5, 66.75);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_f32_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.f32_stack_top_begin_pos != SIZE_MAX && Opt.f32_stack_top_begin_pos != Opt.f32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.f32_stack_top_begin_pos}; pos < Opt.f32_stack_top_end_pos; ++pos)
            {
                curr.f32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_f64_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.f64_stack_top_begin_pos != SIZE_MAX && Opt.f64_stack_top_begin_pos != Opt.f64_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.f64_stack_top_begin_pos}; pos < Opt.f64_stack_top_end_pos; ++pos)
            {
                curr.f64_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_conbine_float_update_local_polymorphic_flush_f32_f64_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 6uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto require_f32_negative_case = [&](::std::size_t index, auto make_set_same_fptr) constexpr noexcept -> int
        {
            auto const& bc = cm.local_funcs.index_unchecked(index).op.operands;
            UWVM2TEST_REQUIRE(!bytecode_contains_f32_variant<Opt>(bc, make_set_same_fptr));
            return 0;
        };

        auto require_f64_negative_case = [&](::std::size_t index, auto make_set_same_fptr) constexpr noexcept -> int
        {
            auto const& bc = cm.local_funcs.index_unchecked(index).op.operands;
            UWVM2TEST_REQUIRE(!bytecode_contains_f64_variant<Opt>(bc, make_set_same_fptr));
            return 0;
        };

        UWVM2TEST_REQUIRE(require_f32_negative_case(
            0uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_f32_add_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
        UWVM2TEST_REQUIRE(require_f32_negative_case(
            1uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_f32_mul_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
        UWVM2TEST_REQUIRE(require_f32_negative_case(
            2uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_f32_sub_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);

        UWVM2TEST_REQUIRE(require_f64_negative_case(
            3uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_f64_add_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
        UWVM2TEST_REQUIRE(require_f64_negative_case(
            4uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_f64_mul_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
        UWVM2TEST_REQUIRE(require_f64_negative_case(
            5uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_f64_sub_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
#endif

        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_f32_i32(9.0f, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 11.25f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_f32_i32(9.0f, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 22.5f);
        UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_f32_i32(9.0f, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 33.75f);

        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_f64_i64(9.0, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 44.5);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(4),
                                              rt.local_defined_function_vec_storage.index_unchecked(4),
                                              pack_f64_i64(9.0, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 55.625);
        UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(5),
                                              rt.local_defined_function_vec_storage.index_unchecked(5),
                                              pack_f64_i64(9.0, 7),
                                              nullptr,
                                              nullptr)
                                       .results) == 66.75);

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_float_update_local_polymorphic_flush_f32_f64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_float_update_local_polymorphic_flush_f32_f64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_float_update_local_polymorphic_flush_f32_f64");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_conbine_float_update_local_polymorphic_flush_f32_f64_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_conbine_float_update_local_polymorphic_flush_f32_f64_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(run_conbine_float_update_local_polymorphic_flush_f32_f64_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(run_conbine_float_update_local_polymorphic_flush_f32_f64_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_float_update_local_polymorphic_flush_f32_f64_suite<opt>(rt) == 0);
        }

        return 0;
    }
}

int main()
{
    return test_translate_conbine_float_update_local_polymorphic_flush_f32_f64();
}
