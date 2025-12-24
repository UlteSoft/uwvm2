/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-19
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
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::initializer::data
{
    // Here, `uint_fast8_t` is used to ensure alignment with `bool` for efficient access.
    enum class wasm_data_segment_kind : ::std::uint_fast8_t
    {
        /// @brief Active segment: applied during instantiation (data section in wasm1 MVP).
        active,
        /// @brief Passive segment: retained for runtime `memory.init` / `data.drop` (bulk memory feature).
        passive,
    };

    struct wasm_data_storage_t
    {
        using byte_type = ::std::byte;

        byte_type const* byte_begin{};
        byte_type const* byte_end{};

        // target memory index
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_idx{};

        // byte offset into target memory (valid only for active segments)
        ::std::uint_least64_t offset{};

        // Here, `uint_fast8_t` is used to ensure alignment with `bool` for efficient access.
        wasm_data_segment_kind kind{wasm_data_segment_kind::active};

        // meaningful only for passive segments; when true the payload is not available.
        // doop will not set byte_begin and byte_end to nullptr, making it easier to verify.
        bool dropped{};
    };
}

UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_zero_default_constructible<::uwvm2::uwvm::wasm::initializer::data::wasm_data_storage_t>
    {
        inline static constexpr bool value = true;
    };

    static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<::uwvm2::uwvm::wasm::initializer::data::wasm_data_storage_t>);
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif

