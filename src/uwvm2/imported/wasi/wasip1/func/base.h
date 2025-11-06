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
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
// platform
# if !defined(_WIN32)
#  include <errno.h>
# endif
// import
# include <fast_io.h>
# include <fast_io_device.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
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
    template <::std::integral I>
        requires (::std::signed_integral<I>)
    inline constexpr bool add_overflow(I a, I b, I & result) noexcept
    {
#if UWVM_HAS_BUILTIN(__builtin_add_overflow)
        return __builtin_add_overflow(a, b, ::std::addressof(result));
#else
        // The result of signed addition is undefined behavior.
        // But the results still have to be provided.

        if(b > 0 && a > ::std::numeric_limits<I>::max() - b)
        {
            // overflow
            // The behavior of __builtin_add_overflow is well-defined as overflow wrap-around, so an unsigned implementation is used here.
            using unsigned_I = ::std::make_unsigned_t<I>;
            result = static_cast<I>(static_cast<unsigned_I>(a) + static_cast<unsigned_I>(b));
            return true;
        }

        if(b < 0 && a < ::std::numeric_limits<I>::min() - b)
        {
            // underflow
            // The behavior of __builtin_add_overflow is well-defined as overflow wrap-around, so an unsigned implementation is used here.
            using unsigned_I = ::std::make_unsigned_t<I>;
            result = static_cast<I>(static_cast<unsigned_I>(a) + static_cast<unsigned_I>(b));
            return true;
        }

        // default, no overflow
        result = a + b;
        return false;
#endif
    }

    template <::std::integral I>
        requires (::std::unsigned_integral<I>)
    inline constexpr bool add_overflow(I a, I b, I & result) noexcept
    {
#if defined(_MSC_VER) && !defined(__clang__)
        if UWVM_IF_NOT_CONSTEVAL
        {
# if defined(_M_X64)
            if constexpr(::std::same_as<I, ::std::uint64_t>)
            {
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                return ::fast_io::intrinsics::msvc::x86::_addcarry_u64(false, a, b, ::std::addressof(result));
            }
            else if constexpr(::std::same_as<I, ::std::uint32_t>)
            {
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                return ::fast_io::intrinsics::msvc::x86::_addcarry_u32(false, a, b, ::std::addressof(result));
            }
            else if constexpr(::std::same_as<I, ::std::uint16_t>)
            {
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                return ::fast_io::intrinsics::msvc::x86::_addcarry_u16(false, a, b, ::std::addressof(result));
            }
            else if constexpr(::std::same_as<I, ::std::uint8_t>)
            {
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                return ::fast_io::intrinsics::msvc::x86::_addcarry_u8(false, a, b, ::std::addressof(result));
            }
            else
            {
                // msvc's `_addcarry_u32` does not specialize for `unsigned long`
                result = static_cast<I>(a + b);
                return result < a;
            }

# elif defined(_M_X32)
            if constexpr(::std::same_as<I, ::std::uint32_t>)
            {
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                return ::fast_io::intrinsics::msvc::x86::_addcarry_u32(false, a, b, ::std::addressof(result));
            }
            else if constexpr(::std::same_as<I, ::std::uint16_t>)
            {
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                return ::fast_io::intrinsics::msvc::x86::_addcarry_u16(false, a, b, ::std::addressof(result));
            }
            else if constexpr(::std::same_as<I, ::std::uint8_t>)
            {
                // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
                return ::fast_io::intrinsics::msvc::x86::_addcarry_u8(false, a, b, ::std::addressof(result));
            }
            else
            {
                result = static_cast<I>(a + b);
                return result < a;
            }

# else  // ARM, ARM64, ARM64ec
            result = static_cast<I>(a + b);
            return result < a;
# endif
        }
        else
        {
            result = static_cast<I>(a + b);
            return result < a;
        }

#elif UWVM_HAS_BUILTIN(__builtin_add_overflow)
        // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
        return __builtin_add_overflow(a, b, ::std::addressof(result));

#else
        result = static_cast<I>(a + b);
        return result < a;
#endif
    }

    struct fast_io_io_scatter_t_allocator_guard
    {
        using allocator = ::fast_io::native_typed_global_allocator<::fast_io::io_scatter_t>;
        ::fast_io::io_scatter_t* ptr{};

        inline constexpr fast_io_io_scatter_t_allocator_guard() noexcept = default;

        inline constexpr fast_io_io_scatter_t_allocator_guard(::fast_io::io_scatter_t* o_ptr) noexcept : ptr{o_ptr} {}

        fast_io_io_scatter_t_allocator_guard(fast_io_io_scatter_t_allocator_guard const&) = delete;
        fast_io_io_scatter_t_allocator_guard& operator= (fast_io_io_scatter_t_allocator_guard const&) = delete;

        fast_io_io_scatter_t_allocator_guard(fast_io_io_scatter_t_allocator_guard&& other) = delete;
        fast_io_io_scatter_t_allocator_guard& operator= (fast_io_io_scatter_t_allocator_guard&& other) = delete;

        inline constexpr ~fast_io_io_scatter_t_allocator_guard()
        {
            // The deallocator performs internal null pointer checks.
            allocator::deallocate(ptr);
        }
    };

    // The path series functions of wasi cannot traverse paths beyond the specified path, not the preopen path.

    enum class dir_type_e : ::std::uint_fast8_t
    {
        curr,
        prev,
        next,
    };

    struct split_path_t
    {
        ::uwvm2::utils::container::u8string next_name{};
        dir_type_e dir_type{};
    };

    struct split_path_res_t
    {
        ::uwvm2::utils::container::vector<split_path_t> res{};
        bool is_absolute{};
    };

    /// @note This function is not compatible with Windows, as Windows uses both forward slashes and backslashes as path separators. If you are using Windows,
    /// you must add a backslash check for each subsequent level to prevent multi-level paths from occurring.
    inline constexpr split_path_res_t split_posix_path(::uwvm2::utils::container::u8string_view path) noexcept
    {
        split_path_res_t result{};

        // Indicates whether the path is absolute; the leading ‘/’ will be skipped during actual splitting.
        result.is_absolute = !path.empty() && path.front_unchecked() == u8'/';

        // Prevent extreme cases from exceeding boundaries (almost impossible to hit).
        if(path.size() == ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { return {}; }

        ::std::size_t i{};
        while(i < path.size())
        {
            // Skip consecutive '/'
            while(i < path.size() && path.index_unchecked(i) == u8'/') { ++i; }
            if(i >= path.size()) { break; }

            ::std::size_t j{i};
            while(j < path.size() && path.index_unchecked(j) != u8'/') { ++j; }
            ::uwvm2::utils::container::u8string_view token{path.subview_unchecked(i, j - i)};

            // Precisely identify '.' and '..', otherwise treat as a normal name field.
            if(token == u8".") { result.res.emplace_back(::uwvm2::utils::container::u8string{}, dir_type_e::curr); }
            else if(token == u8"..") { result.res.emplace_back(::uwvm2::utils::container::u8string{}, dir_type_e::prev); }
            else
            {
                result.res.emplace_back(::uwvm2::utils::container::u8string{token}, dir_type_e::next);
            }

            i = j + 1uz;
        }

        return result;
    }

    inline constexpr ::uwvm2::imported::wasi::wasip1::abi::errno_t path_errno_from_fast_io_error(::fast_io::error e) noexcept
    {
#if defined(_WIN32) && !defined(__CYGWIN__)
        switch(e.domain)
        {
            case ::fast_io::win32_domain_value:
            {
                switch(e.code)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's
                    // ‘ebadf’; here, “eio” is used instead.
                    case 6uz /*ERROR_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 5uz /*ERROR_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case 1uz /*ERROR_INVALID_FUNCTION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    case 50uz /*ERROR_NOT_SUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case 19uz /*ERROR_WRITE_PROTECT*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case 87uz /*ERROR_INVALID_PARAMETER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case 1117uz /*ERROR_IO_DEVICE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 31uz /*ERROR_GEN_FAILURE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 1784uz /*ERROR_INVALID_USER_BUFFER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
                    case 2uz /*ERROR_FILE_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                    case 3uz /*ERROR_PATH_NOT_FOUND*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                    case 4uz /*ERROR_TOO_MANY_OPEN_FILES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emfile;
                    case 24uz /*ERROR_BAD_LENGTH*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case 25uz /*ERROR_SEEK*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
                    case 21uz /*ERROR_NOT_READY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 38uz /*ERROR_HANDLE_EOF*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 112uz /*ERROR_DISK_FULL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case 12uz /*ERROR_INVALID_ACCESS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case 32uz /*ERROR_SHARING_VIOLATION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebusy;
                    case 33uz /*ERROR_LOCK_VIOLATION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                    case 109uz /*ERROR_BROKEN_PIPE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::epipe;
                    case 232uz /*ERROR_NO_DATA*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::epipe;
                    case 8uz /*ERROR_NOT_ENOUGH_MEMORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enomem;
                    case 14uz /*ERROR_OUTOFMEMORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enomem;
                    case 995uz /*ERROR_OPERATION_ABORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ecanceled;
                    case 997uz /*ERROR_IO_PENDING*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einprogress;
                    case 267uz /*ERROR_DIRECTORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                    case 183uz /*ERROR_ALREADY_EXISTS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eexist;
                    case 206uz /*ERROR_FILENAME_EXCED_RANGE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enametoolong;
                    case 123uz /*ERROR_INVALID_NAME*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case 17uz /*ERROR_NOT_SAME_DEVICE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::exdev;
                    case 80uz /*ERROR_FILE_EXISTS*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eexist;
                    case 4uz /*ERROR_TOO_MANY_OPEN_FILES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emfile;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }

                break;
            }
# if !defined(_WIN32_WINDOWS)
            case ::fast_io::nt_domain_value:
            {
                static_assert(sizeof(::fast_io::error::value_type) >= sizeof(::std::uint_least32_t));
                switch(e.code)
                {
                    // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's
                    // ‘ebadf’; here, “eio” is used instead.
                    case 0xC0000008uz /*STATUS_INVALID_HANDLE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC0000022uz /*STATUS_ACCESS_DENIED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
                    case 0xC0000002uz /*STATUS_NOT_IMPLEMENTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    case 0xC00000BBuz /*STATUS_NOT_SUPPORTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
                    case 0xC00000A2uz /*STATUS_MEDIA_WRITE_PROTECTED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
                    case 0xC000000Duz /*STATUS_INVALID_PARAMETER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case 0xC0000185uz /*STATUS_IO_DEVICE_ERROR*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC0000001uz /*STATUS_UNSUCCESSFUL*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC00000E8uz /*STATUS_INVALID_USER_BUFFER*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
                    case 0xC0000034uz /* STATUS_OBJECT_NAME_NOT_FOUND */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                    case 0xC000003Auz /* STATUS_OBJECT_PATH_NOT_FOUND */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                    case 0xC0000043uz /* STATUS_SHARING_VIOLATION */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebusy;
                    case 0xC0000054uz /* STATUS_FILE_LOCK_CONFLICT */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
                    case 0xC000007Fuz /* STATUS_DISK_FULL */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
                    case 0xC0000010uz /* STATUS_INVALID_DEVICE_REQUEST */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enosys;
                    case 0xC0000011uz /* STATUS_END_OF_FILE */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC0000013uz /* STATUS_NO_MEDIA_IN_DEVICE */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                    case 0xC000000Euz /* STATUS_NO_SUCH_DEVICE */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
                    case 0xC0000101uz /* STATUS_DIRECTORY_NOT_EMPTY */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotempty;
                    case 0xC0000281uz /* STATUS_DIRECTORY_IS_A_REPARSE_POINT */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                    case 0xC00000BAuz /* STATUS_FILE_IS_A_DIRECTORY */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                    case 0xC000009Auz /* STATUS_INSUFFICIENT_RESOURCES */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enomem;
                    case 0xC0000017uz /* STATUS_NO_MEMORY */: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enomem;
                    case 0xC0000120uz /*STATUS_CANCELLED*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ecanceled;
                    case 0x00000103uz /*STATUS_PENDING*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einprogress;
                    case 0xC000014Buz /*STATUS_PIPE_BROKEN*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::epipe;
                    case 0xC0000035uz /*STATUS_OBJECT_NAME_COLLISION*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eexist;
                    case 0xC0000106uz /*STATUS_NAME_TOO_LONG*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enametoolong;
                    case 0xC0000103uz /*STATUS_NOT_A_DIRECTORY*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                    case 0xC0000033uz /*STATUS_OBJECT_NAME_INVALID*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    case 0xC00000D4uz /*STATUS_NOT_SAME_DEVICE*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::exdev;
                    case 0xC000011Fuz /*STATUS_TOO_MANY_OPEN_FILES*/: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emfile;
                    default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }

                break;
            }
# endif
            [[unlikely]] default:
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                // Security issues inherent to virtual machines
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            }
        }
