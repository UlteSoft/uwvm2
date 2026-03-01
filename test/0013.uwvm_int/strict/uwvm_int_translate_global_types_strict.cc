#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_global_types_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // 3 mutable globals: i64/f32/f64.
        {
            global_entry g{};
            g.valtype = k_val_i64;
            g.mut = true;
            op(g.init_expr, wasm_op::i64_const);
            i64(g.init_expr, 100);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }
        {
            global_entry g{};
            g.valtype = k_val_f32;
            g.mut = true;
            op(g.init_expr, wasm_op::f32_const);
            append_f32_ieee(g.init_expr, 1.25f);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }
        {
            global_entry g{};
            g.valtype = k_val_f64;
            g.mut = true;
            op(g.init_expr, wasm_op::f64_const);
            append_f64_ieee(g.init_expr, 2.5);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }

        // f0: (param i64) (result i64) -> g0 = g0 + x ; return g0
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::global_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::global_set);
            u32(c, 0u);
            op(c, wasm_op::global_get);
            u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param f32) (result f32) -> g1 = g1 + x ; return g1
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::global_get);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::global_set);
            u32(c, 1u);
            op(c, wasm_op::global_get);
            u32(c, 1u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (param f64) (result f64) -> g2 = g2 + x ; return g2
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::global_get);
            u32(c, 2u);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::global_set);
            u32(c, 2u);
            op(c, wasm_op::global_get);
            u32(c, 2u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_global_suite(byte_vec const& wasm, ::uwvm2::utils::container::u8string_view name) noexcept
    {
        auto prep = prepare_runtime_from_wasm(wasm, name);
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // i64: 100 + 23 = 123 ; then +(-23) => 100
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i64(23),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr0.results) == 123);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i64(-23),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr1.results) == 100);
        }

        // f32: 1.25 + 2.0 = 3.25 ; then +(-1.25) => 2.0
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_f32(2.0f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr0.results) == 3.25f);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_f32(-1.25f),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr1.results) == 2.0f);
        }

        // f64: 2.5 + 1.25 = 3.75 ; then +(-0.25) => 3.5
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_f64(1.25),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr0.results) == 3.75);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(2),
                                   rt.local_defined_function_vec_storage.index_unchecked(2),
                                   pack_f64(-0.25),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr1.results) == 3.5);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_global_types() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_global_types_module();

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_global_suite<opt>(wasm, u8"uwvm2test_global_types_byref") == 0);
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_global_suite<opt>(wasm, u8"uwvm2test_global_types_tail") == 0);
        }

        // Mode C: tailcall + stacktop caching (separate int/float rings).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 5uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 5uz,
                .f32_stack_top_begin_pos = 5uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 5uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_global_suite<opt>(wasm, u8"uwvm2test_global_types_stacktop") == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_global_types();
}

