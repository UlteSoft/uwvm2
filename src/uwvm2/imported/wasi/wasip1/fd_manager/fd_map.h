/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
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

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <limits>
# include <type_traits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include "fd.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::fd_manager
{
    /// @brief [singleton]
    struct wasm_fd_storage_t
    {
        // Ensure that iterators can't fail during expansion
        ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t> opens{};
        // Specialization of renumber, each extension requires a judgment of begin
        ::uwvm2::utils::container::map<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t, ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t>
            renumber_map{};
        // Used to record the coordinates of closure for subsequent builds
        ::uwvm2::utils::container::vector<::std::size_t> closes{};
        ::uwvm2::utils::mutex::rwlock_t fds_rwlock{};  // [singleton]
        ::std::size_t fd_limit{};
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
