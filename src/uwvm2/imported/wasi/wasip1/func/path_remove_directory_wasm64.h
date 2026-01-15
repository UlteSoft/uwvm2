

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
    /// @brief     WasiPreview1.path_remove_directory_wasm64
    /// @details   __wasi_errno_wasm64_t path_remove_directory_wasm64(__wasi_fd_t fd, const char *path, size_t path_len);

    inline ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t path_remove_directory_wasm64(
        ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> & env,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
        ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len) noexcept
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
                                u8"path_remove_directory_wasm64",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"(",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                fd,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                ::fast_io::mnp::addrvw(path_ptrsz),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8",",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                path_len,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(wasi-trace)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
# else
            ::fast_io::io::perr(::fast_io::u8err(),
                                u8"uwvm: [info]  wasip1: path_remove_directory_wasm64(",
                                fd,
                                u8", ",
                                ::fast_io::mnp::addrvw(path_ptrsz),
                                u8", ",
                                path_len,
                                u8") (wasi-trace)\n");
# endif
        }

        // The negative value fd is invalid, and this check prevents subsequent undefined behavior.
        if(fd < 0) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

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
            using unsigned_fd_t = ::std::make_unsigned_t<::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t>;
            auto const unsigned_fd{static_cast<unsigned_fd_t>(fd)};

            // On platforms where `size_t` is smaller than the `fd` type, this check must be added.
            constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
            if constexpr(::std::numeric_limits<unsigned_fd_t>::max() > size_t_max)
            {
                if(unsigned_fd > size_t_max) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }
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
                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf;
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
        if(curr_fd.close_pos != SIZE_MAX) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::ebadf; }

        if((curr_fd.rights_base & ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_path_remove_directory) !=
           ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t::right_path_remove_directory) [[unlikely]]
        {
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable;
        }

        // If ptr is null, it indicates an attempt to open a closed file. However, the preceding check for close pos already prevents such closed files from
        // being processed, making this a virtual machine implementation error.
        if(curr_fd.wasi_fd.ptr == nullptr) [[unlikely]]
        {
// This will be checked at runtime.
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
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file_observer:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir;
            }
            [[likely]] case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir:
            {
                break;
            }
