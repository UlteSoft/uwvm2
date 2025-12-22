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
# if __has_include(<errno.h>)
#  include <errno.h>
# endif
# if __has_include(<sys/fcntl.h>)
#  include <sys/fcntl.h>
# endif
# if __has_include(<sys/socket.h>)
#  include <sys/socket.h>
# endif
# if __has_include(<netinet/in.h>)
#  include <netinet/in.h>
# endif
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
#endif

#ifndef UWVM_CPP_EXCEPTIONS
# warning "Without enabling C++ exceptions, using this WASI function may cause termination."
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t sock_send(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t si_data_ptrsz,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t si_data_len,
        ::uwvm2::imported::wasi::wasip1::abi::siflags_t si_flags,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ret_data_len_ptrsz) noexcept
    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(env.wasip1_memory == nullptr) [[unlikely]]
        {
            // Security issues inherent to virtual machines
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
# endif

        auto& memory{*env.wasip1_memory};

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
                                u8"sock_send",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                sock_fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(si_data_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                si_data_len,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(si_flags)>>>(si_flags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(ret_data_len_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: sock_send(",
                                sock_fd,
                                u8", ",
                                ::fast_io::mnp::addrvw(si_data_ptrsz),
                                u8", ",
                                si_data_len,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(si_flags)>>>(si_flags),
                                u8", ",
                                ::fast_io::mnp::addrvw(ret_data_len_ptrsz),
                                u8") (wasi-trace)\n");
# endif
        }

        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(sock_fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebadf; }

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
            auto const unsigned_fd{static_cast<unsigned_fd_t>(sock_fd)};

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
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(sock_fd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
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
            // is performing fdatasync, no other thread can be executing any close operations simultaneously, eliminating any destruction issues. Therefore,
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

        // Rights check: write uses write permission
        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_write) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_write) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable;
        }

        // Currently, no si_flags bits are defined for WASI Preview1. Reject any non-zero flags as invalid.
        if(si_flags != ::uwvm2::imported::wasi::wasip1::abi::siflags_t{}) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval; }

        // check overflow (wasi_ciovec_t)
        constexpr ::std::size_t max_iovs_len{::std::numeric_limits<::std::size_t>::max() / ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_ciovec_t};
        if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() > max_iovs_len)
        {
            if(si_data_len > max_iovs_len) [[unlikely]]
            {
                // Exceeding the platform's maximum limit but not exceeding the wasi limit uses overflow.
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
            }
        }

        // Additionally, ensure that the multiplication `si_data_len * size_of_wasi_ciovec_t` does not overflow within the wasi_size_t domain itself.
        // This prevents situations where, on 32‑bit wasm, the product would overflow wasi_size_t even though it still fits in the host size_t.
        constexpr auto max_iovs_len_wasi{static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(
            ::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() /
            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_ciovec_t))};
        if(si_data_len > max_iovs_len_wasi) [[unlikely]]
        {
            // Exceeding the wasi_size_t domain uses overflow.
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
        }

        // check size_t and get scatter length, the alignment portion also needs to be processed. (fast_io::io_scatter_t)
        constexpr auto max_iovs_len2{(::std::numeric_limits<::std::size_t>::max() - (alignof(::fast_io::io_scatter_t) - 1uz)) /
                                     sizeof(::fast_io::io_scatter_t)};
        if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() > max_iovs_len2)
        {
            if(si_data_len > max_iovs_len2) [[unlikely]]
            {
                // Exceeding the platform's maximum limit but not exceeding the wasi limit uses overflow.
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
            }
        }

        auto const scatter_length{static_cast<::std::size_t>(si_data_len)};

        // After checking all items, determine whether the result equals zero.
        if(scatter_length == 0uz)
        {
            // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already
            // prevents such closed files from being processed, making this a virtual machine implementation error.
            if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
            {
// This will be checked at runtime.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }

            // For directory descriptors, WASI sock_send must report ENOTSOCK even if the iov length is zero.
            switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
            {
                [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
                case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file: [[fallthrough]];
                case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer:
                {
# if defined(_WIN32) && !defined(__CYGWIN__)
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
# else
                    break;
# endif
                }
                [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
                }
# if defined(_WIN32) && !defined(__CYGWIN__)
                case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket: [[fallthrough]];
                case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer:
                {
                    break;
                }
# endif
            }

            // For other descriptor types, a zero-length send is a successful no-op with nwritten=0.
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(
                memory,
                ret_data_len_ptrsz,
                static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(0uz));

            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
        }

        // check memory bounds
        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm32(memory,
                                                                            si_data_ptrsz,
                                                                            si_data_len * ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_ciovec_t);

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
# if defined(_WIN32) && !defined(__CYGWIN__)
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
# else
                // If using reassignment
                ::uwvm2::imported::wasi::wasip1::func::fast_io_io_scatter_t_allocator_guard tmp_allocator_guard{};

                ::fast_io::io_scatter_t* scatter_base;  // no initialize

                if(scatter_length < 1024uz / sizeof(::fast_io::io_scatter_t)) [[likely]]
                {
                    // may alias
                    using fast_io_io_scatter_t_may_alias UWVM_GNU_MAY_ALIAS = ::fast_io::io_scatter_t*;

                    // When the bytes count less than 1024, it exists on the stack and is highly efficient.
                    auto scatter_alloca_guaranteed_bytes_count{scatter_length * sizeof(::fast_io::io_scatter_t)};
                    auto scatter_alloca_bytes_count{scatter_alloca_guaranteed_bytes_count + (alignof(::fast_io::io_scatter_t) - 1uz)};

#  if UWVM_HAS_BUILTIN(__builtin_alloca)
                    auto tmp_scatter_base_unaligned{__builtin_alloca(scatter_alloca_bytes_count)};
                    scatter_base = reinterpret_cast<fast_io_io_scatter_t_may_alias>(::std::align(alignof(::fast_io::io_scatter_t),
                                                                                                 scatter_alloca_guaranteed_bytes_count,
                                                                                                 tmp_scatter_base_unaligned,
                                                                                                 scatter_alloca_bytes_count));
#  elif defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__)
                    auto tmp_scatter_base_unaligned{_alloca(scatter_alloca_bytes_count)};
                    scatter_base = reinterpret_cast<fast_io_io_scatter_t_may_alias>(::std::align(alignof(::fast_io::io_scatter_t),
                                                                                                 scatter_alloca_guaranteed_bytes_count,
                                                                                                 tmp_scatter_base_unaligned,
                                                                                                 scatter_alloca_bytes_count));
#  else
                    auto tmp_scatter_base_unaligned{alloca(scatter_alloca_bytes_count)};
                    scatter_base = reinterpret_cast<fast_io_io_scatter_t_may_alias>(::std::align(alignof(::fast_io::io_scatter_t),
                                                                                                 scatter_alloca_guaranteed_bytes_count,
                                                                                                 tmp_scatter_base_unaligned,
                                                                                                 scatter_alloca_bytes_count));
#  endif

                    // std::align may return nullptr
                    if(scatter_base == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                }
                else
                {
                    // When the bytes count is greater than 1024, it exists on the heap.
                    // The allocator crashes upon failure.
                    scatter_base = ::uwvm2::imported::wasi::wasip1::func::fast_io_io_scatter_t_allocator_guard::allocator::allocate(scatter_length);

                    // Start the raii guard to deallocate the memory.
                    tmp_allocator_guard.ptr = scatter_base;
                }

                // Full locking is required during writing.
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                // get scatter base
                auto iovs_curr{si_data_ptrsz};

                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t length_counter{};

                for(::std::size_t i{}; i != scatter_length; ++i)
                {
                    ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t wasm_base;  // no initialize
                    ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t wasm_len;       // no initialize

                    if constexpr(::uwvm2::imported::wasi::wasip1::abi::is_default_wasi_ciovec_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::abi::wasi_ciovec_t tmp_iovec;  // no initialize

                        // iovs_len has been checked.
                        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32_unchecked_unlocked(
                            memory,
                            iovs_curr,
                            reinterpret_cast<::std::byte*>(::std::addressof(tmp_iovec)),
                            reinterpret_cast<::std::byte*>(::std::addressof(tmp_iovec)) + sizeof(::uwvm2::imported::wasi::wasip1::abi::wasi_ciovec_t));
                        iovs_curr += ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_ciovec_t;

                        wasm_base = tmp_iovec.buf;
                        wasm_len = tmp_iovec.buf_len;
                    }
                    else
                    {
                        wasm_base = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(memory, iovs_curr);
                        iovs_curr += ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_void_ptr_t;

                        wasm_len = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, iovs_curr);
                        iovs_curr += ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_size_t;
                    }

                    // It is necessary to verify whether the memory referenced within the WASM is sufficient.
                    ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm32_unlocked(memory, wasm_base, wasm_len);

                    auto& curr_tmp_scatter_base{scatter_base[i]};

                    // Already verified as legitimate
                    curr_tmp_scatter_base.base = memory.memory_begin + wasm_base;

                    // check counter
                    if(wasm_len > ::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() - length_counter) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }

                    length_counter += wasm_len;

                    if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() >
                                 ::std::numeric_limits<::fast_io::intfpos_t>::max())
                    {
                        if(length_counter > ::std::numeric_limits<::fast_io::intfpos_t>::max()) [[unlikely]]
                        {
                            // Exceeding the platform's maximum limit but not exceeding the wasi limit uses overflow.
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        }
                    }

                    // Conversion requires verification.
                    if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() > ::std::numeric_limits<::std::size_t>::max())
                    {
                        if(wasm_len > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]]
                        {
                            // Exceeding the platform's maximum limit but not exceeding the wasi limit uses overflow.
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        }
                    }

                    curr_tmp_scatter_base.len = static_cast<::std::size_t>(wasm_len);

