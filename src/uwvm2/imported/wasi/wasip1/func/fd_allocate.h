
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
# include <utility>
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
# endif
# if defined(__CYGWIN__)
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/stat.h>
# endif
# if (defined(__MIPS__) || defined(__mips__) || defined(_MIPS_ARCH))
#  include <sgidefs.h>
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
    /// @brief     WasiPreview1.fd_allocate
    /// @details   __wasi_errno_t fd_allocate(__wasi_fd_t fd, __wasi_filesize_t offset, __wasi_filesize_t len);
    /// @note      Preallocate storage for [offset, offset+len); may extend file size.
    /// @note      On Darwin and Windows platforms, `fd_allocate` is not fully atomic. If allocation succeeds but subsequent file size adjustments fail, the
    ///            allocated space cannot be automatically rolled back. This is a platform limitation, not a code defect.

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_allocate_base(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
        [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_t len) noexcept
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

        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_allocate) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_allocate) [[unlikely]]
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

        [[maybe_unused]] ::fast_io::native_io_observer curr_fd_native_observer{};

        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
        {
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
            [[likely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
            {
                auto& file_fd{
# if defined(_WIN32) && !defined(__CYGWIN__)
                    curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file
# else
                    curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_fd
# endif
                };
                curr_fd_native_observer = file_fd;

                break;
            }
            [[likely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer:
            {
                auto& file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_observer};
                curr_fd_native_observer = file_observer;

                break;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
            }
# if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
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

        using underlying_filesize_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_t>;

        // Trivial success when len == 0
        if(static_cast<underlying_filesize_t>(len) == 0u) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess; }

# if defined(__APPLE__) || defined(__DARWIN_C_LEVEL)
        // Darwin

        auto const& curr_fd_native_file{curr_fd_native_observer};
        auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

        // Do NOT saturate: if the requested size exceeds API limits, report error per WASI semantics.
        if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
        {
            if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            }

            if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            }
        }

        ::off_t fallocate_offset{static_cast<::off_t>(offset)};
        ::off_t fallocate_len{static_cast<::off_t>(len)};

        // macOS: use F_PREALLOCATE / fstore_t
        fstore_t fstore{.fst_flags = F_ALLOCATECONTIG,
                        .fst_posmode = F_VOLPOSMODE,  // the offset parameter in WASI fd_allocate is calculated from the beginning of the file.
                        .fst_offset = fallocate_offset,
                        .fst_length = fallocate_len,
                        .fst_bytesalloc = 0u};

        ::off_t newsize;  // no initialize
        if(::uwvm2::imported::wasi::wasip1::func::add_overflow(fallocate_offset, fallocate_len, newsize)) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
        }

        // Priority Request for Continuous Allocation
        if(::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_fd_native_handle, F_PREALLOCATE, ::std::addressof(fstore)) == -1) [[unlikely]]
        {
            // try non-contiguous allocation
            fstore.fst_flags = F_ALLOCATEALL;
            if(::uwvm2::imported::wasi::wasip1::func::posix::fcntl(curr_fd_native_handle, F_PREALLOCATE, ::std::addressof(fstore)) == -1) [[unlikely]]
            {
                // If preallocation fails, report error directly per WASI semantics
                switch(errno)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                    case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: [[fallthrough]];
#  endif
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
                    case ENXIO: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enxio;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
        }

        // After successful preallocate, ensure file size covers the region
        struct ::stat st;
        if(::uwvm2::imported::wasi::wasip1::func::posix::fstat(curr_fd_native_handle, ::std::addressof(st)) == -1) [[unlikely]]
        {
            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
            switch(errno)
            {
                case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                case EOPNOTSUPP: [[fallthrough]];
#  endif
                case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

        if(st.st_size < newsize) [[unlikely]]
        {
            // Only allowed to extend logical file size when preallocation succeeded
            if(::uwvm2::imported::wasi::wasip1::func::posix::ftruncate(curr_fd_native_handle, newsize) == -1) [[unlikely]]
            {
                switch(errno)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                    case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: [[fallthrough]];
#  endif
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

# elif defined(_WIN32) && !defined(__CYGWIN__)
        // Windows
#  if !defined(__WINE__) && !defined(__BIONIC__) && defined(_WIN32_WINDOWS)
        // Windows 9x
        // Windows 9x does not support any type.
        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
#  elif !defined(_WIN32_WINNT) || _WIN32_WINNT >= 0x600
        // NT Version >= 6.0 (Vista)

        // Windows path. We accept a C runtime descriptor. Convert to HANDLE.
        auto const& curr_native_file{curr_fd_native_observer};
        // Ensure that the handle obtained is for nt. If native is win32 or nt, there will be no overhead.
        auto const curr_nt_io_observer{static_cast<::fast_io::nt_io_observer>(curr_native_file)};
        auto const curr_fd_nt_handle{curr_nt_io_observer.native_handle()};

        underlying_filesize_t allocation_size;  // no initialize

        if(::uwvm2::imported::wasi::wasip1::func::add_overflow(static_cast<underlying_filesize_t>(offset),
                                                               static_cast<underlying_filesize_t>(len),
                                                               allocation_size)) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
        }

        // Attempt to set allocation size using SetFileInformationByHandle (FileAllocationInfo)
        // FILE_ALLOCATION_INFO contains AllocationSize (LARGE_INTEGER)
        // Do NOT saturate: if the requested size exceeds API limits, report error per WASI semantics.
        if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::int_least64_t>::max())
        {
            if(allocation_size > ::std::numeric_limits<::std::int_least64_t>::max()) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            }
        }

        ::fast_io::win32::nt::file_allocation_information faInfo{.AllocationSize = static_cast<::std::int_least64_t>(allocation_size)};
        ::fast_io::win32::nt::io_status_block IoStatusBlock;  // no initialize

        constexpr bool zw{false};
        if(auto const status{::fast_io::win32::nt::nt_set_information_file<zw>(curr_fd_nt_handle,
                                                                               ::std::addressof(IoStatusBlock),
                                                                               ::std::addressof(faInfo),
                                                                               sizeof(::fast_io::win32::nt::file_allocation_information),
                                                                               ::fast_io::win32::nt::file_information_class::FileAllocationInformation)};
           status) [[unlikely]]
        {
            switch(status)
            {
                // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                case 0xC0000008u /*STATUS_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                case 0xC000007Fu /*STATUS_DISK_FULL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                case 0xC0000904u /*STATUS_FILE_TOO_LARGE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                case 0xC000000Du /*STATUS_INVALID_PARAMETER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                case 0xC0000010u /*STATUS_INVALID_DEVICE_REQUEST*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                case 0xC0000022u /*STATUS_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                case 0xC0000185u /*STATUS_IO_DEVICE_ERROR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                case 0xC00000BAu /*STATUS_FILE_IS_A_DIRECTORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

        // Ensure that the logical block is reserved on the physical disk, but it does not necessarily modify the logical size of the file. Therefore, it is
        // necessary to ensure that the logical size of the file covers the entire area.

        // Directly use ntapi to obtain the complete 64-bit file size.
        ::fast_io::win32::nt::file_standard_information fsi;  // no initialize
        ::fast_io::win32::nt::io_status_block block;          // no initialize

        if(::fast_io::win32::nt::nt_query_information_file<zw>(curr_fd_nt_handle,
                                                               ::std::addressof(block),
                                                               ::std::addressof(fsi),
                                                               static_cast<::std::uint_least32_t>(sizeof(::fast_io::win32::nt::file_standard_information)),
                                                               ::fast_io::win32::nt::file_information_class::FileStandardInformation)) [[unlikely]]
        {
            // This may be an unsupported system call or an ID not recognized by the system call. In this case, it is uniformly treated as an unsupported
            // ID.
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
        }

        if(fsi.end_of_file < allocation_size) [[unlikely]]
        {
#   ifdef UWVM_CPP_EXCEPTIONS
            try
#   endif
            {
                truncate(curr_nt_io_observer, static_cast<::std::uintmax_t>(allocation_size));
            }
#   ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error e)
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
#   endif
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

#  else
        // NT Version < 6.0
        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
#  endif

# elif defined(__CYGWIN__)
        // Cygwin (POSIX path)
        auto const& curr_fd_native_file{curr_fd_native_observer};
        auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

        if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
        {
            if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            }

            if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            }
        }

        ::off_t fallocate_offset{static_cast<::off_t>(offset)};
        ::off_t fallocate_len{static_cast<::off_t>(len)};

        int result_pf{::uwvm2::imported::wasi::wasip1::func::posix::posix_fallocate(curr_fd_native_handle, fallocate_offset, fallocate_len)};
        if(result_pf != 0) [[unlikely]]
        {
            switch(result_pf)
            {
                // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
                case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#  if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                case EOPNOTSUPP: [[fallthrough]];
#  endif
                case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

# elif (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !(defined(__MSDOS__) || defined(__DJGPP__)) && !(defined(_WIN32) || defined(__CYGWIN__)) &&            \
     __has_include(<dirent.h>) && !defined(_PICOLIBC__)
        // Prefer posix_fallocate (portable) -- it returns 0 on success or an errno value on failure.
        auto const& curr_fd_native_file{curr_fd_native_observer};
        auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

        // posix_fallocate signature: int posix_fallocate(int fd, ::off_t offset, ::off_t len);
        // But on some platforms ::off_t might be smaller; do saturation.

#  if defined(__linux__) && defined(__NR_fallocate)
        if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint64_t))
        {
            // 64bits platform
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint64_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }
            }

            ::std::uint64_t fallocate_offset{static_cast<::std::uint64_t>(offset)};
            ::std::uint64_t fallocate_len{static_cast<::std::uint64_t>(len)};
            int result_syscall{::fast_io::system_call<__NR_fallocate, int>(curr_fd_native_handle, 0, fallocate_offset, fallocate_len)};

            if(::fast_io::linux_system_call_fails(result_syscall)) [[unlikely]]
            {
                int const err{static_cast<int>(-result_syscall)};
                switch(err)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
                    case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                    case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: [[fallthrough]];
#   endif
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
        }
        else if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint32_t))
        {
            // 32bits platform
#   if (defined(__MIPS__) || defined(__mips__) || defined(_MIPS_ARCH)) && (defined(_MIPS_SIM) && defined(_ABIN32) && _MIPS_SIM == _ABIN32)
            // MIPSN32

            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint64_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }
            }

            ::std::uint64_t fallocate_offset{static_cast<::std::uint64_t>(offset)};

            ::std::uint64_t fallocate_len{static_cast<::std::uint64_t>(len)};

            // N32 ABI: Register width is 64 bits, but pointers/longs remain 32 bits.
            int result_syscall{::fast_io::system_call<__NR_fallocate, int>(curr_fd_native_handle, 0, fallocate_offset, fallocate_len)};

            if(::fast_io::linux_system_call_fails(result_syscall)) [[unlikely]]
            {
                int const err{static_cast<int>(-result_syscall)};
                switch(err)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
                    case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                    case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#    if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: [[fallthrough]];
#    endif
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }

