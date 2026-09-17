#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <fast_io.h>
#include <uwvm2/parser/wasm/standard/wasm2/impl.h>
#include <uwvm2/validation/concepts/impl.h>
#include <uwvm2/validation/standard/wasm2/impl.h>

// Batch binary-corpus probe: one path per stdin line, one phase/error/offset
// record per stdout line. No instantiation, execution, or imported host calls.
int main()
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2 = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using parameters = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using storage = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;
    parameters policy{};
    ::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(policy).cli_mode =
        ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode::direct_wasm2;
    for(::std::string path; ::std::getline(::std::cin, path);)
    {
        ::std::ifstream file{path, ::std::ios::binary};
        if(!file) { ::std::cout << "io 1 0\n"; continue; }
        ::std::vector<char> bytes{::std::istreambuf_iterator<char>{file}, {}};
        // Empty files still need a non-null base for a well-defined pointer range.
        auto const size{bytes.size()};
        bytes.push_back(0);
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes.data())};
        // The version-specific parser assumes the loader selected binary format 1.
        if(::uwvm2::parser::wasm::binfmt::detect_wasm_binfmt_version(begin, begin + size) != 1u)
        {
            ::std::cout << "header 1 0\n";
            continue;
        }
        storage module{};
        ::uwvm2::parser::wasm::base::error_impl parse_error{};
        try
        {
            module = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, begin + size, parse_error, policy);
        }
        catch(::fast_io::error const&)
        {
            ::std::cout << "parse " << static_cast<unsigned>(parse_error.err_code) << ' '
                        << (parse_error.err_curr ? parse_error.err_curr - begin : 0) << '\n';
            continue;
        }
        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        bool rejected{};
        for(::std::size_t index{}; index != codes.codes.size(); ++index)
        {
            auto const& code{codes.codes.index_unchecked(index)};
            ::uwvm2::validation::error::code_validation_error_impl error{};
            try
            {
                ::uwvm2::validation::concepts::dispatch_validate_code(
                    module, imports.importdesc.index_unchecked(0u).size() + index,
                    reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                    reinterpret_cast<::std::byte const*>(code.body.code_end), error, policy);
            }
            catch(::fast_io::error const&)
            {
                ::std::cout << "validate " << static_cast<unsigned>(error.err_code) << ' '
                            << (error.err_curr ? error.err_curr - begin : 0) << '\n';
                rejected = true;
                break;
            }
        }
        if(!rejected) { ::std::cout << "ok 0 0\n"; }
    }
}