#else
        switch(e.code)
        {
            // If “ebadf” appears here, it is caused by a WASI implementation issue. This differs from WASI's ‘ebadf’;
            // here, “eio” is used instead.
            case EBADF: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            case ENOSPC: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enospc;
            case EFBIG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efbig;
            case EINVAL: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            case EACCES: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eacces;
            case EPERM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
            case EISDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
            case EROFS: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::erofs;
# if defined(EDQUOT)
            case EDQUOT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::edquot;
# endif
            case EINTR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eintr;
# if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EAGAIN != EWOULDBLOCK))
            case EWOULDBLOCK: [[fallthrough]];
# endif
            case EAGAIN: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eagain;
            case ESPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::espipe;
# if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
            case EOPNOTSUPP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# endif
# if defined(ENOTSUP)
            case ENOTSUP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotsup;
# endif
            case EFAULT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::efault;
            case ENXIO: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enxio;
            case ENODEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enodev;
            case ENOENT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enoent;
            case ENOTDIR: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
            case EIO: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
            case ENOMEM: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enomem;
            case EOVERFLOW: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
# if defined(ETIMEDOUT)
            case ETIMEDOUT: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::etimedout;
# endif
# if defined(ECONNRESET)
            case ECONNRESET: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::econnreset;
# endif
# if defined(EPIPE)
            case EPIPE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::epipe;
# endif
# if defined(EEXIST)
            case EEXIST: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eexist;
# endif
# if defined(EMFILE)
            case EMFILE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emfile;
# endif
# if defined(ENAMETOOLONG)
            case ENAMETOOLONG: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enametoolong;
# endif
# if defined(ELOOP)
            case ELOOP: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop;
# endif
# if defined(EMLINK)
            case EMLINK: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::emlink;
# endif
# if defined(EXDEV)
            case EXDEV: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::exdev;
# endif
# if defined(EBUSY)
            case EBUSY: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::ebusy;
# endif
# if defined(ENFILE)
            case ENFILE: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enfile;
# endif
            default: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
        }
#endif

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
    }

    inline constexpr ::std::size_t max_symlink_depth{40uz};

    inline constexpr ::uwvm2::imported::wasi::wasip1::abi::errno_t path_symlink_iterative_impl(
        ::std::size_t& symlink_depth,
        ::fast_io::dir_file const& curr_fd_native_dir_file,
        ::uwvm2::utils::container::vector<::fast_io::dir_file>& curr_path_stack,
        ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        if(++symlink_depth > max_symlink_depth) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop; }

        if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval; }

        // WASI does not guarantee that strings are null-terminated, so you must check for zero characters in the middle and construct
        // one yourself.
        auto const u8res{::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(symlink_symbol.cbegin(),
                                                                                                                                       symlink_symbol.cend())};
        if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            // If the path string is not valid UTF-8, the function shall fail with ERRNO_ILSEQ.
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq;
        }

        auto const split_path_res{::uwvm2::imported::wasi::wasip1::func::split_posix_path(symlink_symbol)};

        if(split_path_res.is_absolute) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm; }

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
        // For the Windows API, the parsing strategy differs from POSIX. Windows supports the backslash as a delimiter while rejecting
        // many characters.
        // This only eliminates the possibility of multi-level paths; the remaining issue of invalid filenames is handled by the API.
        for(auto const& split_curr: split_path_res.res)
        {
            if(split_curr.dir_type == ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next)
            {
                auto const& next_name{split_curr.next_name};
                for(auto const curr_char: next_name)
                {
                    // Simultaneously eliminate errors at both the path syntax layer and the file system syntax layer.
                    if(::fast_io::char_category::is_dos_file_invalid_character(curr_char)) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }
        }
#endif

        for(auto const& split_curr: split_path_res.res)
        {
            switch(split_curr.dir_type)
            {
                case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                {
                    break;
                }
                case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                {
                    if(curr_path_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm; }
                    else
                    {
                        curr_path_stack.pop_back_unchecked();
                    }

                    break;
                }
                case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                {
                    auto const& next_name{split_curr.next_name};

                    ::uwvm2::utils::container::u8string symlink_symbol{};
                    bool is_symlink{};

                    if(curr_path_stack.empty())
                    {
#ifdef UWVM_CPP_EXCEPTIONS
                        try
#endif
                        {
                            // readlinkat is symlink_nofollow
                            symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_dir_file), next_name);
                            is_symlink = true;
                        }
#ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error e)
                        {
                        }
#endif

                        if(is_symlink)
                        {
                            if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                            auto const errno_t{
                                path_symlink_iterative_impl(symlink_depth,
                                                            curr_fd_native_dir_file,
                                                            curr_path_stack,
                                                            ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                            if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]] { return errno_t; }
                        }
                        else
                        {
                            ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // default is symlink_nofollow
                                next = ::fast_io::dir_file{at(curr_fd_native_dir_file), next_name};

                                // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                // opened simultaneously. A check must be added here.
                                struct ::stat st;
                                ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                // required.
                                ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                                // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                {
                                    bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                    try
#  endif
                                    {
                                        ::fast_io::native_file{at(curr_fd_native_dir_file), next_name, ::fast_io::open_mode::in};
                                        is_file = true;
                                    }
#  ifdef UWVM_CPP_EXCEPTIONS
                                    catch(::fast_io::error e)
                                    {
                                    }
#  endif
                                    if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
                                }
# endif

                                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                            }
#endif

                            curr_path_stack.push_back(::std::move(next));
                        }
                    }
                    else
                    {
#ifdef UWVM_CPP_EXCEPTIONS
                        try
#endif
                        {
                            // readlinkat is symlink_nofollow
                            symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_path_stack.back_unchecked()), next_name);
                            is_symlink = true;
                        }
#ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error e)
                        {
                        }
#endif

                        if(is_symlink)
                        {
                            if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                            auto const errno_t{
                                path_symlink_iterative_impl(symlink_depth,
                                                            curr_fd_native_dir_file,
                                                            curr_path_stack,
                                                            ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                            if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]] { return errno_t; }
                        }
                        else
                        {
                            ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // default is symlink_nofollow
                                next = ::fast_io::dir_file{at(curr_path_stack.back_unchecked()), next_name};

                                // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                // opened simultaneously. A check must be added here.
                                struct ::stat st;
                                ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                // required.
                                ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                                // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                {
                                    bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                    try
#  endif
                                    {
                                        ::fast_io::native_file{at(curr_path_stack.back_unchecked()), next_name, ::fast_io::open_mode::in};
                                        is_file = true;
                                    }
#  ifdef UWVM_CPP_EXCEPTIONS
                                    catch(::fast_io::error e)
                                    {
                                    }
#  endif
                                    if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
                                }
# endif

                                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                            }
#endif

                            curr_path_stack.push_back(::std::move(next));
                        }
                    }

                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }

    inline constexpr ::uwvm2::imported::wasi::wasip1::abi::errno_t path_symlink_iterative(
        ::fast_io::dir_file const& curr_fd_native_dir_file,
        ::uwvm2::utils::container::vector<::fast_io::dir_file>& curr_path_stack,
        ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        ::std::size_t symlink_depth{};
        return path_symlink_iterative_impl(symlink_depth, curr_fd_native_dir_file, curr_path_stack, symlink_symbol);
    }

    struct file_symlink_iterative_res_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::errno_t err{};
        ::uwvm2::utils::container::u8string file_name{};
    };

    inline constexpr file_symlink_iterative_res_t file_symlink_iterative_impl(::std::size_t& symlink_depth,
                                                                              ::fast_io::dir_file const& curr_fd_native_dir_file,
                                                                              ::uwvm2::utils::container::vector<::fast_io::dir_file>& curr_path_stack,
                                                                              ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        file_symlink_iterative_res_t res{};

        if(++symlink_depth > max_symlink_depth) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop;
            return res;
        }

        if(symlink_symbol.empty()) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            return res;
        }

        // WASI does not guarantee that strings are null-terminated, so you must check for zero characters in the middle and construct
        // one yourself.
        auto const u8res{::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(symlink_symbol.cbegin(),
                                                                                                                                       symlink_symbol.cend())};
        if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            // If the path string is not valid UTF-8, the function shall fail with ERRNO_ILSEQ.
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq;
            return res;
        }

        auto const split_path_res{::uwvm2::imported::wasi::wasip1::func::split_posix_path(symlink_symbol)};

        if(split_path_res.is_absolute) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
            return res;
        }

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
        // For the Windows API, the parsing strategy differs from POSIX. Windows supports the backslash as a delimiter while rejecting
        // many characters.
        // This only eliminates the possibility of multi-level paths; the remaining issue of invalid filenames is handled by the API.
        for(auto const& split_curr: split_path_res.res)
        {
            if(split_curr.dir_type == ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next)
            {
                auto const& next_name{split_curr.next_name};
                for(auto const curr_char: next_name)
                {
                    // Simultaneously eliminate errors at both the path syntax layer and the file system syntax layer.
                    if(::fast_io::char_category::is_dos_file_invalid_character(curr_char)) [[unlikely]]
                    {
                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        return res;
                    }
                }
            }
        }
