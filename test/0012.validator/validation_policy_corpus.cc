#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <uwvm2/validation/standard/wasm2/impl.h>

template <typename Mode>
constexpr Mode mvp_mode() noexcept
{
    // ROS preserves its older spelling while removing deferred execution modes.
    if constexpr(requires { Mode::direct_wasmmvp; }) { return Mode::direct_wasmmvp; }
    else { return Mode::direct_mvp; }
}

// Read binary paths from stdin. This probe performs no instantiation or guest
// execution: imports and trapping start functions must not obscure validation.
// Compare explicit API/version policies, not just CLI defaults. In particular,
// an invalid cold function is still checked here even though lazy execution may
// legally defer its validation until invocation (Core 2 section 7.2.2).
int main(int argc, char** argv)
{
    if(argc == 1) { return 0; } // Batch probe; driven explicitly by the companion corpus script.
    if(argc != 3) { return 2; } // API: wasm1/wasm1p1/wasm2/runtime; policy: mvp/wasm1p1/wasm2
    std::string const api{argv[1]}, profile{argv[2]};
    if(api != "wasm1" && api != "wasm1p1" && api != "wasm2" && api != "runtime") { return 2; }
    if(profile != "mvp" && profile != "wasm1p1" && profile != "wasm2") { return 2; }
    if(api == "wasm1" && profile != "mvp") { return 2; }
    if(api == "wasm2" && profile != "wasm2") { return 2; }
    namespace p = ::uwvm2::parser::wasm;
    namespace v = ::uwvm2::validation::standard;
    using w1 = p::standard::wasm1::features::wasm1;
    using w11 = p::standard::wasm1p1::features::wasm1p1;
    using w2 = p::standard::wasm2::features::wasm2;
    using storage = p::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<w1, w11, w2>;
    p::concepts::feature_parameter_t<w1, w11, w2> policy{};
    auto& parameter{p::standard::wasm1p1::features::get_wasm1p1_parameter(policy)};
    using mode = p::standard::wasm1p1::features::wasm_feature_cli_mode;
    parameter.cli_mode = profile == "mvp" ? mvp_mode<mode>() :
        profile == "wasm2" ? mode::direct_wasm2 : mode::direct_wasm1p1;
    bool const mvp{profile == "mvp"};
    parameter.disable_sign_extension = mvp;
    parameter.disable_nontrapping_float_to_int = mvp;
    parameter.disable_multi_value = mvp;
    parameter.disable_reference_types = mvp;
    parameter.disable_table_instructions = mvp;
    parameter.disable_multiple_tables = mvp;
    parameter.disable_bulk_memory = mvp;
    parameter.disable_simd = mvp;
    parameter.controllable_allow_multi_result_vector = mvp;
    parameter.controllable_allow_multi_table = mvp;
    for(std::string path; std::getline(std::cin, path);)
    {
        std::ifstream file{path, std::ios::binary};
        if(!file) { std::cout << "io 1 0\n"; continue; }
        std::vector<char> bytes{std::istreambuf_iterator<char>{file}, {}};
        auto const size{bytes.size()};
        bytes.push_back(0); // Even an empty input needs a valid pointer range.
        auto const* begin{reinterpret_cast<std::byte const*>(bytes.data())};
        if(p::binfmt::detect_wasm_binfmt_version(begin, begin + size) != 1u)
        { std::cout << "header 1 0\n"; continue; }
        storage module{};
        p::base::error_impl parse_error{};
        try
        { module = p::binfmt::ver1::wasm_binfmt_ver1_handle_func<w1, w11, w2>(begin, begin + size, parse_error, policy); }
        catch(::fast_io::error const&)
        {
            std::cout << "parse " << static_cast<unsigned>(parse_error.err_code) << ' '
                      << (parse_error.err_curr ? parse_error.err_curr - begin : 0) << '\n';
            continue;
        }
        auto const& imports{p::concepts::operation::get_first_type_in_tuple<
            p::standard::wasm1::features::import_section_storage_t<w1, w11, w2>>(module.sections)};
        auto const& codes{p::concepts::operation::get_first_type_in_tuple<
            p::standard::wasm1::features::code_section_storage_t<w1, w11, w2>>(module.sections)};
        bool rejected{};
        for(std::size_t i{}; i != codes.codes.size(); ++i)
        {
            auto const& code{codes.codes.index_unchecked(i)};
            auto const index{imports.importdesc.index_unchecked(0u).size() + i};
            auto const* first{reinterpret_cast<std::byte const*>(code.body.expr_begin)};
            auto const* last{reinterpret_cast<std::byte const*>(code.body.code_end)};
            ::uwvm2::validation::error::code_validation_error_impl error{};
            try
            {
                if(api == "wasm1") { v::wasm1::validate_code(p::standard::wasm1::features::wasm1_code_version{}, module, index, first, last, error); }
                else if(api == "wasm1p1") { v::wasm1p1::validate_code(v::wasm1p1::wasm1p1_code_version{}, module, index, first, last, error, policy); }
                else if(api == "wasm2") { v::wasm2::validate_code(v::wasm2::wasm2_code_version{}, module, index, first, last, error, policy); }
                else { v::wasm2::validate_code_with_runtime_policy(module, index, first, last, error, policy); }
            }
            catch(::fast_io::error const&)
            {
                std::cout << "validate " << static_cast<unsigned>(error.err_code) << ' '
                          << (error.err_curr ? error.err_curr - begin : 0) << '\n';
                rejected = true;
                break;
            }
        }
        if(!rejected) { std::cout << "ok 0 0\n"; }
    }
}
