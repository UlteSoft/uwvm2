#include "../uwvm_int_translate_strict_common.h"

#if defined(__APPLE__) && (defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__) || defined(__SANITIZE_LEAK__))
// Notes:
// - This test relies on catching `fast_io::error` thrown by the validator to early-exit invalid modules.
// - On macOS with sanitizers enabled, C++ exception handling may be unreliable in this project configuration.
//   Rebuild without sanitizer policies to run this test.
int main()
{
    return 0;
}
#else

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using errc = ::uwvm2::validation::error::code_validation_error_code;

    [[nodiscard]] int compile_expect(byte_vec const& wasm, ::uwvm2::utils::container::u8string_view name, errc expected)
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        bool started_compile{};
        try
        {
            auto prep = prepare_runtime_from_wasm(wasm, name);
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            runtime_module_t const& rt = *prep.mod;

            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            optable::compile_option cop{};

            started_compile = true;
            (void)compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        }
        catch(::fast_io::error const&)
        {
            // Expected for validation failures (the compiler throws to exit early).
        }
        catch(...)
        {
            return fail(__LINE__, "unexpected exception type");
        }
        if(!started_compile)
        {
            return fail(__LINE__, "fast_io::error thrown before compiler validation");
        }
        if(err.err_code != expected)
        {
            char buf[128]{};
            ::std::snprintf(buf,
                            sizeof(buf),
                            "err_code mismatch: expected=%u actual=%u",
                            static_cast<unsigned>(expected),
                            static_cast<unsigned>(err.err_code));
            return fail(__LINE__, buf);
        }
        return 0;
    }

    struct code_end_restore_guard
    {
        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t* code{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const* old_end{};

        ~code_end_restore_guard() noexcept
        {
            if(code != nullptr) { code->body.code_end = old_end; }
        }
    };

    [[nodiscard]] int compile_expect_truncated_code_end(byte_vec const& wasm,
                                                        ::uwvm2::utils::container::u8string_view name,
                                                        errc expected,
                                                        ::std::size_t new_expr_len)
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        bool started_compile{};
        try
        {
            auto prep = prepare_runtime_from_wasm(wasm, name);
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            auto& rt = const_cast<runtime_module_t&>(*prep.mod);

            UWVM2TEST_REQUIRE(!rt.local_defined_function_vec_storage.empty());
            auto& fn = rt.local_defined_function_vec_storage.index_unchecked(0);
            UWVM2TEST_REQUIRE(fn.wasm_code_ptr != nullptr);

            auto* code = const_cast<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_wasm_code_t*>(fn.wasm_code_ptr);
            UWVM2TEST_REQUIRE(code != nullptr);
            UWVM2TEST_REQUIRE(code->body.expr_begin != nullptr);
            UWVM2TEST_REQUIRE(code->body.code_end != nullptr);

            auto const* expr_begin = code->body.expr_begin;
            auto const* old_end = code->body.code_end;
            UWVM2TEST_REQUIRE(expr_begin <= old_end);

            auto const old_len = static_cast<::std::size_t>(old_end - expr_begin);
            UWVM2TEST_REQUIRE(new_expr_len <= old_len);

            code->body.code_end = expr_begin + new_expr_len;
            code_end_restore_guard guard{code, old_end};

            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            optable::compile_option cop{};

            started_compile = true;
            (void)compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        }
        catch(::fast_io::error const&)
        {
            // Expected for validation failures (the compiler throws to exit early).
        }
        catch(...)
        {
            return fail(__LINE__, "unexpected exception type");
        }
        if(!started_compile)
        {
            return fail(__LINE__, "fast_io::error thrown before compiler validation");
        }
        if(err.err_code != expected)
        {
            char buf[128]{};
            ::std::snprintf(buf,
                            sizeof(buf),
                            "err_code mismatch: expected=%u actual=%u",
                            static_cast<unsigned>(expected),
                            static_cast<unsigned>(err.err_code));
            return fail(__LINE__, buf);
        }
        return 0;
    }

    [[nodiscard]] byte_vec build_br_table_default_target_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // default label signature mismatch:
        // - targets: depth 1 (inner i32), depth 2 (outer i32)
        // - default: depth 0 (innermost void)
        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;

        op(c, wasm_op::block);
        append_u8(c, k_val_i32);  // outer
        op(c, wasm_op::block);
        append_u8(c, k_val_i32);  // inner
        op(c, wasm_op::block);
        append_u8(c, k_block_empty);  // innermost (void)

        op(c, wasm_op::i32_const);
        i32(c, 123);  // branch value (expected arity=1)
        op(c, wasm_op::i32_const);
        i32(c, 0);  // selector

        op(c, wasm_op::br_table);
        u32(c, 2u);  // vec length
        u32(c, 1u);  // target inner (i32)
        u32(c, 2u);  // target outer (i32)
        u32(c, 0u);  // default innermost (void) => mismatch

        op(c, wasm_op::end);  // end innermost
        op(c, wasm_op::end);  // end inner
        op(c, wasm_op::end);  // end outer
        op(c, wasm_op::end);  // end func

        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_br_table_underflow_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // Missing selector for `br_table` => operand_stack_underflow.
        func_type ty{{}, {}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::br_table);
        u32(c, 0u);  // vec length
        u32(c, 0u);  // default label (function)
        op(c, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_br_table_cond_type_not_i32_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // br_table expects selector i32, but we provide i64.
        func_type ty{{}, {}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::i64_const);
        i64(c, 0);
        op(c, wasm_op::br_table);
        u32(c, 0u);  // vec length
        u32(c, 0u);  // default label (function)
        op(c, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_br_table_value_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // function label expects i32 (arity=1), but we provide i64 as the branch value.
        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        auto& c = fb.code;
        op(c, wasm_op::i64_const);
        i64(c, 0);
        op(c, wasm_op::i32_const);
        i32(c, 0);  // selector
        op(c, wasm_op::br_table);
        u32(c, 0u);  // vec length
        u32(c, 0u);  // default label (function)
        op(c, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_call_underflow_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (i32) -> () : end
        {
            func_type ty{{k_val_i32}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // f1: () -> () : call 0 (missing arg) ; end
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::call);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_call_param_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // f0: (i64) -> () : end
        {
            func_type ty{{k_val_i64}, {}};
            func_body fb{};
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }
        // f1: () -> () : i32.const 0 ; call 0 ; end
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 0);
            op(fb.code, wasm_op::call);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_local_set_underflow_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // (local i32) local.set 0 (no operand) => operand_stack_underflow.
        func_type ty{{}, {}};
        func_body fb{};
        fb.locals.push_back({1u, k_val_i32});
        op(fb.code, wasm_op::local_set);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_local_set_invalid_index_encoding_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        // local.set <truncated leb> => invalid_local_index (requires truncated code_end in the test harness).
        func_type ty{{}, {}};
        func_body fb{};
        fb.locals.push_back({1u, k_val_i32});
        op(fb.code, wasm_op::local_set);
        append_u8(fb.code, 0x80u);  // truncated leb
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_local_set_illegal_index_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // (local i32) local.set 1  (only local 0 exists) => illegal_local_index.
        func_type ty{{}, {}};
        func_body fb{};
        fb.locals.push_back({1u, k_val_i32});
        op(fb.code, wasm_op::local_set);
        u32(fb.code, 1u);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] int test_validate_errors_br_table_call_local_set()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        UWVM2TEST_REQUIRE(compile_expect(build_br_table_default_target_type_mismatch_module(),
                                        u8"uwvm2test_validate_br_table_default_mismatch",
                                        errc::br_table_target_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_br_table_underflow_module(), u8"uwvm2test_validate_br_table_underflow", errc::operand_stack_underflow) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_br_table_cond_type_not_i32_module(), u8"uwvm2test_validate_br_table_cond_ty", errc::br_cond_type_not_i32) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_br_table_value_type_mismatch_module(), u8"uwvm2test_validate_br_table_val_ty", errc::br_value_type_mismatch) == 0);

        UWVM2TEST_REQUIRE(compile_expect(build_call_underflow_module(), u8"uwvm2test_validate_call_underflow", errc::operand_stack_underflow) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_call_param_type_mismatch_module(), u8"uwvm2test_validate_call_param_ty", errc::br_value_type_mismatch) == 0);

        UWVM2TEST_REQUIRE(compile_expect(build_local_set_underflow_module(), u8"uwvm2test_validate_local_set_underflow", errc::operand_stack_underflow) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_local_set_invalid_index_encoding_module(),
                                                           u8"uwvm2test_validate_local_set_invalid_idx_enc",
                                                           errc::invalid_local_index,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_local_set_illegal_index_module(), u8"uwvm2test_validate_local_set_illegal_idx", errc::illegal_local_index) == 0);

        return 0;
    }
}  // namespace

int main()
{
    return test_validate_errors_br_table_call_local_set();
}

#endif

