
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
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
// platform
# if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
#  include <unistd.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  if !(defined(__MSDOS__) || defined(__DJGPP__))
#   include <sys/socket.h>
#  endif
# endif
// import
# include <fast_io.h>
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
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifdef UWVM_IMPORT_WASI_WASIP1

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{

    /// @brief     WasiPreview1.fd_fdstat_set_flags
    /// @details   __wasi_errno_t fd_fdstat_set_flags(__wasi_fd_t fd, __wasi_fdflags_t flags);

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_fdstat_set_flags_base(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t flags) noexcept
    {
        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        // The pointer to `wasm_fd` is fixed and remains unchanged even when the vector within `fd_manager` is resized.
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p;  // no initialize

        // Subsequent operations involving the file descriptor require locking. curr_fd_release_guard release when return.
        ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

        {
            // Prevent operations to obtain the size or perform resizing at this time.
            // Only a lock is required when acquiring the unique pointer for the file descriptor. The lock can be released once the acquisition is complete.
            // Since the file descriptor's location is fixed and accessed via the unique pointer,

            // Simply acquiring data using a shared_lock
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            // Negative states have been excluded, so the conversion result will only be positive numbers.
            using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>;
            auto const unsigned_fd{static_cast<unsigned_fd_t>(fd)};

            // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
            {
                if(unsigned_fd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }
            }

            auto const fd_opens_pos{static_cast<::std::size_t>(unsigned_fd)};

            // The minimum value in rename_map is greater than opensize.
            if(wasm_fd_storage.opens.size() <= fd_opens_pos)
            {
                // Possibly within the tree being renumbered
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                {
                    curr_wasi_fd_t_p = renumber_map_iter->second.fd_p;
                }
                else [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                }
            }
            else
            {
                // The addition here is safe.
                curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;
            }

            // curr_wasi_fd_t_p never nullptr
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(curr_wasi_fd_t_p == nullptr) [[unlikely]]
            {
                // Security issues inherent to virtual machines
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }
# endif

            // Other threads will definitely lock fds_rwlock when performing close operations (since they need to access the fd vector). If the current thread
            // is performing fdstat_get, no other thread can be executing any close operations simultaneously, eliminating any destruction issues. Therefore,
            // acquiring the lock at this point is safe. However, the problem arises when, immediately after acquiring the lock and before releasing the manager
            // lock and beginning fd operations, another thread executes a deletion that removes this fd. Subsequent operations by the current thread would then
            // encounter issues. Thus, locking must occur before releasing fds_rwlock.
            curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
            curr_fd_release_guard.lock();

            // After unlocking fds_lock, members within `wasm_fd_storage_t` can no longer be accessed or modified.
        }

        // curr_fd_uniptr is not null.
        auto& curr_fd{*curr_wasi_fd_t_p};

        // If obtained from the renumber map, it will always be the correct value. If obtained from the open vec, it requires checking whether it is closed.
        // Therefore, a unified check is implemented.
        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_fdstat_set_flags) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_fdstat_set_flags) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable;
        }

        // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files from
        // being processed, making this a virtual machine implementation error.
        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
        {
// This will be checked at runtime.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
        }

        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
        {
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer:
            {
                ::fast_io::native_io_observer curr_fd_native_observer{};

                bool const is_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type == ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer};

                if(is_observer)
                {
                    auto& curr_fd_native_observer_ref{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_observer};
                    curr_fd_native_observer = curr_fd_native_observer_ref;
                }
                else
                {
                    auto& file_fd{
# if defined(_WIN32) && !defined(__CYGWIN__)
                        curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file
# else
                        curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd
# endif
                    };
                    curr_fd_native_observer = file_fd;
                }

                [[maybe_unused]] auto const file_fd_handle{curr_fd_native_observer.native_handle()};

# if defined(_WIN32) && !defined(__CYGWIN__)
                // Win32 only supports setting flags during creation.
                if((flags & (::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append | ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync |
                             ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync | ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync |
                             ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)) != ::uwvm2::imported::wasi::wasip1::abi::fdflags_t{})
                    [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

# elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && __has_include(<dirent.h>) && !defined(_PICOLIBC__) && !(defined(__MSDOS__) || defined(__DJGPP__))

                // Preserve unrelated OS flags: read current flags first, then toggle only WASI-managed bits.
#  if defined(__linux__) && defined(__NR_fcntl)
                int const curr_flags{::fast_io::system_call<__NR_fcntl, int>(file_fd_handle, F_GETFL)};
                if(::fast_io::linux_system_call_fails(curr_flags)) [[unlikely]]
                {
                    int const err{static_cast<int>(-curr_flags)};
                    switch(err)
                    {
                        // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                int new_oflags{curr_flags};
#  else
                // Although djgpp provides these flags, it only supports F_DUPFD, so it is prohibited here.

                int const curr_flags{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(file_fd_handle, F_GETFL)};
                if(curr_flags == -1) [[unlikely]]
                {
                    switch(errno)
                    {
                        // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                int new_oflags{curr_flags};
#  endif

                // Toggle per-WASI bits only.
#  if defined(O_APPEND) && O_APPEND != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append)
                {
                    new_oflags |= O_APPEND;
                }
                else
                {
                    new_oflags &= ~O_APPEND;
                }
#  else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#  endif

#  if defined(O_NONBLOCK) && O_NONBLOCK != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)
                {
                    new_oflags |= O_NONBLOCK;
                }
                else
                {
                    new_oflags &= ~O_NONBLOCK;
                }
#  else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#  endif

#  if (defined(O_DSYNC) && O_DSYNC != 0) && (defined(O_SYNC) && O_SYNC != 0) && (defined(O_RSYNC) && O_RSYNC != 0) && ((O_SYNC | O_DSYNC) == O_SYNC) &&        \
      (O_RSYNC == O_SYNC)
                // On Android, O_SYNC is a superset of O_DSYNC, and O_RSYNC == O_SYNC. Requesting SYNC or RSYNC implies O_SYNC.
                if(((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) ||
                   ((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync))
                {
                    new_oflags |= O_SYNC;
                }
                else if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~(O_SYNC | O_DSYNC);
                }
#  elif (defined(O_DSYNC) && O_DSYNC != 0) && (defined(O_SYNC) && O_SYNC != 0) && ((O_SYNC | O_DSYNC) == O_SYNC)
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    new_oflags |= O_SYNC;
                }
                else if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~O_SYNC;
                }

#   if defined(O_RSYNC) && O_RSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    new_oflags |= O_RSYNC;
                }
                else
                {
                    new_oflags &= ~O_RSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif
#  elif (defined(O_SYNC) && O_SYNC != 0) && (defined(O_RSYNC) && O_RSYNC != 0) && (O_RSYNC == O_SYNC)
                if(((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) ||
                   ((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync))
                {
                    new_oflags |= O_SYNC;
                }
                else
                {
                    new_oflags &= ~O_SYNC;
                }

#   if defined(O_DSYNC) && O_DSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~O_DSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif
#  else
                // default
#   if defined(O_DSYNC) && O_DSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~O_DSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif

#   if defined(O_RSYNC) && O_RSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    new_oflags |= O_RSYNC;
                }
                else
                {
                    new_oflags &= ~O_RSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif

#   if defined(O_SYNC) && O_SYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    new_oflags |= O_SYNC;
                }
                else
                {
                    new_oflags &= ~O_SYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif
#  endif

#  if defined(__linux__) && defined(__NR_fcntl)
                int const set_res{::fast_io::system_call<__NR_fcntl, int>(file_fd_handle, F_SETFL, new_oflags)};

                if(::fast_io::linux_system_call_fails(set_res)) [[unlikely]]
                {
                    int const err{static_cast<int>(-set_res)};
                    switch(err)
                    {
                        // If "ebadf" appears here, it is caused by a WASI implementation issue. This differs from WASI's 'ebadf'; here, "eio" is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        // On Linux/Posix systems, the flags modifiable via `F_SETFL` are typically limited to `O_APPEND`/`O_NONBLOCK` (and a few
                        // implementation-dependent flags). `O_SYNC`/`O_DSYNC`/`O_RSYNC` generally cannot be modified through `F_SETFL`, and the kernel often
                        // returns `EINVAL`. Here, we uniformly note that these flags are not supported.
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                        case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                // Verify that the flags were actually set by reading them back
                int const verify_flags{::fast_io::system_call<__NR_fcntl, int>(file_fd_handle, F_GETFL)};
                if(::fast_io::linux_system_call_fails(verify_flags)) [[unlikely]]
                {
                    int const err{static_cast<int>(-verify_flags)};
                    switch(err)
                    {
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                // Check if the WASI-managed flags match what we expected
                int wasi_managed_flags{};
                int actual_wasi_flags{};

#   if defined(O_APPEND) && O_APPEND != 0
                wasi_managed_flags |= (new_oflags & O_APPEND);
                actual_wasi_flags |= (verify_flags & O_APPEND);
#   endif

#   if defined(O_NONBLOCK) && O_NONBLOCK != 0
                wasi_managed_flags |= (new_oflags & O_NONBLOCK);
                actual_wasi_flags |= (verify_flags & O_NONBLOCK);
#   endif

#   if defined(O_DSYNC) && O_DSYNC != 0
                wasi_managed_flags |= (new_oflags & O_DSYNC);
                actual_wasi_flags |= (verify_flags & O_DSYNC);
#   endif

#   if defined(O_RSYNC) && O_RSYNC != 0
                wasi_managed_flags |= (new_oflags & O_RSYNC);
                actual_wasi_flags |= (verify_flags & O_RSYNC);
#   endif

#   if defined(O_SYNC) && O_SYNC != 0
                wasi_managed_flags |= (new_oflags & O_SYNC);
                actual_wasi_flags |= (verify_flags & O_SYNC);
#   endif

                if(wasi_managed_flags != actual_wasi_flags) [[unlikely]]
                {
                    // Attempt to restore
                    int const set_res{::fast_io::system_call<__NR_fcntl, int>(file_fd_handle, F_SETFL, curr_flags)};
                    if(::fast_io::linux_system_call_fails(set_res)) [[unlikely]]
                    {
                        // eio: An I/O exception occurred that we cannot control or recover from (e.g., rollback failure, sporadic F_GETFL failure, etc.).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                    else
                    {
                        // enotsup: The requested semantics are unsupported, and we guarantee behavior that “appears atomic” to the caller (either no change or
                        // a rollback).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    }
                }

#  else
                int const set_res{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(file_fd_handle, F_SETFL, new_oflags)};

                if(set_res == -1) [[unlikely]]
                {
                    switch(errno)
                    {
                        // If "ebadf" appears here, it is caused by a WASI implementation issue. This differs from WASI's 'ebadf'; here, "eio" is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        // On Linux/Posix systems, the flags modifiable via `F_SETFL` are typically limited to `O_APPEND`/`O_NONBLOCK` (and a few
                        // implementation-dependent flags). `O_SYNC`/`O_DSYNC`/`O_RSYNC` generally cannot be modified through `F_SETFL`, and the kernel often
                        // returns `EINVAL`. Here, we uniformly note that these flags are not supported.
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                        case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                // Verify that the flags were actually set by reading them back
                int const verify_flags{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(file_fd_handle, F_GETFL)};
                if(verify_flags == -1) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                // Check if the WASI-managed flags match what we expected
                int wasi_managed_flags{};
                int actual_wasi_flags{};

#   if defined(O_APPEND) && O_APPEND != 0
                wasi_managed_flags |= (new_oflags & O_APPEND);
                actual_wasi_flags |= (verify_flags & O_APPEND);
#   endif

#   if defined(O_NONBLOCK) && O_NONBLOCK != 0
                wasi_managed_flags |= (new_oflags & O_NONBLOCK);
                actual_wasi_flags |= (verify_flags & O_NONBLOCK);
#   endif

#   if defined(O_DSYNC) && O_DSYNC != 0
                wasi_managed_flags |= (new_oflags & O_DSYNC);
                actual_wasi_flags |= (verify_flags & O_DSYNC);
#   endif

#   if defined(O_RSYNC) && O_RSYNC != 0
                wasi_managed_flags |= (new_oflags & O_RSYNC);
                actual_wasi_flags |= (verify_flags & O_RSYNC);
#   endif

#   if defined(O_SYNC) && O_SYNC != 0
                wasi_managed_flags |= (new_oflags & O_SYNC);
                actual_wasi_flags |= (verify_flags & O_SYNC);
#   endif

                if(wasi_managed_flags != actual_wasi_flags) [[unlikely]]
                {
                    // Attempt to restore
                    int const set_res{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(file_fd_handle, F_SETFL, curr_flags)};
                    if(set_res == -1) [[unlikely]]
                    {
                        // eio: An I/O exception occurred that we cannot control or recover from (e.g., rollback failure, sporadic F_GETFL failure, etc.).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                    else
                    {
                        // enotsup: The requested semantics are unsupported, and we guarantee behavior that “appears atomic” to the caller (either no change or
                        // a rollback).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    }
                }

#  endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# else
                // Systems that do not support modifying file attributes mid-process

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                // When the system does not support modifying file attributes at all, passing flags set to 0 indicates success.
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# endif
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                // Retrieve the current directory, which is the top element of the directory stack.
                auto const& curr_dir_stack{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                if(curr_dir_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                auto const& curr_dir_stack_entry{curr_dir_stack.dir_stack.back_unchecked()};
                if(curr_dir_stack_entry.ptr == nullptr) [[unlikely]]
                {
// This will be checked at runtime.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }

                ::fast_io::dir_io_observer curr_dir_io_observer{};

                bool const is_observer{curr_dir_stack_entry.ptr->dir_stack.is_observer};

                if(is_observer)
                {
                    auto& curr_dir_io_observer_ref{curr_dir_stack_entry.ptr->dir_stack.storage.observer};
                    curr_dir_io_observer = curr_dir_io_observer_ref;
                }
                else
                {
                    auto& curr_dir_file_ref{curr_dir_stack_entry.ptr->dir_stack.storage.file};
                    curr_dir_io_observer = curr_dir_file_ref;
                }

                auto const& curr_dir_file{curr_dir_io_observer};
                [[maybe_unused]] auto const curr_dir_file_native_handle{curr_dir_file.native_handle()};

# if defined(_WIN32) && !defined(__CYGWIN__)
                // Win32 only supports setting flags during creation.
                if((flags & (::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append | ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync |
                             ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync | ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync |
                             ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)) != ::uwvm2::imported::wasi::wasip1::abi::fdflags_t{})
                    [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

# elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && __has_include(<dirent.h>) && !defined(_PICOLIBC__) && !(defined(__MSDOS__) || defined(__DJGPP__))

                // Preserve unrelated OS flags: read current flags first, then toggle only WASI-managed bits.
#  if defined(__linux__) && defined(__NR_fcntl)
                int const curr_flags{::fast_io::system_call<__NR_fcntl, int>(curr_dir_file_native_handle, F_GETFL)};
                if(::fast_io::linux_system_call_fails(curr_flags)) [[unlikely]]
                {
                    int const err{static_cast<int>(-curr_flags)};
                    switch(err)
                    {
                        // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                int new_oflags{curr_flags};
#  else
                // Although djgpp provides these flags, it only supports F_DUPFD, so it is prohibited here.

                int const curr_flags{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_dir_file_native_handle, F_GETFL)};
                if(curr_flags == -1) [[unlikely]]
                {
                    switch(errno)
                    {
                        // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                int new_oflags{curr_flags};
#  endif

                // Toggle per-WASI bits only.
#  if defined(O_APPEND) && O_APPEND != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append)
                {
                    new_oflags |= O_APPEND;
                }
                else
                {
                    new_oflags &= ~O_APPEND;
                }
#  else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#  endif

#  if defined(O_NONBLOCK) && O_NONBLOCK != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)
                {
                    new_oflags |= O_NONBLOCK;
                }
                else
                {
                    new_oflags &= ~O_NONBLOCK;
                }
#  else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#  endif

#  if (defined(O_DSYNC) && O_DSYNC != 0) && (defined(O_SYNC) && O_SYNC != 0) && (defined(O_RSYNC) && O_RSYNC != 0) && ((O_SYNC | O_DSYNC) == O_SYNC) &&        \
      (O_RSYNC == O_SYNC)
                // On Android, O_SYNC is a superset of O_DSYNC, and O_RSYNC == O_SYNC. Requesting SYNC or RSYNC implies O_SYNC.
                if(((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) ||
                   ((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync))
                {
                    new_oflags |= O_SYNC;
                }
                else if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~(O_SYNC | O_DSYNC);
                }
#  elif (defined(O_DSYNC) && O_DSYNC != 0) && (defined(O_SYNC) && O_SYNC != 0) && ((O_SYNC | O_DSYNC) == O_SYNC)
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    new_oflags |= O_SYNC;
                }
                else if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) ==
                        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~O_SYNC;
                }

#   if defined(O_RSYNC) && O_RSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    new_oflags |= O_RSYNC;
                }
                else
                {
                    new_oflags &= ~O_RSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif
#  elif (defined(O_SYNC) && O_SYNC != 0) && (defined(O_RSYNC) && O_RSYNC != 0) && (O_RSYNC == O_SYNC)
                if(((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) ||
                   ((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync))
                {
                    new_oflags |= O_SYNC;
                }
                else
                {
                    new_oflags &= ~O_SYNC;
                }

#   if defined(O_DSYNC) && O_DSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~O_DSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif
#  else
                // default
#   if defined(O_DSYNC) && O_DSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    new_oflags |= O_DSYNC;
                }
                else
                {
                    new_oflags &= ~O_DSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif

#   if defined(O_RSYNC) && O_RSYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    new_oflags |= O_RSYNC;
                }
                else
                {
                    new_oflags &= ~O_RSYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif

#   if defined(O_SYNC) && O_SYNC != 0
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    new_oflags |= O_SYNC;
                }
                else
                {
                    new_oflags &= ~O_SYNC;
                }
#   else
                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }
#   endif
#  endif

#  if defined(__linux__) && defined(__NR_fcntl)
                int const set_res{::fast_io::system_call<__NR_fcntl, int>(curr_dir_file_native_handle, F_SETFL, new_oflags)};

                if(::fast_io::linux_system_call_fails(set_res)) [[unlikely]]
                {
                    int const err{static_cast<int>(-set_res)};
                    switch(err)
                    {
                        // If "ebadf" appears here, it is caused by a WASI implementation issue. This differs from WASI's 'ebadf'; here, "eio" is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                        case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                // Verify that the flags were actually set by reading them back
                int const verify_flags{::fast_io::system_call<__NR_fcntl, int>(curr_dir_file_native_handle, F_GETFL)};
                if(::fast_io::linux_system_call_fails(verify_flags)) [[unlikely]]
                {
                    int const err{static_cast<int>(-verify_flags)};
                    switch(err)
                    {
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                // Check if the WASI-managed flags match what we expected
                int wasi_managed_flags{};
                int actual_wasi_flags{};

#   if defined(O_APPEND) && O_APPEND != 0
                wasi_managed_flags |= (new_oflags & O_APPEND);
                actual_wasi_flags |= (verify_flags & O_APPEND);
#   endif

#   if defined(O_NONBLOCK) && O_NONBLOCK != 0
                wasi_managed_flags |= (new_oflags & O_NONBLOCK);
                actual_wasi_flags |= (verify_flags & O_NONBLOCK);
#   endif

#   if defined(O_DSYNC) && O_DSYNC != 0
                wasi_managed_flags |= (new_oflags & O_DSYNC);
                actual_wasi_flags |= (verify_flags & O_DSYNC);
#   endif

#   if defined(O_RSYNC) && O_RSYNC != 0
                wasi_managed_flags |= (new_oflags & O_RSYNC);
                actual_wasi_flags |= (verify_flags & O_RSYNC);
#   endif

#   if defined(O_SYNC) && O_SYNC != 0
                wasi_managed_flags |= (new_oflags & O_SYNC);
                actual_wasi_flags |= (verify_flags & O_SYNC);
#   endif

                if(wasi_managed_flags != actual_wasi_flags) [[unlikely]]
                {
                    // Attempt to restore
                    int const set_res{::fast_io::system_call<__NR_fcntl, int>(curr_dir_file_native_handle, F_SETFL, curr_flags)};
                    if(::fast_io::linux_system_call_fails(set_res)) [[unlikely]]
                    {
                        // eio: An I/O exception occurred that we cannot control or recover from (e.g., rollback failure, sporadic F_GETFL failure, etc.).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                    else
                    {
                        // enotsup: The requested semantics are unsupported, and we guarantee behavior that “appears atomic” to the caller (either no change or
                        // a rollback).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    }
                }

#  else
                int const set_res{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_dir_file_native_handle, F_SETFL, new_oflags)};

                if(set_res == -1) [[unlikely]]
                {
                    switch(errno)
                    {
                        // If "ebadf" appears here, it is caused by a WASI implementation issue. This differs from WASI's 'ebadf'; here, "eio" is used instead.
                        case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                        case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                // Verify that the flags were actually set by reading them back
                int const verify_flags{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_dir_file_native_handle, F_GETFL)};
                if(verify_flags == -1) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                // Check if the WASI-managed flags match what we expected
                int wasi_managed_flags{};
                int actual_wasi_flags{};

#   if defined(O_APPEND) && O_APPEND != 0
                wasi_managed_flags |= (new_oflags & O_APPEND);
                actual_wasi_flags |= (verify_flags & O_APPEND);
#   endif

#   if defined(O_NONBLOCK) && O_NONBLOCK != 0
                wasi_managed_flags |= (new_oflags & O_NONBLOCK);
                actual_wasi_flags |= (verify_flags & O_NONBLOCK);
#   endif

#   if defined(O_DSYNC) && O_DSYNC != 0
                wasi_managed_flags |= (new_oflags & O_DSYNC);
                actual_wasi_flags |= (verify_flags & O_DSYNC);
#   endif

#   if defined(O_RSYNC) && O_RSYNC != 0
                wasi_managed_flags |= (new_oflags & O_RSYNC);
                actual_wasi_flags |= (verify_flags & O_RSYNC);
#   endif

#   if defined(O_SYNC) && O_SYNC != 0
                wasi_managed_flags |= (new_oflags & O_SYNC);
                actual_wasi_flags |= (verify_flags & O_SYNC);
#   endif

                if(wasi_managed_flags != actual_wasi_flags) [[unlikely]]
                {
                    // Attempt to restore
                    int const set_res{::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_dir_file_native_handle, F_SETFL, curr_flags)};
                    if(set_res == -1) [[unlikely]]
                    {
                        // eio: An I/O exception occurred that we cannot control or recover from (e.g., rollback failure, sporadic F_GETFL failure, etc.).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                    else
                    {
                        // enotsup: The requested semantics are unsupported, and we guarantee behavior that “appears atomic” to the caller (either no change or
                        // a rollback).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    }
                }

#  endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# else
                // Systems that do not support modifying file attributes mid-process

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock) ==
                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                if((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync) == ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                // When the system does not support modifying file attributes at all, passing flags set to 0 indicates success.
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# endif
            }
# if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer:
            {
                ::fast_io::win32_socket_io_observer curr_socket_observer{};

                bool const is_socket_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type ==
                                              ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer};

                if(is_socket_observer)
                {
                    auto& curr_socket_observer_ref{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.socket_observer};
                    curr_socket_observer = curr_socket_observer_ref;
                }
                else
                {
                    auto& curr_socket_observer_ref{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.socket_fd};
                    curr_socket_observer = curr_socket_observer_ref;
                }

                auto const& socket_fd{curr_socket_observer};
                auto const socket_fd_handle{socket_fd.native_handle()};

                // For ws2, support for setting nonblock is available.
                // Since the flags on ws2 are fixed and confirmed, there is no need to perform verification again after configuration.
                if((flags & (::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_append | ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_dsync |
                             ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_rsync | ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_sync)) !=
                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_t{}) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                }

                ::std::uint_least32_t mode{static_cast<::std::uint_least32_t>((flags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock) ==
                                                                              ::uwvm2::imported::wasi::wasip1::abi::fdflags_t::fdflag_nonblock)};

                if(::fast_io::win32::ioctlsocket(socket_fd_handle, 0x8004'667El /*FIONBIO*/, ::std::addressof(mode)) == -1) [[unlikely]]
                {
                    switch(::fast_io::win32::WSAGetLastError())
                    {
                        case 10022 /*WSAEINVAL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        case 10004 /*WSAEINTR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        case 10038 /*WSAENOTSOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                        case 10013 /*WSAEACCES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
# endif
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_fdstat_set_flags(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::fdflags_t flags) noexcept
    {
        auto const trace_wasip1_call{env.trace_wasip1_call};

        if(trace_wasip1_call) [[unlikely]]
        {
# ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"wasip1: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"fd_fdstat_set_flags",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(flags)>>>(flags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: fd_fdstat_set_flags(",
                                fd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(flags)>>>(flags),
                                u8") (wasi-trace)\n");
# endif
        }

        return fd_fdstat_set_flags_base(env, fd, flags);
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

