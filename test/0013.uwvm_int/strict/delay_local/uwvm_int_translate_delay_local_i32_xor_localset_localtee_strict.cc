#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x2(::std::int32_t a, ::std::int32_t b)
    {
        byte_vec out(8);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_delay_local_i32_xor_localset_localtee_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: i32.add local_set variant: b = a + b ; return b
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32.xor local_set variant: b = a ^ b ; return b
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_set);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: regress (local.tee + br_if): delay_local must NOT fire when combine may rewrite local.tee.
        // return (a^b) if non-zero else 7.
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_block_empty);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_tee);
            u32(c, 0u);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::i32_const);
            i32(c, 7);
            op(c, wasm_op::local_set);
            u32(c, 0u);

            op(c, wasm_op::end);  // end block
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: i32.xor local_tee variant: tmp = a ^ b ; drop ; return tmp
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local2
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_xor);
            op(c, wasm_op::local_tee);
            u32(c, 2u);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 4uz);

        using Runner = interpreter_runner<Opt>;
        [[maybe_unused]] auto const curr{make_entry_stacktop_currpos<Opt>()};
        [[maybe_unused]] constexpr auto tuple =
            compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});
        auto const contains_i32_variant{
            [&](auto const& bc, auto make_fptr) constexpr noexcept
            {
                if(bytecode_contains_fptr(bc, make_fptr(curr))) { return true; }

                if constexpr(Opt.i32_stack_top_begin_pos != SIZE_MAX && Opt.i32_stack_top_begin_pos != Opt.i32_stack_top_end_pos)
                {
                    auto curr_variant{curr};
                    for(::std::size_t pos{Opt.i32_stack_top_begin_pos}; pos < Opt.i32_stack_top_end_pos; ++pos)
                    {
                        curr_variant.i32_stack_top_curr_pos = pos;
                        if(bytecode_contains_fptr(bc, make_fptr(curr_variant))) { return true; }
                    }
                }

                return false;
            }};

        if constexpr(Opt.is_tail_call)
        {
#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            UWVM2TEST_REQUIRE(contains_i32_variant(
                cm.local_funcs.index_unchecked(2).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                { return optable::translate::get_uwvmint_br_if_local_tee_nz_fptr_from_tuple<Opt>(curr_variant, tuple); }));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT)
            UWVM2TEST_REQUIRE(contains_i32_variant(
                cm.local_funcs.index_unchecked(0).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i32_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::add>(curr_variant, tuple);
                }));
            UWVM2TEST_REQUIRE(contains_i32_variant(
                cm.local_funcs.index_unchecked(1).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i32_binop_localget_rhs_local_set_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::xor_>(curr_variant, tuple);
                }));
            UWVM2TEST_REQUIRE(contains_i32_variant(
                cm.local_funcs.index_unchecked(3).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::xor_>(curr_variant, tuple);
                }));

            // Regress: delay_local *_local_tee must not be used when local.tee is followed by br_if (local.tee+br_if may fuse).
            UWVM2TEST_REQUIRE(!contains_i32_variant(
                cm.local_funcs.index_unchecked(2).op.operands,
                [&](auto const& curr_variant) constexpr noexcept
                {
                    return optable::translate::get_uwvmint_i32_binop_localget_rhs_local_tee_fptr_from_tuple<
                        Opt,
                        optable::numeric_details::int_binop::xor_>(curr_variant, tuple);
                }));
#endif
        }

        // f0: b = a + b
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32x2(3, 4),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);

        // f1: b = a ^ b
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                              rt.local_defined_function_vec_storage.index_unchecked(1),
                                              pack_i32x2(5, 6),
                                              nullptr,
                                              nullptr)
                                       .results) == 3);

        // f2: regress br_if: return xor unless zero => 7
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(5, 5),
                                              nullptr,
                                              nullptr)
                                       .results) == 7);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(2),
                                              rt.local_defined_function_vec_storage.index_unchecked(2),
                                              pack_i32x2(5, 6),
                                              nullptr,
                                              nullptr)
                                       .results) == 3);

        // f3: tmp = a ^ b
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2(5, 5),
                                              nullptr,
                                              nullptr)
                                       .results) == 0);
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(3),
                                              rt.local_defined_function_vec_storage.index_unchecked(3),
                                              pack_i32x2(5, 6),
                                              nullptr,
                                              nullptr)
                                       .results) == 3);

        return 0;
    }

    [[nodiscard]] int test_translate_delay_local_i32_xor_localset_localtee() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;

        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_delay_local_i32_xor_localset_localtee_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_delay_local_i32_xor_localset_localtee");
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
    return test_translate_delay_local_i32_xor_localset_localtee();
}
