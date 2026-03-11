#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    template <optable::uwvm_interpreter_translate_option_t Opt, typename ByteStorage, typename MakeFptr>
    [[nodiscard]] bool bytecode_contains_i32_variant(ByteStorage const& bc, MakeFptr&& make_fptr) noexcept
    {
        auto curr{make_initial_stacktop_currpos<Opt>()};
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

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_br_if_i32_cmp_generic_more_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        auto add_brif_i32_cmp2 = [&](wasm_op cmp_op, ::std::int32_t taken_ret, ::std::int32_t fall_ret)
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, taken_ret);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, cmp_op);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, fall_ret);

            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_brif_i32_cmp2(wasm_op::i32_eq, 111, 222);    // f0
        add_brif_i32_cmp2(wasm_op::i32_lt_s, 333, 444);  // f1
        add_brif_i32_cmp2(wasm_op::i32_lt_u, 555, 666);  // f2
        add_brif_i32_cmp2(wasm_op::i32_gt_u, 777, 888);  // f3
        add_brif_i32_cmp2(wasm_op::i32_le_s, 999, 123);  // f4

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 5uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
                cm.local_funcs.index_unchecked(0).op.operands,
                [&](auto const& curr_variant) noexcept
                { return optable::translate::get_uwvmint_br_if_i32_eq_fptr_from_tuple<Opt>(curr_variant, tuple); }));
            UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
                cm.local_funcs.index_unchecked(1).op.operands,
                [&](auto const& curr_variant) noexcept
                { return optable::translate::get_uwvmint_br_if_i32_lt_s_fptr_from_tuple<Opt>(curr_variant, tuple); }));
            UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
                cm.local_funcs.index_unchecked(2).op.operands,
                [&](auto const& curr_variant) noexcept
                { return optable::translate::get_uwvmint_br_if_i32_lt_u_fptr_from_tuple<Opt>(curr_variant, tuple); }));
            UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
                cm.local_funcs.index_unchecked(3).op.operands,
                [&](auto const& curr_variant) noexcept
                { return optable::translate::get_uwvmint_br_if_i32_gt_u_fptr_from_tuple<Opt>(curr_variant, tuple); }));
            UWVM2TEST_REQUIRE(bytecode_contains_i32_variant<Opt>(
                cm.local_funcs.index_unchecked(4).op.operands,
                [&](auto const& curr_variant) noexcept
                { return optable::translate::get_uwvmint_br_if_i32_le_s_fptr_from_tuple<Opt>(curr_variant, tuple); }));
        }
#endif

        auto run_i32 = [&](::std::size_t fn_index, ::std::int32_t a, ::std::int32_t b) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fn_index),
                                  rt.local_defined_function_vec_storage.index_unchecked(fn_index),
                                  pack_i32x2(a, b),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        for(auto const [a, b] : ::std::array{
                ::std::pair{7, 7},
                ::std::pair{6, 7},
                ::std::pair{7, 6},
                ::std::pair{-1, 1},
                ::std::pair{static_cast<::std::int32_t>(0x80000000u), 1},
            })
        {
            UWVM2TEST_REQUIRE(run_i32(0uz, a, b) == (a == b ? 111 : 222));
            UWVM2TEST_REQUIRE(run_i32(1uz, a, b) == (a < b ? 333 : 444));
            UWVM2TEST_REQUIRE(run_i32(2uz, a, b) == (static_cast<::std::uint32_t>(a) < static_cast<::std::uint32_t>(b) ? 555 : 666));
            UWVM2TEST_REQUIRE(run_i32(3uz, a, b) == (static_cast<::std::uint32_t>(a) > static_cast<::std::uint32_t>(b) ? 777 : 888));
            UWVM2TEST_REQUIRE(run_i32(4uz, a, b) == (a <= b ? 999 : 123));
        }

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_i32_cmp_generic_more() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_i32_cmp_generic_more_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_i32_cmp_generic_more");
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
        return test_translate_br_if_i32_cmp_generic_more();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
