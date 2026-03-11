#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] constexpr ::std::uint64_t rotl64(::std::uint64_t x, unsigned s) noexcept
    {
        s &= 63u;
        return (x << s) | (x >> ((64u - s) & 63u));
    }

    [[nodiscard]] constexpr ::std::uint64_t rotr64(::std::uint64_t x, unsigned s) noexcept
    {
        s &= 63u;
        return (x >> s) | (x << ((64u - s) & 63u));
    }

    [[nodiscard]] constexpr ::std::uint64_t shr_s64(::std::uint64_t x, unsigned s) noexcept
    {
        s &= 63u;
        if(s == 0u) { return x; }
        if((x >> 63u) == 0u) { return x >> s; }
        return (x >> s) | (~0ull << (64u - s));
    }

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i64_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_initial_stacktop_currpos<Opt>()};
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

    [[nodiscard]] byte_vec build_conbine_i64_imm_localget_more_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        auto add_unary = [&](wasm_op opcode, ::std::int64_t imm)
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, imm);
            op(c, opcode);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_unary(wasm_op::i64_sub, 9);
        add_unary(wasm_op::i64_mul, 6);
        add_unary(wasm_op::i64_or, static_cast<::std::int64_t>(0x0f0f0f0f0f0f0f0full));
        add_unary(wasm_op::i64_xor, static_cast<::std::int64_t>(0x55aa55aa55aa55aaull));
        add_unary(wasm_op::i64_shl, 5);
        add_unary(wasm_op::i64_shr_u, 9);
        add_unary(wasm_op::i64_shr_s, 13);
        add_unary(wasm_op::i64_rotl, 7);
        add_unary(wasm_op::i64_rotr, 11);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 9uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(0).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::sub>(
                    curr_variant,
                    tuple);
            }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(1).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::mul>(
                    curr_variant,
                    tuple);
            }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(2).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::or_>(
                    curr_variant,
                    tuple);
            }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(3).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::xor_>(
                    curr_variant,
                    tuple);
            }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(4).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shl>(
                    curr_variant,
                    tuple);
            }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(5).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shr_u>(
                    curr_variant,
                    tuple);
            }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(6).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::shr_s>(
                    curr_variant,
                    tuple);
            }));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(7).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::rotl>(
                    curr_variant,
                    tuple);
            }));

        UWVM2TEST_REQUIRE(bytecode_contains_i64_variant<Opt>(
            cm.local_funcs.index_unchecked(8).op.operands,
            [&](auto const& curr_variant) constexpr noexcept
            {
                return optable::translate::get_uwvmint_i64_binop_imm_localget_fptr_from_tuple<Opt,
                                                                                                optable::numeric_details::int_binop::rotr>(
                    curr_variant,
                    tuple);
            }));
#endif

        constexpr ::std::uint64_t x{0x123456789abcdef0ull};
        constexpr ::std::uint64_t neg_x{0xf123456789abcdefull};

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(0),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(0),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (x - 9ull));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(1),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(1),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (x * 6ull));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(2),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(2),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (x | 0x0f0f0f0f0f0f0f0full));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(3),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(3),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (x ^ 0x55aa55aa55aa55aaull));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(4),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (x << 5u));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(5),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(5),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == (x >> 9u));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(6),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(6),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(neg_x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == shr_s64(neg_x, 13u));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(7),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(7),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == rotl64(x, 7u));

        UWVM2TEST_REQUIRE(static_cast<::std::uint64_t>(load_i64(Runner::run(cm.local_funcs.index_unchecked(8),
                                                                            rt.local_defined_function_vec_storage.index_unchecked(8),
                                                                            pack_i64(::std::bit_cast<::std::int64_t>(x)),
                                                                            nullptr,
                                                                            nullptr)
                                                                     .results)) == rotr64(x, 11u));

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_i64_imm_localget_more() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_i64_imm_localget_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_i64_imm_localget_more");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_i64_imm_localget_more();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