#endif

        if(split_path_res.res.empty()) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            return res;
        }

        auto const split_path_res_last_ptr{split_path_res.res.cend() - 1u};

        for(auto& split_curr: split_path_res.res)
        {
            // split_curr cannot be const because it will be moved later.

            if(::std::addressof(split_curr) == split_path_res_last_ptr)
            {
                // The last one that isn't a symbolic link is a file.
                switch(split_curr.dir_type)
                {
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                    {
                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                        return res;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                    {
                        if(curr_path_stack.empty()) [[unlikely]]
                        {
                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            return res;
                        }
                        else
                        {
                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                            return res;
                        }
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                    {
                        auto& next_name{split_curr.next_name};

                        ::uwvm2::utils::container::u8string symlink_symbol{};
                        bool is_symlink{};

                        if(curr_path_stack.empty())
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_dir_file), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                return file_symlink_iterative_impl(symlink_depth,
                                                                   curr_fd_native_dir_file,
                                                                   curr_path_stack,
                                                                   ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()});
                            }
                            else
                            {
                                res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                res.file_name = ::std::move(next_name);
                                return res;
                            }
                        }
                        else
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_path_stack.back_unchecked()), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                return file_symlink_iterative_impl(symlink_depth,
                                                                   curr_fd_native_dir_file,
                                                                   curr_path_stack,
                                                                   ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()});
                            }
                            else
                            {
                                res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                res.file_name = ::std::move(next_name);
                                return res;
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::std::unreachable();
                    }
                }
            }
            else
            {
                switch(split_curr.dir_type)
                {
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                    {
                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                    {
                        if(curr_path_stack.empty()) [[unlikely]]
                        {
                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            return res;
                        }
                        else
                        {
                            curr_path_stack.pop_back_unchecked();
                        }

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                    {
                        auto const& next_name{split_curr.next_name};

                        ::uwvm2::utils::container::u8string symlink_symbol{};
                        bool is_symlink{};

                        if(curr_path_stack.empty())
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_dir_file), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                auto const errno_t{
                                    path_symlink_iterative_impl(symlink_depth,
                                                                curr_fd_native_dir_file,
                                                                curr_path_stack,
                                                                ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                                {
                                    res.err = errno_t;
                                    return res;
                                }
                            }
                            else
                            {
                                ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // default is symlink_nofollow
                                    next = ::fast_io::dir_file{at(curr_fd_native_dir_file), next_name};

                                    // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                    // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                    // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                    // opened simultaneously. A check must be added here.
                                    struct ::stat st;
                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                    if(!S_ISDIR(st.st_mode))
                                    {
                                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                        return res;
                                    }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                    // required.
                                    ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                    // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                    if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                    {
                                        bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                        try
#  endif
                                        {
                                            ::fast_io::native_file{at(curr_fd_native_dir_file), next_name, ::fast_io::open_mode::in};
                                            is_file = true;
                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                        }
#  endif
                                        if(is_file)
                                        {
                                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                            return res;
                                        }
                                    }
# endif

                                    res.err = ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                    return res;
                                }
#endif

                                curr_path_stack.push_back(::std::move(next));
                            }
                        }
                        else
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_path_stack.back_unchecked()), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                auto const errno_t{
                                    path_symlink_iterative_impl(symlink_depth,
                                                                curr_fd_native_dir_file,
                                                                curr_path_stack,
                                                                ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                                {
                                    res.err = errno_t;
                                    return res;
                                }
                            }
                            else
                            {
                                ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // default is symlink_nofollow
                                    next = ::fast_io::dir_file{at(curr_path_stack.back_unchecked()), next_name};

                                    // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                    // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                    // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                    // opened simultaneously. A check must be added here.
                                    struct ::stat st;
                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                    if(!S_ISDIR(st.st_mode))
                                    {
                                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                        return res;
                                    }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                    // required.
                                    ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                    // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                    if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                    {
                                        bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                        try
#  endif
                                        {
                                            ::fast_io::native_file{at(curr_path_stack.back_unchecked()), next_name, ::fast_io::open_mode::in};
                                            is_file = true;
                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                        }
#  endif
                                        if(is_file)
                                        {
                                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                            return res;
                                        }
                                    }
# endif

                                    res.err = ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                    return res;
                                }
#endif

                                curr_path_stack.push_back(::std::move(next));
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::std::unreachable();
                    }
                }
            }
        }

        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
        return res;
    }

    inline constexpr file_symlink_iterative_res_t file_symlink_iterative(::fast_io::dir_file const& curr_fd_native_dir_file,
                                                                         ::uwvm2::utils::container::vector<::fast_io::dir_file>& curr_path_stack,
                                                                         ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        ::std::size_t symlink_depth{};
        return file_symlink_iterative_impl(symlink_depth, curr_fd_native_dir_file, curr_path_stack, symlink_symbol);
    }

    struct dir_with_name_t
    {
        ::uwvm2::utils::container::u8string name{};
        ::fast_io::dir_file file{};
    };

    inline constexpr ::uwvm2::imported::wasi::wasip1::abi::errno_t path_symlink_iterative_with_name_impl(
        ::std::size_t& symlink_depth,
        ::fast_io::dir_file const& curr_fd_native_dir_file,
        ::uwvm2::utils::container::vector<dir_with_name_t>& curr_path_stack,
        ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        if(++symlink_depth > max_symlink_depth) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop; }

        if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval; }

        // WASI does not guarantee that strings are null-terminated, so you must check for zero characters in the middle and construct
        // one yourself.
        auto const u8res{::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(symlink_symbol.cbegin(),
                                                                                                                                       symlink_symbol.cend())};
        if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            // If the path string is not valid UTF-8, the function shall fail with ERRNO_ILSEQ.
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq;
        }

        auto const split_path_res{::uwvm2::imported::wasi::wasip1::func::split_posix_path(symlink_symbol)};

        if(split_path_res.is_absolute) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm; }

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
        // For the Windows API, the parsing strategy differs from POSIX. Windows supports the backslash as a delimiter while rejecting
        // many characters.
        // This only eliminates the possibility of multi-level paths; the remaining issue of invalid filenames is handled by the API.
        for(auto const& split_curr: split_path_res.res)
        {
            if(split_curr.dir_type == ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next)
            {
                auto const& next_name{split_curr.next_name};
                for(auto const curr_char: next_name)
                {
                    // Simultaneously eliminate errors at both the path syntax layer and the file system syntax layer.
                    if(::fast_io::char_category::is_dos_file_invalid_character(curr_char)) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                    }
                }
            }
        }
