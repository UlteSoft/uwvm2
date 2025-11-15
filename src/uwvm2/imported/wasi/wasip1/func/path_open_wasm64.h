
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
#  include <errno.h>
#  include <fcntl.h>
#  include <sys/stat.h>
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

UWVM_MODULE_EXPORT namespace uwvm2::imported::wasi::wasip1::func
{
    /// @brief     WasiPreview1.path_open
    /// @details   __wasi_errno_t path_open(__wasi_fd_t dirfd, __wasi_lookupflags_t dirflags, const char *path, size_t path_len, __wasi_oflags_t oflags,
    /// __wasi_rights_t fs_rights_base, __wasi_rights_t fs_rights_inheriting, __wasi_fdflags_t fdflags, __wasi_fd_t *fd);

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t path_open_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t dirfd,
        ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t dirflags,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len,
        ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t oflags,
        ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_base,
        ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_inheriting,
        ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t fdflags,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t fd_ptrsz) noexcept
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(env.wasip1_memory == nullptr) [[unlikely]]
        {
            // Security issues inherent to virtual machines
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif
        auto& memory{*env.wasip1_memory};

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
                                u8"path_open_wasm64",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                dirfd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(dirflags)>>>(dirflags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(path_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8", ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                path_len,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(oflags)>>>(oflags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fs_rights_base)>>>(fs_rights_base),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fs_rights_inheriting)>>>(fs_rights_inheriting),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fdflags)>>>(fdflags),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(fd_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
#else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: path_open_wasm64(",
                                dirfd,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(dirflags)>>>(dirflags),
                                u8", ",
                                ::fast_io::mnp::addrvw(path_ptrsz),
                                u8", ",
                                path_len,
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(oflags)>>>(oflags),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fs_rights_base)>>>(fs_rights_base),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fs_rights_inheriting)>>>(fs_rights_inheriting),
                                u8", ",
                                static_cast<::std::underlying_type_t<::std::remove_cvref_t<decltype(fdflags)>>>(fdflags),
                                u8", ",
                                ::fast_io::mnp::addrvw(fd_ptrsz),
                                u8") (wasi-trace)\n");
