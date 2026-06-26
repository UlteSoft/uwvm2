/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly 1.1 parser and section-details smoke test
 * @details     Parses a small wasm1.1 module and checks printable section_details output.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#include <cstddef>
#include <cstdint>
#include <type_traits>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    using ::uwvm2::utils::container::u8string;

    inline void append_byte(u8string& out, ::std::uint8_t b) noexcept { out.push_back(static_cast<char8_t>(b)); }

    inline void append_uleb128(u8string& out, ::std::uint32_t value) noexcept
    {
        ::std::uint32_t v{value};
        do
        {
            auto byte{static_cast<::std::uint8_t>(v & 0x7Fu)};
            v >>= 7u;
            if(v != 0u) { byte = static_cast<::std::uint8_t>(byte | 0x80u); }
            append_byte(out, byte);
        }
        while(v != 0u);
    }

    inline void append_section_with_payload(u8string& mod, ::std::uint8_t sec_id, u8string const& payload) noexcept
    {
        append_byte(mod, sec_id);
        append_uleb128(mod, static_cast<::std::uint32_t>(payload.size()));
        for(auto ch: payload) { mod.push_back(ch); }
    }

    inline void append_wasm_header(u8string& mod) noexcept
    {
        append_byte(mod, 0x00u);
        append_byte(mod, 0x61u);
        append_byte(mod, 0x73u);
        append_byte(mod, 0x6Du);
        append_byte(mod, 0x01u);
        append_byte(mod, 0x00u);
        append_byte(mod, 0x00u);
        append_byte(mod, 0x00u);
    }

    inline void append_type_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_byte(payload, 0x60u);
        append_uleb128(payload, 0u);
        append_uleb128(payload, 0u);
        append_section_with_payload(mod, 1u, payload);
    }

    inline void append_function_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_uleb128(payload, 0u);
        append_section_with_payload(mod, 3u, payload);
    }

    inline void append_table_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_byte(payload, 0x70u);
        append_byte(payload, 0x00u);
        append_uleb128(payload, 1u);
        append_section_with_payload(mod, 4u, payload);
    }

    inline void append_memory_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_byte(payload, 0x00u);
        append_uleb128(payload, 1u);
        append_section_with_payload(mod, 5u, payload);
    }

    inline void append_global_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_byte(payload, 0x6Fu);
        append_byte(payload, 0x00u);
        append_byte(payload, 0xD0u);
        append_byte(payload, 0x6Fu);
        append_byte(payload, 0x0Bu);
        append_section_with_payload(mod, 6u, payload);
    }

    inline void append_element_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_uleb128(payload, 5u);
        append_byte(payload, 0x70u);
        append_uleb128(payload, 1u);
        append_byte(payload, 0xD2u);
        append_uleb128(payload, 0u);
        append_byte(payload, 0x0Bu);
        append_section_with_payload(mod, 9u, payload);
    }

    inline void append_data_count_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_section_with_payload(mod, 12u, payload);
    }

    inline void append_code_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_uleb128(payload, 2u);
        append_uleb128(payload, 0u);
        append_byte(payload, 0x0Bu);
        append_section_with_payload(mod, 10u, payload);
    }

    inline void append_data_section(u8string& mod) noexcept
    {
        u8string payload;
        append_uleb128(payload, 1u);
        append_uleb128(payload, 1u);
        append_uleb128(payload, 3u);
        append_byte(payload, 0x61u);
        append_byte(payload, 0x62u);
        append_byte(payload, 0x63u);
        append_section_with_payload(mod, 11u, payload);
    }

    [[nodiscard]] inline u8string build_wasm1p1_module() noexcept
    {
        u8string mod;
        append_wasm_header(mod);
        append_type_section(mod);
        append_function_section(mod);
        append_table_section(mod);
        append_memory_section(mod);
        append_global_section(mod);
        append_element_section(mod);
        append_data_count_section(mod);
        append_code_section(mod);
        append_data_section(mod);
        return mod;
    }

    [[noreturn]] inline void fail(char const* msg)
    {
        ::fast_io::io::perrln("wasm1p1_parser_section_details: ", ::fast_io::mnp::os_c_str(msg));
        ::fast_io::fast_terminate();
    }

    template <typename String>
    [[nodiscard]] inline bool contains(String const& haystack, char8_t const* needle) noexcept
    {
        ::std::size_t needle_size{};
        while(needle[needle_size] != u8'\0') { ++needle_size; }
        if(needle_size == 0uz) { return true; }
        auto const haystack_size{haystack.size()};
        if(haystack_size < needle_size) { return false; }
        auto const* const data{haystack.data()};
        for(::std::size_t pos{}; pos <= haystack_size - needle_size; ++pos)
        {
            bool same{true};
            for(::std::size_t idx{}; idx != needle_size; ++idx)
            {
                if(data[pos + idx] != needle[idx])
                {
                    same = false;
                    break;
                }
            }
            if(same) { return true; }
        }
        return false;
    }

    template <typename String>
    inline void expect_contains(String const& haystack, char8_t const* needle, char const* label)
    {
        if(!contains(haystack, needle)) [[unlikely]] { fail(label); }
    }

    template <typename Detail>
    inline void assert_printable_all_chars(Detail const detail)
    {
        ::fast_io::black_hole cdev_null{};
        ::fast_io::wblack_hole wdev_null{};
        ::fast_io::u8black_hole u8dev_null{};
        ::fast_io::u16black_hole u16dev_null{};
        ::fast_io::u32black_hole u32dev_null{};
        ::fast_io::io::print(cdev_null, detail);
        ::fast_io::io::print(wdev_null, detail);
        ::fast_io::io::print(u8dev_null, detail);
        ::fast_io::io::print(u16dev_null, detail);
        ::fast_io::io::print(u32dev_null, detail);
    }

    template <typename Para>
    inline void enable_all_wasm1p1_features(Para& fs_para) noexcept
    {
        auto& p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        p1_para.enable_multi_value = true;
        p1_para.enable_reference_types = true;
        p1_para.enable_bulk_memory = true;
        p1_para.enable_sign_extension = true;
        p1_para.enable_nontrapping_float_to_int = true;
        p1_para.enable_simd = true;
        p1_para.controllable_allow_multi_result_vector = false;
        p1_para.controllable_allow_multi_table = false;
    }
}  // namespace test

