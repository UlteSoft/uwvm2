/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io_dsal/string.h>

# include <uwvm2/utils/container/impl.h>

#else
# error "Module testing is not currently supported"
#endif

namespace test::wasm1_code_section_module_builder
{
    using ::uwvm2::utils::container::u8string;

    inline void append_byte(u8string& out, ::std::uint8_t b) noexcept { out.push_back(static_cast<char8_t>(b)); }

    inline void append_uleb128(u8string& out, ::std::uint32_t value) noexcept
    {
        ::std::uint32_t v{value};
        do
        {
            ::std::uint8_t byte{static_cast<::std::uint8_t>(v & 0x7Fu)};
            v >>= 7u;
            if(v != 0u) { byte = static_cast<::std::uint8_t>(byte | 0x80u); }
            append_byte(out, byte);
        }
        while(v != 0u);
    }

    inline ::std::uint8_t consume_byte(::std::uint8_t const* data, ::std::size_t size, ::std::size_t& pos, ::std::uint8_t fallback = 0u) noexcept
    {
        if(data == nullptr || pos >= size) { return fallback; }
        return data[pos++];
    }

    inline void append_section_with_payload(u8string& mod, ::std::uint8_t sec_id, u8string const& payload) noexcept
    {
        append_byte(mod, sec_id);
        append_uleb128(mod, static_cast<::std::uint32_t>(payload.size()));
        for(auto ch: payload) { mod.push_back(ch); }
    }

    inline void append_limits(u8string& out, bool has_max, ::std::uint32_t min, ::std::uint32_t max) noexcept
    {
        append_byte(out, has_max ? 0x01u : 0x00u);
        append_uleb128(out, min);
        if(has_max) { append_uleb128(out, max); }
    }

    inline ::std::uint8_t select_value_type(::std::uint8_t tag) noexcept
    {
        switch(tag & 0x03u)
        {
            case 0u: return 0x7Fu;  // i32
            case 1u: return 0x7Eu;  // i64
            case 2u: return 0x7Du;  // f32
            default: return 0x7Cu;  // f64
        }
    }