#   else
            // default 32bits platform
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::std::uint64_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::std::uint64_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }
            }

            ::std::uint64_t fallocate_offset{static_cast<::std::uint64_t>(offset)};
            ::std::uint32_t fallocate_offset_low{static_cast<::std::uint32_t>(fallocate_offset)};
            ::std::uint32_t fallocate_offset_high{static_cast<::std::uint32_t>(fallocate_offset >> 32u)};

            ::std::uint64_t fallocate_len{static_cast<::std::uint64_t>(len)};
            ::std::uint32_t fallocate_len_low{static_cast<::std::uint32_t>(fallocate_len)};
            ::std::uint32_t fallocate_len_high{static_cast<::std::uint32_t>(fallocate_len >> 32u)};

            int result_syscall;  // no initlize

            if constexpr(::std::endian::native == ::std::endian::big)
            {
                /* 6 args: fd, offset (high, low), len (high, low) */
                result_syscall = ::fast_io::system_call<__NR_fallocate, int>(curr_fd_native_handle,
                                                                             0,
                                                                             fallocate_offset_high,
                                                                             fallocate_offset_low,
                                                                             fallocate_len_high,
                                                                             fallocate_len_low);
            }
            else
            {
                /* 6 args: fd, offset (low, high), len (low, high) */
                result_syscall = ::fast_io::system_call<__NR_fallocate, int>(curr_fd_native_handle,
                                                                             0,
                                                                             fallocate_offset_low,
                                                                             fallocate_offset_high,
                                                                             fallocate_len_low,
                                                                             fallocate_len_high);
            }

            if(::fast_io::linux_system_call_fails(result_syscall)) [[unlikely]]
            {
                int const err{static_cast<int>(-result_syscall)};
                switch(err)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                    case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#    if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: [[fallthrough]];
#    endif
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }

