#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i64x2_i32(::std::int64_t a, ::std::int64_t b, ::std::int32_t c)
    {
        byte_vec out(20);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x2_i32(double a, double b, ::std::int32_t c)
    {
        byte_vec out(20);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_select_i64_f64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i64 select(local.get a,b,cond)
        // (param i64 i64 i32) (result i64)
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i32}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::select);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: f64 select(local.get a,b,cond)
        // (param f64 f64 i32) (result f64)
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_i32}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::local_get);
            u32(c, 2u);
            op(c, wasm_op::select);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_select_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0 i64
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i64x2_i32(111, 222, 0),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr0.results) == 222);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(0),
                                   rt.local_defined_function_vec_storage.index_unchecked(0),
                                   pack_i64x2_i32(111, 222, 1),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr1.results) == 111);
        }

        // f1 f64
        {
            auto rr0 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_f64x2_i32(1.5, 2.25, 0),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr0.results) == 2.25);

            auto rr1 = Runner::run(cm.local_funcs.index_unchecked(1),
                                   rt.local_defined_function_vec_storage.index_unchecked(1),
                                   pack_f64x2_i32(1.5, 2.25, -7),
                                   nullptr,
                                   nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr1.results) == 1.5);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_select_i64_f64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_select_i64_f64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_select_i64_f64");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Tailcall mode: assert typed `select` opfuncs exist for i64/f64 (no special fusion expected).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr auto exp_sel_i64 = optable::translate::get_uwvmint_select_i64_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_sel_f64 = optable::translate::get_uwvmint_select_f64_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_sel_i64));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_sel_f64));
        }

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_select_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_select_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching (exercise select stacktop specialization paths).
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
            UWVM2TEST_REQUIRE(run_select_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_select_i64_f64();
}