# if defined(_WIN32) && !defined(__CYGWIN__)
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket: [[fallthrough]];
            case ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::socket_observer:
            {
                return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir;
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

        // Retrieve the current directory, which is the top element of the directory stack.
        auto const& curr_dir_stack{curr_fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack};
        if(curr_dir_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio; }

        auto const& curr_dir_stack_entry{curr_dir_stack.dir_stack.back_unchecked()};
        if(curr_dir_stack_entry.ptr == nullptr) [[unlikely]]
        {
// This will be checked at runtime.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
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

        // check path (Copy from WASM memory for use)
        ::uwvm2::utils::container::u8string path{};

        if constexpr(::std::numeric_limits<::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t>::max() > ::std::numeric_limits<::std::size_t>::max())
        {
            if(path_len > ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eoverflow; }
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
            if(u8res.err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eilseq; }
        }

        auto const split_path_res{::uwvm2::imported::wasi::wasip1::func::split_posix_path(::uwvm2::utils::container::u8string_view{path.data(), path.size()})};

        // The path series functions in wasip1 reject absolute paths.
        if(split_path_res.is_absolute) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable; }
        if(split_path_res.res.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval; }

# if (defined(_WIN32) || defined(__CYGWIN__)) || (defined(__MSDOS__) || defined(__DJGPP__))
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
# endif

        ::uwvm2::utils::container::vector<::fast_io::dir_file> path_stack{};
        path_stack.reserve(split_path_res.res.size());

        // cend cannot be nullptr
        auto const split_last{split_path_res.res.cend() - 1u};

        for(auto split_curr{split_path_res.res.cbegin()}; split_curr != split_path_res.res.cend(); ++split_curr)
        {
            if(split_curr == split_last)
            {
                // Create the final path

                switch(split_curr->dir_type)
                {
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::curr:
                    {
                        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::prev:
                    {
                        if(path_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable; }
                        else
                        {
                            return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::einval;
                        }
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                    {
                        auto const& next_name{split_curr->next_name};

                        ::uwvm2::utils::container::u8string symlink_symbol{};

                        if(path_stack.empty())
                        {
                            auto const& open_file_name{next_name};

                            // path_stack is always empty
# ifdef UWVM_CPP_EXCEPTIONS
                            try
# endif
                            {
# if defined(__MSDOS__) || defined(__DJGPP__)
                                // djgpp's `unlink` function does not distinguish between directories and files; manual differentiation is
                                // required.
                                // The directory file defaults to nofollow, but DOS doesn't support symlinks.
                                {
                                    // dir_file default nofollow
                                    ::fast_io::dir_file const new_dir_file{at(curr_fd_native_file), open_file_name};
                                    ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
                                }
# endif

                                // native_unlinkat is default nofollow
                                ::fast_io::native_unlinkat(at(curr_fd_native_file), open_file_name, ::fast_io::native_at_flags::removedir);
                            }
# ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
#  if (defined(__MSDOS__) || defined(__DJGPP__)) || (defined(_WIN32) && defined(_WIN32_WINDOWS))
                                // dos/win9x no enotempty, when notempty, it returns EACCES. You must open the directory yourself to determine the status.
                                if(e.code ==
#   if defined(__MSDOS__) || defined(__DJGPP__)
                                   EACCES
#   elif defined(_WIN32) && defined(_WIN32_WINDOWS)
                                   5uz /*ERROR_ACCESS_DENIED*/
#   endif
                                )
                                {
#   ifdef UWVM_CPP_EXCEPTIONS
                                    try
#   endif
                                    {
                                        // dir_file default nofollow
                                        ::fast_io::dir_file const new_dir_file{at(curr_fd_native_file), open_file_name};

                                        unsigned counter{};
                                        for([[maybe_unused]] auto const& curr_dir: current(at(new_dir_file)))
                                        {
                                            ++counter;
                                            if(counter > 2u) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotempty; }
                                        }
                                    }
#   ifdef UWVM_CPP_EXCEPTIONS
                                    catch(::fast_io::error e)
                                    {
                                        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                                    }
#   endif
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                                }
                                else
                                {
                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                }

#  elif !(defined(_WIN32) && !defined(__CYGWIN__)) && defined(EEXIST)
                                // Note: Modern POSIX systems never return EEXIST for directory removal.
                                // Historically, some early UNIX/Linux systems used EEXIST to indicate
                                // "directory not empty", but this was replaced by ENOTEMPTY after POSIX
                                // standardized the error code. Therefore, any EEXIST returned here should
                                // be treated as legacy behavior equivalent to ENOTEMPTY.

                                if(e.code == EEXIST) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotempty; }
                                else
                                {
                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                }
#  else
                                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
#  endif
                            }
# endif
                        }
                        else
                        {
                            auto const& open_file_name{next_name};

                            // path_stack is not empty
# ifdef UWVM_CPP_EXCEPTIONS
                            try
# endif
                            {
# if defined(__MSDOS__) || defined(__DJGPP__)
                                // djgpp's `unlink` function does not distinguish between directories and files; manual differentiation is
                                // required.
                                // The directory file defaults to nofollow, but DOS doesn't support symlinks.
                                {
                                    // dir_file default nofollow
                                    ::fast_io::dir_file const new_dir_file{at(path_stack.back_unchecked()), open_file_name};
                                    ::fast_io::details::check_dos_fd_is_dir(new_dir_file.native_handle());
                                }
# endif

                                // native_unlinkat is default nofollow
                                ::fast_io::native_unlinkat(at(path_stack.back_unchecked()), open_file_name, ::fast_io::native_at_flags::removedir);
                            }
# ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
#  if (defined(__MSDOS__) || defined(__DJGPP__)) || (defined(_WIN32) && defined(_WIN32_WINDOWS))
                                // dos/win9x no enotempty, when notempty, it returns EACCES. You must open the directory yourself to determine the status.
                                if(e.code ==
#   if defined(__MSDOS__) || defined(__DJGPP__)
                                   EACCES
#   elif defined(_WIN32) && defined(_WIN32_WINDOWS)
                                   5uz /*ERROR_ACCESS_DENIED*/
#   endif
                                )
                                {
#   ifdef UWVM_CPP_EXCEPTIONS
                                    try
#   endif
                                    {
                                        // dir_file default nofollow
                                        ::fast_io::dir_file const new_dir_file{at(path_stack.back_unchecked()), open_file_name};

                                        unsigned counter{};
                                        for([[maybe_unused]] auto const& curr_dir: current(at(new_dir_file)))
                                        {
                                            ++counter;
                                            if(counter > 2u) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotempty; }
                                        }
                                    }
#   ifdef UWVM_CPP_EXCEPTIONS
                                    catch(::fast_io::error e)
                                    {
                                        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                                    }
#   endif
                                    return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eacces;
                                }
                                else
                                {
                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                }

#  elif !(defined(_WIN32) && !defined(__CYGWIN__)) && defined(EEXIST)
                                // Note: Modern POSIX systems never return EEXIST for directory removal.
                                // Historically, some early UNIX/Linux systems used EEXIST to indicate
                                // "directory not empty", but this was replaced by ENOTEMPTY after POSIX
                                // standardized the error code. Therefore, any EEXIST returned here should
                                // be treated as legacy behavior equivalent to ENOTEMPTY.

                                if(e.code == EEXIST) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotempty; }
                                else
                                {
                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                }
#  else
                                return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
#  endif
                            }
# endif
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
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
                        if(path_stack.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotcapable; }
                        else
                        {
                            path_stack.pop_back_unchecked();
                        }

                        break;
                    }
                    case ::uwvm2::imported::wasi::wasip1::func::dir_type_e::next:
                    {
                        auto const& next_name{split_curr->next_name};

                        ::uwvm2::utils::container::u8string symlink_symbol{};
                        bool is_symlink{};

                        if(path_stack.empty())
                        {
# ifdef UWVM_CPP_EXCEPTIONS
                            try
# endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(curr_fd_native_file), next_name);
                                is_symlink = true;
                            }
# ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
# endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio; }

                                auto const errno_wasm64_t{::uwvm2::imported::wasi::wasip1::func::path_symlink_iterative(
                                    env.disable_utf8_check,
                                    curr_fd_native_file,
                                    path_stack,
                                    ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                if(errno_wasm64_t != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) [[unlikely]] { return errno_wasm64_t; }
                            }
                            else
                            {
                                ::fast_io::dir_file next{};
# ifdef UWVM_CPP_EXCEPTIONS
                                try
# endif
                                {
                                    // default is symlink_nofollow
                                    next = ::fast_io::dir_file{at(curr_fd_native_file), next_name};

                                    // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                    // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                    // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

# if defined(__CYGWIN__)
                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                    // opened simultaneously. A check must be added here.
                                    struct ::stat st;
                                    if(::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st)) < 0) [[unlikely]]
                                    {
                                        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                                    }
                                    if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
# elif defined(__MSDOS__) || defined(__DJGPP__)
                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                    // required.
                                    ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
# endif
                                }
# ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                    // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

#  if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                    if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                    {
                                        bool is_file{};
#   ifdef UWVM_CPP_EXCEPTIONS
                                        try
#   endif
                                        {
                                            // native_file default nofollow
                                            ::fast_io::native_file{at(curr_fd_native_file), next_name, ::fast_io::open_mode::in};
                                            is_file = true;
                                        }
#   ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                        }
#   endif
                                        if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                    }
#  endif

                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                }
