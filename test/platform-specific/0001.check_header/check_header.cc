/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-12
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

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <memory>

#include <uwvm2/uwvm/crtmain/impl.h>

int main()
{
    /// @brief Check if all headers are popped out

#ifdef UWVM_DLLIMPORT
    static_assert(false);
#endif

#ifdef UWVM_WASM_SUPPORT_WASM1
    static_assert(false);
#endif

#ifdef UWVM_AES_RST_ALL
    static_assert(false);
#endif

#ifdef UWVM_COLOR_RST_ALL
    static_assert(false);
#endif

#ifdef UWVM_WIN32_TEXTATTR_RST_ALL
    static_assert(false);
#endif
}
