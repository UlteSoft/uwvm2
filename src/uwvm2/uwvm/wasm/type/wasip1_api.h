/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-12
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
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
// import
# include <uwvm2/imported/wasi/wasip1/abi/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{
    inline constexpr ::std::uint_least32_t wasip1_host_api_v1_abi_version{1u};

    /*
        Stable plugin-side WASI Preview 1 host API:
        - disabled by default;
        - enabled via command line;
        - preload modules may export `uwvm_set_wasip1_host_api_v1()` to receive the table automatically;
        - weak-symbol plugins may also call `uwvm_get_wasip1_host_api_v1()` directly and must handle `nullptr`.

        The functions below operate on UWVM's default WASI Preview 1 environment (`default_wasip1_env`),
        so their pointer / length arguments use WASI linear-memory offsets exactly like imported WebAssembly calls.
    */

    extern "C"
    {
        using uwvm_wasip1_args_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argv_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argv_buf_ptrsz);
        using uwvm_wasip1_args_sizes_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argc_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argv_buf_size_ptrsz);
        using uwvm_wasip1_clock_res_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::clockid_t clock_id, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t resolution_ptrsz);
        using uwvm_wasip1_clock_time_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::clockid_t clock_id, ::uwvm2::imported::wasi::wasip1::abi::timestamp_t precision, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t time_ptrsz);
        using uwvm_wasip1_environ_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_buf_ptrsz);
        using uwvm_wasip1_environ_sizes_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_count_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_buf_size_ptrsz);
        using uwvm_wasip1_fd_advise_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset, ::uwvm2::imported::wasi::wasip1::abi::filesize_t len, ::uwvm2::imported::wasi::wasip1::abi::advice_t advice);
        using uwvm_wasip1_fd_allocate_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset, ::uwvm2::imported::wasi::wasip1::abi::filesize_t len);
        using uwvm_wasip1_fd_close_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd);
        using uwvm_wasip1_fd_datasync_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd);
        using uwvm_wasip1_fd_fdstat_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t stat_ptrsz);
        using uwvm_wasip1_fd_fdstat_set_flags_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::fdflags_t flags);
        using uwvm_wasip1_fd_fdstat_set_rights_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_base, ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_inheriting);
        using uwvm_wasip1_fd_filestat_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t stat_ptrsz);
        using uwvm_wasip1_fd_filestat_set_size_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::filesize_t size);
        using uwvm_wasip1_fd_filestat_set_times_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::timestamp_t atim, ::uwvm2::imported::wasi::wasip1::abi::timestamp_t mtim, ::uwvm2::imported::wasi::wasip1::abi::fstflags_t fstflags);
        using uwvm_wasip1_fd_pread_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nread);
        using uwvm_wasip1_fd_prestat_dir_name_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len);
        using uwvm_wasip1_fd_prestat_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz);
        using uwvm_wasip1_fd_pwrite_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nwritten);
        using uwvm_wasip1_fd_read_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nread);
        using uwvm_wasip1_fd_readdir_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t buf_len, ::uwvm2::imported::wasi::wasip1::abi::dircookie_t cookie, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_used_ptrsz);
        using uwvm_wasip1_fd_renumber_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_from, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_to);
        using uwvm_wasip1_fd_seek_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::filedelta_t offset, ::uwvm2::imported::wasi::wasip1::abi::whence_t whence, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_offset_ptrsz);
        using uwvm_wasip1_fd_sync_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd);
        using uwvm_wasip1_fd_tell_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t tell_ptrsz);
        using uwvm_wasip1_fd_write_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nwritten);
        using uwvm_wasip1_path_create_directory_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len);
        using uwvm_wasip1_path_filestat_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz);
        using uwvm_wasip1_path_filestat_set_times_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len, ::uwvm2::imported::wasi::wasip1::abi::timestamp_t atim, ::uwvm2::imported::wasi::wasip1::abi::timestamp_t mtim, ::uwvm2::imported::wasi::wasip1::abi::fstflags_t fstflags);
        using uwvm_wasip1_path_link_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t old_fd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t old_flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t old_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t old_path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t new_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t new_path_len);
        using uwvm_wasip1_path_open_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t dirfd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t dirflags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len, ::uwvm2::imported::wasi::wasip1::abi::oflags_t oflags, ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_base, ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_inheriting, ::uwvm2::imported::wasi::wasip1::abi::fdflags_t fdflags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t fd_ptrsz);
        using uwvm_wasip1_path_readlink_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t buf_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_used_ptrsz);
        using uwvm_wasip1_path_remove_directory_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len);
        using uwvm_wasip1_path_rename_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t old_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t old_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t old_path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t new_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t new_path_len);
        using uwvm_wasip1_path_symlink_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t old_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t old_path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t new_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t new_path_len);
        using uwvm_wasip1_path_unlink_file_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len);
        using uwvm_wasip1_poll_oneoff_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t in, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t out, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t nsubscriptions, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nevents);
        using uwvm_wasip1_proc_exit_t = void (*)(::uwvm2::imported::wasi::wasip1::abi::exitcode_t code);
        using uwvm_wasip1_proc_raise_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::signal_t code);
        using uwvm_wasip1_random_get_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t buf_len);
        using uwvm_wasip1_sched_yield_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)();

