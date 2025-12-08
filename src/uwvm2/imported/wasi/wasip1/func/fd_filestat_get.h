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

    struct alignas(8uz) wasi_filestat_t
    {
        ::uwvm2::imported::wasi::wasip1::abi::device_t st_dev;
        ::uwvm2::imported::wasi::wasip1::abi::inode_t st_ino;
        ::uwvm2::imported::wasi::wasip1::abi::filetype_t st_filetype;
        ::uwvm2::imported::wasi::wasip1::abi::linkcount_t st_nlink;
        ::uwvm2::imported::wasi::wasip1::abi::filesize_t st_size;
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t st_atim;
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t st_mtim;
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t st_ctim;
    };

    inline constexpr ::std::size_t size_of_wasi_filestat_t{64uz};

    inline consteval bool is_default_wasi_filestat_data_layout() noexcept
    {
        // In standard layout mode, data can be transferred in a single memcpy operation (static length), improving read efficiency.
        return __builtin_offsetof(wasi_filestat_t, st_dev) == 0uz && __builtin_offsetof(wasi_filestat_t, st_ino) == 8uz &&
               __builtin_offsetof(wasi_filestat_t, st_filetype) == 16uz && __builtin_offsetof(wasi_filestat_t, st_nlink) == 24uz &&
               __builtin_offsetof(wasi_filestat_t, st_size) == 32uz && __builtin_offsetof(wasi_filestat_t, st_atim) == 40uz &&
               __builtin_offsetof(wasi_filestat_t, st_mtim) == 48uz && __builtin_offsetof(wasi_filestat_t, st_ctim) == 56uz &&
               sizeof(wasi_filestat_t) == size_of_wasi_filestat_t && alignof(wasi_filestat_t) == 8uz && ::std::endian::native == ::std::endian::little;
    }

    /// @brief     WasiPreview1.fd_filestat_get
    /// @details   __wasi_errno_t fd_filestat_get(__wasi_fd_t fd, __wasi_filestat_t *stat);
    inline ::uwvm2::imported::wasi::wasip1::abi::errno_t fd_filestat_get(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t stat_ptrsz) noexcept
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
                                u8"fd_filestat_get",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(stat_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: fd_filestat_get(",
                                fd,
                                u8", ",
                                ::fast_io::mnp::addrvw(stat_ptrsz),
                                u8") (wasi-trace)\n");
# endif
        }

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

        // Rights check: fd_filestat_get requires right_fd_filestat_get.
        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_filestat_get) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_filestat_get) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_t::enotcapable;
        }

        // check memory bounds
        ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm32(memory, stat_ptrsz, size_of_wasi_filestat_t);

        // All require initialization to prevent subsequent unconfigured settings from causing undefined behavior.
        ::uwvm2::imported::wasi::wasip1::abi::device_t st_dev{};         // 0
        ::uwvm2::imported::wasi::wasip1::abi::inode_t st_ino{};          // 8
        ::uwvm2::imported::wasi::wasip1::abi::filetype_t st_filetype{};  // 16
        ::uwvm2::imported::wasi::wasip1::abi::linkcount_t st_nlink{};    // 24
        ::uwvm2::imported::wasi::wasip1::abi::filesize_t st_size{};      // 32
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t st_atim{};     // 40
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t st_mtim{};     // 48
        ::uwvm2::imported::wasi::wasip1::abi::timestamp_t st_ctim{};     // 56

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
                [[maybe_unused]] ::fast_io::native_io_observer curr_fd_native_observer{};

                bool const is_file_observer{curr_fd.wasi_fd.ptr->wasi_fd_storage.type ==
                                            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer};
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

# if defined(_WIN32) && !defined(__CYGWIN__)
                auto const& curr_fd_native_file{curr_fd_native_observer};

                ::fast_io::posix_file_status curr_fd_status;  // no initialize

#  ifdef UWVM_CPP_EXCEPTIONS
                try
#  endif
                {
                    curr_fd_status = status(curr_fd_native_file);
                }
