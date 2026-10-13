/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include "def.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    // Core 3 binary/modules.html#binary-tagsec and binary/types.html#binary-tagtype.
    // Store indices, not pointers into type storage, so parser copies cannot retain stale type addresses.
    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct tag_section_storage_t
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Tag"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{13u};
        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32> type_indices{};
        bool present{};
    };

    // Shared by local and imported tag descriptors. The caller owns [curr,end); a failure never reads past end.
    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    [[nodiscard]] inline constexpr ::std::byte const* parse_core3_tag_type(
        ::std::byte const* curr, ::std::byte const* end,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& index,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        using code = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        if(get_wasm1p1_parameter(parameters).disable_exceptions) [[unlikely]]
        {
            // [tag descriptor ... end) is borrowed; the diagnostic may be end and is not dereferenced.
            // ^^ err_curr
            err.err_curr = curr; err.err_code = code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {.value = 13u,
                .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::tag_type};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        if(curr == end || (static_cast<unsigned>(*curr) & 0xffu) != 0u) [[unlikely]]
        {
            // [descriptor ... end) no read when curr == end; zero is a literal byte, not a LEB128 integer.
            // ^^ err_curr
            err.err_curr = curr; err.err_code = code::wasm3_invalid_tag_type; err.err_selectable.u8 = 0u;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // attribute | typeidx ... end
        // [safe   ]   unsafe (could be end)
        // ^^ curr; the equality check proved one byte. +1 remains within the allocation or equals end.
        ++curr;
        // attribute | typeidx ... end
        // [safe   ]   ^^ curr, checked by the bounded scanner before any read.
        using chars UWVM_GNU_MAY_ALIAS = char8_t const*;
        auto const [next, status]{::fast_io::parse_by_scan(reinterpret_cast<chars>(curr), reinterpret_cast<chars>(end),
            ::fast_io::mnp::leb128_get(index))};
        if(status != ::fast_io::parse_code::ok) [[unlikely]]
        {
            // [typeidx ... end) the failed scanner does not publish its tentative cursor.
            // ^^ err_curr
            err.err_curr = curr; err.err_code = code::invalid_type_index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(status);
        }
        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(module.sections)};
        auto const& types{typesec.types};
        if(index >= types.size()) [[unlikely]]
        {
            // [typeidx ... end) checked bytes; only the integer index, not a derived type pointer, is used.
            // ^^ err_curr
            err.err_curr = curr; err.err_code = code::illegal_type_index; err.err_selectable.u32 = index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        if(!typesec.core3_type_kinds.empty() &&
           (index >= typesec.core3_type_kinds.size() ||
            typesec.core3_type_kinds.index_unchecked(index) !=
                ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind::function)) [[unlikely]]
        {
            // [tag typeidx ... end) was scanned within the section allocation.
            // [safe              ] unsafe (possibly end)
            // ^^ err_curr: borrowed checked cursor, without a dereference.
            err.err_curr = curr; err.err_code = code::illegal_type_index; err.err_selectable.u32 = index;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        auto const& type{types.index_unchecked(index)};
        if(type.result.begin != type.result.end) [[unlikely]]
        {
            // [typeidx ... end) the checked type exists but tags require an empty result sequence.
            // ^^ err_curr
            err.err_curr = curr; err.err_code = code::wasm3_invalid_tag_type; err.err_selectable.u8 = 0u;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // attribute typeidx | following bytes ... end
        // [safe           ]   ^^ returned cursor: successful bounded scan proves next <= end.
        return reinterpret_cast<::std::byte const*>(next);
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<tag_section_storage_t<Fs...>>,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module,
        ::std::byte const* begin, ::std::byte const* end, ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters,
        ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t&, ::std::byte const* id) UWVM_THROWS
    {
        using code = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        auto& section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<tag_section_storage_t<Fs...>>(module.sections)};
        auto const& policy{get_wasm1p1_parameter(parameters)};
        if(policy.disable_exceptions) [[unlikely]]
        {
            // [module ... section-id ... end] id was bounded by the module decoder; diagnostic only.
            //             ^^ err_curr
            err.err_curr = id; err.err_code = code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {.value = 13u,
                .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::tag_type};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        if(section.present) [[unlikely]]
        {
            // [module ... section-id ... end] id is a borrowed diagnostic address, not read here.
            //             ^^ err_curr
            err.err_curr = id; err.err_code = code::duplicate_section; err.err_selectable.u8 = 13u;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        using chars UWVM_GNU_MAY_ALIAS = char8_t const*;
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 count{};
        auto const [next,status]{::fast_io::parse_by_scan(reinterpret_cast<chars>(begin), reinterpret_cast<chars>(end),
            ::fast_io::mnp::leb128_get(count))};
        if(status != ::fast_io::parse_code::ok) [[unlikely]]
        {
            // count ... end
            // unsafe (could be end); the bounded scanner established the failure without overreading.
            // ^^ err_curr
            err.err_curr = begin; err.err_code = code::wasm3_invalid_tag_count; err.err_selectable.u8 = 0u;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(status);
        }
        // count | descriptors ... end
        // [safe]  ^^ curr: successful bounded scanner publishes an address at most end.
        auto curr{reinterpret_cast<::std::byte const*>(next)};
        if(count > policy.parser_limit.max_tag_sec_entries) [[unlikely]]
        {
            // [count ... end) begin is used only as a diagnostic; allocation has not occurred.
            // ^^ err_curr
            err.err_curr = begin; err.err_code = code::exceed_the_max_parser_limit;
            err.err_selectable.exceed_the_max_parser_limit = {u8"tagsec_count", count, policy.parser_limit.max_tag_sec_entries};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module.sections).importdesc.index_unchecked(4uz)};
        if(static_cast<::std::uint_least64_t>(count) + imports.size() > 0xffff'ffffu) [[unlikely]]
        {
            // count ... end: total tag index space is checked in u64 before allocation/narrowing.
            // ^^ err_curr
            err.err_curr = begin; err.err_code = code::wasm3_invalid_tag_count; err.err_selectable.u8 = 0u;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        // Each tag needs attribute + at least one type-index byte. Bound allocation by actual input first.
        if(count > static_cast<::std::size_t>(end - curr) / 2uz) [[unlikely]]
        {
            // count | descriptors ... end: curr lies in the same section allocation, including end.
            //         ^^ err_curr
            err.err_curr = curr; err.err_code = code::wasm3_invalid_tag_count; err.err_selectable.u8 = 0u;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        section.type_indices.reserve(static_cast<::std::size_t>(count));
        for(::std::size_t i{}; i != count; ++i)
        {
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type{};
            // [checked descriptors] attribute typeidx ... end
            // [safe               ] ^^ curr may be end; decoder bounds every read before returning.
            curr = parse_core3_tag_type(curr, end, type, module, err, parameters);
            // [checked descriptors including this tag] remaining ... end
            // [safe                                  ] ^^ curr, possibly end.
            section.type_indices.push_back_unchecked(type);
        }
        if(curr != end) [[unlikely]]
        {
            // [checked descriptors] trailing bytes ... end
            // [safe               ] ^^ err_curr: trailing bytes are not read.
            err.err_curr = curr; err.err_code = code::unexpected_section_data;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        using bytes UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;
        // [begin ... end) both endpoints belong to the caller-proven section allocation; views do not dereference them.
        // ^^ sec_begin
        section.sec_span.sec_begin = reinterpret_cast<bytes>(begin);
        // [begin ... end) end is retained as an exclusive endpoint, never read.
        //            ^^ sec_end
        section.sec_span.sec_end = reinterpret_cast<bytes>(end);
        section.present = true;
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    [[nodiscard]] inline constexpr ::std::byte const* parse_extended_tag_import(
        ::std::byte const* curr, ::std::byte const* end,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32& index,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        // [descriptor ... end) the shared bounded decoder validates the same tag type as a local declaration.
        // ^^ returned cursor lies inside this section or equals its exclusive end.
        return parse_core3_tag_type(curr, end, index, module, err, parameters);
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void validate_extended_tag_export(
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 index,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module,
        ::std::byte const* diagnostic, ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& parameters) UWVM_THROWS
    {
        using code = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
        if(get_wasm1p1_parameter(parameters).disable_exceptions) [[unlikely]]
        {
            // [checked export index ... end) diagnostic is retained only for error reporting.
            // ^^ err_curr
            err.err_curr = diagnostic; err.err_code = code::wasm1p1_feature_required;
            err.err_selectable.wasm1p1_feature_required = {.value = 4u,
                .feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions,
                .subject = ::uwvm2::parser::wasm::base::wasm1p1_error_subject::tag_type};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
        auto const& tags{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<tag_section_storage_t<Fs...>>(module.sections)};
        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(module.sections).importdesc.index_unchecked(4uz)};
        auto const total{static_cast<::std::uint_least64_t>(imports.size()) + tags.type_indices.size()};
        if(index >= total) [[unlikely]]
        {
            // [checked export index ... end) compare integers before deriving any tag address.
            // ^^ err_curr
            err.err_curr = diagnostic; err.err_code = code::exported_index_exceeds_maxvul;
            err.err_selectable.exported_index_exceeds_maxvul = {.idx = index,
                .maxval = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(total), .type = 4u};
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct tag_section_details
    {
        tag_section_storage_t<Fs...> const* section{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections{};
    };
    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr tag_section_details<Fs...> section_details(tag_section_storage_t<Fs...> const& section,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(section), ::std::addressof(all_sections)}; }
    template<::std::integral Char, typename Stream, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<Char, tag_section_details<Fs...>>, Stream&& stream,
        tag_section_details<Fs...> details)
    {
        if(details.section == nullptr || details.all_sections == nullptr) { ::fast_io::fast_terminate(); }
        if(!details.section->present) { return; }
        auto const& imports{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(*details.all_sections).importdesc.index_unchecked(4uz)};
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8"\nTag["),
            ::fast_io::mnp::dec(details.section->type_indices.size()), ::fast_io::mnp::code_cvt(u8"]:\n"));
        ::std::size_t local_index{};
        // Parsing checked the complete imported+local tag index space in u64.
        // Diagnostics keep that width before addition, including ISA32 hosts.
        auto tag_index{static_cast<::std::uint_least64_t>(imports.size())};
        for(auto const type_index : details.section->type_indices)
        {
            ::fast_io::operations::print_freestanding<true>(::std::forward<Stream>(stream), ::fast_io::mnp::code_cvt(u8" - localtag["),
                ::fast_io::mnp::dec(local_index), ::fast_io::mnp::code_cvt(u8"] -> tag["), ::fast_io::mnp::dec(tag_index),
                ::fast_io::mnp::code_cvt(u8"]: {type: "), ::fast_io::mnp::dec(type_index), ::fast_io::mnp::code_cvt(::uwvm2::utils::container::u8string_view{u8"}"}));
            ++local_index;
            ++tag_index;
        }
    }

}
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    // The vector owns heap storage, with no pointer into this aggregate; byte relocation preserves ownership.
    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>
    { inline static constexpr bool value = true; };
    template<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1p1::features::tag_section_storage_t<Fs...>>
    { inline static constexpr bool value = true; };
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
