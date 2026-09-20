#include "../uwvm_int_translate_strict_common.h"

namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;

    [[nodiscard]] byte_vec build_if_no_else_identity_module()
    {
        constexpr ::std::uint8_t bytes[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x0cu, 0x02u, 0x60u, 0x01u, 0x7eu,
                                         0x01u, 0x7eu, 0x60u, 0x02u, 0x7eu, 0x7fu, 0x01u, 0x7eu, 0x03u, 0x02u, 0x01u, 0x01u, 0x0au, 0x0eu,
                                         0x01u, 0x0cu, 0x00u, 0x20u, 0x00u, 0x20u, 0x01u, 0x04u, 0x00u, 0x1au, 0x42u, 0x7fu, 0x0bu, 0x0bu};
        byte_vec result(sizeof(bytes));
        ::std::memcpy(result.data(), bytes, sizeof(bytes));
        return result;
    }

    [[nodiscard]] byte_vec pack_i64_i32(::std::int64_t value, ::std::int32_t condition)
    {
        byte_vec result(12uz);
        ::std::memcpy(result.data(), ::std::addressof(value), sizeof(value));
        ::std::memcpy(result.data() + sizeof(value), ::std::addressof(condition), sizeof(condition));
        return result;
    }

    template <optable::uwvm_interpreter_translate_option_t Option>
    [[nodiscard]] int run_suite(runtime_module_t const& runtime_module) noexcept
    {
        if constexpr(Option.is_tail_call) { static_assert(compiler::details::interpreter_tuple_has_no_holes<Option>()); }

        ::uwvm2::validation::error::code_validation_error_impl error{};
        optable::compile_option compile_option{};
        auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(runtime_module, compile_option, error)};
        UWVM2TEST_REQUIRE(error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok);

        using runner = interpreter_runner<Option>;
        auto const& function{runtime_module.local_defined_function_vec_storage.index_unchecked(0uz)};
        constexpr ::std::int64_t input{0x102030405060708ll};

        auto const false_result{runner::run(compiled.local_funcs.index_unchecked(0uz), function, pack_i64_i32(input, 0), nullptr, nullptr)};
        UWVM2TEST_REQUIRE(load_i64(false_result.results) == input);

        auto const true_result{runner::run(compiled.local_funcs.index_unchecked(0uz), function, pack_i64_i32(input, 1), nullptr, nullptr)};
        UWVM2TEST_REQUIRE(load_i64(true_result.results) == -1);
        return 0;
    }

    [[nodiscard]] int run_test() noexcept
    {
        install_unexpected_traps();
        optable::call_func = strict_terminate_call;
        optable::call_indirect_func = strict_terminate_call_indirect;

        auto const wasm{build_if_no_else_identity_module()};
        auto prepared{prepare_runtime_from_wasm(wasm, u8"uwvm2test_if_no_else_identity", {}, make_wasm1p1_feature_parameter())};
        UWVM2TEST_REQUIRE(prepared.mod != nullptr);

        UWVM2TEST_REQUIRE(run_suite<k_test_byref_opt>(*prepared.mod) == 0);
        UWVM2TEST_REQUIRE(run_suite<k_test_tail_min_opt>(*prepared.mod) == 0);
        return 0;
    }
}  // namespace

int main() { return run_test(); }
