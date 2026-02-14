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
# if !defined(_WIN32)
#  include <errno.h>
#  include <unistd.h>
#  include <sys/stat.h>
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

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_WASM64)

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.fd_tell_wasm64
    /// @details   __wasi_errno_t __wasi_fd_tell(__wasi_fd_t fd, __wasi_void_ptr_t offset);
    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t fd_tell_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t tell_ptrsz) noexcept
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
                                u8"fd_tell_wasm64",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(tell_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: fd_tell_wasm64(",
                                fd,
                                u8", ",
                                ::fast_io::mnp::addrvw(tell_ptrsz),
                                u8") (wasi-trace)\n");
# endif
        }

        if(fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p;  // no initialize

        ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

        {
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t>;
            auto const unsigned_fd{static_cast<unsigned_fd_t>(fd)};

            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
            {
                if(unsigned_fd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }
            }

            auto const fd_opens_pos{static_cast<::std::size_t>(unsigned_fd)};

            if(wasm_fd_storage.opens.size() <= fd_opens_pos)
            {
                if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(fd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
                {
                    curr_wasi_fd_t_p = renumber_map_iter->second.fd_p;
                }
                else [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf;
                }
            }
            else
            {
                curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(fd_opens_pos).fd_p;
            }

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(curr_wasi_fd_t_p == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif

            curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
            curr_fd_release_guard.lock();
        }

        auto& curr_fd{*curr_wasi_fd_t_p};

        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_tell) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_tell) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable;
        }

        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
        {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
        }

        switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
        {
            [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
            }
            [[likely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                [[fallthrough]];
            [[likely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer:
            {
                break;
            }
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
            }
# if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
            }
# endif
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
            }
        }

        ::fast_io::native_io_observer curr_fd_native_observer{};

        bool const is_file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type == ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer};
        if(is_file_observer)
        {
            auto& file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.file_observer};
            curr_fd_native_observer = file_observer;
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

        ::fast_io::intfpos_t tell_fpos;  // no initialize

# if defined(_WIN32) && !defined(__CYGWIN__)

#  ifdef UWVM_CPP_EXCEPTIONS
        try
#  endif
        {
            tell_fpos = ::fast_io::operations::io_stream_seek_bytes(curr_fd_native_observer, 0, ::fast_io::seekdir::cur);
        }
