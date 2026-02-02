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
# include <utility>
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
    inline constexpr void show_wasm_function_section_warning(::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
                                                             ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    {
        // get wasm module storage
        auto const& wasm_module_storage{wasm.get_curr_binfmt_version_wasm_storage<1u>()};

        auto const& function_section{
            ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>(
                wasm_module_storage.sections)};

        // check unused type

        switch(function_section.funcs.mode)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::null:
            {
                // do nothing, no element
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u8_view:
            {
                auto const u8_view_begin{function_section.funcs.storage.typeidx_u8_view.begin};
                auto const u8_view_end{function_section.funcs.storage.typeidx_u8_view.end};

                for(auto u8_view_curr{u8_view_begin}; u8_view_curr != u8_view_end; ++u8_view_curr)
                {
                    auto const type_index{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(*u8_view_curr)};

                    auto& curr_type_unused_checker{
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        warn_storage.unused_type_checker[type_index]
#else
                        warn_storage.unused_type_checker.index_unchecked(type_index)
#endif
                    };

                    curr_type_unused_checker = true;
                }

                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u8_vector:
            {
                for(auto const u8_vec_curr: function_section.funcs.storage.typeidx_u8_vector)
                {
                    auto const type_index{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(u8_vec_curr)};

                    auto& curr_type_unused_checker{
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        warn_storage.unused_type_checker[type_index]
#else
                        warn_storage.unused_type_checker.index_unchecked(type_index)
#endif
                    };

                    curr_type_unused_checker = true;
                }

                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u16_vector:
            {
                for(auto const u16_vec_curr: function_section.funcs.storage.typeidx_u16_vector)
                {
                    auto const type_index{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(u16_vec_curr)};

                    auto& curr_type_unused_checker{
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        warn_storage.unused_type_checker[type_index]
#else
                        warn_storage.unused_type_checker.index_unchecked(type_index)
#endif
                    };

                    curr_type_unused_checker = true;
                }
                break;
            }
            case ::uwvm2::parser::wasm::standard::wasm1::features::vectypeidx_minimize_storage_mode::u32_vector:
            {
                for(auto const u32_vec_curr: function_section.funcs.storage.typeidx_u32_vector)
                {
                    auto const type_index{u32_vec_curr};

                    auto& curr_type_unused_checker{
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        warn_storage.unused_type_checker[type_index]
#else
                        warn_storage.unused_type_checker.index_unchecked(type_index)
#endif
                    };

                    curr_type_unused_checker = true;
                }
                break;
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();
            }
        }
    }

    inline constexpr void show_wasm_section_warning(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::function_section_storage_t>,
        ::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
        ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    { return show_wasm_function_section_warning(wasm, warn_storage); }
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