#   endif
        }
        else
        {
            // unknown linux platform
            if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
            {
                if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }

                if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                }
            }

            ::off_t fallocate_offset{static_cast<::off_t>(offset)};
            ::off_t fallocate_len{static_cast<::off_t>(len)};

            int result_pf{::uwvm2::imported::wasi::wasip1::func::posix::posix_fallocate(curr_fd_native_handle, fallocate_offset, fallocate_len)};
            if(result_pf != 0) [[unlikely]]
            {
                switch(result_pf)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                    case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                    case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                    case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                    case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
                    case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                    case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                    case EOPNOTSUPP: [[fallthrough]];
#   endif
                    case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
            }
        }

#  else
        // bsd series

        if constexpr(::std::numeric_limits<underlying_filesize_t>::max() > ::std::numeric_limits<::off_t>::max())
        {
            if(static_cast<underlying_filesize_t>(offset) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            }

            if(static_cast<underlying_filesize_t>(len) > ::std::numeric_limits<::off_t>::max()) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            }
        }

        ::off_t fallocate_offset{static_cast<::off_t>(offset)};
        ::off_t fallocate_len{static_cast<::off_t>(len)};

        int result_pf{::uwvm2::imported::wasi::wasip1::func::posix::posix_fallocate(curr_fd_native_handle, fallocate_offset, fallocate_len)};
        if(result_pf != 0) [[unlikely]]
        {
            switch(result_pf)
            {
                // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’; here, “eio” is used instead.
                case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
                case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
                case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
                case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                case EOPNOTSUPP: [[fallthrough]];
#   endif
                case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }

#  endif

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;

# else
        // Unsupported platform
        // If the underlying platform cannot guarantee this semantics at all (e.g., it completely lacks fallocate/posix_fallocate, etc.), then this
        // functionality cannot be implemented on that platform.
        // The specification states: Errors must be returned; success must not be simulated.

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
# endif
    }

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_allocate(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
        ::uwvm2::imported::wasi::wasip1::abi::filesize_t len) noexcept
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
                                u8"fd_allocate",
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
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: fd_allocate(",
                                fd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(offset)>>>(offset),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(len)>>>(len),
                                u8") (wasi-trace)\n");
# endif
        }

        return fd_allocate_base(env, fd, offset, len);
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

