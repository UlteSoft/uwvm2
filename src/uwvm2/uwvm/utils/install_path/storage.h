/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-29
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
// platform
# ifdef UWVM_SUPPORT_INSTALL_PATH
#  include <fast_io.h>
#  include <fast_io_driver/install_path.h>
# endif
// import
# include <uwvm2/utils/install_path/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef UWVM_SUPPORT_INSTALL_PATH
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::utils::install_path
{
    /// @brief Path to the program binary itself
    /// @details Only support to get by system parameter, not support to get by argv0, argv0 may be wrong
    inline ::fast_io::install_path install_path{
        ::uwvm2::utils::install_path::get_module_install_path_noexcept()};  // [global] No global variable dependencies from other translation units
}  // namespace uwvm2::utils::install_path
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
