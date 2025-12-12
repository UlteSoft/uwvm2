/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/imported/wasi/wasip1/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::imported::wasi::wasip1::storage
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)

    template <::uwvm2::imported::wasi::wasip1::environment::wasip1_memory memory_type>
    inline constexpr bool init_wasip1_environment(::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<memory_type> & env) noexcept
    {
        /// @brief   Initialize in out err
        /// @details Initialization method: dup into fd0, fd1, fd2

        /// @brief   Initialize mounted directory
        /// @details Starting dup from fd3

        /// @brief   Initialize socket
        /// @details First, sort the file descriptors. Non-contiguous file descriptors will be placed in the renumber_map.
        /// @note    Socket descriptors are currently not auto-discovered or preconfigured here. In particular, this
        ///          function does not modify the host's SIGPIPE handling or per-socket flags (such as SO_NOSIGPIPE).
        ///          Instead, individual WASI socket operations (for example, sock_send via MSG_NOSIGNAL where supported)
        ///          are responsible for preventing SIGPIPE from being delivered to the host process.
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

#  endif

        /// @todo !!!!
    }

# endif
#endif
}  // namespace uwvm2::uwvm::imported::wasi::wasip1::storage

#ifndef UWVM_MODULE
// macro
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