#if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
        using uwvm_wasip1_sock_recv_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ri_data_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t ri_data_len, ::uwvm2::imported::wasi::wasip1::abi::riflags_t ri_flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_data_len_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_flags_ptrsz);
        using uwvm_wasip1_sock_send_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t si_data_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t si_data_len, ::uwvm2::imported::wasi::wasip1::abi::siflags_t si_flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ret_data_len_ptrsz);
        using uwvm_wasip1_sock_shutdown_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd, ::uwvm2::imported::wasi::wasip1::abi::sdflags_t how);
        using uwvm_wasip1_sock_accept_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(
            ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
            ::uwvm2::imported::wasi::wasip1::abi::fdflags_t fd_flags,
            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_fd_ptrsz
# ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
            ,
            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_addr_ptrsz
# endif
        );
#endif

#if defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
        using uwvm_wasip1_args_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argv_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argv_buf_ptrsz);
        using uwvm_wasip1_args_sizes_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argc_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argv_buf_size_ptrsz);
        using uwvm_wasip1_clock_res_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t clock_id, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t resolution_ptrsz);
        using uwvm_wasip1_clock_time_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t clock_id, ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t precision, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t time_ptrsz);
        using uwvm_wasip1_environ_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_buf_ptrsz);
        using uwvm_wasip1_environ_sizes_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_count_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_buf_size_ptrsz);
        using uwvm_wasip1_fd_advise_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset, [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t len, ::uwvm2::imported::wasi::wasip1::abi::advice_wasm64_t advice);
        using uwvm_wasip1_fd_allocate_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset, ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t len);
        using uwvm_wasip1_fd_close_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd);
        using uwvm_wasip1_fd_datasync_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd);
        using uwvm_wasip1_fd_fdstat_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t stat_ptrsz);
        using uwvm_wasip1_fd_fdstat_set_flags_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t flags);
        using uwvm_wasip1_fd_fdstat_set_rights_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_base, ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_inheriting);
        using uwvm_wasip1_fd_filestat_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t stat_ptrsz);
        using uwvm_wasip1_fd_filestat_set_size_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t size);
        using uwvm_wasip1_fd_filestat_set_times_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t atim, ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t mtim, ::uwvm2::imported::wasi::wasip1::abi::fstflags_wasm64_t fstflags);
        using uwvm_wasip1_fd_pread_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nread);
        using uwvm_wasip1_fd_prestat_dir_name_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len);
        using uwvm_wasip1_fd_prestat_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz);
        using uwvm_wasip1_fd_pwrite_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nwritten);
        using uwvm_wasip1_fd_read_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nread);
        using uwvm_wasip1_fd_readdir_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t buf_len, ::uwvm2::imported::wasi::wasip1::abi::dircookie_wasm64_t cookie, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_used_ptrsz);
        using uwvm_wasip1_fd_renumber_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd_from, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd_to);
        using uwvm_wasip1_fd_seek_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t offset, ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t whence, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_offset_ptrsz);
        using uwvm_wasip1_fd_sync_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd);
        using uwvm_wasip1_fd_tell_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t tell_ptrsz);
        using uwvm_wasip1_fd_write_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nwritten);
        using uwvm_wasip1_path_create_directory_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len);
        using uwvm_wasip1_path_filestat_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz);
        using uwvm_wasip1_path_filestat_set_times_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len, ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t atim, ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t mtim, ::uwvm2::imported::wasi::wasip1::abi::fstflags_wasm64_t fstflags);
        using uwvm_wasip1_path_link_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t old_fd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t old_flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t old_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t old_path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t new_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t new_path_len);
        using uwvm_wasip1_path_open_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t dirfd, ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t dirflags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len, ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t oflags, ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_base, ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_inheriting, ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t fdflags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t fd_ptrsz);
        using uwvm_wasip1_path_readlink_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t buf_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_used_ptrsz);
        using uwvm_wasip1_path_remove_directory_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len);
        using uwvm_wasip1_path_rename_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t old_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t old_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t old_path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t new_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t new_path_len);
        using uwvm_wasip1_path_symlink_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t old_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t old_path_len, ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t new_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t new_path_len);
        using uwvm_wasip1_path_unlink_file_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len);
        using uwvm_wasip1_poll_oneoff_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t in, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t out, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t nsubscriptions, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nevents);
        using uwvm_wasip1_proc_exit_wasm64_t = void (*)(::uwvm2::imported::wasi::wasip1::abi::exitcode_wasm64_t code);
        using uwvm_wasip1_proc_raise_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)(::uwvm2::imported::wasi::wasip1::abi::signal_t code);
        using uwvm_wasip1_random_get_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t buf_len);
        using uwvm_wasip1_sched_yield_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t (*)();
