#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_br_if_i32_eqz_stacktop_nofill_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param i32) -> (result i32)
        // block (result i32)
        //   i32.const taken
        //   local.get 0; i32.eqz; br_if 0
        //   drop
        //   i32.const fall
        // end
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            op(c, wasm_op::i32_const);
            i32(c, 111);

            op(c, wasm_op::local_get);
            u32(c, 0u);
            // Prevent `br_if_local_eqz` (local.get + i32.eqz fusion) so we cover the generic
            // `i32.eqz ; br_if` mega-op fusion kind.
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::i32_or);
            op(c, wasm_op::i32_eqz);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            op(c, wasm_op::drop);
            op(c, wasm_op::i32_const);
            i32(c, 222);

            op(c, wasm_op::end);
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
        UWVM2TEST_REQUIRE(cm.local_funcs.size() == 1uz);

        using Runner = interpreter_runner<Opt>;

        // i32.eqz: 0 => taken
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(0),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        // non-zero => fallthrough
        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_i32(1),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS)
        if constexpr(Opt.is_tail_call)
        {
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<Opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<Opt>()>{});

            constexpr auto ring_prev = [](::std::size_t pos, ::std::size_t begin_pos, ::std::size_t end_pos) constexpr noexcept -> ::std::size_t
            { return pos == begin_pos ? (end_pos - 1uz) : (pos - 1uz); };

            // `br_if_i32_eqz` reads the operand from the stack-top cache. If stacktop caching is enabled, the
            // expected specialization depends on the i32 currpos at the fusion site.
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr =
                (Opt.i32_stack_top_begin_pos == Opt.i32_stack_top_end_pos)
                    ? optable::uwvm_interpreter_stacktop_currpos_t{}
                    : optable::uwvm_interpreter_stacktop_currpos_t{
                          // At `i32.eqz` entry, the stack holds:
                          // - i32.const taken (pushed first), then
                          // - (local.get | 0) (pushed second).
                          // So the top i32 currpos is the result of two pushes starting from `begin_pos`.
                          .i32_stack_top_curr_pos =
                              ring_prev(ring_prev(Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_begin_pos, Opt.i32_stack_top_end_pos),
                                        Opt.i32_stack_top_begin_pos,
                                        Opt.i32_stack_top_end_pos),
                          .i64_stack_top_curr_pos = Opt.i64_stack_top_begin_pos,
                          .f32_stack_top_curr_pos = Opt.f32_stack_top_begin_pos,
                          .f64_stack_top_curr_pos = Opt.f64_stack_top_begin_pos,
                          .v128_stack_top_curr_pos = Opt.v128_stack_top_begin_pos,
                      };

            auto const exp = optable::translate::get_uwvmint_br_if_i32_eqz_fptr_from_tuple<Opt>(curr, tuple);
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp));
        }
#endif

        return 0;
    }

    [[nodiscard]] int test_translate_br_if_i32_eqz_stacktop_nofill() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_br_if_i32_eqz_stacktop_nofill_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_br_if_i32_eqz_stacktop_nofill");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall mode (no stacktop caching): exercise br_if_i32_eqz fusion selection.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: ensure the translator models `i32.eqz` pop+push without requiring a fill
        // when `br_if` immediately follows.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 5uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 7uz,
                .f32_stack_top_end_pos = 9uz,
                .f64_stack_top_begin_pos = 9uz,
                .f64_stack_top_end_pos = 11uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
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
        return test_translate_br_if_i32_eqz_stacktop_nofill();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}