#endif
        }

        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(dirfd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

        auto& wasm_fd_storage{env.fd_storage};

        // The pointer to `wasm_fd` is fixed and remains unchanged even when the vector within `fd_manager` is resized.
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t* curr_wasi_fd_t_p;  // no initialize

        // storage, This is new file descriptor; no need to lock it.
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t new_wasi_fd{};

        {
            // Subsequent operations involving the file descriptor require locking. curr_fd_release_guard release when return.
            ::uwvm2::utils::mutex::mutex_merely_release_guard_t curr_fd_release_guard{};

            {
                // Prevent operations to obtain the size or perform resizing at this time.
                // Only a lock is required when acquiring the unique pointer for the file descriptor. The lock can be released once the acquisition is complete.
                // Since the file descriptor's location is fixed and accessed via the unique pointer,

                // Simply acquiring data using a shared_lock
                ::uwvm2::utils::mutex::rw_shared_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

                // Negative states have been excluded, so the conversion result will only be positive numbers.
                using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t>;
                auto const unsigned_dirfd{static_cast<unsigned_fd_t>(dirfd)};

                // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
                constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
                if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
                {
                    if(unsigned_dirfd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }
                }

                auto const dirfd_opens_pos{static_cast<::std::size_t>(unsigned_dirfd)};

                // The minimum value in rename_map is greater than opensize.
                if(wasm_fd_storage.opens.size() <= dirfd_opens_pos)
                {
                    // Possibly within the tree being renumbered
                    if(auto const renumber_map_iter{wasm_fd_storage.renumber_map.find(dirfd)}; renumber_map_iter != wasm_fd_storage.renumber_map.end())
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
                    // The addition here is safe.
                    curr_wasi_fd_t_p = wasm_fd_storage.opens.index_unchecked(dirfd_opens_pos).fd_p;
                }

                // curr_wasi_fd_t_p never nullptr
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                if(curr_wasi_fd_t_p == nullptr) [[unlikely]]
                {
                    // Security issues inherent to virtual machines
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
                }
#endif

                // Other threads will definitely lock fds_rwlock when performing close operations (since they need to access the fd vector). If the current
                // thread is performing fdatasync, no other thread can be executing any close operations simultaneously, eliminating any destruction issues.
                // Therefore, acquiring the lock at this point is safe. However, the problem arises when, immediately after acquiring the lock and before
                // releasing the manager lock and beginning fd operations, another thread executes a deletion that removes this fd. Subsequent operations by the
                // current thread would then encounter issues. Thus, locking must occur before releasing fds_rwlock.
                curr_fd_release_guard.device_p = ::std::addressof(curr_wasi_fd_t_p->fd_mutex);
                curr_fd_release_guard.lock();

                // After unlocking fds_lock, members within `wasm_fd_storage_t` can no longer be accessed or modified.
            }

            // curr_fd_uniptr is not null.
            auto& curr_fd{*curr_wasi_fd_t_p};

            // If obtained from the renumber map, it will always be the correct value. If obtained from the open vec, it requires checking whether it is closed.
            // Therefore, a unified check is implemented.
            if(curr_fd.close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

            if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_path_open) !=
               ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_path_open) [[unlikely]]
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable;
            }

            // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files from
            // being processed, making this a virtual machine implementation error.
            if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
            {
// This will be checked at runtime.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
            }

            switch(curr_fd.wasi_fd.ptr->wasi_fd_storage.type)
            {
                [[unlikely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::null:
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                }
                case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file:
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir;
                }
                [[likely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
                {
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket:
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir;
                }
#endif
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                }
            }

            // Retrieve the current directory, which is the top element of the directory stack.
            auto const& curr_dir_stack{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
            if(curr_dir_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio; }

            auto const& curr_dir_stack_entry{curr_dir_stack.dir_stack.back_unchecked()};
            if(curr_dir_stack_entry.ptr == nullptr) [[unlikely]]
            {
// This will be checked at runtime.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
            }

            auto const& curr_fd_native_file{curr_dir_stack_entry.ptr->dir_stack.file};

            bool const symlink_follow{(dirflags & ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t::lookup_symlink_follow) ==
                                      ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t::lookup_symlink_follow};

            // check path (Copy from WASM memory for use)
            ::uwvm2::utils::container::u8string path{};

            if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t>::max() > ::std::numeric_limits<::std::size_t>::max())
            {
                if(path_len > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eoverflow;
                }
            }

            {
                // Full locking is required during reading.
                [[maybe_unused]] auto const memory_locker_guard{::uwvm2::imported::wasi::wasip1::memory::lock_memory(memory)};

                ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64_unlocked(memory, path_ptrsz, path_len);

                using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

                auto const path_begin{memory.memory_begin + path_ptrsz};

                path.assign(
                    ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(path_begin), static_cast<::std::size_t>(path_len)});
            }

            if(path.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval; }

            // WASI does not guarantee that strings are null-terminated, so you must check for zero characters in the middle and construct one yourself.
            if(!env.disable_utf8_check) [[likely]]
            {
                auto const u8res{
                    ::uwvm2::utils::utf::check_legal_utf8<::uwvm2::utils::utf::utf8_specification::utf8_rfc3629_and_zero_illegal>(path.cbegin(), path.cend())};
                if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
                {
                    // If the path string is not valid UTF-8, the function shall fail with ERRNO_ILSEQ.
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq;
                }
            }
            else
            {
                auto const u8res{::uwvm2::utils::utf::check_has_zero_illegal_unchecked(path.cbegin(), path.cend())};
                if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq;
                }
            }

            auto const split_path_res{
                ::uwvm2::imported::wasi::wasip1::func::split_posix_path(::uwvm2::utils::container::u8string_view{path.data(), path.size()})};

            // The path series functions in wasip1 reject absolute paths.
            if(split_path_res.is_absolute) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm; }
            if(split_path_res.res.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval; }

#if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
            // For the Windows API, the parsing strategy differs from POSIX. Windows supports the backslash as a delimiter while rejecting many characters.
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
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                        }
                    }
                }
            }
