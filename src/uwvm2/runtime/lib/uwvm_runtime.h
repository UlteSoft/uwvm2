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

#include <cstddef>

#ifndef UWVM_MODULE
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::uwvm_int
{
    struct full_compile_run_config
    {
        ::uwvm2::utils::container::u8string_view entry_export_name{u8"_start"};
        bool prefer_start_section{true};
    };

    /// @brief Full-compile and run the main module using the uwvm_int interpreter backend.
    /// @note  This expects uwvm runtime initialization to be complete (runtime storages + import resolution).
    void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config const& cfg = {}) noexcept;
}  // namespace uwvm2::runtime::uwvm_int

