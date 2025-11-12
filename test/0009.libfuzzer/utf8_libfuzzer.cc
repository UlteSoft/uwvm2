/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <uwvm2/utils/utf/utf8.h>
#endif

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    auto const* b8 = reinterpret_cast<char8_t const*>(data);
    auto const* e8 = reinterpret_cast<char8_t const*>(data + size);

    try
    {
        // Route across specifications and checked/unchecked paths
        (void)::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(b8, e8);
        // (void)::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629>(b8, e8);
        (void)::uwvm2::utils::utf::check_legal_utf8_unchecked<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(b8, e8);
        // (void)::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(b8, e8);
    }
    catch(...)
    {
    }

    return 0;
}