#endif

            // The new base permissions must be a subset of the old base permissions.
            // Both base permissions and inherited permissions must be secured simultaneously to prevent access via inherited permissions. After scanning base
            // permissions, operations should proceed.
            //
            if((fs_rights_base & ~curr_fd.rights_base) != ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t{} ||
               (fs_rights_base & ~curr_fd.rights_inherit) != ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t{})
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable;
            }

            // The new inheriting permissions must be a subset of the old inheriting permissions.
            if((fs_rights_inheriting & ~curr_fd.rights_inherit) != ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t{})
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable;
            }

            // check posixfd buffer
            ::uwvm2::imported::wasi::wasip1::memory::check_memory_bounds_wasm64(memory,
                                                                                fd_ptrsz,
                                                                                sizeof(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t));

            // storage, This is new file descriptor; no need to lock it.
            new_wasi_fd.fd_p->rights_base = fs_rights_base;
            new_wasi_fd.fd_p->rights_inherit = fs_rights_inheriting;

            // oflags
            auto const is_creat{(oflags & ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_creat) ==
                                ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_creat};
            auto const is_dir{(oflags & ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_directory) ==
                              ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_directory};
            auto const is_excl{(oflags & ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_excl) ==
                               ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_excl};
            auto const is_trunc{(oflags & ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_trunc) ==
                                ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t::o_trunc};

            auto const is_append{(fdflags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_append) ==
                                 ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_append};
            auto const is_dsync{(fdflags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_dsync) ==
                                ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_dsync};
            auto const is_nonblock{(fdflags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_nonblock) ==
                                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_nonblock};
            auto const is_rsync{(fdflags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_rsync) ==
                                ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_rsync};
            auto const is_sync{(fdflags & ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_sync) ==
                               ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_sync};

            auto const is_read{(fs_rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_read) ==
                               ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_read};
            auto const is_write{(fs_rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write) ==
                                ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_fd_write};

            // If the request is truncated, write permissions must be granted.
            if(is_trunc && !is_write) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval; }
            if(is_dir)
            {
                if(is_trunc || is_append || is_excl || is_creat) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval; }
            }
            if(is_excl && !is_creat) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval; }

            ::fast_io::open_mode fast_io_oflags{};
            if(is_creat) { fast_io_oflags |= ::fast_io::open_mode::creat; }
            if(is_dir) { fast_io_oflags |= ::fast_io::open_mode::directory; }
            if(is_excl) { fast_io_oflags |= ::fast_io::open_mode::excl; }
            if(is_trunc) { fast_io_oflags |= ::fast_io::open_mode::trunc; }

            if(is_append) { fast_io_oflags |= ::fast_io::open_mode::app; }
            if(is_nonblock) { fast_io_oflags |= ::fast_io::open_mode::no_block; }
            if(is_sync) { fast_io_oflags |= ::fast_io::open_mode::sync; }
            if(is_dsync)
            {
#ifdef O_DSYNC
                fast_io_oflags |= ::fast_io::open_mode::dsync;
#else
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup;
#endif
            }
            if(is_rsync)
            {
#ifdef O_RSYNC
                fast_io_oflags |= ::fast_io::open_mode::rsync;
#else
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotsup;
#endif
            }

            if(is_read) { fast_io_oflags |= ::fast_io::open_mode::in; }
            if(is_write) { fast_io_oflags |= ::fast_io::open_mode::out; }

            if(!is_read && !is_write && !is_dir) { fast_io_oflags |= ::fast_io::open_mode::in; }

            // fast_io no fsync and dsync

            if(is_dir) { new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir); }
            else
            {
                new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
                // Since Win32 cannot retrieve flags after creation, they must be recorded simultaneously.
                // Exclude representations not supported by Windows
                constexpr auto and_flags{~(::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_rsync |
                                           ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_dsync |
                                           ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t::fdflag_sync)};
                new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.file_fd.fdflags = fdflags & and_flags;
#endif
            }

            // path stack
            ::uwvm2::utils::container::vector<::uwvm2::imported::wasi::wasip1::func::dir_with_name_t> path_stack{};
            path_stack.reserve(split_path_res.res.size());

            // cend cannot be nullptr
            auto const split_last{split_path_res.res.cend() - 1u};

            for(auto split_curr{split_path_res.res.begin()}; split_curr != split_path_res.res.end(); ++split_curr)
            {
                if(split_curr == split_last)
                {
                    // Create the final path

                    switch(split_curr->dir_type)
                    {
                        case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                        {
                            if(is_dir)
                            {
                                auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                storage_dir_stack = curr_dir_stack;

                                for(auto& dir_with_name_curr: path_stack)
                                {
                                    // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                    auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                    newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr.name);
                                    newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr.file);
                                }
                            }
                            else [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisdir;
                            }

                            break;
                        }
                        case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                        {
                            if(is_dir)
                            {
                                if(path_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm; }
                                else
                                {
                                    auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                    storage_dir_stack = curr_dir_stack;

                                    auto const path_stack_last{path_stack.cend() - 1u};

                                    for(auto dir_with_name_curr{path_stack.cbegin()}; dir_with_name_curr != path_stack_last; ++dir_with_name_curr)
                                    {
                                        // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                        auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                        newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr->name);
                                        newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr->file);
                                    }
                                }
                            }
                            else [[unlikely]]
                            {
                                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eisdir;
                            }

                            break;
                        }
                        case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                        {
                            // detect symlink
                            auto& next_name{split_curr->next_name};

                            ::uwvm2::utils::container::u8string symlink_symbol{};
                            bool is_symlink{};

                            if(path_stack.empty())
                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // readlinkat is symlink_nofollow
                                    symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_file), next_name);
                                    is_symlink = true;
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                }
#endif

                                if(is_symlink)
                                {
                                    if(!symlink_follow)
                                    {
                                        if(is_dir) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                        else
                                        {
                                            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eloop;
                                        }
                                    }
                                    else
                                    {
                                        auto [file_symlink_iterative_err,
                                              file_symlink_iterative_file_name]{::uwvm2::imported::wasi::wasip1::func::file_symlink_iterative_with_name(
                                            env.disable_utf8_check,
                                            curr_fd_native_file,
                                            path_stack,
                                            ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                        if(file_symlink_iterative_err != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) [[unlikely]]
                                        {
                                            return file_symlink_iterative_err;
                                        }

                                        // file_symlink_iterative_file_name is not symlink
                                        // path_stack maybe empty
                                        auto& open_file_name{file_symlink_iterative_file_name};

                                        if(path_stack.empty())
                                        {
                                            if(is_dir)
                                            {
                                                auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                                storage_dir_stack = curr_dir_stack;

                                                for(auto& dir_with_name_curr: path_stack)
                                                {
                                                    // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                                    auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                    newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr.name);
                                                    newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr.file);
                                                }

                                                ::fast_io::dir_file new_dir_file{};

#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // dir_file default nofollow
                                                    new_dir_file = ::fast_io::dir_file{at(curr_fd_native_file), open_file_name, fast_io_oflags};

#if defined(__CYGWIN__)
                                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and
                                                    // files to be opened simultaneously. A check must be added here.
                                                    struct ::stat st;
                                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(new_dir_file.native_handle(), ::std::addressof(st));
                                                    if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                                    // required.
                                                    ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
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
                                                            // native_file default nofollow
                                                            ::fast_io::native_file{at(curr_fd_native_file), open_file_name, ::fast_io::open_mode::in};
                                                            is_file = true;
                                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                                        catch(::fast_io::error e)
                                                        {
                                                        }
#  endif
                                                        if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                                    }
# endif

                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif

                                                auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                newdir.ptr->dir_stack.name = ::std::move(open_file_name);
                                                newdir.ptr->dir_stack.file = ::std::move(new_dir_file);
                                            }
                                            else
                                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // native_file default nofollow
                                                    new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                                                        .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                                                        .file
#endif
                                                        = ::fast_io::native_file{at(curr_fd_native_file), open_file_name, fast_io_oflags};
                                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                                catch(::fast_io::error e)
                                                {
                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif
                                            }
                                        }
                                        else
                                        {
                                            if(is_dir)
                                            {
                                                auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                                storage_dir_stack = curr_dir_stack;

                                                for(auto& dir_with_name_curr: path_stack)
                                                {
                                                    // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                                    auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                    newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr.name);
                                                    newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr.file);
                                                }

                                                ::fast_io::dir_file new_dir_file{};

#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // dir_file default nofollow
                                                    new_dir_file = ::fast_io::dir_file{at(path_stack.back_unchecked().file), open_file_name, fast_io_oflags};

#if defined(__CYGWIN__)
                                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and
                                                    // files to be opened simultaneously. A check must be added here.
                                                    struct ::stat st;
                                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(new_dir_file.native_handle(), ::std::addressof(st));
                                                    if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                                    // required.
                                                    ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
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
                                                            // native_file default nofollow
                                                            ::fast_io::native_file{at(path_stack.back_unchecked().file),
                                                                                   open_file_name,
                                                                                   ::fast_io::open_mode::in};
                                                            is_file = true;
                                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                                        catch(::fast_io::error e)
                                                        {
                                                        }
#  endif
                                                        if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                                    }
# endif

                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif

                                                auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                newdir.ptr->dir_stack.name = ::std::move(open_file_name);
                                                newdir.ptr->dir_stack.file = ::std::move(new_dir_file);
                                            }
                                            else
                                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // native_file default nofollow
                                                    new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                                                        .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                                                        .file
#endif
                                                        = ::fast_io::native_file{at(path_stack.back_unchecked().file), open_file_name, fast_io_oflags};
                                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                                catch(::fast_io::error e)
                                                {
                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    auto& open_file_name{next_name};

                                    if(is_dir)
                                    {
                                        auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                        storage_dir_stack = curr_dir_stack;

                                        for(auto& dir_with_name_curr: path_stack)
                                        {
                                            // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                            auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                            newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr.name);
                                            newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr.file);
                                        }

                                        ::fast_io::dir_file new_dir_file{};

#ifdef UWVM_CPP_EXCEPTIONS
                                        try
#endif
                                        {
                                            // dir_file default nofollow
                                            new_dir_file = ::fast_io::dir_file{at(curr_fd_native_file), open_file_name, fast_io_oflags};

#if defined(__CYGWIN__)
                                            // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files
                                            // to be opened simultaneously. A check must be added here.
                                            struct ::stat st;
                                            ::uwvm2::imported::wasi::wasip1::func::posix::fstat(new_dir_file.native_handle(), ::std::addressof(st));
                                            if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                            // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                            // required.
                                            ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
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
                                                    // native_file default nofollow
                                                    ::fast_io::native_file{at(curr_fd_native_file), open_file_name, ::fast_io::open_mode::in};
                                                    is_file = true;
                                                }
#  ifdef UWVM_CPP_EXCEPTIONS
                                                catch(::fast_io::error e)
                                                {
                                                }
#  endif
                                                if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                            }
# endif

                                            return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                        }
