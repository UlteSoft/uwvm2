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
// platform
# if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) && __has_include(<dirent.h>) && !defined(_PICOLIBC__)
#  include <unistd.h>
#  include <fcntl.h>
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
# include "posix.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.fd_advise
    /// @details   __wasi_errno_t fd_advise( __wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len, __wasi_advice_t advice);
    /// @note      This function only writes sizes; callers must provide valid memory offsets.
    /// @note      On Darwin platforms, `fd_advise` is non-atomic due to platform limitations. In extreme cases, it may only reserve space in the operating
    ///            system without modifying file attributes, rendering the operation invisible. Within the scope of WASM, however, it is atomic.

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_advise_base(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
        [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_t len,
        ::uwvm2::imported::wasi::wasip1::abi::advice_t advice) noexcept
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
            ::uwvm2::utils::mutex::rw_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

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
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(curr_wasi_fd_t_p == nullptr) [[unlikely]]
            {
                // Security issues inherent to virtual machines
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
            }
#endif

            // Other threads will definitely lock fds_rwlock when performing close operations (since they need to access the fd vector). If the current thread
            // is performing fadvise, no other thread can be executing any close operations simultaneously, eliminating any destruction issues. Therefore,
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

        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_advise) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_advise) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable;
        }

        // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files from
        // being processed, making this a virtual machine implementation error.
        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
        {
// This will be checked at runtime.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
        }

        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
        {
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
            {
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                // Under the wasi semantics, advise returns the correct value for any valid fd.
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
#if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket:
            {
                // Under the wasi semantics, advise returns the correct value for any valid fd.
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
#endif
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

        [[maybe_unused]] auto& file_fd{
#if defined(_WIN32) && !defined(__CYGWIN__)
            curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file
#else
            curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd
#endif
        };

#if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)

        auto const curr_fd_native_handle{file_fd.native_handle()};

        switch(advice)
        {
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_normal:
            {
                [[fallthrough]];
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_dontneed:
            {
                [[fallthrough]];
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_noreuse:
            {
                // None of these are supported on Darwin.
                // In WASI or POSIX semantics, `fd_advise` is merely an advisory hint. It does not and cannot alter the correctness of program logic, so it
                // does not return an error on unsupported platforms.

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_sequential:
            {
                ::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_fd_native_handle, F_RDAHEAD, 1);

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_random:
            {
                ::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_fd_native_handle, F_RDAHEAD, 0);

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_willneed:
            {
                using underlying_filesize_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;

                // The kernel uses only the `ra_offset` and `ra_count` fields from the passed `struct radvisory` for prefetching, without relying on or altering
                // the current position of the file descriptor.
                // Set up pre-fetching suggestions. The return value is not checked here, as this is merely a recommendation.

                if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
                {
                    if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                    {
                        // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                    }
                }

                if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<int>::max())
                {
                    if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<int>::max()) [[unlikely]]
                    {
                        // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                    }
                }

                struct radvisory radvisory_advice{.ra_offset = static_cast<::off_t>(offset), .ra_count = static_cast<int>(len)};

                ::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_fd_native_handle, F_RDADVISE, ::std::addressof(radvisory_advice));

                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
            [[unlikely]] default:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            }
        }

#elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(_WIN32) || defined(__CYGWIN__)) &&             \
    __has_include(<dirent.h>) && !defined(_PICOLIBC__)

        int curr_platform_advice;  // no initialize

        switch(advice)
        {
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_normal:
            {
                curr_platform_advice = POSIX_FADV_NORMAL;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_sequential:
            {
                curr_platform_advice = POSIX_FADV_SEQUENTIAL;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_random:
            {
                curr_platform_advice = POSIX_FADV_RANDOM;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_willneed:
            {
                curr_platform_advice = POSIX_FADV_WILLNEED;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_dontneed:
            {
                curr_platform_advice = POSIX_FADV_DONTNEED;
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_noreuse:
            {
                curr_platform_advice = POSIX_FADV_NOREUSE;
                break;
            }
            [[unlikely]] default:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            }
        }

        // If the file descriptor only requires a single operation that does not need locking, the operating system provides its own built-in lock mechanism.
        auto const curr_fd_native_handle{file_fd.native_handle()};

        using underlying_filesize_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;

// Provide advisory hint; ignore return values and always report success per WASI semantics.
# if defined(__linux__)
        if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint64_t))
        {
            // 64bits platform
#  if defined(__NR_fadvise64)
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint64_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            ::std::uint64_t offset_saturation{static_cast<::std::uint64_t>(offset)};
            ::std::uint64_t len_saturation{static_cast<::std::uint64_t>(len)};

            ::fast_io::system_call<__NR_fadvise64, int>(curr_fd_native_handle, offset_saturation, len_saturation, curr_platform_advice);
#  else
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            ::off_t offset_saturation{static_cast<::off_t>(offset)};
            ::off_t len_saturation{static_cast<::off_t>(len)};

            ::uwvm2::imported::wasi::wasip1::func::posix::posix_fadvise(curr_fd_native_handle, offset_saturation, len_saturation, curr_platform_advice);

#  endif
        }
        else if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint32_t))
        {
            // 32bits platform
#  if (defined(__arm__) || defined(_M_ARM)) && defined(__NR_arm_fadvise64_64)
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint64_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            ::std::uint64_t offset_saturation{static_cast<::std::uint64_t>(offset)};
            ::std::uint32_t offset_saturation_low{static_cast<::std::uint32_t>(offset_saturation)};
            ::std::uint32_t offset_saturation_high{static_cast<::std::uint32_t>(offset_saturation >> 32u)};

            ::std::uint64_t len_saturation{static_cast<::std::uint64_t>(len)};
            ::std::uint32_t len_saturation_low{static_cast<::std::uint32_t>(len_saturation)};
            ::std::uint32_t len_saturation_high{static_cast<::std::uint32_t>(len_saturation >> 32u)};

            if constexpr(::std::endian::native == ::std::endian::big)
            {
                /* 6 args: fd, advice, offset (high, low), len (high, low) */
                ::fast_io::system_call<__NR_arm_fadvise64_64, int>(curr_fd_native_handle,
                                                                   curr_platform_advice,
                                                                   offset_saturation_high,
                                                                   offset_saturation_low,
                                                                   len_saturation_high,
                                                                   len_saturation_low);
            }
            else
            {
                /* 6 args: fd, advice, offset (low, high), len (low, high) */
                ::fast_io::system_call<__NR_arm_fadvise64_64, int>(curr_fd_native_handle,
                                                                   curr_platform_advice,
                                                                   offset_saturation_low,
                                                                   offset_saturation_high,
                                                                   len_saturation_low,
                                                                   len_saturation_high);
            }

#  elif defined(__NR_fadvise64_64)
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint64_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            ::std::uint64_t offset_saturation{static_cast<::std::uint64_t>(offset)};
            ::std::uint32_t offset_saturation_low{static_cast<::std::uint32_t>(offset_saturation)};
            ::std::uint32_t offset_saturation_high{static_cast<::std::uint32_t>(offset_saturation >> 32u)};

            ::std::uint64_t len_saturation{static_cast<::std::uint64_t>(len)};
            ::std::uint32_t len_saturation_low{static_cast<::std::uint32_t>(len_saturation)};
            ::std::uint32_t len_saturation_high{static_cast<::std::uint32_t>(len_saturation >> 32u)};

            if constexpr(::std::endian::native == ::std::endian::big)
            {
#   if (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)) || defined(__XTENSA__)
                /* 6 args: fd, advice, offset (high, low), len (high, low) */
                ::fast_io::system_call<__NR_fadvise64_64, int>(curr_fd_native_handle,
                                                               curr_platform_advice,
                                                               offset_saturation_high,
                                                               offset_saturation_low,
                                                               len_saturation_high,
                                                               len_saturation_low);
#   else
                /* 6 args: fd, offset (high, low), len (high, low), advice */
                ::fast_io::system_call<__NR_fadvise64_64, int>(curr_fd_native_handle,
                                                               offset_saturation_high,
                                                               offset_saturation_low,
                                                               len_saturation_high,
                                                               len_saturation_low,
                                                               curr_platform_advice);
#   endif
            }
            else
            {
#   if (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)) || defined(__XTENSA__)
                /* 6 args: fd, advice, offset (low, high), len (low, high) */
                ::fast_io::system_call<__NR_fadvise64_64, int>(curr_fd_native_handle,
                                                               curr_platform_advice,
                                                               offset_saturation_low,
                                                               offset_saturation_high,
                                                               len_saturation_low,
                                                               len_saturation_high);
#   else
                /* 6 args: fd, offset (low, high), len (low, high), advice */
                ::fast_io::system_call<__NR_fadvise64_64, int>(curr_fd_native_handle,
                                                               offset_saturation_low,
                                                               offset_saturation_high,
                                                               len_saturation_low,
                                                               len_saturation_high,
                                                               curr_platform_advice);
#   endif
            }

#  elif defined(__NR_fadvise64)
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint64_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint32_t>::max())
            {
                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::std::uint32_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            ::std::uint64_t offset_saturation{static_cast<::std::uint64_t>(offset)};
            ::std::uint32_t offset_saturation_low{static_cast<::std::uint32_t>(offset_saturation)};
            ::std::uint32_t offset_saturation_high{static_cast<::std::uint32_t>(offset_saturation >> 32u)};

            ::std::uint32_t len_saturation{static_cast<::std::uint32_t>(len)};

            if constexpr(::std::endian::native == ::std::endian::big)
            {
                /* 5 args: fd, offset (high, low), len, advice */
                ::fast_io::system_call<__NR_fadvise64, int>(curr_fd_native_handle,
                                                            offset_saturation_high,
                                                            offset_saturation_low,
                                                            len_saturation,
                                                            curr_platform_advice);
            }
            else
            {
                /* 5 args: fd, offset (low, high), len, advice */
                ::fast_io::system_call<__NR_fadvise64, int>(curr_fd_native_handle,
                                                            offset_saturation_low,
                                                            offset_saturation_high,
                                                            len_saturation,
                                                            curr_platform_advice);
            }
#  else
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            ::off_t offset_saturation{static_cast<::off_t>(offset)};
            ::off_t len_saturation{static_cast<::off_t>(len)};

            ::uwvm2::imported::wasi::wasip1::func::posix::posix_fadvise(curr_fd_native_handle, offset_saturation, len_saturation, curr_platform_advice);

#  endif
        }
        else
        {
            // unknown linux platform

            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                }
            }

            ::off_t offset_saturation{static_cast<::off_t>(offset)};
            ::off_t len_saturation{static_cast<::off_t>(len)};

            ::uwvm2::imported::wasi::wasip1::func::posix::posix_fadvise(curr_fd_native_handle, offset_saturation, len_saturation, curr_platform_advice);
        }

# else
        // bsd series

        if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
        {
            if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }

            if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                // Ensure no incorrect positions are suggested while preserving the wasi semantics (this is a suggestion).
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
        }

        ::off_t offset_saturation{static_cast<::off_t>(offset)};
        ::off_t len_saturation{static_cast<::off_t>(len)};

        ::uwvm2::imported::wasi::wasip1::func::posix::posix_fadvise(curr_fd_native_handle, offset_saturation, len_saturation, curr_platform_advice);
# endif

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

#else
        // In WASI or POSIX semantics, `fd_advise` is merely an advisory hint. It does not and cannot alter the correctness of program logic, so it does not
        // return an error on unsupported platforms.

        // Whether it's a directory, file, or socket, it returns esuccess.

        switch(advice)
        {
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_normal: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_sequential: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_random: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_willneed: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_dontneed: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::abi::advice_t::advice_noreuse:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            }
            [[unlikely]] default:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            }
        }
#endif
    }

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_advise(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
        ::uwvm2::imported::wasi::wasip1::abi::filesize_t len,
        ::uwvm2::imported::wasi::wasip1::abi::advice_t advice) noexcept
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
                                u8"fd_advise",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(offset)>>>(offset),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(len)>>>(len),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(advice)>>>(advice),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: fd_advise(",
                                fd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(offset)>>>(offset),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(len)>>>(len),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(advice)>>>(advice),
                                u8") (wasi-trace)\n");
#endif
        }

        return fd_advise_base(env, fd, offset, len, advice);
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

