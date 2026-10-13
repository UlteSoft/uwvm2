/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <utility>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm3::type
{
    // Custom payloads are opaque to Wasm validation. These optional diagnostic
    // summaries cannot reject a module or allocate from a payload's declared size.
    // https://webassembly.github.io/spec/core/binary/modules.html#custom-section
    // DWARF 5 sections 7.2.2, 7.4, 7.5.1 and 6.2.4 specify initial lengths,
    // unit formats and version fields: https://dwarfstd.org/doc/DWARF5.pdf.
    struct escaped_custom_name_t { ::uwvm2::utils::container::u8string_view value{}; };
    struct custom_payload_details_t
    {
        ::uwvm2::utils::container::u8string_view name{};
        ::std::byte const* begin{};
        ::std::byte const* end{};
    };

    namespace details::custom_payload
    {
        enum class header_status : unsigned { complete, truncated, reserved_length, unsupported_version, unsupported_unit_type };
        enum class unit_header_kind : unsigned { information, line, types };
        struct unit_header_summary
        {
            ::std::size_t units32{};
            ::std::size_t units64{};
            ::std::uint_least16_t minimum_version{};
            ::std::uint_least16_t maximum_version{};
            header_status status{};
        };

        template<::std::size_t Bits, typename Integer>
        [[nodiscard]] inline constexpr bool scan_little_endian(::std::byte const*& curr, ::std::byte const* end,
            Integer& value) noexcept
        {
            // [previous fields] field ... end
            // [safe           ] unsafe (possibly end)
            //                   ^^ curr; the bounded fast_io scanner checks before every input read.
            auto const [next, status]{::fast_io::parse_by_scan(reinterpret_cast<char8_t const*>(curr),
                reinterpret_cast<char8_t const*>(end), ::fast_io::mnp::le_get<Bits>(value))};
            if(status != ::fast_io::parse_code::ok) { return false; }
            // [previous fields field] remaining ... end
            // [safe                 ] unsafe (possibly end)
            //                         ^^ curr: publish only a successful bounded scanner endpoint.
            curr = reinterpret_cast<::std::byte const*>(next);
            return true;
        }

        [[nodiscard]] inline constexpr unit_header_summary summarize_unit_headers(
            ::std::byte const* begin, ::std::byte const* end, unit_header_kind kind = unit_header_kind::information) noexcept
        {
            unit_header_summary summary{};
            if(begin == end) { return summary; } // includes an empty default span; no null pointer subtraction
            // [custom payload ... end)
            // unsafe until each field is bounded
            // ^^ curr: borrow the parser-validated payload allocation, including its end.
            auto curr{begin};
            while(curr != end)
            {
                ::std::uint_least32_t initial_length{};
                if(!scan_little_endian<32uz>(curr, end, initial_length))
                { summary.status = header_status::truncated; return summary; }
                ::std::uint_least64_t length{initial_length};
                bool const format64{initial_length == 0xffff'ffffu};
                if(format64)
                {
                    if(!scan_little_endian<64uz>(curr, end, length))
                    { summary.status = header_status::truncated; return summary; }
                }
                else if(initial_length >= 0xffff'fff0u)
                { summary.status = header_status::reserved_length; return summary; }
                auto const remaining{static_cast<::std::size_t>(end - curr)};
                if(length > remaining || length < 2u)
                { summary.status = header_status::truncated; return summary; }
                // [length field] [unit contribution of checked length] remaining ... end
                // [safe                                                    ]
                //                ^^ curr; length <= end-curr BEFORE forming the endpoint or narrowing to size_t.
                auto const unit_end{curr + static_cast<::std::size_t>(length)};
                ::std::uint_least16_t version{};
                if(!scan_little_endian<16uz>(curr, unit_end, version))
                { summary.status = header_status::truncated; return summary; }
                if(version < 2u || version > 5u)
                { summary.status = header_status::unsupported_version; return summary; }
                ::std::size_t const offset_size{format64 ? 8uz : 4uz};
                auto minimum_header{2uz + offset_size + 1uz}; // info v2-v4: version, abbrev offset, address size
                if(kind == unit_header_kind::types)
                {
                    if(version != 4u) { summary.status = header_status::unsupported_version; return summary; }
                    minimum_header += 8uz + offset_size; // type signature and type offset
                }
                else if(kind == unit_header_kind::line)
                { minimum_header = 2uz + offset_size + (version == 5u ? 2uz : 0uz); }
                else if(version == 5u)
                {
                    ::std::uint_least8_t unit_type{};
                    if(!scan_little_endian<8uz>(curr, unit_end, unit_type))
                    { summary.status = header_status::truncated; return summary; }
                    minimum_header = 4uz + offset_size; // version, unit type, address size, abbrev offset
                    switch(unit_type)
                    {
                        case 1u: case 3u: break; // compile / partial
                        case 4u: case 5u: minimum_header += 8uz; break; // skeleton / split compile: dwo_id
                        case 2u: case 6u: minimum_header += 8uz + offset_size; break; // type / split type
                        default: summary.status = header_status::unsupported_unit_type; return summary;
                    }
                }
                if(length < minimum_header) { summary.status = header_status::truncated; return summary; }
                if(format64) { ++summary.units64; }
                else { ++summary.units32; }
                if(summary.minimum_version == 0u || version < summary.minimum_version) { summary.minimum_version = version; }
                if(version > summary.maximum_version) { summary.maximum_version = version; }
                // [length version ... uninterpreted unit bytes] next contribution ... end
                // [safe                                      ] unsafe (possibly end)
                //                                              ^^ curr: unit_end was range-checked before addition.
                curr = unit_end;
            }
            return summary;
        }

        [[nodiscard]] inline constexpr bool has_unit_headers(::uwvm2::utils::container::u8string_view name) noexcept
        {
            return name == u8".debug_info" || name == u8".debug_info.dwo" || name == u8".debug_line" ||
                name == u8".debug_line.dwo" || name == u8".debug_types" || name == u8".debug_types.dwo";
        }
        [[nodiscard]] inline constexpr bool starts_with(::uwvm2::utils::container::u8string_view name,
            ::uwvm2::utils::container::u8string_view prefix) noexcept
        {
            if(name.size() < prefix.size()) { return false; }
            for(::std::size_t i{}; i != prefix.size(); ++i) { if(name[i] != prefix[i]) { return false; } }
            return true;
        }
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, escaped_custom_name_t>, Stream&& stream,
        escaped_custom_name_t details)
    {
        auto const name{details.value};
        ::std::size_t start{};
        for(::std::size_t i{}; i != name.size(); ++i)
        {
            auto const byte{static_cast<::std::uint_least8_t>(name[i])};
            if(byte >= 0x20u && byte != 0x7fu && byte != '\\' && byte != '(' && byte != ')') { continue; }
            if(i != start)
            {
                // [already emitted] [start ... i) control ... name.end
                // [safe                                               ]
                //                    ^^ chunk: start < i <= name.size() before pointer addition.
                auto const chunk{::uwvm2::utils::container::u8string_view{name.cbegin() + start, i - start}};
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(chunk));
            }
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
                ::fast_io::mnp::code_cvt(u8"\\x"), ::fast_io::mnp::hex<false, true>(byte));
            start = i + 1uz; // i < name.size(); the next integer offset is at most size, without dereferencing end.
        }
        if(start != name.size())
        {
            // [already emitted] [start ... name.end)
            // [safe                                  ]
            //                    ^^ chunk: start < size before pointer addition.
            auto const chunk{::uwvm2::utils::container::u8string_view{name.cbegin() + start, name.size() - start}};
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(chunk));
        }
    }

    template<::std::integral Char, typename Stream>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, custom_payload_details_t>, Stream&& stream,
        custom_payload_details_t details)
    {
        using namespace ::uwvm2::parser::wasm::standard::wasm3::type::details::custom_payload;
        auto const name{details.name};
        if(has_unit_headers(name))
        {
            auto const kind{name == u8".debug_line" || name == u8".debug_line.dwo" ? unit_header_kind::line :
                name == u8".debug_types" || name == u8".debug_types.dwo" ? unit_header_kind::types : unit_header_kind::information};
            auto const summary{summarize_unit_headers(details.begin, details.end, kind)};
            ::uwvm2::utils::container::u8string_view status{u8"complete"};
            switch(summary.status)
            {
                case header_status::complete: break;
                case header_status::truncated: status = u8"truncated"; break;
                case header_status::reserved_length: status = u8"reserved-initial-length"; break;
                case header_status::unsupported_version: status = u8"unsupported-version"; break;
                case header_status::unsupported_unit_type: status = u8"unsupported-unit-type"; break;
            }
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
                ::fast_io::mnp::code_cvt(u8", kind = DWARF, unit-headers = "), ::fast_io::mnp::code_cvt(status),
                ::fast_io::mnp::code_cvt(u8", dwarf32-units = "), ::fast_io::mnp::dec(summary.units32),
                ::fast_io::mnp::code_cvt(u8", dwarf64-units = "), ::fast_io::mnp::dec(summary.units64));
            if(summary.minimum_version != 0u)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
                    ::fast_io::mnp::code_cvt(u8", versions = "), ::fast_io::mnp::dec(summary.minimum_version));
                if(summary.minimum_version != summary.maximum_version)
                { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8".."),
                    ::fast_io::mnp::dec(summary.maximum_version)); }
            }
            return;
        }
        ::uwvm2::utils::container::u8string_view kind{};
        if(starts_with(name, u8".debug_")) { kind = u8"DWARF auxiliary"; }
        else if(starts_with(name, u8".zdebug_")) { kind = u8"compressed DWARF (opaque)"; }
        else if(name == u8"name") { kind = u8"Wasm names"; }
        else if(name == u8"producers") { kind = u8"producer metadata"; }
        else if(name == u8"target_features") { kind = u8"toolchain target features"; }
        else if(name == u8"linking" || starts_with(name, u8"reloc.")) { kind = u8"linker metadata"; }
        else if(name == u8"sourceMappingURL") { kind = u8"source map URL"; }
        else if(name == u8"external_debug_info") { kind = u8"external debug information"; }
        if(!kind.empty())
        { ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream),
            ::fast_io::mnp::code_cvt(u8", kind = "), ::fast_io::mnp::code_cvt(kind)); }
    }
}