#endif

                                        auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                        newdir.ptr->dir_stack.name = ::std::move(open_file_name);
                                        newdir.ptr->dir_stack.file = ::std::move(new_dir_file);
                                    }
                                    else
                                    {
#ifdef UWVM_CPP_EXCEPTIONS
                                        try
#endif
                                        {
                                            // native_file default nofollow
                                            new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                                                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                                                .file
#endif
                                                = ::fast_io::native_file{at(curr_fd_native_file), open_file_name, fast_io_oflags};
                                        }
#ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                            return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                        }
#endif
                                    }
                                }
                            }
                            else
                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // readlinkat is symlink_nofollow
                                    symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(path_stack.back_unchecked().file), next_name);
                                    is_symlink = true;
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                }
#endif

                                if(is_symlink)
                                {
                                    if(!symlink_follow)
                                    {
                                        if(is_dir) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                        else
                                        {
                                            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eloop;
                                        }
                                    }
                                    else
                                    {
                                        auto [file_symlink_iterative_err,
                                              file_symlink_iterative_file_name]{::uwvm2::imported::wasi::wasip1::func::file_symlink_iterative_with_name(
                                            env.disable_utf8_check,
                                            curr_fd_native_file,
                                            path_stack,
                                            ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                        if(file_symlink_iterative_err != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) [[unlikely]]
                                        {
                                            return file_symlink_iterative_err;
                                        }

                                        // file_symlink_iterative_file_name is not symlink
                                        // path_stack maybe empty
                                        auto& open_file_name{file_symlink_iterative_file_name};

                                        if(path_stack.empty())
                                        {
                                            if(is_dir)
                                            {
                                                auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                                storage_dir_stack = curr_dir_stack;

                                                for(auto& dir_with_name_curr: path_stack)
                                                {
                                                    // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                                    auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                    newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr.name);
                                                    newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr.file);
                                                }

                                                ::fast_io::dir_file new_dir_file{};

#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // dir_file default nofollow
                                                    new_dir_file = ::fast_io::dir_file{at(curr_fd_native_file), open_file_name, fast_io_oflags};

#if defined(__CYGWIN__)
                                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and
                                                    // files to be opened simultaneously. A check must be added here.
                                                    struct ::stat st;
                                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(new_dir_file.native_handle(), ::std::addressof(st));
                                                    if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                                    // required.
                                                    ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
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
                                                            // native_file default nofollow
                                                            ::fast_io::native_file{at(curr_fd_native_file), open_file_name, ::fast_io::open_mode::in};
                                                            is_file = true;
                                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                                        catch(::fast_io::error e)
                                                        {
                                                        }
#  endif
                                                        if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                                    }
# endif

                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif

                                                auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                newdir.ptr->dir_stack.name = ::std::move(open_file_name);
                                                newdir.ptr->dir_stack.file = ::std::move(new_dir_file);
                                            }
                                            else
                                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // native_file default nofollow
                                                    new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                                                        .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                                                        .file
#endif
                                                        = ::fast_io::native_file{at(curr_fd_native_file), open_file_name, fast_io_oflags};
                                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                                catch(::fast_io::error e)
                                                {
                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif
                                            }
                                        }
                                        else
                                        {
                                            if(is_dir)
                                            {
                                                auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                                storage_dir_stack = curr_dir_stack;

                                                for(auto& dir_with_name_curr: path_stack)
                                                {
                                                    // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                                    auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                    newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr.name);
                                                    newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr.file);
                                                }

                                                ::fast_io::dir_file new_dir_file{};

#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // dir_file default nofollow
                                                    new_dir_file = ::fast_io::dir_file{at(path_stack.back_unchecked().file), open_file_name, fast_io_oflags};

#if defined(__CYGWIN__)
                                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and
                                                    // files to be opened simultaneously. A check must be added here.
                                                    struct ::stat st;
                                                    ::uwvm2::imported::wasi::wasip1::func::posix::fstat(new_dir_file.native_handle(), ::std::addressof(st));
                                                    if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                                    // required.
                                                    ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
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
                                                            // native_file default nofollow
                                                            ::fast_io::native_file{at(path_stack.back_unchecked().file),
                                                                                   open_file_name,
                                                                                   ::fast_io::open_mode::in};
                                                            is_file = true;
                                                        }
#  ifdef UWVM_CPP_EXCEPTIONS
                                                        catch(::fast_io::error e)
                                                        {
                                                        }
#  endif
                                                        if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                                    }
# endif

                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif

                                                auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                                newdir.ptr->dir_stack.name = ::std::move(open_file_name);
                                                newdir.ptr->dir_stack.file = ::std::move(new_dir_file);
                                            }
                                            else
                                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                                try