#  ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error e)
                {
#   if defined(__CYGWIN__)
                    // Map potential overflow from host fstat/stat to WASI eoverflow for better diagnostics.
                    switch(e.code)
                    {
                        case EOVERFLOW: return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eoverflow;
                        default: break;
                    }
#   endif
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
#  endif

                st_dev = static_cast<::uwvm2::imported::wasi::wasip1::abi::device_t>(curr_fd_status.dev);
                st_ino = static_cast<::uwvm2::imported::wasi::wasip1::abi::inode_t>(curr_fd_status.ino);

                switch(curr_fd_status.type)
                {
                    case ::fast_io::file_type::none:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::not_found:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::regular:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_regular_file;
                        break;
                    }
                    case ::fast_io::file_type::directory:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_directory;
                        break;
                    }
                    case ::fast_io::file_type::symlink:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_symbolic_link;
                        break;
                    }
                    case ::fast_io::file_type::block:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_block_device;
                        break;
                    }
                    case ::fast_io::file_type::character:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_character_device;
                        break;
                    }
                    case ::fast_io::file_type::fifo:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::socket:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_socket_stream;
                        break;
                    }
                    case ::fast_io::file_type::unknown:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::remote:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    [[unlikely]] default:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                }

                st_nlink = static_cast<::uwvm2::imported::wasi::wasip1::abi::linkcount_t>(curr_fd_status.nlink);
                st_size = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(curr_fd_status.size);

                constexpr ::std::uint_least64_t mul_factor{::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u};

                // Since fast_io directly obtains the clock value via clock_getres, this operation will not overflow and will not produce negative
                // values.
                st_atim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.atim.seconds * 1'000'000'000u + curr_fd_status.atim.subseconds / mul_factor));
                st_mtim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.mtim.seconds * 1'000'000'000u + curr_fd_status.mtim.subseconds / mul_factor));
                st_ctim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.ctim.seconds * 1'000'000'000u + curr_fd_status.ctim.subseconds / mul_factor));

                break;

# else

                auto const& curr_fd_native_file{curr_fd_native_observer};

                ::fast_io::posix_file_status curr_fd_status;  // no initialize

#  ifdef UWVM_CPP_EXCEPTIONS
                try
#  endif
                {
                    curr_fd_status = status(curr_fd_native_file);
                }
#  ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
#  endif

                st_dev = static_cast<::uwvm2::imported::wasi::wasip1::abi::device_t>(curr_fd_status.dev);
                st_ino = static_cast<::uwvm2::imported::wasi::wasip1::abi::inode_t>(curr_fd_status.ino);

                switch(curr_fd_status.type)
                {
                    case ::fast_io::file_type::none:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::not_found:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::regular:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_regular_file;
                        break;
                    }
                    case ::fast_io::file_type::directory:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_directory;
                        break;
                    }
                    case ::fast_io::file_type::symlink:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_symbolic_link;
                        break;
                    }
                    case ::fast_io::file_type::block:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_block_device;
                        break;
                    }
                    case ::fast_io::file_type::character:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_character_device;
                        break;
                    }
                    case ::fast_io::file_type::fifo:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::socket:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_socket_stream;

#  if (!defined(__NEWLIB__) || defined(__CYGWIN__)) && !defined(_WIN32) &&                                                                                     \
      __has_include(<dirent.h>) && !defined(_PICOLIBC__) && !(defined(__MSDOS__) || defined(__DJGPP__))
                        int so_type{};
                        auto optlen{static_cast<::socklen_t>(sizeof(so_type))};
                        if(::uwvm2::imported::wasi::wasip1::func::posix::getsockopt(curr_fd_native_file.native_handle(),
                                                                                    SOL_SOCKET,
                                                                                    SO_TYPE,
                                                                                    ::std::addressof(so_type),
                                                                                    ::std::addressof(optlen)) == 0 &&
                           optlen == sizeof(so_type))
                        {
                            if(so_type == SOCK_DGRAM) { st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_socket_dgram; }
                        }
