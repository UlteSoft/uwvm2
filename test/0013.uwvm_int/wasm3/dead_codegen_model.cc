// Consume real wasm-tools-assembled Core 3 dead-execution control fixtures.
// Pure validation and four independent register-ring translation layouts share the same initialized source bytes.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#include <fast_io.h>

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    using error_t = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    unsigned checks{};
    void require(bool condition, unsigned line)
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::print(::fast_io::err(), "FAIL dead codegen model line ", line, "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    auto features()
    {
        auto result{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result)};
        policy.disable_function_references = false;
        policy.disable_gc = false;
        return result;
    }
    auto const& codes()
    {
        auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        return []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
            ::uwvm2::utils::container::tuple<Fs...>) -> auto const&
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);
        }(parsed.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }
    template<typename Operation>
    bool rejected(Operation operation)
    {
        try { operation(); }
        catch(::fast_io::error const& failure)
        {
            // Native/OOM/other-domain failures do not qualify an invalid Wasm module.
            if(failure.domain != ::fast_io::parse_domain_value || failure.code !=
               static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            return true;
        }
        return false;
    }
    template<optable::uwvm_interpreter_translate_option_t Option>
    void translate(byte_vec const& source, bool expected_valid)
    {
        auto policy{features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"dead-codegen-model", {}, policy)};
        auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        error_t pure_error{};
        bool const pure_rejected{rejected([&]
        {
            auto const& body_list{codes().codes};
            for(::std::size_t index{}; index != body_list.size(); ++index)
            {
                // [owned code vector ... index ... end)
                // [safe                            ] index is bounded before this borrowed body read.
                auto const& body{body_list.index_unchecked(index).body};
                // [parser-retained expression ... code_end]
                // [safe                                ] endpoints belong to this live initialized source.
                // The pure API takes the complete function index, including imports.
                // This initialized runtime retains the same parser function-index prefix.
                auto const imported_count{prepared.mod->imported_function_vec_storage.size()};
                if(index > ::std::numeric_limits<::std::size_t>::max() - imported_count)
                { ::fast_io::fast_terminate(); }
                v3::validate_code(v3::wasm3_code_version{}, parsed, imported_count + index,
                    reinterpret_cast<::std::byte const*>(body.expr_begin),
                    reinterpret_cast<::std::byte const*>(body.code_end), pure_error, policy);
            }
        })};
        REQUIRE(pure_rejected != expected_valid);
        REQUIRE((pure_error.err_code == error_code::ok) == expected_valid);
        error_t fused_error{};
        bool const fused_rejected{rejected([&]
        {
            optable::compile_option options;
            auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(
                *prepared.mod, options, fused_error, &policy)};
            (void)compiled;
        })};
        REQUIRE(fused_rejected != expected_valid);
        REQUIRE((fused_error.err_code == error_code::ok) == expected_valid);
        REQUIRE(pure_error.err_code == fused_error.err_code);
    }
}

int main(int argc, char const* const* argv)
{
    REQUIRE(argc >= 3 && argc % 2 == 1);
    for(int index{1}; index != argc; index += 2)
    {
        ::std::string_view const expected{argv[index]};
        REQUIRE(expected == "valid" || expected == "invalid");
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[index + 1]), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz);
        byte_vec source;
        source.resize(input.size());
        // [RAII file bytes ... end) / [source allocation ... end)
        // [safe                                               ] equal checked lengths bound this copy.
        ::std::memcpy(source.data(), input.data(), input.size());
        bool const valid{expected == "valid"};
        translate<k_test_byref_opt>(source, valid);
        translate<make_tailcall_scalar4_merged_opt<1uz>()>(source, valid);
        translate<make_tailcall_scalar4_merged_opt<2uz>()>(source, valid);
        translate<make_tailcall_scalar4_merged_opt<3uz>()>(source, valid);
    }
    ::fast_io::print(::fast_io::out(), "PASS dead codegen model checks=", checks, "\n");
}