int main()
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;

    auto wasm{test::build_wasm1p1_module()};
    auto const* const begin{reinterpret_cast<::std::byte const*>(wasm.data())};
    auto const* const end{begin + wasm.size()};

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1> fs_para{};
    test::enable_all_wasm1p1_features(fs_para);

    ::uwvm2::parser::wasm::base::error_impl err{};
    auto module_storage{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1>(begin, end, err, fs_para)};

    auto const& tablesec{
        ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::table_section_storage_t<wasm1, wasm1p1>>(module_storage.sections)};
    auto const& globalsec{
        ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::global_section_storage_t<wasm1, wasm1p1>>(module_storage.sections)};
    auto const& elemsec{
        ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::element_section_storage_t<wasm1, wasm1p1>>(module_storage.sections)};
    auto const& datacountsec{
        ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<wasm1, wasm1p1>>(module_storage.sections)};
    auto const& datasec{
        ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<wasm1, wasm1p1>>(module_storage.sections)};

    if(tablesec.tables.size() != 1uz) [[unlikely]] { test::fail("table parser count mismatch"); }
    if(globalsec.local_globals.size() != 1uz) [[unlikely]] { test::fail("global parser count mismatch"); }
    if(elemsec.elems.size() != 1uz) [[unlikely]] { test::fail("element parser count mismatch"); }
    if(!datacountsec.present || datacountsec.count != 1u) [[unlikely]] { test::fail("data-count parser mismatch"); }
    if(datasec.datas.size() != 1uz) [[unlikely]] { test::fail("data parser count mismatch"); }

    auto table_details{::fast_io::u8concat_fast_io(section_details(tablesec, module_storage.sections))};
    test::expect_contains(table_details, u8"Table[1]", "table section_details header missing");
    test::expect_contains(table_details, u8"type: funcref", "table section_details type missing");

    auto global_details{::fast_io::u8concat_fast_io(section_details(globalsec, module_storage.sections))};
    test::expect_contains(global_details, u8"Global[1]", "global section_details header missing");
    test::expect_contains(global_details, u8"type: externref", "global section_details type missing");

    auto element_details{::fast_io::u8concat_fast_io(section_details(elemsec, module_storage.sections))};
    test::expect_contains(element_details, u8"Element[1]", "element section_details header missing");
    test::expect_contains(element_details, u8"flag: 5", "element section_details flag missing");
    test::expect_contains(element_details, u8"type: funcref", "element section_details type missing");
    test::expect_contains(element_details, u8"count: 1", "element section_details count missing");

    auto data_count_details{::fast_io::u8concat_fast_io(section_details(datacountsec, module_storage.sections))};
    test::expect_contains(data_count_details, u8"Data Count", "data-count section_details header missing");
    test::expect_contains(data_count_details, u8"count: 1", "data-count section_details count missing");

    auto data_details{::fast_io::u8concat_fast_io(section_details(datasec, module_storage.sections))};
    test::expect_contains(data_details, u8"Data[1]", "data section_details header missing");
    test::expect_contains(data_details, u8"flag: 1", "data section_details flag missing");
    test::expect_contains(data_details, u8"passive, size: 3", "data section_details passive payload missing");

    test::assert_printable_all_chars(section_details(tablesec, module_storage.sections));
    test::assert_printable_all_chars(section_details(globalsec, module_storage.sections));
    test::assert_printable_all_chars(section_details(elemsec, module_storage.sections));
    test::assert_printable_all_chars(section_details(datacountsec, module_storage.sections));
    test::assert_printable_all_chars(section_details(datasec, module_storage.sections));

