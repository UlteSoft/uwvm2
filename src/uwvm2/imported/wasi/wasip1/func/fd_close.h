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
# include <new>
# include <type_traits>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
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

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.fd_close
    /// @details   __wasi_errno_t fd_close(__wasi_fd_t fd);
    /// @note      Close a file descriptor.

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_close_base(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd) noexcept
    {
        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_ref_t old_wasi_fd{::uwvm2::imported::wasi::wasip1::fd_manager::wasi_no_construct};

        // Prevent operations to obtain the size or perform resizing at this time.
        // Only a lock is required when acquiring the unique pointer for the file descriptor. The lock can be released once the acquisition is complete.
        // Since the file descriptor's location is fixed and accessed via the unique pointer,

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

        // Manipulating fd_manager requires a unique_lock.
        {
            ::uwvm2::utils::mutex::rw_unique_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            // The minimum value in rename_map is greater than opensize.
            if(wasm_fd_storage.opens.size() <= fd_opens_pos)
            {
                // Possibly within the tree being renumbered
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                {
                    // You can simply delete it here. The lock will wait to be destroyed upon unlocking, and all files can be automatically closed via RAII.

                    // In the renumber map, all close positions are initial values and hold no practical significance; they need not be checked.
                    // If this mutex is currently blocked and waiting for another thread, destroying it will result in undefined behavior.
                    // Therefore, locking and unlocking are required to ensure no reads occur. Since the lock handover process for other functions is
                    // well-established, no locking actions occur in other threads during the unlock and erase phases of this lock-unlock-erase sequence. Using
                    // an anonymous struct ensures direct simultaneous execution of lock and unlock operations, while RAII guarantees exception safety.
                    ::uwvm2::utils::mutex::mutex_guard_t{renumber_map_iter->second.fd_p->fd_mutex};

                    // automatically close when erase
                    // There's no need to catch it, because the destructor doesn't throw an exception.

                    // To prevent the system close operation from taking too much time, the close operation is performed outside the fdmanager lock here.
                    auto& curr_fd{*renumber_map_iter->second.fd_p};
                    old_wasi_fd.ptr = curr_fd.wasi_fd.ptr;
                    curr_fd.wasi_fd.ptr = nullptr;

                    wasm_fd_storage.renumber_map.erase(renumber_map_iter);
                }
                else [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                }
            }
            else
            {
                // not nullprt -> File descriptors in the open list require additional handling.
                // nullptr -> File descriptors in the renumber map, simply erase it.
                ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_fd_p;  // no initialize
                // If curr_fd_p is not nullptr, this thread-safety guarantee is required.
                ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

                // The addition here is safe.
                curr_fd_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(curr_fd_p == nullptr) [[unlikely]]
                {
                    // Security issues inherent to virtual machines
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                }
#endif

                // Ensure lock granularity reduction
                curr_fd_release_guard.device_p = ::std::addressof(curr_fd_p->fd_mutex);
                curr_fd_release_guard.lock();

                // During the duration of the dual lock, two operations can be performed simultaneously.

                // Detect double-close: if this fd has already been closed and recorded, report EBADF per WASI.
                // If deleted by renumber_map, it returns ebadf directly when not found, yielding the same result.
                if(curr_fd_p->close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

                // Allocate new storage space for closepos while simultaneously acquiring the position for subsequent writes to the internal fd.
                // new_close_pos must be written separately from -cebgin, otherwise it will cause undefined evaluation order issues. If expansion occurs, it
                // will result in UB.
                auto const new_close_pos{::std::addressof(wasm_fd_storage.closes.emplace_back(fd_opens_pos))};

                // Add the position where it closes itself to facilitate subsequent renumbering.
                // Since it is a vector, its internal iterators are contiguous, allowing direct access to adjacent elements.
                curr_fd_p->close_pos = static_cast<::std::size_t>(new_close_pos - wasm_fd_storage.closes.cbegin());

                // The write lock here must remain locked until the entire file is closed, to prevent subsequent open_at operations from acquiring the actual
                // closing position and causing a use-after-free condition.
                auto& curr_fd{*curr_fd_p};

                // Modify `right` in advance to prevent subsequent exceptions from causing the modification to fail.
                curr_fd.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_t{};
                curr_fd.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_t{};

                // The "close" operation always succeeds. Even if an exception is thrown, it will be reset to the initial value.

                // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files
                // from being processed, making this a virtual machine implementation error.
                if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
                {
// This will be checked at runtime.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }

                // In `sys_close_throw_error`, `fast_io` first sets the file descriptor to -1 regardless of whether `close` succeeds or fails, then throws an
                // exception based on the return value. This means that once `curr_fd.close()` throws an exception, the underlying handle is highly likely
                // already unusable (especially on Linux, where `EINTR` and many error scenarios have effectively closed it). If an exception is thrown here and
                // `eio` is returned directly without marking the fd in `wasm_fd_storage.closes`, updating `close_pos`, or resetting rights, the VM will
                // mistakenly believe the fd remains usable despite its actual unavailability. This creates a state inconsistency. It is neither exception-safe
                // (no fallback possible) nor prevents subsequent operations on the fd from failing. Therefore, no action is taken here.

                // The `reset_type` function does not throw exceptions, and the destructor of `fast_io` also does not throw exceptions. It will not close due to
                // a failed close operation. Use `reset` directly. When creating from the vector of closed positions later, use `create_new`.

                // To prevent the system close operation from taking too much time, the close operation is performed outside the fdmanager lock here.
                old_wasi_fd.ptr = curr_fd.wasi_fd.ptr;
                curr_fd.wasi_fd.ptr = nullptr;
            }

            // After unlocking fds_lock, members within `wasm_fd_storage_t` can no longer be accessed or modified.
        }

        if(old_wasi_fd.ptr != nullptr) { old_wasi_fd.reset(); }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_close(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd) noexcept
    {
        auto const trace_wasip1_call{env.trace_wasip1_call};

        if(trace_wasip1_call) [[unlikely]]
        {
#ifdef UWVM
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"wasip1: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"fd_close",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#else
            ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [info]  wasip1: fd_close(", fd, u8") (wasi-trace)\n");
#endif
        }

        return fd_close_base(env, fd);
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

