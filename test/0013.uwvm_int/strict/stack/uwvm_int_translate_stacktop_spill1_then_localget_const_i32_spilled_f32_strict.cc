#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

    [[nodiscard]] byte_vec build_stacktop_spill1_then_localget_const_i32_spilled_f32_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };

        // f0: (param i32 x) -> (result i32)
        // Fill a merged i32/f32 ring (size 2) with {f32, i32}, then `local.get x` forces a spill of the f32 and should
        // be fused into `stacktop_spill1_then_local_get_typed<spilled=f32, pushed=i32>`.
        // Semantics: return x.
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i32_const);
            i32(c, 2);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::nop);

            // Pop x (store back to itself), drop the carried values, then return x.
            op(c, wasm_op::local_set);
            u32(c, 0u);
            op(c, wasm_op::drop);
            op(c, wasm_op::drop);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (result i32) (local i32 tmp)
        // Fill a merged i32/f32 ring (size 2) with {f32, i32}, then `i32.const 3` forces a spill of f32 and should
        // be fused into `stacktop_spill1_then_const_typed<spilled=f32, pushed=i32>`.
        // Semantics: return 5.
        {
            func_type ty{{}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local#0: i32 tmp
            auto& c = fb.code;

            op(c, wasm_op::f32_const);
            f32(c, 1.0f);
            op(c, wasm_op::i32_const);
            i32(c, 2);

            op(c, wasm_op::i32_const);
            i32(c, 3);
            op(c, wasm_op::nop);

            op(c, wasm_op::i32_add);      // 2 + 3 = 5
            op(c, wasm_op::local_set);    // tmp = 5
            u32(c, 0u);
            op(c, wasm_op::drop);         // drop carried f32
            op(c, wasm_op::local_get);    // return tmp
            u32(c, 0u);
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
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 2uz);

        using Runner = interpreter_runner<Opt>;

        // f0: return x
        for(::std::int32_t x : {0, 1, 2, 123, -1, 0x7fffffff, static_cast<::std::int32_t>(0x80000000u)})
        {
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32(x),
                                                  nullptr,
                                                  nullptr)
                                           .results) == x);
        }

        // f1: return 5
        {
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_no_params(),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 5);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_stacktop_spill1_then_localget_const_i32_spilled_f32() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_spill1_then_localget_const_i32_spilled_f32_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_spill1_then_localget_const_i32_spilled_f32");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // byref semantics smoke
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // tailcall + stacktop caching: i32/f32 merged ring (size 2), exercise spilled_vt=f32 branches in spill-fusion patching.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 5uz,
                .f64_stack_top_begin_pos = 7uz,
                .f64_stack_top_end_pos = 9uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());

            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            // Fused fptr selection depends on the currpos of the merged i32/f32 ring.
            bool has_fused_localget{};
            bool has_fused_const{};
            for(::std::size_t pos = opt.i32_stack_top_begin_pos; pos < opt.i32_stack_top_end_pos; ++pos)
            {
                optable::uwvm_interpreter_stacktop_currpos_t curr{
                    .i32_stack_top_curr_pos = pos,
                    .i64_stack_top_curr_pos = opt.i64_stack_top_begin_pos,
                    .f32_stack_top_curr_pos = pos,
                    .f64_stack_top_curr_pos = opt.f64_stack_top_begin_pos,
                    .v128_stack_top_curr_pos = SIZE_MAX,
                };

                auto const f_localget =
                    optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_f32, wasm_i32>(curr, tuple);
                auto const f_const =
                    optable::translate::get_uwvmint_stacktop_spill1_then_const_typed_fptr_from_tuple<opt, wasm_f32, wasm_i32>(curr, tuple);

                if(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, f_localget)) { has_fused_localget = true; }
                if(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, f_const)) { has_fused_const = true; }
            }
            UWVM2TEST_REQUIRE(has_fused_localget);
            UWVM2TEST_REQUIRE(has_fused_const);
#endif

            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_spill1_then_localget_const_i32_spilled_f32();
}