#  if CHAR_BIT != 8
#   if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    auto const curr_tmp_scatter_base_base{reinterpret_cast<::std::byte const*>(curr_tmp_scatter_base.base)};
                    auto const curr_tmp_scatter_base_end{curr_tmp_scatter_base_base + curr_tmp_scatter_base.len};
                    for(auto curr{curr_tmp_scatter_base_base}; curr != curr_tmp_scatter_base_end; ++curr)
                    {
                        if(::std::to_integer<unsigned>(*curr) & ~0xFFu != 0u) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
                    }
#   endif
#  endif
                }

                ::fast_io::native_io_observer curr_fd_native_observer{};

                bool const is_file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type ==
                                            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer};
                if(is_file_observer)
                {
                    auto& file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_observer};
                    curr_fd_native_observer = file_observer;
                }
                else
                {
                    auto& file_fd{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd};
                    curr_fd_native_observer = file_fd;
                }

                auto const& curr_fd_native_file{curr_fd_native_observer};

                int const native_fd{curr_fd_native_file.native_handle()};

                // ::fast_io::io_scatter_t -> struct ::iovec
                using iovec_may_alias UWVM_GNU_MAY_ALIAS = struct ::iovec*;
                static_assert(sizeof(struct ::iovec) == sizeof(::fast_io::io_scatter_t) && alignof(struct ::iovec) == alignof(::fast_io::io_scatter_t) &&
                              __builtin_offsetof(struct ::iovec, iov_base) == __builtin_offsetof(::fast_io::io_scatter_t, base) &&
                              __builtin_offsetof(struct ::iovec, iov_len) == __builtin_offsetof(::fast_io::io_scatter_t, len) &&
                              sizeof(decltype(::std::declval<struct ::iovec>().iov_base)) == sizeof(decltype(::std::declval<::fast_io::io_scatter_t>().base)) &&
                              sizeof(decltype(::std::declval<struct ::iovec>().iov_len)) == sizeof(decltype(::std::declval<::fast_io::io_scatter_t>().len)));

                struct ::msghdr msg{};
                msg.msg_iov = reinterpret_cast<iovec_may_alias>(scatter_base);

                if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<decltype(msg.msg_iovlen)>::max())
                {
                    if(scatter_length > ::std::numeric_limits<decltype(msg.msg_iovlen)>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                msg.msg_iovlen = static_cast<decltype(msg.msg_iovlen)>(scatter_length);

                int send_flags{};

                // No POSIX-level flags corresponding to WASI si_flags are defined yet. However, to avoid delivering SIGPIPE
                // to the host process when writing to a closed peer, use MSG_NOSIGNAL where available.
#  ifdef MSG_NOSIGNAL
                send_flags |= MSG_NOSIGNAL;
#  endif

                auto const send_res{::uwvm2::imported::wasi::wasip1::func::posix::sendmsg(native_fd, ::std::addressof(msg), send_flags)};

                if(send_res < 0) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(
                        ::fast_io::error{::fast_io::posix_domain_value, static_cast<::fast_io::error::value_type>(static_cast<unsigned>(errno))});
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(
                    memory,
                    ret_data_len_ptrsz,
                    static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(send_res));

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
# endif
            }
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
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

                auto const& curr_fd_native_file{curr_socket_observer};

                // Full locking is required during writing.
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                using wsabuf_t = ::fast_io::win32::wsabuf;
                using wsabuf_may_alias UWVM_GNU_MAY_ALIAS = wsabuf_t*;

                wsabuf_t* wsabufs;  // no initialize

                struct wsabuf_allocator_guard_t
                {
                    using allocator = ::fast_io::native_typed_thread_local_allocator<wsabuf_t>;
                    wsabuf_t* ptr{};

                    inline constexpr ~wsabuf_allocator_guard_t() noexcept { allocator::deallocate(ptr); }
                } wsabuf_guard{};

                if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<::std::uint_least32_t>::max())
                {
                    if(scatter_length > ::std::numeric_limits<::std::uint_least32_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                    }
                }

                if(scatter_length < 1024uz / sizeof(wsabuf_t)) [[likely]]
                {
                    auto wsabuf_alloca_guaranteed_bytes_count{scatter_length * sizeof(wsabuf_t)};
                    auto wsabuf_alloca_bytes_count{wsabuf_alloca_guaranteed_bytes_count + (alignof(wsabuf_t) - 1uz)};

#  if UWVM_HAS_BUILTIN(__builtin_alloca)
                    auto tmp_wsabuf_base_unaligned{__builtin_alloca(wsabuf_alloca_bytes_count)};
                    wsabufs = reinterpret_cast<wsabuf_may_alias>(
                        ::std::align(alignof(wsabuf_t), wsabuf_alloca_guaranteed_bytes_count, tmp_wsabuf_base_unaligned, wsabuf_alloca_bytes_count));
#  elif defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__)
                    auto tmp_wsabuf_base_unaligned{_alloca(wsabuf_alloca_bytes_count)};
                    wsabufs = reinterpret_cast<wsabuf_may_alias>(
                        ::std::align(alignof(wsabuf_t), wsabuf_alloca_guaranteed_bytes_count, tmp_wsabuf_base_unaligned, wsabuf_alloca_bytes_count));
#  else
                    auto tmp_wsabuf_base_unaligned{alloca(wsabuf_alloca_bytes_count)};
                    wsabufs = reinterpret_cast<wsabuf_may_alias>(
                        ::std::align(alignof(wsabuf_t), wsabuf_alloca_guaranteed_bytes_count, tmp_wsabuf_base_unaligned, wsabuf_alloca_bytes_count));
#  endif

                    if(wsabufs == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                }
                else
                {
                    wsabufs = wsabuf_allocator_guard_t::allocator::allocate(scatter_length);
                    wsabuf_guard.ptr = wsabufs;
                }

                auto iovs_curr{si_data_ptrsz};

                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t length_counter{};

                for(::std::size_t i{}; i != scatter_length; ++i)
                {
                    ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t wasm_base;  // no initialize
                    ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t wasm_len;       // no initialize

                    if constexpr(::uwvm2::imported::wasi::wasip1::abi::is_default_wasi_ciovec_data_layout())
                    {
                        ::uwvm2::imported::wasi::wasip1::abi::wasi_ciovec_t tmp_iovec;  // no initialize

                        ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm32_unchecked_unlocked(
                            memory,
                            iovs_curr,
                            reinterpret_cast<::std::byte*>(::std::addressof(tmp_iovec)),
                            reinterpret_cast<::std::byte*>(::std::addressof(tmp_iovec)) + sizeof(::uwvm2::imported::wasi::wasip1::abi::wasi_ciovec_t));
                        iovs_curr += ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_ciovec_t;

                        wasm_base = tmp_iovec.buf;
                        wasm_len = tmp_iovec.buf_len;
                    }
                    else
                    {
                        wasm_base = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(memory, iovs_curr);
                        iovs_curr += ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_void_ptr_t;

                        wasm_len = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32_unchecked_unlocked<
                            ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(memory, iovs_curr);
                        iovs_curr += ::uwvm2::imported::wasi::wasip1::abi::size_of_wasi_size_t;
                    }

                    ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm32_unlocked(memory, wasm_base, wasm_len);

                    if(wasm_len > ::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() - length_counter) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }

                    length_counter += wasm_len;

                    if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>::max() >
                                 ::std::numeric_limits<::std::uint_least32_t>::max())
                    {
                        if(wasm_len > ::std::numeric_limits<::std::uint_least32_t>::max()) [[unlikely]]
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        }
                    }

                    wsabufs[i].buf = reinterpret_cast<char*>(memory.memory_begin + wasm_base);
                    wsabufs[i].len = static_cast<::std::uint_least32_t>(wasm_len);
                }

                ::std::uint_least32_t bytes_sent{};

                auto const socket_handle{curr_fd_native_file.native_handle()};

                auto const wsa_ret{::fast_io::win32::WSASend(socket_handle,
                                                             wsabufs,
                                                             static_cast<::std::uint_least32_t>(scatter_length),
                                                             ::std::addressof(bytes_sent),
                                                             0u,
                                                             nullptr,
                                                             nullptr)};

                if(wsa_ret != 0) [[unlikely]]
                {
                    auto const wsa_err{static_cast<::std::uint_least32_t>(::fast_io::win32::WSAGetLastError())};

                    switch(wsa_err)
                    {
                        // Winsock (WSA*) error codes mapping
                        case 10004uz /*WSAEINTR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                        // align with ebadf policy
                        case 10009uz /*WSAEBADF*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10013uz /*WSAEACCES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                        case 10014uz /*WSAEFAULT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
                        case 10022uz /*WSAEINVAL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        case 10024uz /*WSAEMFILE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emfile;
                        case 10035uz /*WSAEWOULDBLOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                        case 10036uz /*WSAEINPROGRESS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einprogress;
                        case 10037uz /*WSAEALREADY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ealready;
                        case 10038uz /*WSAENOTSOCK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsock;
                        case 10039uz /*WSAEDESTADDRREQ*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edestaddrreq;
                        case 10040uz /*WSAEMSGSIZE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emsgsize;
                        case 10041uz /*WSAEPROTOTYPE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eprototype;
                        case 10042uz /*WSAENOPROTOOPT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoprotoopt;
                        case 10043uz /*WSAEPROTONOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eprotonosupport;
                        case 10044uz /*WSAESOCKTNOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case 10045uz /*WSAEOPNOTSUPP*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case 10046uz /*WSAEPFNOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eafnosupport;
                        case 10047uz /*WSAEAFNOSUPPORT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eafnosupport;
                        case 10048uz /*WSAEADDRINUSE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eaddrinuse;
                        case 10049uz /*WSAEADDRNOTAVAIL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eaddrnotavail;
                        case 10050uz /*WSAENETDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetdown;
                        case 10051uz /*WSAENETUNREACH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetunreach;
                        case 10052uz /*WSAENETRESET*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetreset;
                        case 10053uz /*WSAECONNABORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnaborted;
                        case 10054uz /*WSAECONNRESET*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnreset;
                        case 10055uz /*WSAENOBUFS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enobufs;
                        case 10056uz /*WSAEISCONN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisconn;
                        case 10057uz /*WSAENOTCONN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotconn;
                        case 10058uz /*WSAESHUTDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotconn;
                        case 10059uz /*WSAETOOMANYREFS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10060uz /*WSAETIMEDOUT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::etimedout;
                        case 10061uz /*WSAECONNREFUSED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnrefused;
                        case 10062uz /*WSAELOOP*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop;
                        case 10063uz /*WSAENAMETOOLONG*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enametoolong;
                        case 10064uz /*WSAEHOSTDOWN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ehostunreach;
                        case 10065uz /*WSAEHOSTUNREACH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ehostunreach;
                        case 10066uz /*WSAENOTEMPTY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotempty;
                        case 10067uz /*WSAEPROCLIM*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                        case 10068uz /*WSAEUSERS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                        case 10069uz /*WSAEDQUOT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                        case 10070uz /*WSAESTALE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::estale;
                        case 10071uz /*WSAEREMOTE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10091uz /*WSASYSNOTREADY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enetdown;
                        case 10092uz /*WSAVERNOTSUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                        case 10093uz /*WSANOTINITIALISED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                        case 10101uz /*WSAEDISCON*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnreset;
                        case 10102uz /*WSAENOMORE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10103uz /*WSAECANCELLED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ecanceled;
                        case 10104uz /*WSAEINVALIDPROCTABLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10105uz /*WSAEINVALIDPROVIDER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10106uz /*WSAEPROVIDERFAILEDINIT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10107uz /*WSASYSCALLFAILURE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10108uz /*WSASERVICE_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                        case 10109uz /*WSATYPE_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                        case 10110uz /*WSA_E_NO_MORE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                        case 10111uz /*WSA_E_CANCELLED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ecanceled;
                        case 10112uz /*WSAEREFUSED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnrefused;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    }
                }

                ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unlocked(
                    memory,
                    ret_data_len_ptrsz,
                    static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_size_t>(bytes_sent));

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
    }
}

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