    inline void append_zero_const_expr(u8string& out, ::std::uint8_t value_type) noexcept
    {
        switch(value_type)
        {
            case 0x7Fu:  // i32
                append_byte(out, 0x41u);
                append_byte(out, 0x00u);
                break;
            case 0x7Eu:  // i64
                append_byte(out, 0x42u);
                append_byte(out, 0x00u);
                break;
            case 0x7Du:  // f32
                append_byte(out, 0x43u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                break;
            default:  // f64
                append_byte(out, 0x44u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                append_byte(out, 0x00u);
                break;
        }
        append_byte(out, 0x0Bu);
    }

    inline void append_module_header(u8string& mod) noexcept
    {
        mod.push_back(u8'\0');
        mod.push_back(u8'a');
        mod.push_back(u8's');
        mod.push_back(u8'm');
        mod.push_back(static_cast<char8_t>(0x01));
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');
        mod.push_back(u8'\0');
    }

    [[nodiscard]] inline ::std::uint32_t build_fuzzed_type_section(u8string& out,
                                                                   ::std::uint8_t const* data,
                                                                   ::std::size_t size,
                                                                   ::std::size_t& pos) noexcept
    {
        u8string payload{};
        auto const type_count{static_cast<::std::uint32_t>(1u + (consume_byte(data, size, pos, 0u) % 4u))};
        append_uleb128(payload, type_count);

        for(::std::uint32_t type_idx{}; type_idx != type_count; ++type_idx)
        {
            append_byte(payload, 0x60u);  // functype

            auto const param_count{static_cast<::std::uint32_t>(consume_byte(data, size, pos, type_idx == 0u ? 0u : 1u) % 3u)};
            append_uleb128(payload, param_count);
            for(::std::uint32_t param_idx{}; param_idx != param_count; ++param_idx)
            {
                append_byte(payload, select_value_type(consume_byte(data, size, pos, static_cast<::std::uint8_t>(param_idx))));
            }

            auto const result_count{static_cast<::std::uint32_t>(consume_byte(data, size, pos, 0u) & 0x01u)};
            append_uleb128(payload, result_count);
            if(result_count != 0u)
            {
                append_byte(payload, select_value_type(consume_byte(data, size, pos, static_cast<::std::uint8_t>(type_idx))));
            }
        }

        append_section_with_payload(out, 1u, payload);
        return type_count;
    }

    inline void build_empty_import_section(u8string& out) noexcept
    {
        u8string payload{};
        append_uleb128(payload, 0u);  // import count
        append_section_with_payload(out, 2u, payload);
    }

    inline void build_fuzzed_function_section(u8string& out,
                                              ::std::uint32_t type_count,
                                              ::std::uint8_t const* data,
                                              ::std::size_t size,
                                              ::std::size_t& pos) noexcept
    {
        u8string payload{};
        append_uleb128(payload, 1u);  // function count
        append_uleb128(payload, static_cast<::std::uint32_t>(consume_byte(data, size, pos, 0u) % type_count));
        append_section_with_payload(out, 3u, payload);
    }

    inline void build_fuzzed_table_section(u8string& out, ::std::uint8_t const* data, ::std::size_t size, ::std::size_t& pos) noexcept
    {
        u8string payload{};
        auto const table_count{static_cast<::std::uint32_t>(consume_byte(data, size, pos, 1u) & 0x01u)};
        append_uleb128(payload, table_count);

        if(table_count != 0u)
        {
            auto const min{static_cast<::std::uint32_t>(consume_byte(data, size, pos, 1u) % 3u)};
            auto const has_max{(consume_byte(data, size, pos, 0u) & 0x01u) != 0u};
            auto const max{static_cast<::std::uint32_t>(min + (consume_byte(data, size, pos, 0u) % 3u))};
            append_byte(payload, 0x70u);  // funcref
            append_limits(payload, has_max, min, max);
        }

        append_section_with_payload(out, 4u, payload);
    }

    inline void build_fuzzed_memory_section(u8string& out, ::std::uint8_t const* data, ::std::size_t size, ::std::size_t& pos) noexcept
    {
        u8string payload{};
        auto const memory_count{static_cast<::std::uint32_t>(consume_byte(data, size, pos, 1u) & 0x01u)};
        append_uleb128(payload, memory_count);

        if(memory_count != 0u)
        {
            auto const min{static_cast<::std::uint32_t>(consume_byte(data, size, pos, 1u) % 3u)};
            auto const has_max{(consume_byte(data, size, pos, 0u) & 0x01u) != 0u};
            auto const max{static_cast<::std::uint32_t>(min + (consume_byte(data, size, pos, 0u) % 3u))};
            append_limits(payload, has_max, min, max);
        }

        append_section_with_payload(out, 5u, payload);
    }

    inline void build_fuzzed_global_section(u8string& out, ::std::uint8_t const* data, ::std::size_t size, ::std::size_t& pos) noexcept
    {
        u8string payload{};
        auto const global_count{static_cast<::std::uint32_t>(consume_byte(data, size, pos, 1u) % 3u)};
        append_uleb128(payload, global_count);

        for(::std::uint32_t global_idx{}; global_idx != global_count; ++global_idx)
        {
            auto const value_type{select_value_type(consume_byte(data, size, pos, static_cast<::std::uint8_t>(global_idx)))};
            auto const is_mutable{static_cast<::std::uint8_t>(consume_byte(data, size, pos, global_idx == 0u ? 1u : 0u) & 0x01u)};
            append_byte(payload, value_type);
            append_byte(payload, is_mutable);
            append_zero_const_expr(payload, value_type);
        }

        append_section_with_payload(out, 6u, payload);
    }

    inline void build_code_section_from_fuzz(u8string& out, ::std::uint8_t const* data, ::std::size_t size, ::std::size_t& pos) noexcept
    {
        u8string payload{};
        append_uleb128(payload, 1u);  // code count

        u8string body{};

        auto const local_group_count{static_cast<::std::uint32_t>(consume_byte(data, size, pos, 1u) % 4u)};
        append_uleb128(body, local_group_count);

        for(::std::uint32_t group_idx{}; group_idx != local_group_count; ++group_idx)
        {
            auto const local_count{static_cast<::std::uint32_t>(1u + (consume_byte(data, size, pos, 0u) % 4u))};
            append_uleb128(body, local_count);
            append_byte(body, select_value_type(consume_byte(data, size, pos, group_idx == 0u ? 0u : 1u)));
        }

        for(; pos < size; ++pos) { append_byte(body, data[pos]); }

        if(body.empty() || static_cast<::std::uint8_t>(body.back()) != 0x0Bu) { append_byte(body, 0x0Bu); }

        append_uleb128(payload, static_cast<::std::uint32_t>(body.size()));
        for(auto ch: body) { payload.push_back(ch); }

        append_section_with_payload(out, 10u, payload);
    }

    [[nodiscard]] inline u8string build_module_from_code_validation_bytes(::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        u8string mod{};
        ::std::size_t pos{};

        append_module_header(mod);
        auto const type_count{build_fuzzed_type_section(mod, data, size, pos)};
        build_empty_import_section(mod);
        build_fuzzed_function_section(mod, type_count, data, size, pos);
        build_fuzzed_table_section(mod, data, size, pos);
        build_fuzzed_memory_section(mod, data, size, pos);
        build_fuzzed_global_section(mod, data, size, pos);
        build_code_section_from_fuzz(mod, data, size, pos);
        return mod;
    }

    [[nodiscard]] inline u8string build_module_from_code_section_bytes(::std::uint8_t const* data, ::std::size_t size) noexcept
    { return build_module_from_code_validation_bytes(data, size); }
}  // namespace test::wasm1_code_section_module_builder
