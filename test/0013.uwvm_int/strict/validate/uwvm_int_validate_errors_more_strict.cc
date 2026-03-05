#include "../uwvm_int_translate_strict_common.h"

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_UNDEFINED__) || defined(__SANITIZE_LEAK__)
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

    [[nodiscard]] byte_vec build_missing_end_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        // Note: the parser enforces that the last byte of a function body is `end`.
        // We simulate a "missing end" for the compiler validator by truncating `code_end` at runtime.
        op(fb.code, wasm_op::nop);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_trailing_code_after_end_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        // Two `end`s: the first ends the function body, the second is trailing code.
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_missing_block_type_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        // Note: we simulate a missing blocktype by truncating `code_end` right after `block`.
        op(fb.code, wasm_op::block);
        append_u8(fb.code, k_block_empty);
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_block_type_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::block);
        append_u8(fb.code, 0x00u);  // illegal blocktype
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_opbase_module()
    {
        module_builder mb{};
        auto op_u8 = [&](byte_vec& c, ::std::uint8_t v) { append_u8(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op_u8(fb.code, 0xffu);  // illegal opcode
        op_u8(fb.code, 0x0bu);  // end
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_select_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 1);
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 2);
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 0);
        op(fb.code, wasm_op::select);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_select_cond_type_not_i32_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 1);
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 2);
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 0);
        op(fb.code, wasm_op::select);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_else_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::else_);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_if_then_result_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 1);
        op(fb.code, wasm_op::if_);
        append_u8(fb.code, k_val_i32);  // (result i32)
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 0);
        op(fb.code, wasm_op::else_);
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 0);
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::drop);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_if_missing_else_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 1);
        op(fb.code, wasm_op::if_);
        append_u8(fb.code, k_val_i32);  // (result i32)
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 0);
        op(fb.code, wasm_op::end);  // end if => should trigger if_missing_else
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_end_result_mismatch_count_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::end);  // missing i32 result
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_end_result_mismatch_type_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 0);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_label_index_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::br);
        append_u8(fb.code, 0x80u);  // truncated leb
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_function_index_encoding_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::call);
        append_u8(fb.code, 0x80u);  // truncated leb (will be truncated at runtime)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_function_index_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::call);
        u32(fb.code, 12345u);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_local_index_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::local_get);
        append_u8(fb.code, 0x80u);  // truncated leb (will be truncated at runtime)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_global_get_index_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::global_get);
        append_u8(fb.code, 0x80u);  // truncated leb (will be truncated at runtime)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_global_set_index_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::global_set);
        append_u8(fb.code, 0x80u);  // truncated leb (will be truncated at runtime)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_memory_size_index_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::memory_size);
        append_u8(fb.code, 0x80u);  // truncated leb (will be truncated at runtime)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_memory_grow_index_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::memory_grow);
        append_u8(fb.code, 0x80u);  // truncated leb (will be truncated at runtime)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_memory_size_index_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::memory_size);
        u32(fb.code, 1u);  // illegal memidx (must be 0 for MVP)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_memory_grow_index_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::memory_grow);
        u32(fb.code, 1u);  // illegal memidx (must be 0 for MVP)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_local_set_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{k_val_i32}, {}};
        func_body fb{};
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 0);
        op(fb.code, wasm_op::local_set);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_local_tee_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 0);
        op(fb.code, wasm_op::local_tee);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::drop);
        op(fb.code, wasm_op::i32_const);
        append_i32_leb(fb.code, 0);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_memarg_align_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::local_get);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::i32_load);
        append_u8(fb.code, 0x80u);  // truncated leb for align
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_memarg_offset_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{k_val_i32}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::local_get);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::i32_load);
        u32(fb.code, 2u);          // align=4
        append_u8(fb.code, 0x80u); // truncated leb for offset
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_store_value_type_mismatch_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::i32_const);
        append_i32_leb(fb.code, 0);  // addr
        op(fb.code, wasm_op::f32_const);
        append_f32_ieee(fb.code, 0.0f);  // wrong value type for i32.store
        op(fb.code, wasm_op::i32_store);
        u32(fb.code, 2u);  // align=4
        u32(fb.code, 0u);  // offset
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_memory_grow_delta_type_not_i32_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 1);
        op(fb.code, wasm_op::memory_grow);
        append_u8(fb.code, 0x00u);  // reserved (memidx)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_const_immediate_i32_module()
    {
        module_builder mb{};
        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::i32_const);
        append_u8(fb.code, 0x80u);  // truncated leb (will be truncated at runtime)
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_numeric_operand_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {k_val_i32}};
        func_body fb{};
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 1);
        op(fb.code, wasm_op::f32_const);
        append_f32_ieee(fb.code, 2.0f);
        op(fb.code, wasm_op::i32_add);  // operands not i32
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_type_index_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::call_indirect);
        append_u8(fb.code, 0x80u);  // truncated leb for typeidx
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_invalid_table_index_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::call_indirect);
        u32(fb.code, 0u);
        append_u8(fb.code, 0x80u);  // truncated leb for tableidx
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_table_index_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::call_indirect);
        u32(fb.code, 0u);
        u32(fb.code, 0u);  // tableidx=0, but module has no table => illegal_table_index
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));
        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_type_index_module()
    {
        module_builder mb{};
        mb.has_table = true;
        mb.table_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // Only one local function => type section size is 1 (typeidx=0 only).
        // f0: call_indirect typeidx=1 (oob) tableidx=0
        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::call_indirect);
        u32(fb.code, 1u);  // illegal type index
        u32(fb.code, 0u);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_br_value_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::block);
        append_u8(fb.code, k_val_i32);  // (result i32)
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 0);
        op(fb.code, wasm_op::br);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_br_cond_type_not_i32_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        op(fb.code, wasm_op::block);
        append_u8(fb.code, k_block_empty);
        op(fb.code, wasm_op::i64_const);
        i64(fb.code, 0);
        op(fb.code, wasm_op::br_if);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] byte_vec build_br_table_target_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        func_type ty{{}, {}};
        func_body fb{};
        // outer: (result i32)
        op(fb.code, wasm_op::block);
        append_u8(fb.code, k_val_i32);
        // inner: empty
        op(fb.code, wasm_op::block);
        append_u8(fb.code, k_block_empty);
        op(fb.code, wasm_op::i32_const);
        i32(fb.code, 0);
        op(fb.code, wasm_op::br_table);
        u32(fb.code, 2u);
        u32(fb.code, 0u);
        u32(fb.code, 1u);
        u32(fb.code, 0u);
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::end);
        op(fb.code, wasm_op::end);
        (void)mb.add_func(::std::move(ty), ::std::move(fb));

        return mb.build();
    }

    [[nodiscard]] int test_validate_errors_more()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        UWVM2TEST_REQUIRE(
            compile_expect_truncated_code_end(build_missing_end_module(), u8"uwvm2test_validate_missing_end", errc::missing_end, 1uz) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_trailing_code_after_end_module(), u8"uwvm2test_validate_trailing", errc::trailing_code_after_end) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_missing_block_type_module(),
                                                           u8"uwvm2test_validate_missing_block_type",
                                                           errc::missing_block_type,
                                                           1uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_block_type_module(), u8"uwvm2test_validate_illegal_block_type", errc::illegal_block_type) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_opbase_module(), u8"uwvm2test_validate_illegal_opbase", errc::illegal_opbase) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_select_type_mismatch_module(), u8"uwvm2test_validate_select_ty", errc::select_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_select_cond_type_not_i32_module(), u8"uwvm2test_validate_select_cond", errc::select_cond_type_not_i32) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_else_module(), u8"uwvm2test_validate_illegal_else", errc::illegal_else) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_if_then_result_mismatch_module(), u8"uwvm2test_validate_if_then_mismatch", errc::if_then_result_mismatch) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_if_missing_else_module(), u8"uwvm2test_validate_if_missing_else", errc::if_missing_else) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_end_result_mismatch_count_module(), u8"uwvm2test_validate_end_res_mismatch_count", errc::end_result_mismatch) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_end_result_mismatch_type_module(), u8"uwvm2test_validate_end_res_mismatch_type", errc::end_result_mismatch) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_label_index_module(),
                                                           u8"uwvm2test_validate_invalid_label",
                                                           errc::invalid_label_index,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect_truncated_code_end(build_invalid_function_index_encoding_module(),
                                             u8"uwvm2test_validate_invalid_funcidx_enc",
                                             errc::invalid_function_index_encoding,
                                             2uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_invalid_function_index_module(), u8"uwvm2test_validate_invalid_funcidx", errc::invalid_function_index) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_local_index_module(),
                                                           u8"uwvm2test_validate_invalid_localidx",
                                                           errc::invalid_local_index,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_global_get_index_module(),
                                                           u8"uwvm2test_validate_invalid_globalget_idx",
                                                           errc::invalid_global_index,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_global_set_index_module(),
                                                           u8"uwvm2test_validate_invalid_globalset_idx",
                                                           errc::invalid_global_index,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_memory_size_index_module(),
                                                           u8"uwvm2test_validate_invalid_memory_size_idx",
                                                           errc::invalid_memory_index,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_memory_grow_index_module(),
                                                           u8"uwvm2test_validate_invalid_memory_grow_idx",
                                                           errc::invalid_memory_index,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_illegal_memory_size_index_module(), u8"uwvm2test_validate_illegal_memory_size_idx", errc::illegal_memory_index) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_illegal_memory_grow_index_module(), u8"uwvm2test_validate_illegal_memory_grow_idx", errc::illegal_memory_index) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_local_set_type_mismatch_module(), u8"uwvm2test_validate_local_set_mismatch", errc::local_set_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_local_tee_type_mismatch_module(), u8"uwvm2test_validate_local_tee_mismatch", errc::local_tee_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_memarg_align_module(),
                                                           u8"uwvm2test_validate_invalid_memarg_align",
                                                           errc::invalid_memarg_align,
                                                           4uz) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect_truncated_code_end(build_invalid_memarg_offset_module(),
                                             u8"uwvm2test_validate_invalid_memarg_offset",
                                             errc::invalid_memarg_offset,
                                             5uz) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_store_value_type_mismatch_module(), u8"uwvm2test_validate_store_mismatch", errc::store_value_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_memory_grow_delta_type_not_i32_module(),
                                         u8"uwvm2test_validate_memory_grow_delta_ty",
                                         errc::memory_grow_delta_type_not_i32) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_const_immediate_i32_module(),
                                                           u8"uwvm2test_validate_invalid_i32_const",
                                                           errc::invalid_const_immediate,
                                                           2uz) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_numeric_operand_type_mismatch_module(), u8"uwvm2test_validate_numeric_ty_mismatch", errc::numeric_operand_type_mismatch) ==
            0);
        UWVM2TEST_REQUIRE(
            compile_expect_truncated_code_end(build_invalid_type_index_module(), u8"uwvm2test_validate_invalid_typeidx", errc::invalid_type_index, 2uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect_truncated_code_end(build_invalid_table_index_module(),
                                                           u8"uwvm2test_validate_invalid_tableidx",
                                                           errc::invalid_table_index,
                                                           3uz) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_table_index_module(), u8"uwvm2test_validate_illegal_tableidx", errc::illegal_table_index) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_type_index_module(), u8"uwvm2test_validate_illegal_typeidx", errc::illegal_type_index) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_br_value_type_mismatch_module(), u8"uwvm2test_validate_br_val_mismatch", errc::br_value_type_mismatch) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_br_cond_type_not_i32_module(), u8"uwvm2test_validate_br_cond_ty", errc::br_cond_type_not_i32) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_br_table_target_type_mismatch_module(), u8"uwvm2test_validate_br_table_mismatch", errc::br_table_target_type_mismatch) ==
            0);

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_validate_errors_more();
    }
    catch(::fast_io::error const&)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught fast_io::error");
    }
    catch(...)
    {
        return ::uwvm2test::uwvm_int_strict::fail(__LINE__, "uncaught unknown exception");
    }
}

#endif
