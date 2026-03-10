#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_conbine_f32_unop_trunc_localget_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        auto add_f32_unop = [&](wasm_op unop) -> void
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, unop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i32_from_f32 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_i32_from_f64 = [&](wasm_op conv) -> void
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, conv);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        // f32 unops (local.get pending allow-list in conbine_can_continue)
        add_f32_unop(wasm_op::f32_ceil);     // f0
        add_f32_unop(wasm_op::f32_floor);    // f1
        add_f32_unop(wasm_op::f32_trunc);    // f2
        add_f32_unop(wasm_op::f32_nearest);  // f3

        // trunc conversions (also in the f32/f64 local.get allow-list)
        add_i32_from_f32(wasm_op::i32_trunc_f32_s);  // f4
        add_i32_from_f32(wasm_op::i32_trunc_f32_u);  // f5
        add_i32_from_f64(wasm_op::i32_trunc_f64_s);  // f6
        add_i32_from_f64(wasm_op::i32_trunc_f64_u);  // f7

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 8uz);

        using Runner = interpreter_runner<Opt>;

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto curr{make_entry_stacktop_currpos<Opt>()};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            // f32 ceil/floor/trunc/nearest only need local.get to stay on the pending allow-list;
            // they do not all have dedicated localget opfuncs. Keep strict fptr checks only for
            // the convert cases that do expose specialized localget fusions.
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands,
                                                    optable::translate::get_uwvmint_i32_from_f32_trunc_s_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands,
                                                    optable::translate::get_uwvmint_i32_from_f32_trunc_u_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands,
                                                    optable::translate::get_uwvmint_i32_from_f64_trunc_s_fptr_from_tuple<Opt>(curr, tuple)));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands,
                                                    optable::translate::get_uwvmint_i32_from_f64_trunc_u_fptr_from_tuple<Opt>(curr, tuple)));
        }
#endif

        auto run_f32 = [&](::std::size_t fidx, float x) noexcept -> float
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32(x),
                                  nullptr,
                                  nullptr);
            return load_f32(rr.results);
        };

        auto run_i32_from_f32 = [&](::std::size_t fidx, float x) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f32(x),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        auto run_i32_from_f64 = [&](::std::size_t fidx, double x) noexcept -> ::std::int32_t
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(fidx),
                                  rt.local_defined_function_vec_storage.index_unchecked(fidx),
                                  pack_f64(x),
                                  nullptr,
                                  nullptr);
            return load_i32(rr.results);
        };

        // f32 unops
        UWVM2TEST_REQUIRE(run_f32(0, 1.25f) == 2.0f);    // ceil
        UWVM2TEST_REQUIRE(run_f32(1, -1.25f) == -2.0f);  // floor
        UWVM2TEST_REQUIRE(run_f32(2, -1.25f) == -1.0f);  // trunc
        UWVM2TEST_REQUIRE(run_f32(3, 2.5f) == 2.0f);     // nearest (ties-to-even)

        // trunc conversions (safe in-range inputs)
        UWVM2TEST_REQUIRE(run_i32_from_f32(4, -3.0f) == -3);
        UWVM2TEST_REQUIRE(run_i32_from_f32(5, 3.0f) == 3);
        UWVM2TEST_REQUIRE(run_i32_from_f64(6, -5.0) == -5);
        UWVM2TEST_REQUIRE(run_i32_from_f64(7, 7.0) == 7);

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_f32_unop_trunc_localget() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_f32_unop_trunc_localget_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_f32_unop_trunc_localget");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE((run_suite<opt>(rt)) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_f32_unop_trunc_localget();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
