#include "../uwvm_int_translate_strict_common.h"

#include <cstdlib>

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

    [[nodiscard]] byte_vec pack_f32x3(float a, float b, float c)
    {
        byte_vec out(12);
        ::std::memcpy(out.data(), ::std::addressof(a), 4);
        ::std::memcpy(out.data() + 4, ::std::addressof(b), 4);
        ::std::memcpy(out.data() + 8, ::std::addressof(c), 4);
        return out;
    }

    [[nodiscard]] byte_vec pack_i64x3(::std::int64_t a, ::std::int64_t b, ::std::int64_t c)
    {
        byte_vec out(24);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        return out;
    }

    [[nodiscard]] byte_vec pack_f64x3(double a, double b, double c)
    {
        byte_vec out(24);
        ::std::memcpy(out.data(), ::std::addressof(a), 8);
        ::std::memcpy(out.data() + 8, ::std::addressof(b), 8);
        ::std::memcpy(out.data() + 16, ::std::addressof(c), 8);
        return out;
    }

    [[nodiscard]] byte_vec build_mac_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: i32 mac local.set acc: acc = acc + x*y ; return acc
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f1: i32 mac local.tee acc: acc = acc + x*y ; return (top)
        {
            func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i32_mul);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f2: f32 mac local.set acc
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f3: f32 mac local.tee acc
        {
            func_type ty{{k_val_f32, k_val_f32, k_val_f32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f32_mul);
            op(c, wasm_op::f32_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f4: i64 mac local.set acc: acc = acc + x*y ; return acc
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_mul);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f5: i64 mac local.tee acc: acc = acc + x*y ; return (top)
        {
            func_type ty{{k_val_i64, k_val_i64, k_val_i64}, {k_val_i64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::i64_mul);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f6: f64 mac local.set acc: acc = acc + x*y ; return acc
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_set); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        // f7: f64 mac local.tee acc: acc = acc + x*y ; return (top)
        {
            func_type ty{{k_val_f64, k_val_f64, k_val_f64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::local_get); u32(c, 1u);
            op(c, wasm_op::local_get); u32(c, 2u);
            op(c, wasm_op::f64_mul);
            op(c, wasm_op::f64_add);
            op(c, wasm_op::local_tee); u32(c, 0u);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[maybe_unused]] [[nodiscard]] byte_vec build_i32_mac_local_tee_only_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // (param i32 i32 i32) (result i32)
        // acc = acc + x*y ; return (top)
        func_type ty{{k_val_i32, k_val_i32, k_val_i32}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;

        op(c, wasm_op::local_get); u32(c, 0u);
        op(c, wasm_op::local_get); u32(c, 1u);
        op(c, wasm_op::local_get); u32(c, 2u);
        op(c, wasm_op::i32_mul);
        op(c, wasm_op::i32_add);
        op(c, wasm_op::local_tee); u32(c, 0u);
        op(c, wasm_op::end);

        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] int test_translate_mac_fusion() noexcept
    {
        if(::std::getenv("UWVM2TEST_RTLOG") != nullptr)
        {
            ::uwvm2::uwvm::io::enable_runtime_log = true;
            ::uwvm2::uwvm::io::u8runtime_log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
        }

        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_mac_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_mac_fusion");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        // Byref mode: semantics smoke (mac fusion itself requires conbine pending across opcodes).
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            optable::compile_option cop{};
            auto cm = compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
            UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

            using Runner = interpreter_runner<opt>;

            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);

            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_i32x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);

            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_f32x3(1.5f, 2.0f, 4.0f),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5f);

            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(3),
                                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                                  pack_f32x3(1.5f, 2.0f, 4.0f),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5f);

            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                                  pack_i64x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);

            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(5),
                                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                                  pack_i64x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);

            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(6),
                                                  rt.local_defined_function_vec_storage.index_unchecked(6),
                                                  pack_f64x3(1.5, 2.0, 4.0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5);

            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(7),
                                                  rt.local_defined_function_vec_storage.index_unchecked(7),
                                                  pack_f64x3(1.5, 2.0, 4.0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5);
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

            constexpr auto exp_i32_set = optable::translate::get_uwvmint_i32_mac_local_set_acc_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i32_tee = optable::translate::get_uwvmint_i32_mac_local_tee_acc_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f32_set = optable::translate::get_uwvmint_f32_mac_local_set_acc_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f32_tee = optable::translate::get_uwvmint_f32_mac_local_tee_acc_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i64_set = optable::translate::get_uwvmint_i64_mac_local_set_acc_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_i64_tee = optable::translate::get_uwvmint_i64_mac_local_tee_acc_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_set = optable::translate::get_uwvmint_f64_mac_local_set_acc_fptr_from_tuple<opt>(curr, tuple);
            constexpr auto exp_f64_tee = optable::translate::get_uwvmint_f64_mac_local_tee_acc_fptr_from_tuple<opt>(curr, tuple);

            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_set));
            if(!bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_tee)) [[unlikely]]
            {
                constexpr auto exp_local_get_i32 = optable::translate::get_uwvmint_local_get_i32_fptr_from_tuple<opt>(curr, tuple);
                constexpr auto exp_local_tee_i32 = optable::translate::get_uwvmint_local_tee_i32_fptr_from_tuple<opt>(curr, tuple);
                constexpr auto exp_i32_mul = optable::translate::get_uwvmint_i32_mul_fptr_from_tuple<opt>(curr, tuple);
                constexpr auto exp_i32_add = optable::translate::get_uwvmint_i32_add_fptr_from_tuple<opt>(curr, tuple);

                // Compile the i32 local.tee mac function in isolation to detect cross-function state bugs.
                bool isolated_has_i32_tee{};
                {
                    auto wasm2 = build_i32_mac_local_tee_only_module();
                    auto prep2 = prepare_runtime_from_wasm(wasm2, u8"uwvm2test_mac_fusion_i32_tee_only");
                    if(prep2.mod != nullptr)
                    {
                        runtime_module_t const& rt2 = *prep2.mod;
                        ::uwvm2::validation::error::code_validation_error_impl err2{};
                        optable::compile_option cop2{};
                        auto cm2 = compiler::compile_all_from_uwvm_single_func<opt>(rt2, cop2, err2);
                        if(err2.err_code == ::uwvm2::validation::error::code_validation_error_code::ok && !cm2.local_funcs.empty())
                        {
                            isolated_has_i32_tee = bytecode_contains_fptr(cm2.local_funcs.index_unchecked(0).op.operands, exp_i32_tee);
                        }
                    }
                }
                ::std::fprintf(stderr,
                               "uwvm2test DIAG: mac fusion missing: fn=1 bc=%zu has_i32_set=%d has_local_get_i32=%d has_i32_mul=%d has_i32_add=%d "
                               "has_local_tee_i32=%d isolated_has_i32_tee=%d | fn0 has_i32_set=%d has_i32_mul=%d has_i32_add=%d\n",
                               cm.local_funcs.index_unchecked(1).op.operands.size(),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_set)),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_local_get_i32)),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_mul)),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_add)),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_local_tee_i32)),
                               static_cast<int>(isolated_has_i32_tee),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_set)),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_mul)),
                               static_cast<int>(bytecode_contains_fptr(cm.local_funcs.index_unchecked(0).op.operands, exp_i32_add)));
            }
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(1).op.operands, exp_i32_tee));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(2).op.operands, exp_f32_set));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(3).op.operands, exp_f32_tee));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(4).op.operands, exp_i64_set));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(5).op.operands, exp_i64_tee));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(6).op.operands, exp_f64_set));
            UWVM2TEST_REQUIRE(bytecode_contains_fptr(cm.local_funcs.index_unchecked(7).op.operands, exp_f64_tee));
