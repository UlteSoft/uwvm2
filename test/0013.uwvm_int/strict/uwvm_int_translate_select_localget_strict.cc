#include "uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec pack_i32x3(::std::int32_t a, ::std::int32_t b, ::std::int32_t c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_f32x2_i32(float a, float b, ::std::int32_t c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec build_select_localget_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i32 select(local.get a,b,cond) ; local.set dst ; return dst
        // (param i32 i32 i32) (result i32) (local i32)
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_i32});
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::select);
            op(c, wasm_op::local_set); u32(c, 3u);
            op(c, wasm_op::local_get); u32(c, 3u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: f32 select(local.get a,b,cond) ; local.set dst ; return dst
        // (param f32 f32 i32) (result f32) (local f32)
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_i32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::select);
            op(c, wasm_op::local_set); u32(c, 3u);
            op(c, wasm_op::local_get); u32(c, 3u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32 select(local.get a,b,cond) ; local.tee dst ; return (top)
        // (param f32 f32 i32) (result f32) (local f32)
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_i32}, {k_val_f32}};
            func_body fb{};
            fb.locals.push_back({1u, k_val_f32});
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::select);
            op(c, wasm_op::local_tee); u32(c, 3u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int test_translate_select_localget() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_select_localget_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_select_localget");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke (select(local.get a,b,cond) fusion requires conbine pending across opcodes).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            // f0: i32
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x3(111, 222, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 222);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x3(111, 222, 1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 111);

            // f1: f32
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_f32x2_i32(1.5f, 2.25f, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 2.25f);
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_f32x2_i32(1.5f, 2.25f, -7),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 1.5f);

            // f2: f32 local.tee
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_f32x2_i32(9.0f, -1.0f, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == -1.0f);
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_f32x2_i32(9.0f, -1.0f, 1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.0f);
        }

        // Tailcall mode is required to keep conbine pending across opcodes.
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr auto exp_i32_sel_lset = optable::translate::get_uwvmint_i32_select_local_set_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f32_sel_lset = optable::translate::get_uwvmint_f32_select_local_set_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f32_sel_ltee = optable::translate::get_uwvmint_f32_select_local_tee_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_sel_lset));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_f32_sel_lset));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_sel_ltee));
#endif

#if defined(UWVM_ENABLE_UWVM_INT_COMBINE_OPS) && !defined(UWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS)
            // Default "heavy" mode keeps only a single delayed `local.get` and will not form the 2-local pending window needed
            // for `select(local.get a,b,cond)` fusion. In that case, we still expect the normal typed `select` opfuncs.
            constexpr optable::uwvm_interpreter_stacktop_currpos_t curr{};
            constexpr auto tuple =
                compiler::details::make_interpreter_tuple<opt>(::std::make_index_sequence<compiler::details::interpreter_tuple_size<opt>()>{});

            constexpr auto exp_sel_i32 = optable::translate::get_uwvmint_select_i32_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_sel_f32 = optable::translate::get_uwvmint_select_f32_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_sel_i32));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_sel_f32));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_sel_f32));
#endif

            using Runner = interpreter_runner<opt>;

            // f0: i32
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x3(111, 222, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 222);
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x3(111, 222, 1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 111);

            // f1: f32
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_f32x2_i32(1.5f, 2.25f, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 2.25f);
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_f32x2_i32(1.5f, 2.25f, -7),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 1.5f);

            // f2: f32 local.tee fused path candidate
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_f32x2_i32(9.0f, -1.0f, 0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == -1.0f);
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_f32x2_i32(9.0f, -1.0f, 1),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.0f);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_select_localget();
}