#endif
                                                {
                                                    // native_file default nofollow
                                                    new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                                                        .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                                                        .file
#endif
                                                        = ::fast_io::native_file{at(path_stack.back_unchecked().file), open_file_name, fast_io_oflags};
                                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                                catch(::fast_io::error e)
                                                {
                                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                                }
#endif
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    auto& open_file_name{next_name};

                                    if(is_dir)
                                    {
                                        auto& storage_dir_stack{new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
                                        storage_dir_stack = curr_dir_stack;

                                        for(auto& dir_with_name_curr: path_stack)
                                        {
                                            // Create a new `dir_file` by moving an existing one to ensure no touch-and-touch-out issues.
                                            auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                            newdir.ptr->dir_stack.name = ::std::move(dir_with_name_curr.name);
                                            newdir.ptr->dir_stack.file = ::std::move(dir_with_name_curr.file);
                                        }

                                        ::fast_io::dir_file new_dir_file{};

#ifdef UWVM_CPP_EXCEPTIONS
                                        try
#endif
                                        {
                                            // dir_file default nofollow
                                            new_dir_file = ::fast_io::dir_file{at(path_stack.back_unchecked().file), open_file_name, fast_io_oflags};

#if defined(__CYGWIN__)
                                            // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files
                                            // to be opened simultaneously. A check must be added here.
                                            struct ::stat st;
                                            ::uwvm2::imported::wasi::wasip1::func::posix::fstat(new_dir_file.native_handle(), ::std::addressof(st));
                                            if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
#elif defined(__MSDOS__) || defined(__DJGPP__)
                                            // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                            // required.
                                            ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
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
                                                    // native_file default nofollow
                                                    ::fast_io::native_file{at(path_stack.back_unchecked().file), open_file_name, ::fast_io::open_mode::in};
                                                    is_file = true;
                                                }
#  ifdef UWVM_CPP_EXCEPTIONS
                                                catch(::fast_io::error e)
                                                {
                                                }
#  endif
                                                if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                            }
# endif

                                            return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                        }
#endif

                                        auto& newdir{storage_dir_stack.dir_stack.emplace_back()};
                                        newdir.ptr->dir_stack.name = ::std::move(open_file_name);
                                        newdir.ptr->dir_stack.file = ::std::move(new_dir_file);
                                    }
                                    else
                                    {
#ifdef UWVM_CPP_EXCEPTIONS
                                        try
#endif
                                        {
                                            // native_file default nofollow
                                            new_wasi_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                                                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                                                .file
#endif
                                                = ::fast_io::native_file{at(path_stack.back_unchecked().file), open_file_name, fast_io_oflags};
                                        }
#ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                            return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                        }
#endif
                                    }
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
                    // The path traversed in the middle

                    switch(split_curr->dir_type)
                    {
                        case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                        {
                            break;
                        }
                        case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                        {
                            if(path_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eperm; }
                            else
                            {
                                path_stack.pop_back_unchecked();
                            }

                            break;
                        }
                        case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                        {
                            auto& next_name{split_curr->next_name};

                            ::uwvm2::utils::container::u8string symlink_symbol{};
                            bool is_symlink{};

                            if(path_stack.empty())
                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // readlinkat is symlink_nofollow
                                    symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_file), next_name);
                                    is_symlink = true;
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                }
#endif

                                if(is_symlink)
                                {
                                    if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio; }

                                    auto const errno_wasm64_t{::uwvm2::imported::wasi::wasip1::func::path_symlink_iterative_with_name(
                                        env.disable_utf8_check,
                                        curr_fd_native_file,
                                        path_stack,
                                        ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                    if(errno_wasm64_t != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) [[unlikely]] { return errno_wasm64_t; }
                                }
                                else
                                {
                                    ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                                    try
#endif
                                    {
                                        // default is symlink_nofollow
                                        next = ::fast_io::dir_file{at(curr_fd_native_file), next_name};

                                        // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                        // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                        // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                        // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                        // opened simultaneously. A check must be added here.
                                        struct ::stat st;
                                        ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                        if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
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
                                                // native_file default nofollow
                                                ::fast_io::native_file{at(curr_fd_native_file), next_name, ::fast_io::open_mode::in};
                                                is_file = true;
                                            }
#  ifdef UWVM_CPP_EXCEPTIONS
                                            catch(::fast_io::error e)
                                            {
                                            }
#  endif
                                            if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                        }
# endif

                                        return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                    }
