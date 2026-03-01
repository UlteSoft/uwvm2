#include "uwvm_int_translate_strict_common.h"

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

    [[nodiscard]] byte_vec build_stack_underflow_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };

        // f0: () -> () : drop (underflow)
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::drop);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_if_cond_type_not_i32_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // f0: () -> () : i64.const 0 ; if ... end  (condition not i32)
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i64_const);
            i64(fb.code, 0);
            op(fb.code, wasm_op::if_);
            append_u8(fb.code, k_block_empty);
            op(fb.code, wasm_op::end);  // end if
            op(fb.code, wasm_op::end);  // end func
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_label_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: () -> () : br 1  (out of range: only the implicit function label exists)
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::br);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_local_index_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (param i32) -> (result i32) : local.get 1  (only local 0 exists)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 1u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_no_memory_load_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (param i32) -> (result i32) : local.get 0 ; i32.load (no memory section)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_load);
            u32(fb.code, 2u);  // align=4 (natural for i32.load)
            u32(fb.code, 0u);  // offset=0
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_memarg_alignment_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (param i32) -> (result i32) : local.get 0 ; i32.load with align=8 (illegal for i32.load)
        {
            func_type ty{{k_val_i32}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_load);
            u32(fb.code, 3u);  // align=8, max_align=2 for i32.load
            u32(fb.code, 0u);  // offset=0
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_memarg_address_type_not_i32_module()
    {
        module_builder mb{};
        mb.has_memory = true;
        mb.memory_min = 1u;

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: (param i64) -> (result i32) : local.get 0 ; i32.load (address type not i32)
        {
            func_type ty{{k_val_i64}, {k_val_i32}};
            func_body fb{};
            op(fb.code, wasm_op::local_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::i32_load);
            u32(fb.code, 2u);  // align=4
            u32(fb.code, 0u);  // offset=0
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_illegal_global_index_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };

        // f0: () -> () : global.get 0 (no globals)
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::global_get);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_immutable_global_set_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };

        // global0: (i32 const 0), immutable
        {
            global_entry g{};
            g.valtype = k_val_i32;
            g.mut = false;
            op(g.init_expr, wasm_op::i32_const);
            i32(g.init_expr, 0);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }

        // f0: i32.const 1 ; global.set 0 (immutable)
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::global_set);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] byte_vec build_global_set_type_mismatch_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // global0: (i64 const 0), mutable
        {
            global_entry g{};
            g.valtype = k_val_i64;
            g.mut = true;
            op(g.init_expr, wasm_op::i64_const);
            i64(g.init_expr, 0);
            op(g.init_expr, wasm_op::end);
            mb.globals.push_back(::std::move(g));
        }

        // f0: i32.const 1 ; global.set 0 (type mismatch: i64 expected)
        {
            func_type ty{{}, {}};
            func_body fb{};
            op(fb.code, wasm_op::i32_const);
            i32(fb.code, 1);
            op(fb.code, wasm_op::global_set);
            u32(fb.code, 0u);
            op(fb.code, wasm_op::end);
            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    [[nodiscard]] int compile_expect(byte_vec const& wasm, ::uwvm2::utils::container::u8string_view name, errc expected)
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            auto prep = prepare_runtime_from_wasm(wasm, name);
            UWVM2TEST_REQUIRE(prep.mod != nullptr);
            runtime_module_t const& rt = *prep.mod;

            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            optable::compile_option cop{};

            (void)compiler::compile_all_from_uwvm_single_func<opt>(rt, cop, err);
        }
        catch(::fast_io::error const&)
        {
            // Expected for validation failures (the compiler throws to exit early).
        }
        catch(...)
        {
            // Any other exception is treated as a test failure.
            return fail(__LINE__, "unexpected exception type");
        }
        UWVM2TEST_REQUIRE(err.err_code == expected);
        return 0;
    }

    [[nodiscard]] int test_validate_errors()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        UWVM2TEST_REQUIRE(compile_expect(build_stack_underflow_module(), u8"uwvm2test_validate_underflow", errc::operand_stack_underflow) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_if_cond_type_not_i32_module(), u8"uwvm2test_validate_ifcond", errc::if_cond_type_not_i32) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_label_module(), u8"uwvm2test_validate_illegal_label", errc::illegal_label_index) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_local_index_module(), u8"uwvm2test_validate_illegal_local", errc::illegal_local_index) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_no_memory_load_module(), u8"uwvm2test_validate_no_memory", errc::no_memory) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_illegal_memarg_alignment_module(), u8"uwvm2test_validate_illegal_memarg_align", errc::illegal_memarg_alignment) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_memarg_address_type_not_i32_module(),
                                         u8"uwvm2test_validate_memarg_addr_ty",
                                         errc::memarg_address_type_not_i32) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_illegal_global_index_module(), u8"uwvm2test_validate_illegal_global", errc::illegal_global_index) == 0);
        UWVM2TEST_REQUIRE(compile_expect(build_immutable_global_set_module(), u8"uwvm2test_validate_immutable_global_set", errc::immutable_global_set) == 0);
        UWVM2TEST_REQUIRE(
            compile_expect(build_global_set_type_mismatch_module(), u8"uwvm2test_validate_global_set_mismatch", errc::global_set_type_mismatch) == 0);

        return 0;
    }
}  // namespace

int main()
{
    try
    {
        return test_validate_errors();
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
