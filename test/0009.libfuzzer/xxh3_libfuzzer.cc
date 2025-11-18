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
#include <cstring>

#ifndef UWVM_MODULE
# include <uwvm2/utils/hash/xxh3.h>
#endif

static inline std::uint64_t read_seed(std::uint8_t const* data, std::size_t size)
{
    std::uint64_t seed{};
    std::size_t const n = size < 8 ? size : 8;
    for(std::size_t i{}; i < n; ++i) { seed ^= static_cast<std::uint64_t>(data[i]) << (i * 8u); }
    return seed;
}

extern "C" int LLVMFuzzerTestOneInput(std::uint8_t const* data, std::size_t size)
{
    auto const* b = reinterpret_cast<std::byte const*>(data);
    std::uint64_t const seed = read_seed(data, size);

    try
    {
        (void)::uwvm2::utils::hash::xxh3_64bits(b, size, seed);
        if(size > 0) { (void)::uwvm2::utils::hash::xxh3_64bits(b + 1, size - 1, seed ^ 0x9E3779B185EBCA87ull); }

        ::uwvm2::utils::hash::xxh3_64bits_context ctx;
        ctx.seed64 = seed;
        ctx.reset();
        ctx.update(b, b + size);
        (void)ctx.digest_value();
    }
    catch(...){ }

    return 0;
}


