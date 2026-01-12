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

// libwabt uses assert() in headers and may abort on malformed inputs when NDEBUG is not set.
// For differential fuzzing, treat those cases as validation failures instead of crashing.
#ifndef NDEBUG
# define NDEBUG 1
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#ifndef UWVM_MODULE
# include <fast_io.h>

# include <uwvm2/compiler/validation/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm_custom/customs/name.h>

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-function"
#  pragma clang diagnostic ignored "-Wunused-parameter"
# endif

// WABT headers
# include "wabt/binary-reader-ir.h"
# include "wabt/validator.h"

# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using uwvm_feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;

    // Some environments run binaries under ptrace-like supervision, which makes LeakSanitizer abort.
    // Disable leak checking by default so the fuzzer can run reliably.
    extern "C" const char* __asan_default_options() { return "detect_leaks=0"; }

    [[nodiscard]] static bool wabt_strict_mode() noexcept
    {
        auto const* v = ::std::getenv("UWVM_WABT_STRICT");
        return v != nullptr && *v != '\0' && !(v[0] == '0' && v[1] == '\0');
    }

    [[nodiscard]] static constexpr bool is_wasm_binfmt_ver1_mvp(::std::uint8_t const* data, ::std::size_t size) noexcept;

    [[nodiscard]] static bool read_leb_u32(::std::uint8_t const*& p, ::std::uint8_t const* end, ::std::uint32_t& out) noexcept
    {
        ::std::uint32_t result{};
        unsigned shift{};
        for(unsigned i{}; i != 5u; ++i)
        {
            if(p == end) { return false; }
            auto const byte{*p++};
            result |= static_cast<::std::uint32_t>(byte & 0x7fu) << shift;
            if((byte & 0x80u) == 0u)
            {
                out = result;
                return true;
            }
            shift += 7u;
        }
        return false;
    }

    [[nodiscard]] static bool skip_leb_s32(::std::uint8_t const*& p, ::std::uint8_t const* end) noexcept
    {
        for(unsigned i{}; i != 5u; ++i)
        {
            if(p == end) { return false; }
            auto const byte{*p++};
            if((byte & 0x80u) == 0u) { return true; }
        }
        return false;
    }

    [[nodiscard]] static bool skip_leb_s64(::std::uint8_t const*& p, ::std::uint8_t const* end) noexcept
    {
        for(unsigned i{}; i != 10u; ++i)
        {
            if(p == end) { return false; }
            auto const byte{*p++};
            if((byte & 0x80u) == 0u) { return true; }
        }
        return false;
    }

    // WABT may accept some post-MVP encodings (e.g. element segment flags/passive segments) even when features are disabled.
    // This fuzzer targets wasm1 MVP behaviour; filter out element sections that are not representable in MVP.
    [[nodiscard]] static bool has_non_mvp_element_section_encoding(::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        if(!is_wasm_binfmt_ver1_mvp(data, size)) { return false; }

        auto const* p{data + 8u};
        auto const* const end{data + size};

        while(p != end)
        {
            auto const section_id{*p++};

            ::std::uint32_t section_size_u32{};
            if(!read_leb_u32(p, end, section_size_u32)) { return false; }

            auto const section_size{static_cast<::std::size_t>(section_size_u32)};
            if(static_cast<::std::size_t>(end - p) < section_size) { return false; }

            auto const* const payload_begin{p};
            auto const* const payload_end{p + section_size};

            if(section_id == 9u)  // element section
            {
                auto const* q{payload_begin};

                ::std::uint32_t elem_count{};
                if(!read_leb_u32(q, payload_end, elem_count)) { return false; }

                for(::std::uint32_t i{}; i != elem_count; ++i)
                {
                    ::std::uint32_t table_idx{};
                    if(!read_leb_u32(q, payload_end, table_idx)) { return false; }

                    // MVP has at most one table, so table index must be 0. Non-zero usually indicates element segment flags encoding.
                    if(table_idx != 0u) { return true; }

                    // Parse MVP init_expr until 0x0B (end).
                    for(;;)
                    {
                        if(q == payload_end) { return false; }
                        auto const op{*q++};
                        if(op == 0x0Bu) { break; }
                        switch(op)
                        {
                            case 0x41u:  // i32.const
                                if(!skip_leb_s32(q, payload_end)) { return false; }
                                break;
                            case 0x42u:  // i64.const
                                if(!skip_leb_s64(q, payload_end)) { return false; }
                                break;
                            case 0x43u:  // f32.const
                                if(static_cast<::std::size_t>(payload_end - q) < 4u) { return false; }
                                q += 4u;
                                break;
                            case 0x44u:  // f64.const
                                if(static_cast<::std::size_t>(payload_end - q) < 8u) { return false; }
                                q += 8u;
                                break;
                            case 0x23u:  // global.get
                            {
                                ::std::uint32_t idx{};
                                if(!read_leb_u32(q, payload_end, idx)) { return false; }
                                break;
                            }
                            default:
                                // Not MVP-encodable init_expr opcode.
                                return true;
                        }
                    }

                    ::std::uint32_t funcidx_count{};
                    if(!read_leb_u32(q, payload_end, funcidx_count)) { return false; }
                    for(::std::uint32_t j{}; j != funcidx_count; ++j)
                    {
                        ::std::uint32_t func_idx{};
                        if(!read_leb_u32(q, payload_end, func_idx)) { return false; }
                    }
                }
            }

            p = payload_end;
        }

        return false;
    }

    [[nodiscard]] static bool strip_custom_sections(::std::uint8_t const* data, ::std::size_t size, ::std::vector<::std::uint8_t>& out) noexcept
    {
        if(!is_wasm_binfmt_ver1_mvp(data, size)) { return false; }

        out.clear();
        out.reserve(size);
        out.insert(out.end(), data, data + 8u);

        auto const* p{data + 8u};
        auto const* const end{data + size};

        while(p != end)
        {
            auto const section_id{*p++};

            auto const* const size_begin{p};
            ::std::uint32_t section_size_u32{};
            if(!read_leb_u32(p, end, section_size_u32)) { return false; }

            auto const section_size{static_cast<::std::size_t>(section_size_u32)};
            if(static_cast<::std::size_t>(end - p) < section_size) { return false; }

            auto const* const payload_begin{p};
            auto const* const payload_end{p + section_size};

            if(section_id != 0u)
            {
                out.push_back(section_id);
                out.insert(out.end(), size_begin, p);
                out.insert(out.end(), payload_begin, payload_end);
            }

            p = payload_end;
        }

        return true;
    }

    [[nodiscard]] static constexpr bool is_wasm_binfmt_ver1_mvp(::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        // WABT requires a valid magic + version(1). UWVM's parser checks magic but does not reject unknown versions by default.
        return size >= 8u && data != nullptr && data[0] == 0x00u && data[1] == 0x61u && data[2] == 0x73u && data[3] == 0x6Du && data[4] == 0x01u &&
               data[5] == 0x00u && data[6] == 0x00u && data[7] == 0x00u;
    }

    [[nodiscard]] static bool validate_with_uwvm(::std::uint8_t const* data, ::std::size_t size)
    {
        if(!is_wasm_binfmt_ver1_mvp(data, size)) { return false; }

        auto const* begin = reinterpret_cast<::std::byte const*>(data);
        auto const* end = begin + size;

        // Phase 1: parser check (must pass before running validator).
        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<uwvm_feature> module_storage{};

        try
        {
            module_storage = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<uwvm_feature>(begin, end, parse_err, {});
        }
        catch(::fast_io::error const&)
        {
            return false;
        }
        if(parse_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return false; }

        // Phase 1.5: parse and validate "name" custom section (debug names), to match WABT's default behaviour.
        if(wabt_strict_mode())
        {
            auto const& customsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::custom_section_storage_t>(module_storage.sections)};

            ::uwvm2::parser::wasm_custom::customs::name_parser_param_t name_param{};

            for(auto const& cs: customsec.customs)
            {
                if(cs.custom_name != u8"name") { continue; }

                auto const* const name_begin{reinterpret_cast<::std::byte const*>(cs.custom_begin)};
                auto const* const name_end{reinterpret_cast<::std::byte const*>(cs.sec_span.sec_end)};

                // WABT allows multiple "name" custom sections; validate each independently.
                ::uwvm2::parser::wasm_custom::customs::name_storage_t name_storage{};
                ::uwvm2::utils::container::vector<::uwvm2::parser::wasm_custom::customs::name_err_t> name_errs{};
                ::uwvm2::parser::wasm_custom::customs::parse_name_storage(name_storage, name_begin, name_end, name_errs, name_param);
                if(!name_errs.empty()) { return false; }
            }
        }

        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<uwvm_feature>>(module_storage.sections)};
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<uwvm_feature>>(module_storage.sections)};

        // Phase 2: validate each local function body.
        for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
        {
            auto const& code{codesec.codes.index_unchecked(local_idx)};
            auto const* const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const* const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

            ::uwvm2::compiler::validation::error::code_validation_error_impl v_err{};

            try
            {
                ::uwvm2::compiler::validation::standard::wasm1::validate_code<uwvm_feature>(
                    ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                    module_storage,
                    import_func_count + local_idx,
                    code_begin_ptr,
                    code_end_ptr,
                    v_err);
            }
            catch(::fast_io::error const&)
            {
                return false;
            }
            if(v_err.err_code != ::uwvm2::compiler::validation::error::code_validation_error_code::ok) { return false; }
        }

        return true;
    }

    [[nodiscard]] static bool validate_with_wabt(::std::uint8_t const* data, ::std::size_t size)
    {
        if(!is_wasm_binfmt_ver1_mvp(data, size)) { return false; }

        using namespace ::wabt;

        Errors errors;
        Module module;
        Features validate_features;

        // Restrict to a Wasm1 MVP-like feature set (match UWVM wasm1 implementation as closely as possible).
        validate_features.disable_exceptions();
        validate_features.disable_sat_float_to_int();
        validate_features.disable_sign_extension();
        validate_features.disable_simd();
        validate_features.disable_threads();
        validate_features.disable_function_references();
        validate_features.disable_multi_value();
        validate_features.disable_tail_call();
        validate_features.disable_bulk_memory();
        validate_features.disable_reference_types();
        validate_features.disable_code_metadata();
        validate_features.disable_annotations();
        validate_features.disable_gc();
        validate_features.disable_memory64();
        validate_features.disable_multi_memory();
        validate_features.disable_extended_const();
        validate_features.disable_relaxed_simd();
        validate_features.disable_custom_page_sizes();

        const bool kStopOnFirstError = true;
        const bool kReadDebugNames = wabt_strict_mode();
        const bool kFailOnCustomSectionError = wabt_strict_mode();

        // Parse and validate with the same strict feature set as UWVM. WABT may contain debug assertions when
        // parsing malformed inputs; keep WABT built in Release (and force `NDEBUG` above) so those assertions
        // don't abort the fuzzer.
        ReadBinaryOptions read_options(validate_features, nullptr, kReadDebugNames, kStopOnFirstError, kFailOnCustomSectionError);

        Result result = ReadBinaryIr("<buffer>", data, size, read_options, &errors, &module);
        if(Failed(result)) { return false; }

        ValidateOptions validate_options(validate_features);
        result = ValidateModule(&module, &errors, validate_options);
        return Succeeded(result);
    }
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    if(data == nullptr) { return 0; }

    if(!is_wasm_binfmt_ver1_mvp(data, size)) { return 0; }

    bool const strict{wabt_strict_mode()};

    ::std::uint8_t const* test_data{data};
    ::std::size_t test_size{size};

    // Default to stripping custom sections to avoid noise from toolchain-specific custom sections
    // (e.g. reloc/linking/name), and to focus on core Wasm validity.
    static thread_local ::std::vector<::std::uint8_t> stripped{};
    if(!strict)
    {
        if(!strip_custom_sections(data, size, stripped)) { return 0; }
        test_data = stripped.data();
        test_size = stripped.size();
    }

    if(has_non_mvp_element_section_encoding(test_data, test_size)) { return 0; }

    auto const uwvm_ok{validate_with_uwvm(test_data, test_size)};
    auto const wabt_ok{validate_with_wabt(test_data, test_size)};

    if(uwvm_ok != wabt_ok) { __builtin_trap(); }

    return 0;
}
