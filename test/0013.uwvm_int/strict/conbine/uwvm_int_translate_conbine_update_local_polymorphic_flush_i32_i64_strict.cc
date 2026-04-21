#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    using errc = ::uwvm2::validation::error::code_validation_error_code;

    [[nodiscard]] byte_vec build_conbine_update_local_polymorphic_flush_i32_i64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto i64 = [&](byte_vec& c, ::std::int64_t v) { append_i64_leb(c, v); };

        // After `unreachable`, the compiler may still have pending local-update combine
        // state, but the concrete `local.set` operand must still be type-checked.
        {
            func_type ty{{k_val_i32, k_val_i64}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_i32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i32_const); i32(c, 5);
            op(c, wasm_op::i32_add);
            op(c, wasm_op::local_set); u32(c, 1u);  // local 1 is i64
            op(c, wasm_op::i32_const); i32(c, 1);

            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        {
            func_type ty{{k_val_i64, k_val_i32}, {k_val_i32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_i32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::i64_const); i64(c, 7);
            op(c, wasm_op::i64_add);
            op(c, wasm_op::local_set); u32(c, 1u);  // local 1 is i32
            op(c, wasm_op::i32_const); i32(c, 1);

            op(c, wasm_op::else_);
            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        }

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_expect_local_set_mismatch(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        bool started_compile{};
        try
        {
            started_compile = true;
            (void)compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        }
        catch(::fast_io::error const&)
        {
            // Expected for validation failures.
        }
        catch(...)
        {
            return fail(__LINE__, "unexpected exception type");
        }

        if(!started_compile) { return fail(__LINE__, "fast_io::error thrown before compiler validation"); }
        if(err.err_code != errc::local_set_type_mismatch)
        {
            char buf[128]{};
            ::std::snprintf(buf,
                            sizeof(buf),
                            "err_code mismatch: expected=%u actual=%u",
                            static_cast<unsigned>(errc::local_set_type_mismatch),
                            static_cast<unsigned>(err.err_code));
            return fail(__LINE__, buf);
        }

        return 0;
    }

    [[nodiscard]] int test_translate_conbine_update_local_polymorphic_flush_i32_i64() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto wasm = build_conbine_update_local_polymorphic_flush_i32_i64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_conbine_update_local_polymorphic_flush_i32_i64");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        if(abi_mode_enabled("tail-min"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = true};
            UWVM2TEST_REQUIRE(compile_expect_local_set_mismatch<opt>(rt) == 0);
        }

        if(abi_mode_enabled("byref"))
        {
            constexpr optable::uwvm_interpreter_translate_option_t opt{.is_tail_call = false};
            UWVM2TEST_REQUIRE(compile_expect_local_set_mismatch<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE(compile_expect_local_set_mismatch<opt>(rt) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE(compile_expect_local_set_mismatch<opt>(rt) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            static_assert(compiler::details::interpreter_tuple_has_no_holes<opt>());
            UWVM2TEST_REQUIRE(compile_expect_local_set_mismatch<opt>(rt) == 0);
        }

        return 0;
    }
}  // namespace

int main()
{
    return test_translate_conbine_update_local_polymorphic_flush_i32_i64();
}