#  ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error e)
        {
            switch(e.domain)
            {
                case ::fast_io::win32_domain_value:
                {
                    switch(e.code)
                    {
                        case 6uz /*ERROR_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 5uz /*ERROR_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                        case 1uz /*ERROR_INVALID_FUNCTION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                        case 50uz /*ERROR_NOT_SUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup;
                        case 19uz /*ERROR_WRITE_PROTECT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::erofs;
                        case 87uz /*ERROR_INVALID_PARAMETER*/:
                        {
                            if((::fast_io::win32::GetFileType(curr_fd_native_observer.native_handle()) & 0xFFFF7FFFu) == 3 /*FILE_TYPE_PIPE*/)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                            }
                            else
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                            }
                        }
                        case 1117uz /*ERROR_IO_DEVICE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 31uz /*ERROR_GEN_FAILURE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 1784uz /*ERROR_INVALID_USER_BUFFER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::efault;
                        case 2uz /*ERROR_FILE_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent;
                        case 3uz /*ERROR_PATH_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent;
                        case 4uz /*ERROR_TOO_MANY_OPEN_FILES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile;
                        case 24uz /*ERROR_BAD_LENGTH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                        case 25uz /*ERROR_SEEK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                        case 21uz /*ERROR_NOT_READY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 38uz /*ERROR_HANDLE_EOF*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 112uz /*ERROR_DISK_FULL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enospc;
                        case 12uz /*ERROR_INVALID_ACCESS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                        case 32uz /*ERROR_SHARING_VIOLATION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebusy;
                        case 33uz /*ERROR_LOCK_VIOLATION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eagain;
                        case 109uz /*ERROR_BROKEN_PIPE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::epipe;
                        case 232uz /*ERROR_NO_DATA*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::epipe;
                        case 8uz /*ERROR_NOT_ENOUGH_MEMORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enomem;
                        case 14uz /*ERROR_OUTOFMEMORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enomem;
                        case 267uz /*ERROR_DIRECTORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                    }

                    break;
                }
#   if !defined(_WIN32_WINDOWS)
                case ::fast_io::nt_domain_value:
                {
                    static_assert(sizeof(::fast_io::error::value_type) >= sizeof(::std::uint_least32_t));
                    switch(e.code)
                    {
                        case 0xC0000008uz /*STATUS_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 0xC0000022uz /*STATUS_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                        case 0xC0000002uz /*STATUS_NOT_IMPLEMENTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enosys;
                        case 0xC00000BBuz /*STATUS_NOT_SUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup;
                        case 0xC00000A2uz /*STATUS_MEDIA_WRITE_PROTECTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::erofs;
                        case 0xC000000Duz /*STATUS_INVALID_PARAMETER*/:
                        {
                            ::fast_io::win32::nt::io_status_block isb;       // no initialize
                            ::fast_io::win32::nt::file_fs_device_type ffdt;  // no initialize

                            constexpr bool zw{false};
                            auto const status{::fast_io::win32::nt::nt_query_volume_information_file<zw>(
                                curr_fd_native_observer.native_handle(),
                                ::std::addressof(isb),
                                ::std::addressof(ffdt),
                                static_cast<::std::uint_least32_t>(sizeof(ffdt)),
                                ::fast_io::win32::nt::fs_information_class::FileFsDeviceInformation)};

                            switch(status)
                            {
                                [[likely]] case 0x00000000u:
                                    break;
                                case 0xC0000010u /*STATUS_INVALID_DEVICE_REQUEST*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                            }

                            if(ffdt.DeviceType == 0x00000011u /*FILE_DEVICE_NAMED_PIPE*/)
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                            }

                            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                        }
                        case 0xC0000185uz /*STATUS_IO_DEVICE_ERROR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 0xC0000001uz /*STATUS_UNSUCCESSFUL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 0xC00000E8uz /*STATUS_INVALID_USER_BUFFER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::efault;
                        case 0xC0000034uz /* STATUS_OBJECT_NAME_NOT_FOUND */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent;
                        case 0xC000003Auz /* STATUS_OBJECT_PATH_NOT_FOUND */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent;
                        case 0xC0000043uz /* STATUS_SHARING_VIOLATION */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebusy;
                        case 0xC0000054uz /* STATUS_FILE_LOCK_CONFLICT */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eagain;
                        case 0xC000007Fuz /* STATUS_DISK_FULL */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enospc;
                        case 0xC0000010uz /* STATUS_INVALID_DEVICE_REQUEST */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                        case 0xC0000011uz /* STATUS_END_OF_FILE */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 0xC0000013uz /* STATUS_NO_MEDIA_IN_DEVICE */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent;
                        case 0xC000000Euz /* STATUS_NO_SUCH_DEVICE */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent;
                        case 0xC0000101uz /* STATUS_DIRECTORY_NOT_EMPTY */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotempty;
                        case 0xC0000281uz /* STATUS_DIRECTORY_IS_A_REPARSE_POINT */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                        case 0xC00000BAuz /* STATUS_FILE_IS_A_DIRECTORY */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                        case 0xC000009Auz /* STATUS_INSUFFICIENT_RESOURCES */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enomem;
                        case 0xC0000017uz /* STATUS_NO_MEMORY */: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enomem;
                        default: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                    }

                    break;
                }
#   endif
                [[unlikely]] default:
                {
#   if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#   endif
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                }
            }
        }
#  endif
# else
#  ifdef UWVM_CPP_EXCEPTIONS
        try
#  endif
        {
            tell_fpos = ::fast_io::operations::io_stream_seek_bytes(curr_fd_native_observer, 0, ::fast_io::seekdir::cur);
        }
#  ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error e)
        {
            switch(e.code)
            {
                case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enospc;
                case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::efbig;
                case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm;
                case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
                case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::erofs;
#   if defined(EDQUOT)
                case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::edquot;
#   endif
                case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eintr;
#   if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EAGAIN != EWOULDBLOCK))
                case EWOULDBLOCK: [[fallthrough]];
#   endif
                case EAGAIN: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eagain;
                case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::espipe;
#   if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
                case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup;
#   endif
#   if defined(ENOTSUP)
                case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup;
#   endif
                case EFAULT: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::efault;
                case ENXIO: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enxio;
                case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enodev;
                case ENOENT: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enoent;
                case ENOTDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir;
                case EIO: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                case ENOMEM: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enomem;
                case EOVERFLOW: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eoverflow;
#   if defined(ETIMEDOUT)
                case ETIMEDOUT: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::etimedout;
#   endif
#   if defined(ECONNRESET)
                case ECONNRESET: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::econnreset;
#   endif
#   if defined(EPIPE)
                case EPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::epipe;
#   endif
                default: return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
            }
        }
#  endif

# endif

        [[assume(tell_fpos >= 0)]];

        using underlying_new_offset_type_t = ::std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t>;

        if constexpr(::std::numeric_limits<::fast_io::intfpos_t>::max() > ::std::numeric_limits<underlying_new_offset_type_t>::max())
        {
            if(tell_fpos > ::std::numeric_limits<underlying_new_offset_type_t>::max())
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eoverflow;
            }
        }

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        tell_ptrsz,
                                                                                        static_cast<underlying_new_offset_type_t>(tell_fpos));

        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess;
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

