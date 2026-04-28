

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
# include <climits>
# include <cstring>
# include <limits>
# include <concepts>
# include <bit>
# include <memory>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/utils/mutex/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/object/memory/linear/impl.h>
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
# include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
# include <uwvm2/imported/wasi/wasip1/memory/impl.h>
# include <uwvm2/imported/wasi/wasip1/environment/impl.h>
# include "base.h"
# include "posix.h"
# include "proc_exit.h"
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wcpp"
# elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcpp"
# endif
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__)
#  pragma GCC diagnostic pop
# endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_WASM64)

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.proc_exit
    /// @details   void proc_exit(__wasi_exitcode_t code);

    inline void proc_exit_wasm64(::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
                                 ::uwvm2::imported::wasi::wasip1::abi::exitcode_wasm64_t code) noexcept
    {
        auto const trace_wasip1_call{env.trace_wasip1_call};

        if(trace_wasip1_call) [[unlikely]]
        {
            ::uwvm2::imported::wasi::wasip1::func::print_wasip1_trace_message(env,
                                u8"proc_exit_wasm64(",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(code)>>>(code),
                                u8")");
        }

        ::uwvm2::imported::wasi::wasip1::func::proc_exit_impl(env, code);
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