#endif

        for(auto& split_curr: split_path_res.res)
        {
            switch(split_curr.dir_type)
            {
                case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                {
                    break;
                }
                case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                {
                    if(curr_path_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm; }
                    else
                    {
                        curr_path_stack.pop_back_unchecked();
                    }

                    break;
                }
                case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                {
                    auto& next_name{split_curr.next_name};

                    ::uwvm2::utils::container::u8string symlink_symbol{};
                    bool is_symlink{};

                    if(curr_path_stack.empty())
                    {
#ifdef UWVM_CPP_EXCEPTIONS
                        try
#endif
                        {
                            // readlinkat is symlink_nofollow
                            symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_dir_file), next_name);
                            is_symlink = true;
                        }
#ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error e)
                        {
                        }
#endif

                        if(is_symlink)
                        {
                            if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                            auto const errno_t{
                                path_symlink_iterative_with_name_impl(symlink_depth,
                                                                      curr_fd_native_dir_file,
                                                                      curr_path_stack,
                                                                      ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                            if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]] { return errno_t; }
                        }
                        else
                        {
                            ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // default is symlink_nofollow
                                next = ::fast_io::dir_file{at(curr_fd_native_dir_file), next_name};

                                // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                // opened simultaneously. A check must be added here.
                                struct ::stat st;
                                ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                // required.
                                ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                                // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                {
                                    bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                    try
#  endif
                                    {
                                        ::fast_io::native_file{at(curr_fd_native_dir_file), next_name, ::fast_io::open_mode::in};
                                        is_file = true;
                                    }
#  ifdef UWVM_CPP_EXCEPTIONS
                                    catch(::fast_io::error e)
                                    {
                                    }
#  endif
                                    if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
                                }
# endif

                                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                            }
#endif

                            curr_path_stack.emplace_back(::std::move(next_name), ::std::move(next));
                        }
                    }
                    else
                    {
#ifdef UWVM_CPP_EXCEPTIONS
                        try
#endif
                        {
                            // readlinkat is symlink_nofollow
                            symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_path_stack.back_unchecked().file), next_name);
                            is_symlink = true;
                        }
