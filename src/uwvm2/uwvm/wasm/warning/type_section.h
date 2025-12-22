/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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
# include <cstddef>
# include <cstdint>
# include <climits>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/custom/impl.h>
# include "warn_storage.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::warning
{
    /// @brief Define a function to show warning for the type section
    /// @details Depends on adl, adl type is ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_type_type_t<Fs...>>
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void define_type_show_warning(
        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...> /*[adl]*/> const& types) noexcept
    {
        ::uwvm2::utils::container::unordered_flat_map<::uwvm2::parser::wasm::standard::wasm1::features::type_function_checker,
                                                      ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>
            duplicate_type_function_checker{};

        duplicate_type_function_checker.reserve(types.size());

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 type_counter{};
        for(auto const& curr_type: types)
        {
            ::uwvm2::parser::wasm::standard::wasm1::features::type_function_checker tmp{};
            tmp.parameter.begin = reinterpret_cast<::std::byte const*>(curr_type.parameter.begin);
            tmp.parameter.end = reinterpret_cast<::std::byte const*>(curr_type.parameter.end);
            tmp.result.begin = reinterpret_cast<::std::byte const*>(curr_type.result.begin);
            tmp.result.end = reinterpret_cast<::std::byte const*>(curr_type.result.end);

            auto const [it, is_inserted]{duplicate_type_function_checker.try_emplace(tmp, type_counter)};
            if(!is_inserted) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    // 1
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"type[",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    type_counter,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"] (func) in Type Section is duplicated, previous definition in type[",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    it->second,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"]. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(parser)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                if(::uwvm2::uwvm::io::parser_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(parser)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            ++type_counter;
        }
    }

    inline constexpr void show_wasm_type_section_warning(::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
                                                         [[maybe_unused]] ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    {
        constexpr auto get_prohibit_duplicate_types_from_features_tuple{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(::uwvm2::utils::container::tuple<Fs...>) consteval noexcept -> bool
            { return ::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<Fs...>(); }};

        constexpr bool prohibit_duplicate_types{get_prohibit_duplicate_types_from_features_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};

        // If no checks are performed under the parser, then a warning mechanism must be provided.
        if constexpr(!prohibit_duplicate_types)
        {
            auto const& binfmt_ver1_storage{wasm.get_curr_binfmt_version_wasm_storage<1u>()};

            constexpr auto get_typesec_from_features_tuple{
                []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(auto const& section,
                                                                                           ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
                {
                    return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                        ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(section);
                }};

            auto const& typesec{get_typesec_from_features_tuple(binfmt_ver1_storage.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};

            // Call using simple ADL (without complex concept control)
            define_type_show_warning(typesec.types);
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void show_wasm_section_warning(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>,
        ::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
        ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    { return show_wasm_type_section_warning(wasm, warn_storage); }
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
