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

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <memory>
#include <random>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/cmdline/callback/impl.h>
#else
# error "Module testing is not currently supported"
#endif

void test_build()
{
    ::uwvm2::uwvm::wasm::type::all_module_t s{};

    ::fast_io::black_hole dev_null{};
    ::fast_io::io::print(dev_null, ::uwvm2::uwvm::wasm::section_detail::section_details(s));

    ::fast_io::wblack_hole wdev_null{};
    ::fast_io::io::print(wdev_null, ::uwvm2::uwvm::wasm::section_detail::section_details(s));

    ::fast_io::u8black_hole u8dev_null{};
    ::fast_io::io::print(u8dev_null, ::uwvm2::uwvm::wasm::section_detail::section_details(s));

    ::fast_io::u16black_hole u16dev_null{};
    ::fast_io::io::print(u16dev_null, ::uwvm2::uwvm::wasm::section_detail::section_details(s));

    ::fast_io::u32black_hole u32dev_null{};
    ::fast_io::io::print(u32dev_null, ::uwvm2::uwvm::wasm::section_detail::section_details(s));
}

int main() {}
