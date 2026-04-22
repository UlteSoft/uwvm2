/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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
# include <cstdint>
# include <cstddef>
# include <climits>
# include <cstring>
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

    /// @details    Assumed safe range: [module_curr, module_curr + 4)
    /// @note       Before using the unchecked function, make sure that the safe part of the assume has been checked.
    inline constexpr bool is_wasm_file_unchecked(::std::byte const* module_curr) noexcept
    {
        // [00 61 73 6D  ] Version ... (end)
        // [safe (assume)]
        //  ^^ module_curr

        return ::std::memcmp(module_curr, u8"\0asm", 4uz * sizeof(char8_t)) == 0;
    }

    /// @details    Assumed safe range: [module_curr - 4, module_curr + 4)
    /// @note       Before using the unchecked function, make sure that the safe part of the assume has been checked.
    inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 detect_wasm_binfmt_version_unchecked(::std::byte const* module_curr) noexcept
    {

#if CHAR_BIT > 8
        // CHAR_BIT > 8: The number of bits per byte is greater than 8, when only the lower eight bits of information are valid.

        // assuming:
        // [?00 ?61 ?73 ?6D Version ...] (end)
        // [           safe (assume)   ] unsafe
        //                  ^^ module_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 temp{};

        // simulate little endian
        // [??? ??? ??? ??? ?00 ?01 ?02 ?03 ...] (end)
        // [             safe (assume)         ] unsafe
        //              ^^ module_curr - 1u: minus 1u is assumed to be legitimate
        //                  ^^ module_curr
        //                              ^^ module_curr + 3u

        // 1st: module_curr + 3u
        // 2nd: module_curr + 2u
        // 3rd: module_curr + 1u
        // 4th: module_curr
        // 5th: (break) module_curr - 1u

        for(auto c{module_curr + 3u}; c != module_curr - 1u; --c)
        {
            auto const c_val{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(*c) & 0xFFu};

            temp <<= 8u;
            temp |= c_val;
        }

        return temp;

#else
        // [00 61 73 6D Version ...] (end)
        // [         safe (assume) ] unsafe
        //              ^^ module_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 temp;
        ::std::memcpy(::std::addressof(temp), module_curr, sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32));
        // Size of temp greater than one requires little-endian conversion
        // supported: big-endian, little-endian, pdp-endian
        temp = ::fast_io::little_endian(temp);
        return temp;

#endif
    }

    /// @brief      detect wasm binfmt version
    /// @return     0: error, other: binfmt version
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

        // assume that the safe part of the unchecked function is already checked.
        auto const binfmt_ver{detect_wasm_binfmt_version_unchecked(module_curr)};

        return binfmt_ver;
    }
}
