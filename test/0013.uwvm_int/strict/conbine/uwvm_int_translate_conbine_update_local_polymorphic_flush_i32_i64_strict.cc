#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32_i64(::std::int32_t a, ::std::int64_t b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_i64_i32(::std::int64_t a, ::std::int32_t b)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_conbine_update_local_polymorphic_flush_i32_i64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        auto add_i32_flush_case = [&](wasm_op binop, ::std::int32_t imm)
        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_i32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set); u32(c, 1u);  // wrong-type/same-offset barrier under polymorphic stack
            op(c, wasm_op::i32_const); i32(c, 1);

            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i64_flush_case = [&](wasm_op binop, ::std::int64_t imm)
        {
            func_type ty{{k_val_i64, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_i32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set); u32(c, 1u);  // wrong-type/same-offset barrier under polymorphic stack
            op(c, wasm_op::i32_const); i32(c, 1);

            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_i32_flush_case(wasm_op::i32_add, 5);
        add_i32_flush_case(wasm_op::i32_sub, 7);
        add_i32_flush_case(wasm_op::i32_mul, 3);
        add_i32_flush_case(wasm_op::i32_and, static_cast<::std::int32_t>(0x00ff00ffu));
        add_i32_flush_case(wasm_op::i32_or, static_cast<::std::int32_t>(0x0f0f0f0fu));
        add_i32_flush_case(wasm_op::i32_xor, static_cast<::std::int32_t>(0x55aa55aau));
        add_i32_flush_case(wasm_op::i32_shl, 5);
        add_i32_flush_case(wasm_op::i32_shr_s, 13);
        add_i32_flush_case(wasm_op::i32_shr_u, 9);
        add_i32_flush_case(wasm_op::i32_rotl, 11);
        add_i32_flush_case(wasm_op::i32_rotr, 7);

        add_i64_flush_case(wasm_op::i64_add, 5);
        add_i64_flush_case(wasm_op::i64_sub, 7);
        add_i64_flush_case(wasm_op::i64_mul, 3);
        add_i64_flush_case(wasm_op::i64_and, static_cast<::std::int64_t>(0x00ff00ff00ff00ffull));
        add_i64_flush_case(wasm_op::i64_or, static_cast<::std::int64_t>(0x0f0f0f0f0f0f0f0full));
        add_i64_flush_case(wasm_op::i64_xor, static_cast<::std::int64_t>(0x55aa55aa55aa55aaull));
        add_i64_flush_case(wasm_op::i64_shl, 5);
        add_i64_flush_case(wasm_op::i64_shr_s, 13);
        add_i64_flush_case(wasm_op::i64_shr_u, 9);
        add_i64_flush_case(wasm_op::i64_rotl, 11);
        add_i64_flush_case(wasm_op::i64_rotr, 7);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i32_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos < Opt.i32_stack_top_end_pos; ++pos)
            {
                curr.i32_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i64_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_entry_stacktop_currpos<Opt>()};
        if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

        if constexpr(Opt.i64_stack_top_begin_pos != SIZE_MAX && Opt.i64_stack_top_begin_pos != Opt.i64_stack_top_end_pos)
        {
            for(::std::size_t pos{Opt.i64_stack_top_begin_pos}; pos < Opt.i64_stack_top_end_pos; ++pos)
            {
                curr.i64_stack_top_curr_pos = pos;
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }
            }
        }

        return false;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_conbine_update_local_polymorphic_flush_i32_i64_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 22uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        auto require_i32_flush_case = [&](::std::size_t index, auto make_localget_fptr, auto make_set_same_fptr) constexpr noexcept -> int
        {
            auto const& bc = cm.local_funcs.index_unchecked(index).op.operands;
            UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(bc, make_localget_fptr));
            UWVM2TEST_REQUIRE(!bytecode_contains_i32_variant<Opt>(bc, make_set_same_fptr));
            return 0;
        };

        auto require_i64_flush_case = [&](::std::size_t index, auto make_localget_fptr, auto make_set_same_fptr) constexpr noexcept -> int
        {
            auto const& bc = cm.local_funcs.index_unchecked(index).op.operands;
            UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(bc, make_localget_fptr));
            UWVM2TEST_REQUIRE(!bytecode_contains_i64_variant<Opt>(bc, make_set_same_fptr));
            return 0;
        };

        UWVM2TEST_REQUIRE(require_i32_flush_case(
            0uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_i32_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple); },
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_i32_add_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            1uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::sub>(curr,
                                                                                                                                          tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::sub>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            2uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::mul>(curr,
                                                                                                                                          tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::mul>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            3uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::and_>(curr,
                                                                                                                                           tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::and_>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            4uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::or_>(curr,
                                                                                                                                          tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::or_>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            5uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::xor_>(curr,
                                                                                                                                           tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::xor_>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            6uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shl>(curr,
                                                                                                                                          tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::shl>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            7uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shr_s>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::shr_s>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            8uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shr_u>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::shr_u>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            9uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::rotl>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::rotl>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i32_flush_case(
            10uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::rotr>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i32_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::rotr>(
                    curr,
                    tuple);
            }) == 0);

        UWVM2TEST_REQUIRE(require_i64_flush_case(
            11uz,
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_i64_add_imm_localget_fptr_from_tuple<Opt>(curr, tuple); },
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_i64_add_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            12uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::sub>(curr,
                                                                                                                                          tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::sub>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            13uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::mul>(curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_i64_mul_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            14uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::and_>(curr,
                                                                                                                                           tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::and_>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            15uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::or_>(curr,
                                                                                                                                          tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::or_>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            16uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::xor_>(curr,
                                                                                                                                           tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::xor_>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            17uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shl>(curr,
                                                                                                                                          tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::shl>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            18uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shr_s>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::shr_s>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            19uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shr_u>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::shr_u>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            20uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::rotl>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_local_set_same_fptr_from_tuple<Opt,
                                                                                                     optable::numeric_details::int_binop::rotl>(
                    curr,
                    tuple);
            }) == 0);
        UWVM2TEST_REQUIRE(require_i64_flush_case(
            21uz,
            [&](auto const& curr) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::rotr>(
                    curr,
                    tuple);
            },
            [&](auto const& curr) constexpr noexcept
            { return optable::translate::get_uwvmint_i64_rotr_imm_local_set_same_fptr_from_tuple<Opt>(curr, tuple); }) == 0);
#endif

        for(::std::size_t i{}; i != 11uz; ++i)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(i),
                                  rt.local_defined_function_vec_storage.index_unchecked(i),
                                  pack_i32_i64(7, 9),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
        }

        for(::std::size_t i{11uz}; i != 22uz; ++i)
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(i),
                                  rt.local_defined_function_vec_storage.index_unchecked(i),
                                  pack_i64_i32(7, 9),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == 0);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_update_local_polymorphic_flush_i32_i64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_update_local_polymorphic_flush_i32_i64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_update_local_polymorphic_flush_i32_i64");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_conbine_update_local_polymorphic_flush_i32_i64_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_conbine_update_local_polymorphic_flush_i32_i64_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_update_local_polymorphic_flush_i32_i64_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_update_local_polymorphic_flush_i32_i64_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_update_local_polymorphic_flush_i32_i64_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_conbine_update_local_polymorphic_flush_i32_i64();
}
