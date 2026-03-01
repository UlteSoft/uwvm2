#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_return_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: (param i32) (result i32) stack repair before return (extra stack value).
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const);
            i32(c, 123);  // extra value below return value
            op(c, wasm_op::local_get);
            u32(c, 0u);  // return value
            op(c, wasm_op::return_);
            op(c, wasm_op::i32_const);
            i32(c, 999);  // unreachable
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: (param i64) (result i64) stack repair before return (3 extras).
        {
            func_type ty{{k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i64_const);
            i64(c, 1);
            op(c, wasm_op::i64_const);
            i64(c, 2);
            op(c, wasm_op::i64_const);
            i64(c, 3);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::return_);
            op(c, wasm_op::i64_const);
            i64(c, 0);  // unreachable
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: (param f32) (result f32) stack repair before return (extra f32 const).
        {
            func_type ty{{k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f32_const);
            append_f32_ieee(c, -0.0f);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::return_);
            op(c, wasm_op::f32_const);
            append_f32_ieee(c, 1.0f);  // unreachable
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: (param f64) (result f64) stack repair before return (extra f64 const).
        {
            func_type ty{{k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 0.5);
            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::return_);
            op(c, wasm_op::f64_const);
            append_f64_ieee(c, 2.0);  // unreachable
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: (param i32 i32) (result i32) ensure return flushes stacktop (no extras below return value).
        // returns a+b via explicit return.
        {
            func_type ty{{k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get);
            u32(c, 0u);
            op(c, wasm_op::local_get);
            u32(c, 1u);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::return_);
            op(c, wasm_op::i32_const);
            i32(c, 0);  // unreachable
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int run_return_suite(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto cm = compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using Runner = interpreter_runner<Opt>;

        // f0
        for(::std::int32_t v : {0, 1, -1, 12345, -12345})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(0),
                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                  pack_i32(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == v);
        }

        // f1
        for(::std::int64_t v : {0ll, 1ll, -1ll, 0x1122334455667788ll, -0x112233445566778ll})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(1),
                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                  pack_i64(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i64(rr.results) == v);
        }

        // f2
        for(float v : {0.0f, -0.0f, 1.25f, -3.5f, 1234.0f})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(2),
                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                  pack_f32(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f32(rr.results) == v);
        }

        // f3
        for(double v : {0.0, -0.0, 1.25, -3.5, 1e20})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(3),
                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                  pack_f64(v),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_f64(rr.results) == v);
        }

        // f4
        for(::std::pair<::std::int32_t, ::std::int32_t> ab : {::std::pair{0, 0},
                                                             ::std::pair{1, 2},
                                                             ::std::pair{-7, 9},
                                                             ::std::pair{123, -456}})
        {
            auto rr = Runner::run(cm.local_funcs.index_unchecked(4),
                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                  [&]()
                                  {
                                      byte_vec out(8);
                                      ::std::memcpy(out.data(), ::std::addressof(ab.first), 4);
                                      ::std::memcpy(out.data() + 4, ::std::addressof(ab.second), 4);
                                      return out;
                                  }(),
                                  nullptr,
                                  nullptr);
            UWVM2TEST_REQUIRE(load_i32(rr.results) == ab.first + ab.second);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_return() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_return_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_return");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Mode A: byref
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(run_return_suite<opt>(rt) == 0);
        }

        // Mode B: tailcall (no caching)
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(run_return_suite<opt>(rt) == 0);
        }

        // Mode C: tailcall + stacktop caching (exercise stacktop_flush_all_to_operand_stack before return).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{
                .is_tail_call = true,
                .i32_stack_top_begin_pos = 3uz,
                .i32_stack_top_end_pos = 7uz,
                .i64_stack_top_begin_pos = 3uz,
                .i64_stack_top_end_pos = 7uz,
                .f32_stack_top_begin_pos = 3uz,
                .f32_stack_top_end_pos = 7uz,
                .f64_stack_top_begin_pos = 3uz,
                .f64_stack_top_end_pos = 7uz,
                .v128_stack_top_begin_pos = SIZE_MAX,
                .v128_stack_top_end_pos = SIZE_MAX,
            };
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(run_return_suite<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_return();
}
