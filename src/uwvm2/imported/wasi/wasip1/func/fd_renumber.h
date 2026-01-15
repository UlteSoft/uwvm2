
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
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
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

#ifdef UWVM_IMPORT_WASI_WASIP1

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.fd_renumber
    /// @details   __wasi_errno_t fd_renumber(__wasi_fd_t fd_from, __wasi_fd_t fd_to);
    /// @note      Renumber a file descriptor.

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_renumber_base(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_from,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_to) noexcept
    {
        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(fd_from < 0 || fd_to < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        // Prevent operations to obtain the size or perform resizing at this time.
        // Only a lock is required when acquiring the unique pointer for the file descriptor. The lock can be released once the acquisition is complete.
        // Since the file descriptor's location is fixed and accessed via the unique pointer,

        // Negative states have been excluded, so the conversion result will only be positive numbers.
        using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>;
        auto const unsigned_fd_from{static_cast<unsigned_fd_t>(fd_from)};
        auto const unsigned_fd_to{static_cast<unsigned_fd_t>(fd_to)};

        // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
        {
            if(unsigned_fd_from > size_t_max || unsigned_fd_to > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }
        }

        auto const fd_opens_pos_from{static_cast<::std::size_t>(unsigned_fd_from)};
        auto const fd_opens_pos_to{static_cast<::std::size_t>(unsigned_fd_to)};

        // If same fd, only succeed when fd_from is valid (exists and not closed)
        if(fd_opens_pos_from == fd_opens_pos_to) [[unlikely]]
        {
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};
            bool valid;  // no initialize
            if(wasm_fd_storage.opens.size() > fd_opens_pos_from)
            {
                auto const& from_uniptr{wasm_fd_storage.opens.index_unchecked(fd_opens_pos_from)};
                valid = from_uniptr.fd_p->close_pos == SIZE_MAX;
            }
            else
            {
                valid = wasm_fd_storage.renumber_map.find(fd_from) != wasm_fd_storage.renumber_map.end();
            }
            return valid ? ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess : ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
        }

        // Preserve the move of fd_from
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_fd_p_from;  // no initialize

        // For delayed destruction of displaced fd to avoid UB during lock-held destruction
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* displaced_fd_p{};

        // Before use, it is assumed that there is a gap between open and renumber in the map.
        {
            // If curr_fd_p is not nullptr, this thread-safety guarantee is required.
            ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard_from{};

            // Manipulating fd_manager requires a unique_lock.
            ::uwvm2::utils::mutex::rw_fair_unique_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            // from

            // The minimum value in rename_map is greater than opensize.
            if(wasm_fd_storage.opens.size() <= fd_opens_pos_from)
            {
                // Possibly within the tree being renumbered
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd_from)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                {
                    // You can simply delete it here. The lock will wait to be destroyed upon unlocking, and all files can be automatically closed via RAII.
                    curr_fd_p_from = renumber_map_iter->second.release();

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    if(curr_fd_p_from == nullptr) [[unlikely]]
                    {
                        // Security issues inherent to virtual machines
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                    }
# endif

                    // In the renumber map, all close positions are initial values and hold no practical significance; they need not be checked.
                    // If this mutex is currently blocked and waiting for another thread, destroying it will result in undefined behavior.
                    // Therefore, locking and unlocking are required to ensure no reads occur. Since the lock handover process for other functions is
                    // well-established, no locking actions occur in other threads during the unlock and erase phases of this lock-unlock-erase sequence. Using
                    // an anonymous struct ensures direct simultaneous execution of lock and unlock operations, while RAII guarantees exception safety.
                    curr_fd_release_guard_from.device_p = ::std::addressof(curr_fd_p_from->fd_mutex);
                    curr_fd_release_guard_from.lock();

                    // It has been moved away and can be erased.
                    wasm_fd_storage.renumber_map.erase(renumber_map_iter);
                }
                else [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf;
                }
            }
            else
            {
                // If the file has been closed, it cannot be obtained.
                auto& fd_uniptr{wasm_fd_storage.opens.index_unchecked(fd_opens_pos_from)};
                if(fd_uniptr.fd_p->close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

                // release
                curr_fd_p_from = fd_uniptr.release();

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(curr_fd_p_from == nullptr) [[unlikely]]
                {
                    // Security issues inherent to virtual machines
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                }
# endif

                // Ensure lock granularity reduction
                curr_fd_release_guard_from.device_p = ::std::addressof(curr_fd_p_from->fd_mutex);
                curr_fd_release_guard_from.lock();

                // Construct a new one, then add it to close_pos.
                fd_uniptr.reconstruct();
                auto const new_close_pos_from{::std::addressof(wasm_fd_storage.closes.emplace_back(fd_opens_pos_from))};
                fd_uniptr.fd_p->close_pos = static_cast<::std::size_t>(new_close_pos_from - wasm_fd_storage.closes.cbegin());
            }

            // Ensure that `curr_fd_p_from->close_pos` remains at `SIZE_MAX`, indicating that the file descriptor is active.

            // to

            // The minimum value in rename_map is greater than opensize.
            if(wasm_fd_storage.opens.size() <= fd_opens_pos_to)
            {
                // Possibly within the tree being renumbered
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd_to)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                {
                    // fd_to already exists in renumber_map - need to replace it safely
                    auto* old_to{renumber_map_iter->second.release()};

                    // Wait for any ongoing operations on old_to to complete
                    ::uwvm2::utils::mutex::mutex_guard_t{old_to->fd_mutex};

                    renumber_map_iter->second.clear_destroy_and_assign(curr_fd_p_from);
                    displaced_fd_p = old_to;  // Delay destruction until after fds_lock is released
                }
                else
                {
                    wasm_fd_storage.renumber_map.emplace(fd_to, curr_fd_p_from);

                    // To normalize it, start traversing from renumber. If it's an open size, move it to open vec.
                    auto open_size{wasm_fd_storage.opens.size()};

                    // Use while loop to avoid iterator invalidation during erase
                    for(;;)
                    {
                        auto it2{wasm_fd_storage.renumber_map.find(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t>(open_size))};
                        if(it2 == wasm_fd_storage.renumber_map.end()) { break; }

                        wasm_fd_storage.opens.push_back(it2->second.release());
                        wasm_fd_storage.renumber_map.erase(it2);
                        ++open_size;
                    }
                }
            }
            else
            {
                // to is within opens range - need to replace the existing fd
                auto& to_uniptr{wasm_fd_storage.opens.index_unchecked(fd_opens_pos_to)};
                if(to_uniptr.fd_p->close_pos != SIZE_MAX)
                {
                    // to slot is closed - safe to replace directly, but must fix close_pos indices
                    auto const erased_pos{to_uniptr.fd_p->close_pos};

                    // Remove the close record for this slot
                    wasm_fd_storage.closes.erase_index(erased_pos);

                    // Fix shifted elements' close_pos to their new indices
                    for(::std::size_t i{erased_pos}; i < wasm_fd_storage.closes.size(); ++i)
                    {
                        auto const closed_open_idx{wasm_fd_storage.closes.index_unchecked(i)};
                        auto& closed_uniptr{wasm_fd_storage.opens.index_unchecked(closed_open_idx)};
                        closed_uniptr.fd_p->close_pos = i;
                    }

                    // Replace the fd
                    to_uniptr.clear_destroy_and_assign(curr_fd_p_from);
                }
                else
                {
                    // to slot is active - need to wait for ongoing operations and replace safely
                    auto old_to{to_uniptr.release()};

                    // Wait for any ongoing operations on old_to to complete
                    ::uwvm2::utils::mutex::mutex_guard_t{old_to->fd_mutex};

                    to_uniptr.clear_destroy_and_assign(curr_fd_p_from);
                    displaced_fd_p = old_to;  // Delay destruction until after fds_lock is released
                }
            }

            // After unlocking fds_lock, members within `wasm_fd_storage_t` can no longer be accessed or modified.

            // curr_fd_release_guard_from release
        }

        // Delayed destruction of displaced fd to avoid UB during lock-held destruction
        if(displaced_fd_p) { ::uwvm2::imported::wasi::wasip1::fd_manager::destroy_wasi_fd(displaced_fd_p); }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_renumber(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_from,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_to) noexcept
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
                                u8"fd_renumber",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd_from,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd_to,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(), u8"uwvm: [info]  wasip1: fd_renumber(", fd_from, u8", ", fd_to, u8") (wasi-trace)\n");
# endif
        }

        return fd_renumber_base(env, fd_from, fd_to);
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

