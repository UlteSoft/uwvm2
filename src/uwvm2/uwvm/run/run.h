/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <bit>
# include <charconv>
# include <cstddef>
# include <cstdint>
# include <cmath>
# include <cstring>
# include <limits>
# include <memory>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# include <uwvm2/runtime/lib/uwvm_runtime.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/runtime/impl.h>
# include "retval.h"
# include "loader.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::run
{
#if defined(UWVM_RUNTIME_HAS_BACKEND)
    inline ::std::size_t resolve_default_first_entry_function_index(::uwvm2::utils::container::u8string_view main_module_name) noexcept
    {
        using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
        using start_section_t = ::uwvm2::parser::wasm::standard::wasm1::features::start_section_storage_t;
        using external_types = ::uwvm2::parser::wasm::standard::wasm1::type::external_types;
        using imported_function_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
        using func_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;

        // We currently do not provide host arguments to the entry function; therefore, the entry must be `() -> ()`.
        // NOTE: start section in wasm1.0 requires `() -> ()` by the spec; exported fallbacks are kept consistent.
        auto const resolve_import_leaf{[](imported_function_storage_t const* imp) noexcept -> imported_function_storage_t const*
                                       {
                                           constexpr ::std::size_t kMaxChain{4096uz};
                                           for(::std::size_t steps{}; steps != kMaxChain; ++steps)
                                           {
                                               if(imp == nullptr) [[unlikely]] { return nullptr; }
                                               if(imp->link_kind != func_link_kind::imported) { return imp; }
                                               imp = imp->target.imported_ptr;
                                           }
                                           return nullptr;
                                       }};

        auto const is_void_to_void_wasm_func_index{[&](::std::size_t func_index) noexcept -> bool
                                                   {
                                                       auto const rt_it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(main_module_name)};
                                                       if(rt_it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) [[unlikely]]
                                                       {
                                                           return false;
                                                       }

                                                       auto const& rt{rt_it->second};
                                                       auto const import_n{rt.imported_function_vec_storage.size()};
                                                       auto const local_n{rt.local_defined_function_vec_storage.size()};
                                                       if(func_index >= import_n + local_n) [[unlikely]] { return false; }

                                                       auto const is_void_to_void_ft{
                                                           [](auto const& ft) constexpr noexcept -> bool
                                                           { return ft.parameter.begin == ft.parameter.end && ft.result.begin == ft.result.end; }};

                                                       if(func_index < import_n)
                                                       {
                                                           auto const imp{::std::addressof(rt.imported_function_vec_storage.index_unchecked(func_index))};
                                                           auto const leaf{resolve_import_leaf(imp)};
                                                           if(leaf == nullptr) [[unlikely]] { return false; }

                                                           // Allow imported entry only when it ultimately resolves to a wasm-defined function.
                                                           if(leaf->link_kind != func_link_kind::defined) { return false; }
                                                           auto const f{leaf->target.defined_ptr};
                                                           if(f == nullptr || f->function_type_ptr == nullptr) [[unlikely]] { return false; }
                                                           return is_void_to_void_ft(*f->function_type_ptr);
                                                       }

                                                       auto const local_idx{func_index - import_n};
                                                       auto const f{::std::addressof(rt.local_defined_function_vec_storage.index_unchecked(local_idx))};
                                                       if(f == nullptr || f->function_type_ptr == nullptr) [[unlikely]] { return false; }
                                                       return is_void_to_void_ft(*f->function_type_ptr);
                                                   }};

        // Prefer start section when present.
        auto const all_module_it{::uwvm2::uwvm::wasm::storage::all_module.find(main_module_name)};
        if(all_module_it != ::uwvm2::uwvm::wasm::storage::all_module.end())
        {
            auto const& am{all_module_it->second};
            if(am.type == module_type_t::exec_wasm || am.type == module_type_t::preloaded_wasm)
            {
                auto const wf{am.module_storage_ptr.wf};
                if(wf != nullptr && wf->binfmt_ver == 1u)
                {
                    auto const& mod{wf->wasm_module_storage.wasm_binfmt_ver1_storage};
                    auto const& startsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<start_section_t>(mod.sections)};

                    // Note: do not subtract pointers here; the default (absent) span is {nullptr, nullptr} and pointer
                    // subtraction would be UB. `sec_begin != nullptr` is the parser's "section present" flag.
                    if(startsec.sec_span.sec_begin != nullptr)
                    {
                        if(auto const rt_it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(main_module_name)};
                           rt_it != ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end())
                        {
                            auto const import_n{rt_it->second.imported_function_vec_storage.size()};
                            auto const idx{static_cast<::std::size_t>(startsec.start_idx)};
                            auto const total_n{import_n + rt_it->second.local_defined_function_vec_storage.size()};
                            if(idx < total_n && is_void_to_void_wasm_func_index(idx)) { return idx; }
                        }
                    }
                }

                static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
            }
        }

        // Otherwise, fall back to exported entrypoints.
        ::std::size_t idx{};
        auto const mit{::uwvm2::uwvm::wasm::storage::all_module_export.find(main_module_name)};
        if(mit != ::uwvm2::uwvm::wasm::storage::all_module_export.end())
        {
            auto const try_export{[&](::uwvm2::utils::container::u8string_view export_name) noexcept -> bool
                                  {
                                      auto const eit{mit->second.find(export_name)};
                                      if(eit == mit->second.end()) { return false; }

                                      auto const& ex{eit->second};
                                      if(ex.type != module_type_t::exec_wasm && ex.type != module_type_t::preloaded_wasm) { return false; }

                                      auto const exp{ex.storage.wasm_file_export_storage_ptr.storage.wasm_binfmt_ver1_export_storage_ptr};
                                      if(exp == nullptr || exp->type != external_types::func) { return false; }

                                      auto const resolved{static_cast<::std::size_t>(exp->storage.func_idx)};
                                      auto const rt_it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(main_module_name)};
                                      if(rt_it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) [[unlikely]] { return false; }
                                      auto const import_n{rt_it->second.imported_function_vec_storage.size()};
                                      auto const total_n{import_n + rt_it->second.local_defined_function_vec_storage.size()};
                                      if(resolved >= total_n) { return false; }
                                      if(!is_void_to_void_wasm_func_index(resolved)) { return false; }

                                      idx = resolved;
                                      return true;
                                  }};

            if(try_export(::uwvm2::utils::container::u8string_view{u8"_start"})) { return idx; }
            if(try_export(::uwvm2::utils::container::u8string_view{u8"main"})) { return idx; }
        }

        // Fallback: if `all_module_export` is missing/stale, resolve from the parsed export section directly.
        // This avoids relying on a separately-constructed export map for entrypoint resolution.
        if(all_module_it != ::uwvm2::uwvm::wasm::storage::all_module.end())
        {
            auto const& am{all_module_it->second};
            if(am.type == module_type_t::exec_wasm || am.type == module_type_t::preloaded_wasm)
            {
                auto const wf{am.module_storage_ptr.wf};
                if(wf != nullptr && wf->binfmt_ver == 1u)
                {
                    auto const& mod{wf->wasm_module_storage.wasm_binfmt_ver1_storage};

                    auto get_exportsec_from_feature_tuple{[&mod]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(
                                                              ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept -> decltype(auto)
                                                          {
                                                              return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                                                                  ::uwvm2::parser::wasm::standard::wasm1::features::export_section_storage_t<Fs...>>(
                                                                  mod.sections);
                                                          }};

                    auto const& exportsec{get_exportsec_from_feature_tuple(::uwvm2::uwvm::wasm::feature::all_features)};
                    if(exportsec.sec_span.sec_begin != nullptr)
                    {
                        auto const try_export_from_section{[&](::uwvm2::utils::container::u8string_view export_name) noexcept -> bool
                                                           {
                                                               for(auto const& e: exportsec.exports)
                                                               {
                                                                   if(e.export_name != export_name) { continue; }
                                                                   if(e.exports.type != external_types::func) { return false; }

                                                                   auto const resolved{static_cast<::std::size_t>(e.exports.storage.func_idx)};
                                                                   if(!is_void_to_void_wasm_func_index(resolved)) { return false; }
                                                                   idx = resolved;
                                                                   return true;
                                                               }
                                                               return false;
                                                           }};

                        if(try_export_from_section(::uwvm2::utils::container::u8string_view{u8"_start"})) { return idx; }
                        if(try_export_from_section(::uwvm2::utils::container::u8string_view{u8"main"})) { return idx; }
                    }
                }

                static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
            }
        }

        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"Cannot resolve entry function for module=\"",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            main_module_name,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"\": expected start section or exported function \"_start\"/\"main\" with signature () -> ().\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        ::fast_io::fast_terminate();
    }

    struct runtime_entry_invocation
    {
        ::std::size_t function_index{};
        ::uwvm2::utils::container::vector<::std::byte> param_buffer{};
        ::uwvm2::utils::container::vector<::std::byte> result_buffer{};
    };

    [[nodiscard]] inline constexpr ::std::uint_least8_t wasm_entry_type_code(auto type) noexcept
    { return static_cast<::std::uint_least8_t>(type); }

    [[nodiscard]] inline constexpr ::std::size_t wasm_entry_scalar_abi_size(::std::uint_least8_t code) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        switch(static_cast<wasm_value_type>(code))
        {
            case wasm_value_type::i32: return 4uz;
            case wasm_value_type::i64: return 8uz;
            case wasm_value_type::f32: return 4uz;
            case wasm_value_type::f64: return 8uz;
            default: return 0uz;
        }
    }

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view wasm_entry_type_name(::std::uint_least8_t code) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        switch(static_cast<wasm_value_type>(code))
        {
            case wasm_value_type::i32: return {u8"i32"};
            case wasm_value_type::i64: return {u8"i64"};
            case wasm_value_type::f32: return {u8"f32"};
            case wasm_value_type::f64: return {u8"f64"};
            default:
            {
                if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)) { return {u8"v128"}; }
                if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref)) { return {u8"funcref"}; }
                if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref)) { return {u8"externref"}; }
                return {u8"unknown"};
            }
        }
    }

    template <typename ScanManipulator>
    [[nodiscard]] inline constexpr bool wasm_entry_scan_exact(char8_t const* first, char8_t const* last, ScanManipulator scan_manipulator) noexcept
    {
        auto const [next, err]{::fast_io::parse_by_scan(first, last, scan_manipulator)};
        return err == ::fast_io::parse_code::ok && next == last;
    }

    template <bool allow_signed_decimal, typename Out, typename Unsigned>
    [[nodiscard]] inline constexpr bool parse_wasm_entry_integer(::uwvm2::utils::container::u8string_view str, Out& out) noexcept
    {
        static_assert(sizeof(Out) == sizeof(Unsigned));
        auto const first{str.cbegin()};
        auto const last{str.cend()};
        if(first == last) [[unlikely]] { return false; }

        if constexpr(allow_signed_decimal)
        {
            Out signed_value; // no init necessary
            if(wasm_entry_scan_exact(first, last, ::fast_io::mnp::dec_get<true, false>(signed_value)))
            {
                out = signed_value;
                return true;
            }
        }

        Unsigned unsigned_value; // no init necessary
        if(wasm_entry_scan_exact(first, last, ::fast_io::mnp::dec_get<true, false>(unsigned_value)) ||
           wasm_entry_scan_exact(first, last, ::fast_io::mnp::hex_get<true, false, true>(unsigned_value)) ||
           wasm_entry_scan_exact(first, last, ::fast_io::mnp::bin_get<true, false, true>(unsigned_value)) ||
           wasm_entry_scan_exact(first, last, ::fast_io::mnp::oct_get<true, false, true, true>(unsigned_value)))
        {
            out = ::std::bit_cast<Out>(unsigned_value);
            return true;
        }

        return false;
    }

    template <typename... Args>
    [[noreturn]] inline void wasm_set_start_func_fatal(Args&&... args) noexcept
    {
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            ::std::forward<Args>(args)...,
                            u8"\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        ::fast_io::fast_terminate();
    }

    [[nodiscard]] inline constexpr bool parse_wasm_entry_i32(::uwvm2::utils::container::u8string_view str,
                                                             ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32& out) noexcept
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
        static_assert(sizeof(wasm_i32) == 4uz && sizeof(wasm_u32) == 4uz);
        return parse_wasm_entry_integer<true, wasm_i32, wasm_u32>(str, out);
    }

    [[nodiscard]] inline constexpr bool parse_wasm_entry_i64(::uwvm2::utils::container::u8string_view str,
                                                             ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64& out) noexcept
    {
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
        static_assert(sizeof(wasm_i64) == 8uz && sizeof(wasm_u64) == 8uz);
        return parse_wasm_entry_integer<true, wasm_i64, wasm_u64>(str, out);
    }

    [[nodiscard]] inline constexpr bool wasm_entry_hex_digit(char8_t ch, ::std::uint_least8_t& value) noexcept
    {
        ::std::uint_least8_t parsed{};
        auto const first{::std::addressof(ch)};
        auto const [next, err]{::fast_io::parse_by_scan(first, first + 1u, ::fast_io::mnp::hex_get<true, false, false>(parsed))};
        if(err != ::fast_io::parse_code::ok || next != first + 1u) { return false; }
        value = parsed;
        return true;
    }

    // Fallback for exact C-style hexadecimal floating literals, e.g. 0x1.8p0.
    template <typename T>
    [[nodiscard]] inline bool parse_wasm_entry_hexfloat_range(char8_t const* first, char8_t const* last, T& out) noexcept
    {
        if(first == last) { return false; }
        if(*first == u8'-' || *first == u8'+') { return false; }

        if(last - first < 2 || first[0] != u8'0' || (first[1] != u8'x' && first[1] != u8'X')) { return false; }
        first += 2;

        long double significand{};
        bool has_digit{};
        ::std::int_least64_t fractional_hex_digits{};

        while(first != last)
        {
            ::std::uint_least8_t digit{};
            if(!wasm_entry_hex_digit(*first, digit)) { break; }
            significand = significand * 16.0L + static_cast<long double>(digit);
            has_digit = true;
            ++first;
        }

        if(first != last && *first == u8'.')
        {
            ++first;
            while(first != last)
            {
                ::std::uint_least8_t digit{};
                if(!wasm_entry_hex_digit(*first, digit)) { break; }
                significand = significand * 16.0L + static_cast<long double>(digit);
                has_digit = true;
                ++fractional_hex_digits;
                ++first;
            }
        }

        ::std::int_least64_t exponent{};
        if(!has_digit || first == last || (*first != u8'p' && *first != u8'P')) { return false; }
        ++first;
        if(first == last) { return false; }
        if(*first == u8'+')
        {
            ++first;
            if(first == last) { return false; }
        }

        auto const [next, err]{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, false>(exponent))};
        if(err != ::fast_io::parse_code::ok || next != last) { return false; }

        auto const adjusted_exponent{exponent - fractional_hex_digits * 4};
        if(adjusted_exponent > static_cast<::std::int_least64_t>((::std::numeric_limits<int>::max)()))
        {
            out = (::std::numeric_limits<T>::infinity)();
            return true;
        }
        if(adjusted_exponent < static_cast<::std::int_least64_t>((::std::numeric_limits<int>::min)()))
        {
            out = static_cast<T>(0.0);
            return true;
        }

        auto const scaled{::std::ldexp(significand, static_cast<int>(adjusted_exponent))};
        out = static_cast<T>(scaled);
        return true;
    }

    template <typename T>
    [[nodiscard]] inline bool parse_wasm_entry_float_range(char8_t const* first, char8_t const* last, T& out) noexcept
    {
        if(first == last) { return false; }
#if defined(FAST_IO_NOT_USE_FAST_FLOAT)
        auto const char_first{reinterpret_cast<char const*>(first)};
        auto const char_last{reinterpret_cast<char const*>(last)};
        auto const [next, err]{::std::from_chars(char_first, char_last, out, ::std::chars_format::general)};
        if(err == ::std::errc{} && next == char_last) { return true; }
#else
        auto const [next, err]{::fast_io::parse_by_scan(first, last, out)};
        if(err == ::fast_io::parse_code::ok && next == last) { return true; }
#endif
        return parse_wasm_entry_hexfloat_range(first, last, out);
    }

    template <typename T>
    [[nodiscard]] inline bool parse_wasm_entry_float(::uwvm2::utils::container::u8string_view str, T& out) noexcept
    {
        auto first{str.cbegin()};
        auto last{str.cend()};
        if(parse_wasm_entry_float_range(first, last, out)) { return true; }
        if(first != last && (last[-1] == u8'f' || last[-1] == u8'F'))
        {
            --last;
            if(parse_wasm_entry_float_range(first, last, out)) { return true; }
        }
        return false;
    }

    [[nodiscard]] inline ::uwvm2::utils::container::u8string_view wasm_entry_input_literal_type(
        ::uwvm2::utils::container::u8string_view str) noexcept
    {
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 i32_value{};
        if(parse_wasm_entry_i32(str, i32_value)) { return {u8"i32/i64 literal"}; }

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 i64_value{};
        if(parse_wasm_entry_i64(str, i64_value)) { return {u8"i64 literal"}; }

        float f32_value{};
        if(parse_wasm_entry_float(str, f32_value)) { return {u8"f32/f64 literal"}; }

        double f64_value{};
        if(parse_wasm_entry_float(str, f64_value)) { return {u8"f64 literal"}; }

        return {u8"unknown literal"};
    }

    template <typename Output, typename ValueTypePtr>
    inline void print_wasm_entry_type_span(Output& output, ValueTypePtr begin, ValueTypePtr end) noexcept
    {
        ::fast_io::io::perr(output, u8"(");
        bool first{true};
        for(auto curr{begin}; curr != end; ++curr)
        {
            if(!first) { ::fast_io::io::perr(output, u8", "); }
            first = false;
            ::fast_io::io::perr(output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                wasm_entry_type_name(wasm_entry_type_code(*curr)),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE));
        }
        ::fast_io::io::perr(output, u8")");
    }

    template <typename Output>
    inline void print_wasm_entry_info_prefix(Output& output) noexcept
    {
        ::fast_io::io::perr(output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                            u8"[info]  ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE));
    }

    template <typename FunctionType, typename Tokens>
    [[noreturn]] inline void wasm_set_start_func_arity_mismatch_fatal(::std::uint32_t local_function_index,
                                                                      FunctionType const& ft,
                                                                      Tokens const& argument_tokens) noexcept
    {
        auto const param_count{static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)};
        // Emit this multi-part diagnostic under one stream lock, then write through the unlocked stream reference.
        auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
        auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

        ::fast_io::io::perr(u8log_output_ul,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"--wasm-set-start-func argument count mismatch for local function ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            local_function_index,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8": expected ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            param_count,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" argument(s), got ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            argument_tokens.size(),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8".\n");
        print_wasm_entry_info_prefix(u8log_output_ul);
        ::fast_io::io::perr(u8log_output_ul, u8"function type: ");
        print_wasm_entry_type_span(u8log_output_ul, ft.parameter.begin, ft.parameter.end);
        ::fast_io::io::perr(u8log_output_ul, u8" -> ");
        print_wasm_entry_type_span(u8log_output_ul, ft.result.begin, ft.result.end);
        ::fast_io::io::perr(u8log_output_ul, u8"\n");
        print_wasm_entry_info_prefix(u8log_output_ul);
        ::fast_io::io::perr(u8log_output_ul, u8"input types:   (");
        for(::std::size_t i{}; i != argument_tokens.size(); ++i)
        {
            if(i != 0uz) { ::fast_io::io::perr(u8log_output_ul, u8", "); }
            // The loop bound is exactly `argument_tokens.size()`, so this unchecked lookup is range-proven here.
            auto const arg{argument_tokens.index_unchecked(i)};
            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                wasm_entry_input_literal_type(arg),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"\"",
                                arg,
                                u8"\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE));
        }
        ::fast_io::io::perr(u8log_output_ul,
                            u8")\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        ::fast_io::fast_terminate();
    }

    template <typename T>
    inline void write_wasm_entry_value(::uwvm2::utils::container::vector<::std::byte>& buffer, ::std::size_t& offset, T const& value) noexcept
    {
        static_assert(::std::is_trivially_copyable_v<T>);
        if(offset > buffer.size() || sizeof(T) > buffer.size() - offset) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::std::memcpy(buffer.data() + offset, ::std::addressof(value), sizeof(T));
        offset += sizeof(T);
    }

    template <typename T>
    [[nodiscard]] inline T read_wasm_entry_value(::uwvm2::utils::container::vector<::std::byte> const& buffer, ::std::size_t& offset) noexcept
    {
        static_assert(::std::is_trivially_copyable_v<T>);
        if(offset > buffer.size() || sizeof(T) > buffer.size() - offset) [[unlikely]] { ::fast_io::fast_terminate(); }
        T value{};
        ::std::memcpy(::std::addressof(value), buffer.data() + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }

    inline void pack_wasm_entry_argument(::uwvm2::utils::container::vector<::std::byte>& buffer,
                                         ::std::size_t& offset,
                                         ::uwvm2::utils::container::u8string_view arg,
                                         ::std::uint_least8_t type_code,
                                         ::std::size_t arg_index) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        switch(static_cast<wasm_value_type>(type_code))
        {
            case wasm_value_type::i32:
            {
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 value{};
                if(!parse_wasm_entry_i32(arg, value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected i32, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            case wasm_value_type::i64:
            {
                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 value{};
                if(!parse_wasm_entry_i64(arg, value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected i64, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            case wasm_value_type::f32:
            {
                float value{};
                if(!parse_wasm_entry_float(arg, value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected f32, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            case wasm_value_type::f64:
            {
                double value{};
                if(!parse_wasm_entry_float(arg, value)) [[unlikely]]
                {
                    wasm_set_start_func_fatal(u8"Invalid argument #", arg_index, u8" for --wasm-set-start-func: expected f64, got \"", arg, u8"\".");
                }
                write_wasm_entry_value(buffer, offset, value);
                break;
            }
            default:
            {
                wasm_set_start_func_fatal(u8"Unsupported --wasm-set-start-func parameter type: ",
                                          wasm_entry_type_name(type_code),
                                          u8". Only i32, i64, f32, and f64 are currently supported.");
            }
        }
    }

    template <typename Output, typename Signed, typename Unsigned>
    inline void print_wasm_entry_integer_formats(Output& output, Signed value) noexcept
    {
        static_assert(sizeof(Signed) == sizeof(Unsigned));
        auto const bits{::std::bit_cast<Unsigned>(value)};
        ::fast_io::io::perr(output,
                            u8"bin=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::bin<true>(bits),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8", oct=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::oct<true, false, true>(bits),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8", sdec=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::dec(value),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8", udec=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::dec(bits),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8", hex=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            ::fast_io::mnp::hex0x(bits),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE));
    }

    template <typename Output>
    inline void print_wasm_entry_argument_verbose(Output& output,
                                                  ::uwvm2::utils::container::vector<::std::byte> const& buffer,
                                                  ::std::size_t& offset,
                                                  ::uwvm2::utils::container::u8string_view arg,
                                                  ::std::uint_least8_t type_code,
                                                  ::std::size_t arg_index) noexcept
    {
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

        ::fast_io::io::perr(output,
                            u8"#",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            arg_index,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            wasm_entry_type_name(type_code),
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" input=\"",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                            arg,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"\" => ");

        switch(static_cast<wasm_value_type>(type_code))
        {
            case wasm_value_type::i32:
            {
                using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
                using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
                auto const value{read_wasm_entry_value<wasm_i32>(buffer, offset)};
                print_wasm_entry_integer_formats<Output, wasm_i32, wasm_u32>(output, value);
                break;
            }
            case wasm_value_type::i64:
            {
                using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
                using wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
                auto const value{read_wasm_entry_value<wasm_i64>(buffer, offset)};
                print_wasm_entry_integer_formats<Output, wasm_i64, wasm_u64>(output, value);
                break;
            }
            case wasm_value_type::f32:
            {
                auto const value{read_wasm_entry_value<float>(buffer, offset)};
                ::std::uint32_t bits{};
                ::std::memcpy(::std::addressof(bits), ::std::addressof(value), sizeof(bits));
                ::fast_io::io::perr(output,
                                    u8"value=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::general(value),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8", hexfloat=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hexfloat(value),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8", bitfloat(hex)=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hex0x</*full=*/true>(bits),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE));
                break;
            }
            case wasm_value_type::f64:
            {
                auto const value{read_wasm_entry_value<double>(buffer, offset)};
                ::std::uint64_t bits{};
                ::std::memcpy(::std::addressof(bits), ::std::addressof(value), sizeof(bits));
                ::fast_io::io::perr(output,
                                    u8"value=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::general(value),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8", hexfloat=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hexfloat(value),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8", bitfloat(hex)=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    ::fast_io::mnp::hex0x</*full=*/true>(bits),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE));
                break;
            }
            default: ::fast_io::fast_terminate();
        }

        ::fast_io::io::perr(output, u8"\n");
    }

    template <typename FunctionType, typename Tokens>
    inline void print_wasm_set_start_func_verbose(::std::uint32_t local_function_index,
                                                  ::std::size_t function_index,
                                                  ::std::size_t import_count,
                                                  FunctionType const& ft,
                                                  Tokens const& argument_tokens,
                                                  ::uwvm2::utils::container::vector<::std::byte> const& param_buffer) noexcept
    {
        constexpr ::uwvm2::utils::container::u8string_view body_indent{u8"              "};
        constexpr ::uwvm2::utils::container::u8string_view argument_indent{u8"                "};

        // Emit the multi-line verbose record as one locked output unit.
        auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
        ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
            ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
        auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

        ::fast_io::io::perr(u8log_output_ul,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                            u8"[info]  ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"--wasm-set-start-func resolved.\n",
                            body_indent,
                            u8"local-defined func index: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            local_function_index,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"\n",
                            body_indent,
                            u8"wasm func index:          ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            function_index,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8" (import-inclusive, import-count=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                            import_count,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8")\n",
                            body_indent,
                            u8"function type:            ");
        print_wasm_entry_type_span(u8log_output_ul, ft.parameter.begin, ft.parameter.end);
        ::fast_io::io::perr(u8log_output_ul, u8" -> ");
        print_wasm_entry_type_span(u8log_output_ul, ft.result.begin, ft.result.end);
        ::fast_io::io::perr(u8log_output_ul, u8"\n", body_indent, u8"arguments:\n");

        if(argument_tokens.empty())
        {
            ::fast_io::io::perr(u8log_output_ul, argument_indent, u8"<none>\n");
        }
        else
        {
            ::std::size_t offset{};
            for(::std::size_t i{}; i != argument_tokens.size(); ++i)
            {
                // The caller already checked arity, so this token and parameter-type lookup are range-proven.
                ::fast_io::io::perr(u8log_output_ul, argument_indent);
                print_wasm_entry_argument_verbose(u8log_output_ul,
                                                  param_buffer,
                                                  offset,
                                                  argument_tokens.index_unchecked(i),
                                                  wasm_entry_type_code(ft.parameter.begin[i]),
                                                  i);
            }
            if(offset != param_buffer.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        }

        ::fast_io::io::perr(u8log_output_ul,
                            body_indent,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                            u8"[",
                            ::uwvm2::uwvm::io::get_local_realtime(),
                            u8"] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                            u8"(verbose)\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
    }

    template <typename ValueTypePtr>
    [[nodiscard]] inline ::std::size_t calculate_wasm_entry_abi_bytes(ValueTypePtr begin, ValueTypePtr end, bool is_result) noexcept
    {
        ::std::size_t total{};
        for(auto curr{begin}; curr != end; ++curr)
        {
            auto const type_code{wasm_entry_type_code(*curr)};
            auto const size{wasm_entry_scalar_abi_size(type_code)};
            if(size == 0uz) [[unlikely]]
            {
                auto const kind{is_result ? ::uwvm2::utils::container::u8string_view{u8"result"}
                                          : ::uwvm2::utils::container::u8string_view{u8"parameter"}};
                wasm_set_start_func_fatal(u8"Unsupported --wasm-set-start-func ",
                                          kind,
                                          u8" type: ",
                                          wasm_entry_type_name(type_code),
                                          u8". Only i32, i64, f32, and f64 are currently supported.");
            }
            if(total > (::std::numeric_limits<::std::size_t>::max() - size)) [[unlikely]] { ::fast_io::fast_terminate(); }
            total += size;
        }
        return total;
    }

    inline void configure_runtime_entry_buffers(auto& cfg, runtime_entry_invocation& entry) noexcept
    {
        cfg.entry_function_index = entry.function_index;
        cfg.entry_abi_buffers.param_buffer = entry.param_buffer.empty() ? nullptr : entry.param_buffer.data();
        cfg.entry_abi_buffers.param_bytes = entry.param_buffer.size();
        cfg.entry_abi_buffers.result_buffer = entry.result_buffer.empty() ? nullptr : entry.result_buffer.data();
        cfg.entry_abi_buffers.result_bytes = entry.result_buffer.size();
    }

    inline runtime_entry_invocation resolve_runtime_entry_invocation(::uwvm2::utils::container::u8string_view main_module_name) noexcept
    {
        runtime_entry_invocation entry{};
        auto const& requested{::uwvm2::uwvm::wasm::storage::start_func_call};
        if(!requested.enabled)
        {
            entry.function_index = resolve_default_first_entry_function_index(main_module_name);
            return entry;
        }

        auto const rt_it{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(main_module_name)};
        if(rt_it == ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& rt{rt_it->second};
        auto const local_index{static_cast<::std::size_t>(requested.local_function_index)};
        auto const local_n{rt.local_defined_function_vec_storage.size()};
        if(local_index >= local_n) [[unlikely]]
        {
            wasm_set_start_func_fatal(u8"--wasm-set-start-func selected local function index ",
                                      requested.local_function_index,
                                      u8", but the main module only has ",
                                      local_n,
                                      u8" local defined functions. The index is local-defined, not import-inclusive.");
        }

        auto const& runtime_func{rt.local_defined_function_vec_storage.index_unchecked(local_index)};
        auto const ft{runtime_func.function_type_ptr};
        if(ft == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const param_count{static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)};
        if(requested.argument_tokens.size() != param_count) [[unlikely]]
        {
            wasm_set_start_func_arity_mismatch_fatal(requested.local_function_index, *ft, requested.argument_tokens);
        }

        auto const param_bytes{calculate_wasm_entry_abi_bytes(ft->parameter.begin, ft->parameter.end, false)};
        auto const result_bytes{calculate_wasm_entry_abi_bytes(ft->result.begin, ft->result.end, true)};

        auto const import_count{rt.imported_function_vec_storage.size()};
        entry.function_index = import_count + local_index;
        entry.param_buffer.resize(param_bytes);
        entry.result_buffer.resize(result_bytes);

        ::std::size_t offset{};
        for(::std::size_t i{}; i != param_count; ++i)
        {
            // The arity check above proves `argument_tokens.size() == param_count`; this unchecked lookup is bounded by the loop.
            pack_wasm_entry_argument(entry.param_buffer, offset, requested.argument_tokens.index_unchecked(i), wasm_entry_type_code(ft->parameter.begin[i]), i);
        }
        if(offset != param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            print_wasm_set_start_func_verbose(requested.local_function_index,
                                              entry.function_index,
                                              import_count,
                                              *ft,
                                              requested.argument_tokens,
                                              entry.param_buffer);
        }

        return entry;
    }

    inline constexpr ::std::size_t calculate_default_runtime_compile_threads(::std::size_t max_compile_threads) noexcept
    {
        // Follow the default policy upper bound: roughly one compiler thread per log2(N) CPUs.
        ::std::size_t compiler_threads{};
        while(max_compile_threads > 1uz)
        {
            max_compile_threads >>= 1u;
            ++compiler_threads;
        }
        return compiler_threads == 0uz ? 1uz : compiler_threads;
    }

    inline constexpr ::std::size_t calculate_aggressive_runtime_compile_threads(::std::size_t max_compile_threads) noexcept
    {
        if(max_compile_threads < 2uz) [[unlikely]] { return 0uz; }

        // Prevent overflow
        auto const quotient{max_compile_threads / 3uz};
        auto const remainder{max_compile_threads % 3uz};
        return quotient * 2uz + (remainder * 2uz) / 3uz;
    }

    inline ::std::size_t resolve_runtime_compile_threads() noexcept
    {
        using runtime_compile_threads_type = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_type;
        using runtime_compile_threads_unsigned_type = ::std::make_unsigned_t<runtime_compile_threads_type>;
        using runtime_compile_threads_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_policy_t;

        constexpr auto runtime_compile_threads_warn{
            []<typename... Args>(Args&&... args) constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    ::std::forward<Args>(args)...,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8" (runtime-compile-threads)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }};

        constexpr auto runtime_compile_threads_warn_to_fatal{
            []() constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Convert warnings to fatal errors. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(runtime-compile-threads)\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }};

        constexpr auto runtime_compile_threads_fatal{
            []<typename... Args>(Args&&... args) constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    ::std::forward<Args>(args)...,
                                    u8"\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }};

        constexpr auto runtime_compile_threads_verbose_info{
            []<typename... Args>(Args&&... args) constexpr noexcept
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    ::std::forward<Args>(args)...,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    u8"[",
                                    ::uwvm2::uwvm::io::get_local_realtime(),
                                    u8"] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }};

# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
        ::std::size_t max_compile_threads{::uwvm2::utils::thread::hardware_concurrency()};
        if(max_compile_threads == 0uz) [[unlikely]] { max_compile_threads = 1uz; }
        auto const default_compile_threads{calculate_default_runtime_compile_threads(max_compile_threads)};
        auto const aggressive_compile_threads{calculate_aggressive_runtime_compile_threads(max_compile_threads)};
# else
        constexpr ::std::size_t max_compile_threads{1uz};
# endif

        ::std::size_t resolved_compile_threads{
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            default_compile_threads
# endif
        };

        if(::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed)
        {
            auto const requested_compile_threads_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy};

            if(requested_compile_threads_policy == runtime_compile_threads_policy_t::default_policy)
            {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                resolved_compile_threads = default_compile_threads;
# else
                resolved_compile_threads = 0uz;
# endif
            }
            else if(requested_compile_threads_policy == runtime_compile_threads_policy_t::aggressive)
            {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                resolved_compile_threads = aggressive_compile_threads;
# else
                resolved_compile_threads = 0uz;
                if(::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                {
                    runtime_compile_threads_warn(u8"Requested runtime compile thread policy \"",
                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                 u8"aggressive",
                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                 u8"\", but this platform does not provide fast_io::native_thread. Falling back to 0 extra compile threads.");

                    if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                }
# endif
            }
            else
            {
                auto const requested_compile_threads{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads};
                if(requested_compile_threads >= 0)
                {
                    resolved_compile_threads = 0uz;

# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                    resolved_compile_threads = static_cast<::std::size_t>(requested_compile_threads);
                    if(resolved_compile_threads > max_compile_threads && ::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                    {
                        runtime_compile_threads_warn(u8"Requested runtime compile thread count (requested=",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                     resolved_compile_threads,
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8", detected-max=",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                     max_compile_threads,
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8"). The requested value will still be used, but performance may be suboptimal.");

                        if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                    }
# else
                    if(requested_compile_threads != 0 && ::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                    {
                        runtime_compile_threads_warn(
                            u8"Requested runtime compile thread count (requested=",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                            requested_compile_threads,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"), but this platform does not provide fast_io::native_thread. Falling back to 0 extra compile threads.");

                        if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                    }
# endif
                }
                else
                {
                    auto const requested_compile_threads_abs{static_cast<runtime_compile_threads_unsigned_type>(0u) -
                                                             static_cast<runtime_compile_threads_unsigned_type>(requested_compile_threads)};

                    if(requested_compile_threads_abs > static_cast<runtime_compile_threads_unsigned_type>(max_compile_threads)) [[unlikely]]
                    {
                        runtime_compile_threads_fatal(u8"Invalid negative runtime compile thread count (requested=",
                                                      ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                      requested_compile_threads,
                                                      ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                      u8", detected-max=",
                                                      ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                      max_compile_threads,
                                                      ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                      u8"): the absolute value of a negative setting must not exceed the detected max thread count.");
                    }

                    resolved_compile_threads = max_compile_threads - static_cast<::std::size_t>(requested_compile_threads_abs);
                }
            }
        }

        ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved = resolved_compile_threads;

        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
        {
            if(::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed)
            {
                auto const requested_compile_threads_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy};

                if(requested_compile_threads_policy == runtime_compile_threads_policy_t::default_policy)
                {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                    runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         u8"default",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8", detected-max=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         max_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8", per-module full-compile scheduling may adapt below this upper bound). ");
# else
                    runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         u8"default",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8", fast_io::native_thread unavailable on this platform). ");
# endif
                }
                else if(requested_compile_threads_policy == runtime_compile_threads_policy_t::aggressive)
                {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                    runtime_compile_threads_verbose_info(
                        u8"Runtime compile thread upper bound resolved to ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        resolved_compile_threads,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8" (requested=",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        u8"aggressive",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8", detected-max=",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        max_compile_threads,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8", aggressive-policy=floor(max*2/3), per-module full-compile scheduling may adapt below this upper bound). ");
# else
                    runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         u8"aggressive",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8", fast_io::native_thread unavailable on this platform). ");
# endif
                }
                else
                {
                    runtime_compile_threads_verbose_info(u8"Runtime compile threads resolved to ",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         resolved_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" (requested=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8", detected-max=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         max_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8"). ");
                }
            }
            else
            {
# ifdef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
                runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                     resolved_compile_threads,
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8" by the default policy (detected-max=",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                     max_compile_threads,
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8", per-module full-compile scheduling may adapt below this upper bound). ");
# else
                runtime_compile_threads_verbose_info(u8"Runtime compile thread upper bound resolved to ",
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                     resolved_compile_threads,
                                                     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                     u8" by the default policy (fast_io::native_thread unavailable on this platform). ");
# endif
            }
        }

        return resolved_compile_threads;
    }
#endif

    inline int run() noexcept
    {
        // already preload wasm module

        // already bind dl

        // load main module
        if(auto const ret{::uwvm2::uwvm::run::load_exec_wasm_module()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // load local modules
        if(auto const ret{::uwvm2::uwvm::run::load_local_modules()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // load weak symbol modules
        if(auto const ret{::uwvm2::uwvm::run::load_weak_symbol_modules()}; ret != static_cast<int>(::uwvm2::uwvm::run::retval::ok)) [[unlikely]] { return ret; }

        // check duplicate module and construct ::uwvm2::uwvm::wasm::storage::all_module
        if(auto const ret{::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module()};
           ret != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) [[unlikely]]
        {
            return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
        }

        // section details occurs before dependency checks
        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_mode)
        {
            case ::uwvm2::uwvm::wasm::base::mode::section_details:
            {
                // All modules loaded
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Start printing section details. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }

                ::uwvm2::uwvm::wasm::section_detail::print_section_details();

                // Return directly
                return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
            }
            case ::uwvm2::uwvm::wasm::base::mode::validation:
            {
                // Validate all wasm code

                // Runtime initialization is not performed; only validity checks are conducted using the parser's built-in validation, not the runtime
                // validation with compilation and partitioning capabilities.

                // validate_all_wasm_code has verbose message, no necessary to print again.
                if(!::uwvm2::uwvm::runtime::validator::validate_all_wasm_code()) [[unlikely]]
                {
                    return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
                }

                // Return directly
                return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
            }
            default:
            {
                break;
            }
        }

#if !defined(UWVM_RUNTIME_HAS_BACKEND)
        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"This build was configured without executable runtime backends. Only validation and section details are available.\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
#else
        // run vm

        // check import exist and detect cycles
        if(auto const ret{::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles()};
           ret != ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok) [[unlikely]]
        {
            return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
        }

        // initialize runtime
        ::uwvm2::uwvm::runtime::initializer::initialize_runtime();

# if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter &&
           ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode != ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile) [[unlikely]]
        {
            if(::uwvm2::uwvm::io::show_runtime_warning)
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Debug interpreter requires full compile; forcing full compile.",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8" (runtime)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::runtime_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(runtime)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile;
        }
# endif

        resolve_runtime_compile_threads();
        auto runtime_entry{resolve_runtime_entry_invocation(::uwvm2::uwvm::wasm::storage::execute_wasm.module_name)};

        // run vm
        switch(::uwvm2::uwvm::wasm::storage::execute_wasm_mode)
        {
            case ::uwvm2::uwvm::wasm::base::mode::section_details: [[fallthrough]];
            case ::uwvm2::uwvm::wasm::base::mode::validation:
            {
                /// this is an vm bug
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                ::std::unreachable();
            }
            case ::uwvm2::uwvm::wasm::base::mode::run:
            {
                switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode)
                {
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile:
                    {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
                        bool lazy_backend_supported{};
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_LLVM_JIT)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
                        if(!lazy_backend_supported) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"Lazy compilation currently supports the uwvm-int, llvm-jit, and tiered backends (-Rcc int|jit|tiered). ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                                u8"(runtime)\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        ::uwvm2::runtime::lib::lazy_compile_run_config cfg{};
                        configure_runtime_entry_buffers(cfg, runtime_entry);
                        cfg.assume_full_code_verified = false;
                        ::uwvm2::runtime::lib::lazy_compile_and_run_main_module(::uwvm2::uwvm::wasm::storage::execute_wasm.module_name, cfg);
# else
                        ::fast_io::io::perr(
                            ::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"Lazy compilation is not currently supported. The current VM only supports full compile with int or jit (-Rcm full -Rcc int|jit). ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                            u8"(runtime)\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
# endif

                        break;
                    }
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile_with_full_code_verification:
                    {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
                        bool lazy_backend_supported{};
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_LLVM_JIT)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                        if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                           ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered)
                        {
                            lazy_backend_supported = true;
                        }
#  endif
                        if(!lazy_backend_supported) [[unlikely]]
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"Lazy compilation currently supports the uwvm-int, llvm-jit, and tiered backends (-Rcc int|jit|tiered). ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                                u8"(runtime)\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        if(!::uwvm2::uwvm::runtime::validator::validate_all_wasm_code()) [[unlikely]]
                        {
                            return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error);
                        }

                        ::uwvm2::runtime::lib::lazy_compile_run_config cfg{};
                        configure_runtime_entry_buffers(cfg, runtime_entry);
                        cfg.assume_full_code_verified = true;
                        ::uwvm2::runtime::lib::lazy_compile_and_run_main_module(::uwvm2::uwvm::wasm::storage::execute_wasm.module_name, cfg);
# else
                        ::fast_io::io::perr(
                            ::uwvm2::uwvm::io::u8log_output,
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                            u8"uwvm: ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                            u8"[fatal] ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                            u8"Lazy compilation with full code verification is not currently supported. The current VM only supports full compile with int or jit (-Rcm full -Rcc int|jit). ",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                            u8"(runtime)\n\n",
                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
# endif

                        break;
                    }
                    case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile:
                    {
                        switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler)
                        {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only:
                            {
                                // full compile + uwvm_int interpreter backend
                                ::uwvm2::runtime::lib::full_compile_run_config cfg{};
                                configure_runtime_entry_buffers(cfg, runtime_entry);
                                ::uwvm2::runtime::lib::full_compile_and_run_main_module(::uwvm2::uwvm::wasm::storage::execute_wasm.module_name, cfg);

                                break;
                            }
# endif
# if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter:
                            {
                                // not supported yet
                                ::fast_io::io::perr(
                                    ::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Debug Interpreter is not currently supported. The current VM only supports full compile with int or jit (-Rcm full -Rcc int|jit). ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(runtime)\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();

                                break;
                            }
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered:
                            {
                                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                    u8"uwvm: ",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                    u8"[fatal] ",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                    u8"Tiered compilation conflicts with full compilation. ",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                                    u8"(runtime)\n\n",
                                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                                ::fast_io::fast_terminate();

                                break;
                            }
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
                            case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only:
                            {
                                ::uwvm2::runtime::lib::full_compile_run_config cfg{};
                                configure_runtime_entry_buffers(cfg, runtime_entry);
                                ::uwvm2::runtime::lib::full_compile_and_run_main_module(::uwvm2::uwvm::wasm::storage::execute_wasm.module_name, cfg);

                                break;
                            }
# endif
                            [[unlikely]] default:
                            {
/// @warning Maybe I forgot to realize it.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                ::std::unreachable();
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }
                break;
            }
            /// @todo add more modes here
            [[unlikely]] default:
            {
                /// @warning Maybe I forgot to realize it.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                ::std::unreachable();
            }
        }

        return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
#endif
    }
}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
