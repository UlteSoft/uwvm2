/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        time.h
 * @brief       Exception-safe timestamp helper for UWVM diagnostics.
 * @details     This header provides the local realtime timestamp helper used by verbose and diagnostic output paths.
 *              Logging code must remain `noexcept`, so the helper catches `fast_io::error` when C++ exceptions are
 *              enabled and returns a default-initialized timestamp if the platform clock query fails.
 *
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
    /// @brief Returns the current local realtime timestamp for diagnostic records.
    /// @details The timestamp is produced from `fast_io::posix_clock_gettime(posix_clock_id::realtime)` and converted to
    ///          local ISO-8601 form.  In exception-enabled builds, `fast_io::error` is caught so callers can use this
    ///          helper from logging and failure-reporting paths without throwing.
    /// @return The current local ISO-8601 timestamp, or a default-initialized timestamp if clock acquisition fails.
    /// @see show_verbose
    /// @see u8log_output
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
