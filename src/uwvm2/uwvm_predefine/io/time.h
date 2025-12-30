/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-16
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
// import
# include <fast_io.h>
# include <fast_io_device.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef UWVM

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::io
{
    inline constexpr ::fast_io::iso8601_timestamp get_local_realtime() noexcept
    {
        ::fast_io::iso8601_timestamp local_realtime{};
# ifdef UWVM_CPP_EXCEPTIONS
        try
# endif
        {
            local_realtime = ::fast_io::local(::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime));
        }
# ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            // keep default timestamp
        }
# endif

        return local_realtime;
    }
}  // uwvm2::uwvm::io

#endif