#endif

                                    path_stack.emplace_back(::std::move(next_name), ::std::move(next));
                                }
                            }
                            else
                            {
#ifdef UWVM_CPP_EXCEPTIONS
                                try
#endif
                                {
                                    // readlinkat is symlink_nofollow
                                    symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(path_stack.back_unchecked().file), next_name);
                                    is_symlink = true;
                                }
#ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                }
#endif

                                if(is_symlink)
                                {
                                    if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio; }

                                    auto const errno_wasm64_t{::uwvm2::imported::wasi::wasip1::func::path_symlink_iterative_with_name(
                                        env.disable_utf8_check,
                                        curr_fd_native_file,
                                        path_stack,
                                        ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                    if(errno_wasm64_t != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) [[unlikely]] { return errno_wasm64_t; }
                                }
                                else
                                {
                                    ::fast_io::dir_file next{};
#ifdef UWVM_CPP_EXCEPTIONS
                                    try
#endif
                                    {
                                        // default is symlink_nofollow
                                        next = ::fast_io::dir_file{at(path_stack.back_unchecked().file), next_name};

                                        // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                        // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                        // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

#if defined(__CYGWIN__)
                                        // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                        // opened simultaneously. A check must be added here.
                                        struct ::stat st;
                                        ::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st));
                                        if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
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
                                                // native_file default nofollow
                                                ::fast_io::native_file{at(path_stack.back_unchecked().file), next_name, ::fast_io::open_mode::in};
                                                is_file = true;
                                            }
