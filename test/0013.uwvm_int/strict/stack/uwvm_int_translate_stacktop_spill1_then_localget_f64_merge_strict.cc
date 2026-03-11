#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
    using wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;

    [[nodiscard]] byte_vec build_stacktop_spill1_then_localget_f64_merge_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };

        // f0: push 2 f32 (fills merged f32/f64 ring size 2), then local.get(f64) forces spill1 of f32.
        // Save f64 to local, drop the f32s, return the f64.
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f64});  // local#1: f64
            auto& c = fb.code;

            op(c, wasm_op::f32_const); f32(c, 1.0f);
            op(c, wasm_op::f32_const); f32(c, 2.0f);

            op(c, wasm_op::local_get); u32(c, 0u);  // f64 param
            op(c, wasm_op::local_set); u32(c, 1u);  // save f64 to local#1

            op(c, wasm_op::drop);
            op(c, wasm_op::drop);

            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_stacktop_spill1_then_localget_f64_merge() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_stacktop_spill1_then_localget_f64_merge_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_stacktop_spill1_then_localget_f64_merge");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall + merged f32/f64 ring (size 2): should use spill1+local.get fused opfunc when local.get(f64) forces a spill of f32.
        constexpr optable::uwvm_interpreter_translate_option_t opt{
            .is_tail_call = true,
            .i32_stack_top_begin_pos = 3uz,
            .i32_stack_top_end_pos = 5uz,
            .i64_stack_top_begin_pos = 5uz,
            .i64_stack_top_end_pos = 7uz,
            .f32_stack_top_begin_pos = 7uz,
            .f32_stack_top_end_pos = 9uz,
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

        using Runner = interpreter_runner<opt>;

        // Semantics: return param unchanged.
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_f64(42.5),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == 42.5);
        }

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        // Verify we emitted the fused spill1+local.get opfunc (spilled=f32, local=f64). The opfunc is selected by f64 currpos.
        constexpr auto tuple = compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});
        bool ok{};
        for(::std::size_t pos = opt.f64_stack_top_begin_pos; pos < opt.f64_stack_top_end_pos; ++pos)
        {
            optable::uwvm_interpreter_stacktop_currpos_t curr{};
            curr.f64_stack_top_curr_pos = pos;
            auto const fptr = optable::translate::get_uwvmint_stacktop_spill1_then_local_get_typed_fptr_from_tuple<opt, wasm_f32, wasm_f64>(curr, tuple);
            if(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, fptr)) { ok = true; break; }
        }
        UWVM2TEST_REQUIRE(ok);
#endif

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_stacktop_spill1_then_localget_f64_merge();
}
