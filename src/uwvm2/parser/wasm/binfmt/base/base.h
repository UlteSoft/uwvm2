/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-09
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import uwvm2.parser.wasm.standard.wasm1.type;
#else
// std
# include <cstdint>
# include <cstddef>
# include <type_traits>
# include <memory>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::binfmt
{
    struct module_span_t
    {
        ::std::byte const* module_begin{};
        ::std::byte const* module_end{};
    };

    inline constexpr bool is_wasm_file_unchecked(::std::byte const* module_curr) noexcept
    {
        // assuming:
        // [00 61 73 6D] Version ... (end)
        // [   safe    ]
        //  ^^ module_curr

        return ::fast_io::freestanding::my_memcmp(module_curr, u8"\0asm", 4uz) == 0;
    }

    inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 detect_wasm_binfmt_version_unchecked(::std::byte const* module_curr) noexcept
    {
        // assuming:
        // [00 61 73 6D Version ...] (end)
        // [           safe        ] unsafe
        //              ^^ module_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 temp{};
        ::fast_io::freestanding::my_memcpy(::std::addressof(temp), module_curr, sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32));
        static_assert(sizeof(temp) > 1);
        // Size of temp greater than one requires little-endian conversion
        temp = ::fast_io::little_endian(temp);
        return temp;
    }

    /// @brief      detect wasm binfmt version
    /// @return     0 : error, other : binfmt version
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 detect_wasm_binfmt_version(::std::byte const* const module_begin,
                                                                                                       ::std::byte const* const module_end) noexcept
    {
        // Checking for invalid pointer parameters
        if(module_begin > module_end) [[unlikely]] { return 0u; }

        ::std::byte const* module_curr{module_begin};

        // 00 61 73 6D Version ... (end)
        // unsafe
        // ^^ module_curr

        // Due to wasm magic and versioning, wasm must be greater than or equal to 8
        if(static_cast<::std::size_t>(module_end - module_curr) < 8uz || !::uwvm2::parser::wasm::binfmt::is_wasm_file_unchecked(module_curr)) [[unlikely]]
        {
            return 0u;
        }

        // [00 61 73 6D Version ...] (end)
        // [           safe        ] unsafe
        //  ^^ module_curr

        module_curr += 4uz;

        // [00 61 73 6D Version ...] (end)
        // [           safe        ] unsafe
        //              ^^ module_curr

        auto const binfmt_ver{detect_wasm_binfmt_version_unchecked(module_curr)};

        return binfmt_ver;
    }
}  // namespace uwvm2::parser::wasm::binfmt