#  ifdef UWVM_CPP_EXCEPTIONS
                                            catch(::fast_io::error e)
                                            {
                                            }
#  endif
                                            if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                        }
# endif

                                        return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                    }
#endif

                                    path_stack.emplace_back(::std::move(next_name), ::std::move(next));
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

            // curr_fd_release_guard destructor release the lock.
        }

        // When modifying fd_manager, ensure no fd_mutex is held. Otherwise, deadlocks may occur.

        using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;

        fd_t new_fd{};

        {
            // Manipulating fd_manager requires a unique_lock.
            ::uwvm2::utils::mutex::rw_unique_guard_t fds_lock{wasm_fd_storage.fds_rwlock};

            // check limit
            using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
            if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
            {
                if(env.fd_storage.fd_limit > ::std::numeric_limits<fd_t>::max()) [[unlikely]]
                {
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                }
            }

            auto const open_size{wasm_fd_storage.opens.size()};
            auto const close_size{wasm_fd_storage.closes.size()};
            auto const renumber_size{wasm_fd_storage.renumber_map.size()};

            // There is no need to worry about overflow here, as all files in wasip1 are created using path_open and are incrementally assigned.
            auto const all_size{open_size - close_size + renumber_size};
            if(all_size >= env.fd_storage.fd_limit) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile; }

            // create new
            if(!wasm_fd_storage.closes.empty())
            {
                auto const new_fd_pos{wasm_fd_storage.closes.back_unchecked()};
                wasm_fd_storage.closes.pop_back_unchecked();

                // Do not use `index_unchecked` here. Use a checked index `operator[]` to ensure safety.
                auto& new_wasi_fd_p{wasm_fd_storage.opens[new_fd_pos]};

                new_wasi_fd_p = ::std::move(new_wasi_fd);

                new_fd = static_cast<fd_t>(new_fd_pos);
            }
            else
            {

                if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
                {
                    if(wasm_fd_storage.opens.size() >= ::std::numeric_limits<fd_t>::max()) [[unlikely]]
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::emfile;
                    }
                }

                auto const& new_wasm_fd{wasm_fd_storage.opens.emplace_back(::std::move(new_wasi_fd))};

                new_fd = static_cast<fd_t>(static_cast<::std::size_t>(::std::addressof(new_wasm_fd) - wasm_fd_storage.opens.cbegin()));

                // To normalize it, start traversing from renumber. If it's an open size, move it to open vec.
                auto open_size{wasm_fd_storage.opens.size()};

                // Use while loop to avoid iterator invalidation during erase
                for(;;)
                {
                    auto it2{wasm_fd_storage.renumber_map.find(static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t>(open_size))};
                    if(it2 == wasm_fd_storage.renumber_map.end()) { break; }

                    wasm_fd_storage.opens.push_back(it2->second.release());
                    wasm_fd_storage.renumber_map.erase(it2);
                    ++open_size;
                }
            }
        }

        // checked before
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64_unchecked(memory, fd_ptrsz, new_fd);

        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess;
    }
}  // namespace uwvm2::imported::wasi::wasip1::func

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif

