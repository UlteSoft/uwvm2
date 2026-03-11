#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_conbine_for_i32_inc_f64_lt_u_flush_brif_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (param f64 n) (result i32)
        // Exercises: `for_i32_inc_f64_lt_u_eqz_*` conbine chain, but forces an intermediate flush at `after_cmp`
        // by placing `br_if` directly after `f64.lt` (skipping the usual `i32.eqz`).
        //
        // Returns:
        // - n < 1.0  => 111 (taken)
        // - else     => 222
        {
            func_type ty{{k_val_f64}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});  // local1: i
            auto& c = fb.code;

            // i = 0
            op(c, wasm_op::i32_const);
            i32(c, 0);
            op(c, wasm_op::local_set);
            u32(c, 1u);

            op(c, wasm_op::block);
            append_u8(c, k_val_i32);

            // taken value for label0
            op(c, wasm_op::i32_const);
            i32(c, 111);

            // local.get n; local.get i; i32.const 1; i32.add; local.tee i; f64.convert_i32_u; f64.lt; br_if 0
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_const);
            i32(c, 1);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee);
            u32(c, 1u);
            op(c, wasm_op::f64_convert_i32_u);
            op(c, wasm_op::f64_lt);
            op(c, wasm_op::br_if);
            u32(c, 0u);

            // fallthrough: drop taken value and return 222
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
    [[nodiscard]] int run_conbine_for_i32_inc_f64_lt_u_flush_brif_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_f64(0.5),
                                              nullptr,
                                              nullptr)
                                       .results) == 111);

        UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                              rt.local_defined_function_vec_storage.index_unchecked(0),
                                              pack_f64(2.0),
                                              nullptr,
                                              nullptr)
                                       .results) == 222);

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_for_i32_inc_f64_lt_u_flush_brif() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_for_i32_inc_f64_lt_u_flush_brif_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_for_i32_inc_f64_lt_u_flush_brif");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_conbine_for_i32_inc_f64_lt_u_flush_brif_suite<opt>(rt) == 0);
        }

        // Tailcall: baseline.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_conbine_for_i32_inc_f64_lt_u_flush_brif_suite<opt>(rt) == 0);
        }

        // Tailcall + stacktop caching: fully merged scalar ring (i32/i64/f32/f64) to cover merged-range fast paths.
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
            UWVM2TEST_REQUIRE(run_conbine_for_i32_inc_f64_lt_u_flush_brif_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_translate_conbine_for_i32_inc_f64_lt_u_flush_brif();
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught exception");
    }
}