#  endif

                        break;
                    }
                    case ::fast_io::file_type::unknown:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    case ::fast_io::file_type::remote:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                    [[unlikely]] default:
                    {
                        st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_unknown;
                        break;
                    }
                }

                st_nlink = static_cast<::uwvm2::imported::wasi::wasip1::abi::linkcount_t>(curr_fd_status.nlink);
                st_size = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(curr_fd_status.size);

                constexpr ::std::uint_least64_t mul_factor{::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u};

                // Since fast_io directly obtains the clock value via clock_getres, this operation will not overflow and will not produce negative values.
                st_atim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.atim.seconds * 1'000'000'000u + curr_fd_status.atim.subseconds / mul_factor));
                st_mtim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.mtim.seconds * 1'000'000'000u + curr_fd_status.mtim.subseconds / mul_factor));
                st_ctim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.ctim.seconds * 1'000'000'000u + curr_fd_status.ctim.subseconds / mul_factor));

                break;
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

                auto const& curr_fd_native_file{curr_dir_io_observer};

                ::fast_io::posix_file_status curr_fd_status;  // no initialize

# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    curr_fd_status = status(curr_fd_native_file);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_t::eio;
                }
# endif

                st_dev = static_cast<::uwvm2::imported::wasi::wasip1::abi::device_t>(curr_fd_status.dev);
                st_ino = static_cast<::uwvm2::imported::wasi::wasip1::abi::inode_t>(curr_fd_status.ino);

                // curr_fd_status.type always directory
                st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_directory;

                st_nlink = static_cast<::uwvm2::imported::wasi::wasip1::abi::linkcount_t>(curr_fd_status.nlink);
                st_size = static_cast<::uwvm2::imported::wasi::wasip1::abi::filesize_t>(curr_fd_status.size);

                constexpr ::std::uint_least64_t mul_factor{::fast_io::uint_least64_subseconds_per_second / 1'000'000'000u};

                // Since fast_io directly obtains the clock value via clock_getres, this operation will not overflow and will not produce negative
                // values.
                st_atim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.atim.seconds * 1'000'000'000u + curr_fd_status.atim.subseconds / mul_factor));
                st_mtim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.mtim.seconds * 1'000'000'000u + curr_fd_status.mtim.subseconds / mul_factor));
                st_ctim = static_cast<::uwvm2::imported::wasi::wasip1::abi::timestamp_t>(
                    static_cast<::std::uint_least64_t>(curr_fd_status.ctim.seconds * 1'000'000'000u + curr_fd_status.ctim.subseconds / mul_factor));

                break;
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
                auto const curr_fd_native_handle{curr_fd_native_file.native_handle()};

                st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_socket_stream;

                char so_type{};
                int optlen{static_cast<int>(sizeof(so_type))};
                if(::fast_io::win32::getsockopt(curr_fd_native_handle,
                                                0xFFFF /*SOL_SOCKET*/,
                                                0x1008 /*SO_TYPE*/,
                                                ::std::addressof(so_type),
                                                ::std::addressof(optlen)) == 0 &&
                   optlen == sizeof(so_type))
                {
                    if(so_type == 2 /*SOCK_DGRAM*/) { st_filetype = ::uwvm2::imported::wasi::wasip1::abi::filetype_t::filetype_socket_dgram; }
                }

                st_nlink = static_cast<::uwvm2::imported::wasi::wasip1::abi::linkcount_t>(1u);

                break;
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

        // write
        if constexpr(is_default_wasi_filestat_data_layout())
        {
            wasi_filestat_t const tmp_wasi_filestat{st_dev, st_ino, st_filetype, st_nlink, st_size, st_atim, st_mtim, st_ctim};

            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz,
                reinterpret_cast<::std::byte const*>(::std::addressof(tmp_wasi_filestat)),
                reinterpret_cast<::std::byte const*>(::std::addressof(tmp_wasi_filestat)) + sizeof(tmp_wasi_filestat));
        }
        else
        {
            // Fallback for non-default host layout: store members individually at WASI-defined offsets.
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_dev)>>>(st_dev));
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz + 8u,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_ino)>>>(st_ino));
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz + 16u,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_filetype)>>>(st_filetype));
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz + 24u,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_nlink)>>>(st_nlink));
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz + 32u,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_size)>>>(st_size));
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz + 40u,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_atim)>>>(st_atim));
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz + 48u,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_mtim)>>>(st_mtim));
            ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32_unchecked(
                memory,
                stat_ptrsz + 56u,
                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(st_ctim)>>>(st_ctim));
        }

        return ::uwvm2::imported::wasi::wasip1::abi::errno_t::esuccess;
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

