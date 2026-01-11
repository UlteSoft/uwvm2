/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\\ \\      / /\\ \\   / /|  \\/  | *
 * | | | | \\ \\ /\\ / /  \\ \\ / / | |\\/| | *
 * | |_| |  \\ V  V /    \\ V /  | |  | | *
 *  \\___/    \\_/\\_/      \\_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#ifndef UWVM_MODULE
# include <fast_io.h>

# include <uwvm2/compiler/validation/error/error_code_output.h>
# include <uwvm2/compiler/validation/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/base/error_code_output.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm_custom/customs/name.h>
# include <uwvm2/parser/wasm_custom/customs/name_error_output.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using feature_t = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    // Some environments run binaries under ptrace-like supervision, which makes LeakSanitizer abort.
    // Disable leak checking by default so this tool can be used reliably.
    extern "C" const char* __asan_default_options() { return "detect_leaks=0"; }

    struct options_t
    {
        bool list_custom_sections{};
        bool parse_name_section{true};
        bool ignore_name_errors{};
        bool validate_code{true};
        bool ansi{};
    };

    inline constexpr void print_usage() noexcept
    {
        ::fast_io::io::perrln("Usage:",
                              "\n  uwvm_wasm_validate [options] <file>...",
                              "\n  uwvm_wasm_validate [options] --base64 <b64>",
                              "\n\nOptions:",
                              "\n  --list-custom-sections   Print custom section names",
                              "\n  --no-name                Skip parsing custom section \"name\"",
                              "\n  --ignore-name-errors     Do not fail on name-section errors",
                              "\n  --no-validate-code       Skip validate_code (parser only)",
                              "\n  --ansi                   Enable ANSI colors in UWVM outputs",
                              "\n  -h, --help               Show this help");
    }

    [[nodiscard]] inline constexpr bool is_space_char(char ch) noexcept
    {
        return ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\v' || ch == '\f';
    }

    [[nodiscard]] inline constexpr int base64_value(unsigned char ch) noexcept
    {
        if(ch >= 'A' && ch <= 'Z') { return static_cast<int>(ch - 'A'); }
        if(ch >= 'a' && ch <= 'z') { return static_cast<int>(ch - 'a') + 26; }
        if(ch >= '0' && ch <= '9') { return static_cast<int>(ch - '0') + 52; }
        if(ch == '+') { return 62; }
        if(ch == '/') { return 63; }
        return -1;
    }

    [[nodiscard]] static bool decode_base64(::std::string_view in, ::std::vector<::std::byte>& out) noexcept
    {
        out.clear();
        out.reserve((in.size() / 4u) * 3u);

        unsigned int val{};
        int valb{-8};

        for(char ch: in)
        {
            if(is_space_char(ch)) { continue; }
            if(ch == '=') { break; }

            int const d{base64_value(static_cast<unsigned char>(ch))};
            if(d < 0) { return false; }

            val = (val << 6u) | static_cast<unsigned int>(d);
            valb += 6;
            if(valb >= 0)
            {
                out.emplace_back(static_cast<::std::byte>((val >> static_cast<unsigned>(valb)) & 0xFFu));
                valb -= 8;
            }
        }

        return true;
    }

    [[nodiscard]] static bool validate_buffer(options_t const& opt,
                                              ::std::string_view label,
                                              ::std::byte const* begin,
                                              ::std::byte const* end) noexcept
    {
        if(begin == nullptr || end == nullptr || end < begin) [[unlikely]]
        {
            ::fast_io::io::perrln(label, ": invalid buffer");
            return false;
        }

        auto const size{static_cast<::std::size_t>(end - begin)};
        if(size < 8u) [[unlikely]]
        {
            ::fast_io::io::perrln(label, ": too small (", size, " bytes)");
            return false;
        }

        // Phase 1: parser check (must pass before running validator).
        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<feature_t> module_storage{};
        try
        {
            module_storage = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<feature_t>(begin, end, parse_err, {});
        }
        catch(::fast_io::error const&)
        {
            ::uwvm2::parser::wasm::base::error_output_t errout{};
            errout.module_begin = begin;
            errout.err = parse_err;
            errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(opt.ansi);
            ::fast_io::io::perrln(label, ": parser FAIL");
            ::fast_io::io::perrln(errout);
            return false;
        }

        ::fast_io::io::perrln(label, ": parser OK");

        // Phase 1.5: name custom section (debug names). Mirrors WABT default: read_debug_names=true, fail_on_custom_section_error=true.
        bool name_ok{true};
        {
            auto const& customsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::custom_section_storage_t>(module_storage.sections)};

            if(opt.list_custom_sections)
            {
                ::fast_io::io::perrln(label, ": custom section count=", customsec.customs.size());
                for(auto const& cs: customsec.customs)
                {
                    ::fast_io::io::perrln(::fast_io::u8err(), u8"  - \"", cs.custom_name, u8"\"");
                }
            }

            if(opt.parse_name_section)
            {
                ::std::size_t name_sec_index{};
                for(auto const& cs: customsec.customs)
                {
                    if(cs.custom_name != u8"name") { continue; }

                    auto const* const name_begin{reinterpret_cast<::std::byte const*>(cs.custom_begin)};
                    auto const* const name_end{reinterpret_cast<::std::byte const*>(cs.sec_span.sec_end)};

                    ::uwvm2::parser::wasm_custom::customs::name_storage_t name_storage{};
                    ::uwvm2::utils::container::vector<::uwvm2::parser::wasm_custom::customs::name_err_t> name_errs{};
                    ::uwvm2::parser::wasm_custom::customs::name_parser_param_t name_param{};

                    ::uwvm2::parser::wasm_custom::customs::parse_name_storage(name_storage, name_begin, name_end, name_errs, name_param);

                    if(!name_errs.empty())
                    {
                        name_ok = false;
                        ::fast_io::io::perrln(label, ": name section #", name_sec_index, " FAIL");
                        for(auto const& ne: name_errs)
                        {
                            ::uwvm2::parser::wasm_custom::customs::name_error_output_t errout{};
                            errout.name_begin = name_begin;
                            errout.name_err = ne;
                            errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(opt.ansi);
                            ::fast_io::io::perrln(errout);
                        }
                    }
                    ++name_sec_index;
                }
            }
        }

        if(opt.parse_name_section)
        {
            if(name_ok)
            {
                ::fast_io::io::perrln(label, ": name OK");
            }
            else
            {
                ::fast_io::io::perrln(label, ": name FAIL");
                if(!opt.ignore_name_errors) { return false; }
            }
        }

        if(!opt.validate_code)
        {
            return true;
        }

        // Phase 2: validate each local function body.
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<feature_t>>(module_storage.sections)};
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<feature_t>>(module_storage.sections)};

        for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
        {
            auto const& code{codesec.codes.index_unchecked(local_idx)};
            auto const* const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const* const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

            ::uwvm2::compiler::validation::error::code_validation_error_impl v_err{};
            try
            {
                ::uwvm2::compiler::validation::standard::wasm1::validate_code<feature_t>(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                                                                                        module_storage,
                                                                                        import_func_count + local_idx,
                                                                                        code_begin_ptr,
                                                                                        code_end_ptr,
                                                                                        v_err);
            }
            catch(::fast_io::error const&)
            {
                ::uwvm2::compiler::validation::error::error_output_t errout{};
                errout.module_begin = begin;
                errout.err = v_err;
                errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(opt.ansi);
                ::fast_io::io::perrln(label, ": validate_code FAIL (local_idx=", local_idx, ")");
                ::fast_io::io::perrln(errout);
                return false;
            }
        }

        ::fast_io::io::perrln(label, ": validate_code OK");
        return true;
    }
}  // namespace

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        print_usage();
        return 1;
    }

    options_t opt{};
    ::std::vector<::std::string_view> file_inputs{};
    ::std::vector<::std::string_view> base64_inputs{};

    for(int i{1}; i < argc; ++i)
    {
        ::std::string_view arg{argv[i]};
        if(arg == "-h" || arg == "--help")
        {
            print_usage();
            return 0;
        }
        if(arg == "--list-custom-sections")
        {
            opt.list_custom_sections = true;
            continue;
        }
        if(arg == "--no-name")
        {
            opt.parse_name_section = false;
            continue;
        }
        if(arg == "--ignore-name-errors")
        {
            opt.ignore_name_errors = true;
            continue;
        }
        if(arg == "--no-validate-code")
        {
            opt.validate_code = false;
            continue;
        }
        if(arg == "--ansi")
        {
            opt.ansi = true;
            continue;
        }
        if(arg == "--base64")
        {
            if(i + 1 >= argc)
            {
                ::fast_io::io::perrln("--base64 requires an argument");
                return 1;
            }

            base64_inputs.emplace_back(argv[++i]);
            continue;
        }

        file_inputs.emplace_back(arg);
    }

    if(file_inputs.empty() && base64_inputs.empty())
    {
        print_usage();
        return 1;
    }

    bool any_failed{};

    for(auto const& b64: base64_inputs)
    {
        ::std::vector<::std::byte> bytes{};
        if(!decode_base64(b64, bytes))
        {
            ::fast_io::io::perrln("base64 decode failed");
            any_failed = true;
            continue;
        }

        auto const ok{validate_buffer(opt, "<base64>", bytes.data(), bytes.data() + bytes.size())};
        if(!ok) { any_failed = true; }
    }

    for(auto const& path: file_inputs)
    {
        try
        {
            ::fast_io::native_file_loader loader{path, ::fast_io::open_mode::in | ::fast_io::open_mode::follow};
            auto const* begin = reinterpret_cast<::std::byte const*>(loader.data());
            auto const* end = begin + loader.size();

            auto const ok{validate_buffer(opt, path, begin, end)};
            if(!ok) { any_failed = true; }
        }
        catch(::fast_io::error const& e)
        {
            ::fast_io::io::perrln(path, ": open/read failed: ", e);
            any_failed = true;
        }
    }

    return any_failed ? 1 : 0;
}