# if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
        using uwvm_wasip1_sock_recv_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ri_data_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t ri_data_len, ::uwvm2::imported::wasi::wasip1::abi::riflags_wasm64_t ri_flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_data_len_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_flags_ptrsz);
        using uwvm_wasip1_sock_send_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t si_data_ptrsz, ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t si_data_len, ::uwvm2::imported::wasi::wasip1::abi::siflags_wasm64_t si_flags, ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ret_data_len_ptrsz);
        using uwvm_wasip1_sock_shutdown_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd, ::uwvm2::imported::wasi::wasip1::abi::sdflags_wasm64_t how);
        using uwvm_wasip1_sock_accept_wasm64_t = ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t (*)(
            ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd,
            ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t fd_flags,
            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_fd_ptrsz
#  ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
            ,
            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_addr_ptrsz
#  endif
        );
# endif
#endif

        struct uwvm_wasip1_host_api_v1
        {
            ::std::size_t struct_size;
            ::std::uint_least32_t abi_version;
            uwvm_wasip1_args_get_t args_get;
            uwvm_wasip1_args_sizes_get_t args_sizes_get;
            uwvm_wasip1_clock_res_get_t clock_res_get;
            uwvm_wasip1_clock_time_get_t clock_time_get;
            uwvm_wasip1_environ_get_t environ_get;
            uwvm_wasip1_environ_sizes_get_t environ_sizes_get;
            uwvm_wasip1_fd_advise_t fd_advise;
            uwvm_wasip1_fd_allocate_t fd_allocate;
            uwvm_wasip1_fd_close_t fd_close;
            uwvm_wasip1_fd_datasync_t fd_datasync;
            uwvm_wasip1_fd_fdstat_get_t fd_fdstat_get;
            uwvm_wasip1_fd_fdstat_set_flags_t fd_fdstat_set_flags;
            uwvm_wasip1_fd_fdstat_set_rights_t fd_fdstat_set_rights;
            uwvm_wasip1_fd_filestat_get_t fd_filestat_get;
            uwvm_wasip1_fd_filestat_set_size_t fd_filestat_set_size;
            uwvm_wasip1_fd_filestat_set_times_t fd_filestat_set_times;
            uwvm_wasip1_fd_pread_t fd_pread;
            uwvm_wasip1_fd_prestat_dir_name_t fd_prestat_dir_name;
            uwvm_wasip1_fd_prestat_get_t fd_prestat_get;
            uwvm_wasip1_fd_pwrite_t fd_pwrite;
            uwvm_wasip1_fd_read_t fd_read;
            uwvm_wasip1_fd_readdir_t fd_readdir;
            uwvm_wasip1_fd_renumber_t fd_renumber;
            uwvm_wasip1_fd_seek_t fd_seek;
            uwvm_wasip1_fd_sync_t fd_sync;
            uwvm_wasip1_fd_tell_t fd_tell;
            uwvm_wasip1_fd_write_t fd_write;
            uwvm_wasip1_path_create_directory_t path_create_directory;
            uwvm_wasip1_path_filestat_get_t path_filestat_get;
            uwvm_wasip1_path_filestat_set_times_t path_filestat_set_times;
            uwvm_wasip1_path_link_t path_link;
            uwvm_wasip1_path_open_t path_open;
            uwvm_wasip1_path_readlink_t path_readlink;
            uwvm_wasip1_path_remove_directory_t path_remove_directory;
            uwvm_wasip1_path_rename_t path_rename;
            uwvm_wasip1_path_symlink_t path_symlink;
            uwvm_wasip1_path_unlink_file_t path_unlink_file;
            uwvm_wasip1_poll_oneoff_t poll_oneoff;
            uwvm_wasip1_proc_exit_t proc_exit;
            uwvm_wasip1_proc_raise_t proc_raise;
            uwvm_wasip1_random_get_t random_get;
            uwvm_wasip1_sched_yield_t sched_yield;
#if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            uwvm_wasip1_sock_accept_t sock_accept;
            uwvm_wasip1_sock_recv_t sock_recv;
            uwvm_wasip1_sock_send_t sock_send;
            uwvm_wasip1_sock_shutdown_t sock_shutdown;
#endif
#if defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
            uwvm_wasip1_args_get_wasm64_t args_get_wasm64;
            uwvm_wasip1_args_sizes_get_wasm64_t args_sizes_get_wasm64;
            uwvm_wasip1_clock_res_get_wasm64_t clock_res_get_wasm64;
            uwvm_wasip1_clock_time_get_wasm64_t clock_time_get_wasm64;
            uwvm_wasip1_environ_get_wasm64_t environ_get_wasm64;
            uwvm_wasip1_environ_sizes_get_wasm64_t environ_sizes_get_wasm64;
            uwvm_wasip1_fd_advise_wasm64_t fd_advise_wasm64;
            uwvm_wasip1_fd_allocate_wasm64_t fd_allocate_wasm64;
            uwvm_wasip1_fd_close_wasm64_t fd_close_wasm64;
            uwvm_wasip1_fd_datasync_wasm64_t fd_datasync_wasm64;
            uwvm_wasip1_fd_fdstat_get_wasm64_t fd_fdstat_get_wasm64;
            uwvm_wasip1_fd_fdstat_set_flags_wasm64_t fd_fdstat_set_flags_wasm64;
            uwvm_wasip1_fd_fdstat_set_rights_wasm64_t fd_fdstat_set_rights_wasm64;
            uwvm_wasip1_fd_filestat_get_wasm64_t fd_filestat_get_wasm64;
            uwvm_wasip1_fd_filestat_set_size_wasm64_t fd_filestat_set_size_wasm64;
            uwvm_wasip1_fd_filestat_set_times_wasm64_t fd_filestat_set_times_wasm64;
            uwvm_wasip1_fd_pread_wasm64_t fd_pread_wasm64;
            uwvm_wasip1_fd_prestat_dir_name_wasm64_t fd_prestat_dir_name_wasm64;
            uwvm_wasip1_fd_prestat_get_wasm64_t fd_prestat_get_wasm64;
            uwvm_wasip1_fd_pwrite_wasm64_t fd_pwrite_wasm64;
            uwvm_wasip1_fd_read_wasm64_t fd_read_wasm64;
            uwvm_wasip1_fd_readdir_wasm64_t fd_readdir_wasm64;
            uwvm_wasip1_fd_renumber_wasm64_t fd_renumber_wasm64;
            uwvm_wasip1_fd_seek_wasm64_t fd_seek_wasm64;
            uwvm_wasip1_fd_sync_wasm64_t fd_sync_wasm64;
            uwvm_wasip1_fd_tell_wasm64_t fd_tell_wasm64;
            uwvm_wasip1_fd_write_wasm64_t fd_write_wasm64;
            uwvm_wasip1_path_create_directory_wasm64_t path_create_directory_wasm64;
            uwvm_wasip1_path_filestat_get_wasm64_t path_filestat_get_wasm64;
            uwvm_wasip1_path_filestat_set_times_wasm64_t path_filestat_set_times_wasm64;
            uwvm_wasip1_path_link_wasm64_t path_link_wasm64;
            uwvm_wasip1_path_open_wasm64_t path_open_wasm64;
            uwvm_wasip1_path_readlink_wasm64_t path_readlink_wasm64;
            uwvm_wasip1_path_remove_directory_wasm64_t path_remove_directory_wasm64;
            uwvm_wasip1_path_rename_wasm64_t path_rename_wasm64;
            uwvm_wasip1_path_symlink_wasm64_t path_symlink_wasm64;
            uwvm_wasip1_path_unlink_file_wasm64_t path_unlink_file_wasm64;
            uwvm_wasip1_poll_oneoff_wasm64_t poll_oneoff_wasm64;
            uwvm_wasip1_proc_exit_wasm64_t proc_exit_wasm64;
            uwvm_wasip1_proc_raise_wasm64_t proc_raise_wasm64;
            uwvm_wasip1_random_get_wasm64_t random_get_wasm64;
            uwvm_wasip1_sched_yield_wasm64_t sched_yield_wasm64;
# if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            uwvm_wasip1_sock_accept_wasm64_t sock_accept_wasm64;
            uwvm_wasip1_sock_recv_wasm64_t sock_recv_wasm64;
            uwvm_wasip1_sock_send_wasm64_t sock_send_wasm64;
            uwvm_wasip1_sock_shutdown_wasm64_t sock_shutdown_wasm64;
# endif
#endif
        };

        using uwvm_set_wasip1_host_api_v1_t = void (*)(uwvm_wasip1_host_api_v1 const*);

        uwvm_wasip1_host_api_v1 const* uwvm_get_wasip1_host_api_v1() noexcept;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