#endif

            using Runner = interpreter_runner<opt>;

            // i32
            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(0),
                                                  rt.local_defined_function_vec_storage.index_unchecked(0),
                                                  pack_i32x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);

            UWVM2TEST_REQUIRE(load_i32(Runner::run(cm.local_funcs.index_unchecked(1),
                                                  rt.local_defined_function_vec_storage.index_unchecked(1),
                                                  pack_i32x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);

            // f32: use exactly representable values
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(2),
                                                  rt.local_defined_function_vec_storage.index_unchecked(2),
                                                  pack_f32x3(1.5f, 2.0f, 4.0f),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5f);
            UWVM2TEST_REQUIRE(load_f32(Runner::run(cm.local_funcs.index_unchecked(3),
                                                  rt.local_defined_function_vec_storage.index_unchecked(3),
                                                  pack_f32x3(1.5f, 2.0f, 4.0f),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5f);

            // i64
            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(4),
                                                  rt.local_defined_function_vec_storage.index_unchecked(4),
                                                  pack_i64x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);
            UWVM2TEST_REQUIRE(load_i64(Runner::run(cm.local_funcs.index_unchecked(5),
                                                  rt.local_defined_function_vec_storage.index_unchecked(5),
                                                  pack_i64x3(10, 2, 3),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 16);

            // f64: use exactly representable values
            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(6),
                                                  rt.local_defined_function_vec_storage.index_unchecked(6),
                                                  pack_f64x3(1.5, 2.0, 4.0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5);
            UWVM2TEST_REQUIRE(load_f64(Runner::run(cm.local_funcs.index_unchecked(7),
                                                  rt.local_defined_function_vec_storage.index_unchecked(7),
                                                  pack_f64x3(1.5, 2.0, 4.0),
                                                  nullptr,
                                                  nullptr)
                                           .results) == 9.5);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_mac_fusion();
}
