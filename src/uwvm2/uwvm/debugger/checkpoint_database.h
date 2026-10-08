/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "checkpoint_codec.h"
# include <fast_io.h>
# include <fast_io_device.h>
# include <fast_io_dsal/string.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::checkpoint
{
    enum class sync_guarantee { none, os_file_sync, qualified_power_loss };
    struct file_write_result
    {
        error status{error::unavailable_capability};
        int native_error{};
        sync_guarantee synchronized{};
    };
    [[nodiscard]] inline auto checkpoint_file_name(::std::uint64_t identifier)
    { return ::fast_io::concat_fast_io("checkpoint-", ::fast_io::mnp::dec(identifier), ".dmp"); }
    // The trusted manager creates this output with native_file RAII, at its
    // retained non-guest/preopen directory, O_EXCL/equivalent and mode 0600,
    // and registers its REAL file identity in the existing sealed-asset layer
    // before any guest resumes. No pathname/descriptor from Wasm is accepted.
    // This helper writes an UNPUBLISHED immutable transaction, not a live VM
    // restore or directory commit. A failed temporary stays unpublished; the
    // manager closes/drains it and retires its sealed identity before unlink.
    // A successful return still requires checked file close, no-clobber atomic
    // publication, supported directory sync and immutable-read owner acquisition.
    [[nodiscard]] inline file_write_result write_unpublished_database(::fast_io::obuf_file& file,
        state const& snapshot, limits const& cap = {})
    {
        if(auto const status{encode_database(file, snapshot, cap)}; status != error::none) { return {status}; }
        ::fast_io::operations::output_stream_buffer_flush(file);
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
        auto const result{::fast_io::posix_fsync_nothrow(file.handle)};
        if(result.error != 0) { return {error::incomplete_commit, result.error}; }
        // fsync has requested OS synchronization. It does not prove that the
        // filesystem/storage honors power-loss durability, directory sync or
        // Darwin F_FULLFSYNC; those need separately qualified manager providers.
        return {error::none, 0, sync_guarantee::os_file_sync};
#elif defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::data_sync(file.handle, ::fast_io::data_sync_flags::normal);
        return {error::none, 0, sync_guarantee::os_file_sync};
#else
        return {error::unavailable_capability};
#endif
    }
}
