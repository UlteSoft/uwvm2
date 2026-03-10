#include "../uwvm_int_translate_strict_common.h"

#include <string>
#include <string_view>

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] std::string read_text_file(char const* path)
    {
        ::std::FILE* fp{::std::fopen(path, "rb")};
        if(fp == nullptr) { return {}; }

        if(::std::fseek(fp, 0, SEEK_END) != 0)
        {
            ::std::fclose(fp);
            return {};
        }

        long const sz{::std::ftell(fp)};
        if(sz < 0)
        {
            ::std::fclose(fp);
            return {};
        }

        if(::std::fseek(fp, 0, SEEK_SET) != 0)
        {
            ::std::fclose(fp);
            return {};
        }

        std::string text(static_cast<::std::size_t>(sz), '\0');
        if(sz != 0)
        {
            auto const nread{::std::fread(text.data(), 1uz, static_cast<::std::size_t>(sz), fp)};
            text.resize(nread);
        }

        ::std::fclose(fp);
        return text;
    }

    [[nodiscard]] bool log_contains_kind(std::string_view log_text, std::string_view kind) noexcept
    {
        std::string needle;
        needle.reserve(kind.size() + 5uz);
        needle.append("kind=");
        needle.append(kind.data(), kind.size());
        return log_text.find(needle) != std::string_view::npos;
    }

    [[nodiscard]] byte_vec build_runtime_log_conbine_flush_update_local_polymorphic_f32_f64_module()
    {
        module_builder mb{};

        auto op = [&](byte_vec& c, wasm_op o) { append_u8(c, u8(o)); };
        auto u32 = [&](byte_vec& c, ::std::uint32_t v) { append_u32_leb(c, v); };
        auto i32 = [&](byte_vec& c, ::std::int32_t v) { append_i32_leb(c, v); };
        auto f32 = [&](byte_vec& c, float v) { append_f32_ieee(c, v); };
        auto f64 = [&](byte_vec& c, double v) { append_f64_ieee(c, v); };

        auto add_f32_flush_case = [&](wasm_op binop, float imm)
        {
            func_type ty{{k_val_f32, k_val_i32}, {k_val_f32}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_f32);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f32_const); f32(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::f32_const); f32(c, 1.0f);

            op(c, wasm_op::else_);
            op(c, wasm_op::f32_const); f32(c, 0.0f);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        auto add_f64_flush_case = [&](wasm_op binop, double imm)
        {
            func_type ty{{k_val_f64, k_val_i64}, {k_val_f64}};
            func_body fb{};
            auto& c = fb.code;

            op(c, wasm_op::i32_const); i32(c, 0);
            op(c, wasm_op::if_); append_u8(c, k_val_f64);

            op(c, wasm_op::unreachable);
            op(c, wasm_op::local_get); u32(c, 0u);
            op(c, wasm_op::f64_const); f64(c, imm);
            op(c, binop);
            op(c, wasm_op::local_set); u32(c, 1u);
            op(c, wasm_op::f64_const); f64(c, 1.0);

            op(c, wasm_op::else_);
            op(c, wasm_op::f64_const); f64(c, 0.0);
            op(c, wasm_op::end);
            op(c, wasm_op::end);

            (void)mb.add_func(::std::move(ty), ::std::move(fb));
        };

        add_f32_flush_case(wasm_op::f32_add, 1.5f);
        add_f32_flush_case(wasm_op::f32_mul, 2.0f);
        add_f32_flush_case(wasm_op::f32_sub, 0.25f);

        add_f64_flush_case(wasm_op::f64_add, 1.125);
        add_f64_flush_case(wasm_op::f64_mul, 3.0);
        add_f64_flush_case(wasm_op::f64_sub, 0.5);

        return mb.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    [[nodiscard]] int compile_only_with_runtime_log(runtime_module_t const& rt) noexcept
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        (void)compiler::compile_all_from_uwvm_single_func<Opt>(rt, cop, err);
        UWVM2TEST_REQUIRE(err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);
        return 0;
    }

    [[nodiscard]] int test_translate_runtime_log_conbine_flush_update_local_polymorphic_f32_f64()
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };

        auto const wasm = build_runtime_log_conbine_flush_update_local_polymorphic_f32_f64_module();
        auto prep = prepare_runtime_from_wasm(wasm, u8"uwvm2test_runtime_log_conbine_flush_update_local_polymorphic_f32_f64");
        UWVM2TEST_REQUIRE(prep.mod != nullptr);
        runtime_module_t const& rt = *prep.mod;

        constexpr char kLogPath[]{"/tmp/uwvm_int_translate_runtime_log_conbine_flush_update_local_polymorphic_f32_f64_strict.log"};
        (void)::std::remove(kLogPath);

#if defined(_WIN32) || defined(_WIN64)
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(
            u8"uwvm_int_translate_runtime_log_conbine_flush_update_local_polymorphic_f32_f64_strict.log",
            ::fast_io::open_mode::out);
#else
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(
            u8"/tmp/uwvm_int_translate_runtime_log_conbine_flush_update_local_polymorphic_f32_f64_strict.log",
            ::fast_io::open_mode::out);
#endif
        ::uwvm2::uwvm::io::enable_runtime_log = true;

        if(abi_mode_enabled("byref"))
        {
            constexpr auto opt{k_test_byref_opt};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-min"))
        {
            constexpr auto opt{k_test_tail_min_opt};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-sysv"))
        {
            constexpr auto opt{k_test_tail_sysv_opt};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(abi_mode_enabled("tail-aapcs64"))
        {
            constexpr auto opt{k_test_tail_aapcs64_opt};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        if(legacy_layouts_enabled())
        {
            constexpr auto opt{make_tailcall_scalar4_merged_opt<2uz>()};
            UWVM2TEST_REQUIRE((compile_only_with_runtime_log<opt>(rt)) == 0);
        }

        ::uwvm2::uwvm::io::enable_runtime_log = false;
#if defined(_WIN32) || defined(_WIN64)
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"NUL", ::fast_io::open_mode::out);
#else
        ::uwvm2::uwvm::io::u8runtime_log_output.reopen(u8"/dev/null", ::fast_io::open_mode::out);
#endif

        auto const log_text{read_text_file(kLogPath)};
        UWVM2TEST_REQUIRE(!log_text.empty());

        UWVM2TEST_REQUIRE(log_contains_kind(log_text, "local_get_const_f32"));
        UWVM2TEST_REQUIRE(log_contains_kind(log_text, "local_get_const_f64"));

        for(char const* kind : {
                "f32_add_imm_local_settee_same",
                "f32_mul_imm_local_settee_same",
                "f32_sub_imm_local_settee_same",
                "f64_add_imm_local_settee_same",
                "f64_mul_imm_local_settee_same",
                "f64_sub_imm_local_settee_same",
            })
        {
            UWVM2TEST_REQUIRE(!log_contains_kind(log_text, kind));
        }

        (void)::std::remove(kLogPath);
        return 0;
    }
}

int main()
{
    return test_translate_runtime_log_conbine_flush_update_local_polymorphic_f32_f64();
}