# endif

                                path_stack.push_back(::std::move(next));
                            }
                        }
                        else
                        {
# ifdef UWVM_CPP_EXCEPTIONS
                            try
# endif
                            {
                                // readlinkat is symlink_nofollow
                                symlink_symbol = ::fast_io::native_readlinkat<char8_t>(at(path_stack.back_unchecked()), next_name);
                                is_symlink = true;
                            }
# ifdef UWVM_CPP_EXCEPTIONS
                            catch(::fast_io::error e)
                            {
                            }
# endif

                            if(is_symlink)
                            {
                                if(symlink_symbol.empty()) [[unlikely]] { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio; }

                                auto const errno_wasm64_t{::uwvm2::imported::wasi::wasip1::func::path_symlink_iterative(
                                    env.disable_utf8_check,
                                    curr_fd_native_file,
                                    path_stack,
                                    ::uwvm2::utils::container::u8string_view{symlink_symbol.data(), symlink_symbol.size()})};
                                if(errno_wasm64_t != ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::esuccess) [[unlikely]] { return errno_wasm64_t; }
                            }
                            else
                            {
                                ::fast_io::dir_file next{};
# ifdef UWVM_CPP_EXCEPTIONS
                                try
# endif
                                {
                                    // default is symlink_nofollow
                                    next = ::fast_io::dir_file{at(path_stack.back_unchecked()), next_name};

                                    // The `Win32 CreateFileW` function can simultaneously open both a directory and a file when specifying a
                                    // directory. However, under the current fast_io strategy, `dir_file` does not utilize `CreateFileW`.
                                    // Instead, it employs `NTCreateFile` on NT systems and `FindNextFile` on Win9x systems.

# if defined(__CYGWIN__)
                                    // Cygwin uses the Win32 CreateFile function internally to open directories, allowing both directories and files to be
                                    // opened simultaneously. A check must be added here.
                                    struct ::stat st;
                                    if(::uwvm2::imported::wasi::wasip1::func::posix::fstat(next.native_handle(), ::std::addressof(st)) < 0) [[unlikely]]
                                    {
                                        return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::eio;
                                    }
                                    if(!S_ISDIR(st.st_mode)) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
# elif defined(__MSDOS__) || defined(__DJGPP__)
                                    // djgpp's `open` function does not distinguish between directories and files; manual differentiation is
                                    // required.
                                    ::fast_io::details::check_dos_fd_is_dir(next.native_handle());
# endif
                                }
# ifdef UWVM_CPP_EXCEPTIONS
                                catch(::fast_io::error e)
                                {
                                    // Windows 9x can only distinguish between a directory and other items (files or nothing at all).

#  if defined(_WIN32) && defined(_WIN32_WINDOWS)
                                    if(e.code == 2uz /*ERROR_FILE_NOT_FOUND*/) [[unlikely]]
                                    {
                                        bool is_file{};
#   ifdef UWVM_CPP_EXCEPTIONS
                                        try
#   endif
                                        {
                                            // native_file default nofollow
                                            ::fast_io::native_file{at(path_stack.back_unchecked()), next_name, ::fast_io::open_mode::in};
                                            is_file = true;
                                        }
#   ifdef UWVM_CPP_EXCEPTIONS
                                        catch(::fast_io::error e)
                                        {
                                        }
#   endif
                                        if(is_file) { return ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t::enotdir; }
                                    }
#  endif

                                    return ::uwvm2::imported::wasi::wasip1::func::path_errno_from_fast_io_error(e);
                                }
# endif

                                path_stack.push_back(::std::move(next));
                            }
                        }

                        break;
                    }
                    [[unlikely]] default:
                    {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                        ::std::unreachable();
                    }
                }
            }
        }

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