#ifdef UWVM_CPP_EXCEPTIONS
                        catch(::fast_io::error e)
                        {
                        }
#endif

                        if(is_symlink)
                        {
                            if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio; }

                            auto const errno_t{
                                path_symlink_iterative_with_name_impl(symlink_depth,
                                                                      curr_fd_native_dir_file,
                                                                      curr_path_stack,
                                                                      ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                            if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]] { return errno_t; }
                        }
                        else
                        {
                            ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // default is symlink_nofollow
                                next = ::fast_io::dir_file{at(curr_path_stack.back_unchecked().file), next_name};

                                // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                // opened simultaneously. A check must be added here.
                                struct ::stat st;
                                ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                // required.
                                ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                                // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                {
                                    bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                    try
#  endif
                                    {
                                        ::fast_io::native_file{at(curr_path_stack.back_unchecked().file), next_name, ::fast_io::open_mode::in};
                                        is_file = true;
                                    }
#  ifdef UWVM_CPP_EXCEPTIONS
                                    catch(::fast_io::error e)
                                    {
                                    }
#  endif
                                    if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir; }
                                }
# endif

                                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                            }
#endif

                            curr_path_stack.emplace_back(::std::move(next_name), ::std::move(next));
                        }
                    }

                    break;
                }
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }

    inline constexpr ::uwvm2::imported::wasi::wasip1::abi::errno_t path_symlink_iterative_with_name(
        ::fast_io::dir_file const& curr_fd_native_dir_file,
        ::uwvm2::utils::container::vector<dir_with_name_t>& curr_path_stack,
        ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        ::std::size_t symlink_depth{};
        return path_symlink_iterative_with_name_impl(symlink_depth, curr_fd_native_dir_file, curr_path_stack, symlink_symbol);
    }

    inline constexpr file_symlink_iterative_res_t file_symlink_iterative_with_name_impl(::std::size_t& symlink_depth,
                                                                                        ::fast_io::dir_file const& curr_fd_native_dir_file,
                                                                                        ::uwvm2::utils::container::vector<dir_with_name_t>& curr_path_stack,
                                                                                        ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        file_symlink_iterative_res_t res{};

        if(++symlink_depth > max_symlink_depth) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eloop;
            return res;
        }

        if(symlink_symbol.empty()) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
            return res;
        }

        // WASI does not guarantee that strings are null-terminated, so you must check for zero characters in the middle and construct
        // one yourself.
        auto const u8res{::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(symlink_symbol.cbegin(),
                                                                                                                                       symlink_symbol.cend())};
        if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            // If the path string is not valid UTF-8, the function shall fail with ERRNO_ILSEQ.
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eilseq;
            return res;
        }

        auto const split_path_res{::uwvm2::imported::wasi::wasip1::func::split_posix_path(symlink_symbol)};

        if(split_path_res.is_absolute) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
            return res;
        }

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
        // For the Windows API, the parsing strategy differs from POSIX. Windows supports the backslash as a delimiter while rejecting
        // many characters.
        // This only eliminates the possibility of multi-level paths; the remaining issue of invalid filenames is handled by the API.
        for(auto const& split_curr: split_path_res.res)
        {
            if(split_curr.dir_type == ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next)
            {
                auto const& next_name{split_curr.next_name};
                for(auto const curr_char: next_name)
                {
                    // Simultaneously eliminate errors at both the path syntax layer and the file system syntax layer.
                    if(::fast_io::char_category::is_dos_file_invalid_character(curr_char)) [[unlikely]]
                    {
                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::einval;
                        return res;
                    }
                }
            }
        }
#endif

        if(split_path_res.res.empty()) [[unlikely]]
        {
            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
            return res;
        }

        auto const split_path_res_last_ptr{split_path_res.res.cend() - 1u};

        for(auto& split_curr: split_path_res.res)
        {
            // split_curr cannot be const because it will be moved later.

            if(::std::addressof(split_curr) == split_path_res_last_ptr)
            {
                // The last one that isn't a symbolic link is a file.
                switch(split_curr.dir_type)
                {
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                    {
                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                        return res;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                    {
                        if(curr_path_stack.empty()) [[unlikely]]
                        {
                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            return res;
                        }
                        else
                        {
                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eisdir;
                            return res;
                        }
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                    {
                        auto& next_name{split_curr.next_name};

                        ::uwvm2::utils::container::u8string symlink_symbol{};
                        bool is_symlink{};

                        if(curr_path_stack.empty())
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_dir_file), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                return file_symlink_iterative_with_name_impl(
                                    symlink_depth,
                                    curr_fd_native_dir_file,
                                    curr_path_stack,
                                    ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()});
                            }
                            else
                            {
                                res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                res.file_name = ::std::move(next_name);
                                return res;
                            }
                        }
                        else
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_path_stack.back_unchecked().file), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                return file_symlink_iterative_with_name_impl(
                                    symlink_depth,
                                    curr_fd_native_dir_file,
                                    curr_path_stack,
                                    ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()});
                            }
                            else
                            {
                                res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
                                res.file_name = ::std::move(next_name);
                                return res;
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::std::unreachable();
                    }
                }
            }
            else
            {
                switch(split_curr.dir_type)
                {
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                    {
                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                    {
                        if(curr_path_stack.empty()) [[unlikely]]
                        {
                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eperm;
                            return res;
                        }
                        else
                        {
                            curr_path_stack.pop_back_unchecked();
                        }

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                    {
                        auto& next_name{split_curr.next_name};

                        ::uwvm2::utils::container::u8string symlink_symbol{};
                        bool is_symlink{};

                        if(curr_path_stack.empty())
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_dir_file), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                auto const errno_t{path_symlink_iterative_with_name_impl(
                                    symlink_depth,
                                    curr_fd_native_dir_file,
                                    curr_path_stack,
                                    ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                                {
                                    res.err = errno_t;
                                    return res;
                                }
                            }
                            else
                            {
                                ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // default is symlink_nofollow
                                    next = ::fast_io::dir_file{at(curr_fd_native_dir_file), next_name};

                                    // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                    // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                    // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                    // opened simultaneously. A check must be added here.
                                    struct ::stat st;
                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                    if(!S_ISDIR(st.st_mode))
                                    {
                                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                        return res;
                                    }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                    // required.
                                    ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                    // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                    if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                    {
                                        bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                        try
#  endif
                                        {
                                            ::fast_io::native_file{at(curr_fd_native_dir_file), next_name, ::fast_io::open_mode::in};
                                            is_file = true;
                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                        }
#  endif
                                        if(is_file)
                                        {
                                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                            return res;
                                        }
                                    }
# endif

                                    res.err = ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                    return res;
                                }
#endif

                                curr_path_stack.emplace_back(::std::move(next_name), ::std::move(next));
                            }
                        }
                        else
                        {
#ifdef UWVM_CPP_EXCEPTIONS
                            try
#endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_path_stack.back_unchecked().file), next_name);
                                is_symlink = true;
                            }
#ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
#endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]]
                                {
                                    res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                                    return res;
                                }

                                auto const errno_t{path_symlink_iterative_with_name_impl(
                                    symlink_depth,
                                    curr_fd_native_dir_file,
                                    curr_path_stack,
                                    ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                if(errno_t != ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess) [[unlikely]]
                                {
                                    res.err = errno_t;
                                    return res;
                                }
                            }
                            else
                            {
                                ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // default is symlink_nofollow
                                    next = ::fast_io::dir_file{at(curr_path_stack.back_unchecked().file), next_name};

                                    // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                    // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                    // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                    // opened simultaneously. A check must be added here.
                                    struct ::stat st;
                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                    if(!S_ISDIR(st.st_mode))
                                    {
                                        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                        return res;
                                    }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                    // required.
                                    ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
#endif
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                    // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

# if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                    if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                    {
                                        bool is_file{};
#  ifdef UWVM_CPP_EXCEPTIONS
                                        try
#  endif
                                        {
                                            ::fast_io::native_file{at(curr_path_stack.back_unchecked().file), next_name, ::fast_io::open_mode::in};
                                            is_file = true;
                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                        }
#  endif
                                        if(is_file)
                                        {
                                            res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotdir;
                                            return res;
                                        }
                                    }
# endif

                                    res.err = ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                    return res;
                                }
#endif

                                curr_path_stack.emplace_back(::std::move(next_name), ::std::move(next));
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                        ::std::unreachable();
                    }
                }
            }
        }

        res.err = ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
        return res;
    }

    inline constexpr file_symlink_iterative_res_t file_symlink_iterative_with_name(::fast_io::dir_file const& curr_fd_native_dir_file,
                                                                                   ::uwvm2::utils::container::vector<dir_with_name_t>& curr_path_stack,
                                                                                   ::uwvm2::utils::container::u8string_view symlink_symbol) noexcept
    {
        ::std::size_t symlink_depth{};
        return file_symlink_iterative_with_name_impl(symlink_depth, curr_fd_native_dir_file, curr_path_stack, symlink_symbol);
    }

}  // namespace uwvm2::imported::wasi::wasip1::func

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