#ifdef UWVM_CPP_EXCEPTIONS
    {
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1> limited_para{};
        test::enable_all_wasm1p1_features(limited_para);
        auto& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(limited_para)};
        wasm1p1_para.parser_limit.max_elem_sec_expr = 0uz;

        ::uwvm2::parser::wasm::base::error_impl limited_err{};
        try
        {
            static_cast<void>(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1>(begin, end, limited_err, limited_para));
            test::fail("element expression parser limit was not enforced");
        }
        catch(::fast_io::error const&)
        {
        }

        auto const& exceeded{limited_err.err_selectable.exceed_the_max_parser_limit};
        if(limited_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit ||
           !test::contains(exceeded.name, u8"elemsec_expr") || exceeded.value != 1uz || exceeded.maxval != 0uz) [[unlikely]]
        {
            test::fail("element expression parser limit produced the wrong error");
        }
    }

    {
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1> limited_para{};
        test::enable_all_wasm1p1_features(limited_para);
        auto& wasm1p1_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(limited_para)};
        wasm1p1_para.parser_limit.max_data_count_sec_count = 0uz;

        ::uwvm2::parser::wasm::base::error_impl limited_err{};
        try
        {
            static_cast<void>(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1>(begin, end, limited_err, limited_para));
            test::fail("data-count parser limit was not enforced");
        }
        catch(::fast_io::error const&)
        {
        }

        auto const& exceeded{limited_err.err_selectable.exceed_the_max_parser_limit};
        if(limited_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit ||
           !test::contains(exceeded.name, u8"datacountsec_count") || exceeded.value != 1uz || exceeded.maxval != 0uz) [[unlikely]]
        {
            test::fail("data-count parser limit produced the wrong error");
        }
    }
#endif
}
