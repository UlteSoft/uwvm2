/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-09
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
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include "section.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::binfmt::ver1
{
    /// @brief      Structures are specialized to store wasm binfmt ver1's module
    /// @details    module_span: The entire scope of the module, due to the 0-copy technique used by the interpreter, has to continuously open the file mapping
    ///             sections:    Stores a tuple of all sections, which are merged together and can be extended by templates.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm_binfmt_ver1_module_extensible_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

        ::uwvm2::parser::wasm::binfmt::module_span_t module_span{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> sections{};

        static_assert(::fast_io::is_tuple<::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...>>);  // check sections is tuple
    };

    /// @brief Wrapper for the module storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct wasm_binfmt_ver1_module_extensible_storage_section_details_wrapper_t
    {
        wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const* module_storage_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm_binfmt_ver1_module_extensible_storage_section_details_wrapper_t<Fs...> section_details(
        wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage) noexcept
    { return {::std::addressof(module_storage)}; }

    /// @brief Print the module section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, wasm_binfmt_ver1_module_extensible_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       wasm_binfmt_ver1_module_extensible_storage_section_details_wrapper_t<Fs...> const section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(section_details_wrapper.module_storage_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif
        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             "Module span: ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             " - ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_end),
                                                             " (length=",
                                                             static_cast<::std::size_t>(section_details_wrapper.module_storage_ptr->module_span.module_end -
                                                                                        section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             ")\nSection Details:\n",
                                                             section_details<Fs...>(section_details_wrapper.module_storage_ptr->sections));
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             L"Module span: ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             L" - ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_end),
                                                             L" (length=",
                                                             static_cast<::std::size_t>(section_details_wrapper.module_storage_ptr->module_span.module_end -
                                                                                        section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             L")\nSection Details:\n",
                                                             section_details<Fs...>(section_details_wrapper.module_storage_ptr->sections));
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u8"Module span: ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             u8" - ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_end),
                                                             u8" (length=",
                                                             static_cast<::std::size_t>(section_details_wrapper.module_storage_ptr->module_span.module_end -
                                                                                        section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             u8")\nSection Details:\n",
                                                             section_details<Fs...>(section_details_wrapper.module_storage_ptr->sections));
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             u"Module span: ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             u" - ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_end),
                                                             u" (length=",
                                                             static_cast<::std::size_t>(section_details_wrapper.module_storage_ptr->module_span.module_end -
                                                                                        section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             u")\nSection Details:\n",
                                                             section_details<Fs...>(section_details_wrapper.module_storage_ptr->sections));
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             U"Module span: ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             U" - ",
                                                             ::fast_io::mnp::pointervw(section_details_wrapper.module_storage_ptr->module_span.module_end),
                                                             U" (length=",
                                                             static_cast<::std::size_t>(section_details_wrapper.module_storage_ptr->module_span.module_end -
                                                                                        section_details_wrapper.module_storage_ptr->module_span.module_begin),
                                                             U")\nSection Details:\n",
                                                             section_details<Fs...>(section_details_wrapper.module_storage_ptr->sections));
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
