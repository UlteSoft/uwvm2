/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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

// std
#include <cstddef>
#include <cstdint>
#include <memory>
// macro
#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>

#ifndef UWVM_MODULE
# include <uwvm2/imported/wasi/wasip1/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
#endif

// wasip1 host api wrappers
#if defined(UWVM_IMPORT_WASI_WASIP1)
extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_args_get(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argv_ptrsz,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argv_buf_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::args_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, argv_ptrsz, argv_buf_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t
    uwvm_wasip1_args_sizes_get(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argc_ptrsz,
                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t argv_buf_size_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::args_sizes_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                 argc_ptrsz,
                                                                 argv_buf_size_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t
    uwvm_wasip1_clock_res_get(::uwvm2::imported::wasi::wasip1::abi::clockid_t clock_id,
                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t resolution_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::clock_res_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, clock_id, resolution_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_clock_time_get(::uwvm2::imported::wasi::wasip1::abi::clockid_t clock_id,
                                                                                    ::uwvm2::imported::wasi::wasip1::abi::timestamp_t precision,
                                                                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t time_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::clock_time_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                 clock_id,
                                                                 precision,
                                                                 time_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t
    uwvm_wasip1_environ_get(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_ptrsz,
                            ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_buf_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::environ_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                              environ_ptrsz,
                                                              environ_buf_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t
    uwvm_wasip1_environ_sizes_get(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_count_ptrsz,
                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t environ_buf_size_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::environ_sizes_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                    environ_count_ptrsz,
                                                                    environ_buf_size_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_advise(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::filesize_t len,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::advice_t advice) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_advise(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, offset, len, advice); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_allocate(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::filesize_t len) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_allocate(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, offset, len); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_close(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_close(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_datasync(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_datasync(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_fdstat_get(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t stat_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, stat_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_fdstat_set_flags(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                         ::uwvm2::imported::wasi::wasip1::abi::fdflags_t flags) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_flags(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, flags); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t
    uwvm_wasip1_fd_fdstat_set_rights(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                     ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_base,
                                     ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_inheriting) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                       fd,
                                                                       fs_rights_base,
                                                                       fs_rights_inheriting);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_filestat_get(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t stat_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, stat_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_filestat_set_size(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                          ::uwvm2::imported::wasi::wasip1::abi::filesize_t size) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, size); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_filestat_set_times(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                           ::uwvm2::imported::wasi::wasip1::abi::timestamp_t atim,
                                                                                           ::uwvm2::imported::wasi::wasip1::abi::timestamp_t mtim,
                                                                                           ::uwvm2::imported::wasi::wasip1::abi::fstflags_t fstflags) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                        fd,
                                                                        atim,
                                                                        mtim,
                                                                        fstflags);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_pread(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nread) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_pread(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                           fd,
                                                           iovs,
                                                           iovs_len,
                                                           offset,
                                                           nread);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_prestat_dir_name(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path,
                                                                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, path, path_len); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_prestat_get(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_prestat_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, buf_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_pwrite(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::filesize_t offset,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nwritten) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_pwrite(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                            fd,
                                                            iovs,
                                                            iovs_len,
                                                            offset,
                                                            nwritten);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_read(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs,
                                                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len,
                                                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nread) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_read(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, iovs, iovs_len, nread); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_readdir(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t buf_len,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::dircookie_t cookie,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_used_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_readdir(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                             fd,
                                                             buf_ptrsz,
                                                             buf_len,
                                                             cookie,
                                                             buf_used_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_renumber(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_from,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd_to) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_renumber(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd_from, fd_to); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_seek(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                             ::uwvm2::imported::wasi::wasip1::abi::filedelta_t offset,
                                                                             ::uwvm2::imported::wasi::wasip1::abi::whence_t whence,
                                                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_offset_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_seek(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                          fd,
                                                          offset,
                                                          whence,
                                                          new_offset_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_sync(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_sync(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_tell(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t tell_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_tell(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, tell_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_fd_write(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t iovs,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t iovs_len,
                                                                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nwritten) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_write(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, iovs, iovs_len, nwritten); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_create_directory(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz,
                                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_create_directory(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                        fd,
                                                                        path_ptrsz,
                                                                        path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_filestat_get(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                       ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t flags,
                                                                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz,
                                                                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len,
                                                                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_filestat_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                    fd,
                                                                    flags,
                                                                    path_ptrsz,
                                                                    path_len,
                                                                    buf_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_filestat_set_times(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                             ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t flags,
                                                                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz,
                                                                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len,
                                                                                             ::uwvm2::imported::wasi::wasip1::abi::timestamp_t atim,
                                                                                             ::uwvm2::imported::wasi::wasip1::abi::timestamp_t mtim,
                                                                                             ::uwvm2::imported::wasi::wasip1::abi::fstflags_t fstflags) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_filestat_set_times(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                          fd,
                                                                          flags,
                                                                          path_ptrsz,
                                                                          path_len,
                                                                          atim,
                                                                          mtim,
                                                                          fstflags);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_link(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t old_fd,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t old_flags,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t old_path_ptrsz,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t old_path_len,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t new_fd,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_path_ptrsz,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t new_path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_link(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                            old_fd,
                                                            old_flags,
                                                            old_path_ptrsz,
                                                            old_path_len,
                                                            new_fd,
                                                            new_path_ptrsz,
                                                            new_path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_open(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t dirfd,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::lookupflags_t dirflags,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::oflags_t oflags,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_base,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::rights_t fs_rights_inheriting,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::fdflags_t fdflags,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t fd_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_open(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                            dirfd,
                                                            dirflags,
                                                            path_ptrsz,
                                                            path_len,
                                                            oflags,
                                                            fs_rights_base,
                                                            fs_rights_inheriting,
                                                            fdflags,
                                                            fd_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t
    uwvm_wasip1_path_readlink(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz,
                              ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len,
                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_ptrsz,
                              ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t buf_len,
                              ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf_used_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_readlink(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                fd,
                                                                path_ptrsz,
                                                                path_len,
                                                                buf_ptrsz,
                                                                buf_len,
                                                                buf_used_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_remove_directory(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz,
                                                                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_remove_directory(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                        fd,
                                                                        path_ptrsz,
                                                                        path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_rename(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t old_fd,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t old_path_ptrsz,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t old_path_len,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t new_fd,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_path_ptrsz,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t new_path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_rename(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                              old_fd,
                                                              old_path_ptrsz,
                                                              old_path_len,
                                                              new_fd,
                                                              new_path_ptrsz,
                                                              new_path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_symlink(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t old_path_ptrsz,
                                                                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t old_path_len,
                                                                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t new_fd,
                                                                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t new_path_ptrsz,
                                                                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t new_path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_symlink(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                               old_path_ptrsz,
                                                               old_path_len,
                                                               new_fd,
                                                               new_path_ptrsz,
                                                               new_path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_path_unlink_file(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t fd,
                                                                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t path_ptrsz,
                                                                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_unlink_file(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                   fd,
                                                                   path_ptrsz,
                                                                   path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_poll_oneoff(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t in,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t out,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t nsubscriptions,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t nevents) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::poll_oneoff(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                              in,
                                                              out,
                                                              nsubscriptions,
                                                              nevents);
}

extern "C" void uwvm_wasip1_proc_exit(::uwvm2::imported::wasi::wasip1::abi::exitcode_t code) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::proc_exit(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, code); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_proc_raise(::uwvm2::imported::wasi::wasip1::abi::signal_t code) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::proc_raise(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, code); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_random_get(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t buf,
                                                                                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t buf_len) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::random_get(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, buf, buf_len); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_sched_yield() noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::sched_yield(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env); }

# if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_sock_accept(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::fdflags_t fd_flags,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_fd_ptrsz
#  ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
                                                                                 ,
                                                                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_addr_ptrsz
#  endif
                                                                                 ) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::sock_accept(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                              sock_fd,
                                                              fd_flags,
                                                              ro_fd_ptrsz
#  ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
                                                              ,
                                                              ro_addr_ptrsz
#  endif
    );
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_sock_recv(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ri_data_ptrsz,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t ri_data_len,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::riflags_t ri_flags,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_data_len_ptrsz,
                                                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ro_flags_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::sock_recv(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                            sock_fd,
                                                            ri_data_ptrsz,
                                                            ri_data_len,
                                                            ri_flags,
                                                            ro_data_len_ptrsz,
                                                            ro_flags_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t
    uwvm_wasip1_sock_send(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
                          ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t si_data_ptrsz,
                          ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t si_data_len,
                          ::uwvm2::imported::wasi::wasip1::abi::siflags_t si_flags,
                          ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t ret_data_len_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::sock_send(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                            sock_fd,
                                                            si_data_ptrsz,
                                                            si_data_len,
                                                            si_flags,
                                                            ret_data_len_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_sock_shutdown(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t sock_fd,
                                                                                   ::uwvm2::imported::wasi::wasip1::abi::sdflags_t how) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::sock_shutdown(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, sock_fd, how); }

# endif
# if defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_args_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argv_ptrsz,
                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argv_buf_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::args_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                  argv_ptrsz,
                                                                  argv_buf_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_args_sizes_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argc_ptrsz,
                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t argv_buf_size_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::args_sizes_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                        argc_ptrsz,
                                                                        argv_buf_size_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_clock_res_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t clock_id,
                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t resolution_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::clock_res_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                       clock_id,
                                                                       resolution_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_clock_time_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::clockid_wasm64_t clock_id,
                                      ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t precision,
                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t time_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::clock_time_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                        clock_id,
                                                                        precision,
                                                                        time_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_environ_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_ptrsz,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_buf_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::environ_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                     environ_ptrsz,
                                                                     environ_buf_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_environ_sizes_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_count_ptrsz,
                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t environ_buf_size_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::environ_sizes_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                           environ_count_ptrsz,
                                                                           environ_buf_size_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_advise_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                 [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset,
                                 [[maybe_unused]] ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t len,
                                 ::uwvm2::imported::wasi::wasip1::abi::advice_wasm64_t advice) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, offset, len, advice); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_allocate_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                   ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset,
                                   ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t len) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_allocate_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, offset, len); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_close_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_datasync_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_datasync_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_fdstat_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t stat_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, stat_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_fdstat_set_flags_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                           ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t flags) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_flags_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, flags); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_fdstat_set_rights_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                            ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_base,
                                            ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_inheriting) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                              fd,
                                                                              fs_rights_base,
                                                                              fs_rights_inheriting);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_filestat_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                       ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t stat_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, stat_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_filestat_set_size_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                            ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t size) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_size_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, size); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_filestat_set_times_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                             ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t atim,
                                             ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t mtim,
                                             ::uwvm2::imported::wasi::wasip1::abi::fstflags_wasm64_t fstflags) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                               fd,
                                                                               atim,
                                                                               mtim,
                                                                               fstflags);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_pread_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs,
                                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len,
                                ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset,
                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nread) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                  fd,
                                                                  iovs,
                                                                  iovs_len,
                                                                  offset,
                                                                  nread);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_prestat_dir_name_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path,
                                           ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                             fd,
                                                                             path,
                                                                             path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_prestat_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                      ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_prestat_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, buf_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_pwrite_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len,
                                 ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t offset,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nwritten) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_pwrite_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                   fd,
                                                                   iovs,
                                                                   iovs_len,
                                                                   offset,
                                                                   nwritten);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_read_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs,
                               ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len,
                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nread) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, iovs, iovs_len, nread); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_readdir_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz,
                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t buf_len,
                                  ::uwvm2::imported::wasi::wasip1::abi::dircookie_wasm64_t cookie,
                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_used_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_readdir_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                    fd,
                                                                    buf_ptrsz,
                                                                    buf_len,
                                                                    cookie,
                                                                    buf_used_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_renumber_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd_from,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd_to) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd_from, fd_to); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_seek_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                               ::uwvm2::imported::wasi::wasip1::abi::filedelta_wasm64_t offset,
                               ::uwvm2::imported::wasi::wasip1::abi::whence_wasm64_t whence,
                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_offset_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_seek_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                 fd,
                                                                 offset,
                                                                 whence,
                                                                 new_offset_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_sync_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_sync_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_tell_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t tell_ptrsz) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::fd_tell_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, fd, tell_ptrsz); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_fd_write_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t iovs,
                                ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t iovs_len,
                                ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nwritten) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::fd_write_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                  fd,
                                                                  iovs,
                                                                  iovs_len,
                                                                  nwritten);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_create_directory_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_create_directory_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                               fd,
                                                                               path_ptrsz,
                                                                               path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_filestat_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                         ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t flags,
                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len,
                                         ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_filestat_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                           fd,
                                                                           flags,
                                                                           path_ptrsz,
                                                                           path_len,
                                                                           buf_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_filestat_set_times_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                               ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t flags,
                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
                                               ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len,
                                               ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t atim,
                                               ::uwvm2::imported::wasi::wasip1::abi::timestamp_wasm64_t mtim,
                                               ::uwvm2::imported::wasi::wasip1::abi::fstflags_wasm64_t fstflags) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_filestat_set_times_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                                 fd,
                                                                                 flags,
                                                                                 path_ptrsz,
                                                                                 path_len,
                                                                                 atim,
                                                                                 mtim,
                                                                                 fstflags);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_link_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t old_fd,
                                 ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t old_flags,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t old_path_ptrsz,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t old_path_len,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t new_fd,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_path_ptrsz,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t new_path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_link_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                   old_fd,
                                                                   old_flags,
                                                                   old_path_ptrsz,
                                                                   old_path_len,
                                                                   new_fd,
                                                                   new_path_ptrsz,
                                                                   new_path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_open_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t dirfd,
                                 ::uwvm2::imported::wasi::wasip1::abi::lookupflags_wasm64_t dirflags,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len,
                                 ::uwvm2::imported::wasi::wasip1::abi::oflags_wasm64_t oflags,
                                 ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_base,
                                 ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t fs_rights_inheriting,
                                 ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t fdflags,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t fd_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_open_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                   dirfd,
                                                                   dirflags,
                                                                   path_ptrsz,
                                                                   path_len,
                                                                   oflags,
                                                                   fs_rights_base,
                                                                   fs_rights_inheriting,
                                                                   fdflags,
                                                                   fd_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_readlink_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len,
                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_ptrsz,
                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t buf_len,
                                     ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf_used_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_readlink_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                       fd,
                                                                       path_ptrsz,
                                                                       path_len,
                                                                       buf_ptrsz,
                                                                       buf_len,
                                                                       buf_used_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_remove_directory_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
                                             ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_remove_directory_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                               fd,
                                                                               path_ptrsz,
                                                                               path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_rename_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t old_fd,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t old_path_ptrsz,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t old_path_len,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t new_fd,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_path_ptrsz,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t new_path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_rename_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                     old_fd,
                                                                     old_path_ptrsz,
                                                                     old_path_len,
                                                                     new_fd,
                                                                     new_path_ptrsz,
                                                                     new_path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_symlink_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t old_path_ptrsz,
                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t old_path_len,
                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t new_fd,
                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t new_path_ptrsz,
                                    ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t new_path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_symlink_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                      old_path_ptrsz,
                                                                      old_path_len,
                                                                      new_fd,
                                                                      new_path_ptrsz,
                                                                      new_path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_path_unlink_file_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t fd,
                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t path_ptrsz,
                                        ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t path_len) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::path_unlink_file_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                          fd,
                                                                          path_ptrsz,
                                                                          path_len);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_poll_oneoff_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t in,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t out,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t nsubscriptions,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t nevents) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::poll_oneoff_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                     in,
                                                                     out,
                                                                     nsubscriptions,
                                                                     nevents);
}

extern "C" void uwvm_wasip1_proc_exit_wasm64(::uwvm2::imported::wasi::wasip1::abi::exitcode_wasm64_t code) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::proc_exit_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, code); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_proc_raise_wasm64(::uwvm2::imported::wasi::wasip1::abi::signal_t code) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::proc_raise_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, code); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_random_get_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t buf,
                                  ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t buf_len) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::random_get_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, buf, buf_len); }

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_t uwvm_wasip1_sched_yield_wasm64() noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::sched_yield_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env); }

#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_sock_accept_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd,
                                   ::uwvm2::imported::wasi::wasip1::abi::fdflags_wasm64_t fd_flags,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_fd_ptrsz
#   ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
                                   ,
                                   ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_addr_ptrsz
#   endif
                                   ) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::sock_accept_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                     sock_fd,
                                                                     fd_flags,
                                                                     ro_fd_ptrsz
#   ifdef UWVM_IMPORT_WASI_WASIP1_SUPPORT_WASIX_SOCKET
                                                                     ,
                                                                     ro_addr_ptrsz
#   endif
    );
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_sock_recv_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ri_data_ptrsz,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t ri_data_len,
                                 ::uwvm2::imported::wasi::wasip1::abi::riflags_wasm64_t ri_flags,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_data_len_ptrsz,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ro_flags_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::sock_recv_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                   sock_fd,
                                                                   ri_data_ptrsz,
                                                                   ri_data_len,
                                                                   ri_flags,
                                                                   ro_data_len_ptrsz,
                                                                   ro_flags_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_sock_send_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t si_data_ptrsz,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t si_data_len,
                                 ::uwvm2::imported::wasi::wasip1::abi::siflags_wasm64_t si_flags,
                                 ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t ret_data_len_ptrsz) noexcept
{
    return ::uwvm2::imported::wasi::wasip1::func::sock_send_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                                   sock_fd,
                                                                   si_data_ptrsz,
                                                                   si_data_len,
                                                                   si_flags,
                                                                   ret_data_len_ptrsz);
}

extern "C" ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t
    uwvm_wasip1_sock_shutdown_wasm64(::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t sock_fd,
                                     ::uwvm2::imported::wasi::wasip1::abi::sdflags_wasm64_t how) noexcept
{ return ::uwvm2::imported::wasi::wasip1::func::sock_shutdown_wasm64(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env, sock_fd, how); }

#  endif
# endif
#endif

namespace uwvm2::uwvm::wasm::type
{
    extern "C" uwvm_wasip1_host_api_v1 const* uwvm_get_wasip1_host_api_v1() noexcept
    {
#if defined(UWVM_IMPORT_WASI_WASIP1)
        if(!::uwvm2::uwvm::wasm::storage::preload_expose_wasip1_host_api) [[unlikely]] { return nullptr; }

        static uwvm_wasip1_host_api_v1 const wasip1_host_api_v1{
            .struct_size = sizeof(uwvm_wasip1_host_api_v1),
            .abi_version = wasip1_host_api_v1_abi_version,
            .args_get = uwvm_wasip1_args_get,
            .args_sizes_get = uwvm_wasip1_args_sizes_get,
            .clock_res_get = uwvm_wasip1_clock_res_get,
            .clock_time_get = uwvm_wasip1_clock_time_get,
            .environ_get = uwvm_wasip1_environ_get,
            .environ_sizes_get = uwvm_wasip1_environ_sizes_get,
            .fd_advise = uwvm_wasip1_fd_advise,
            .fd_allocate = uwvm_wasip1_fd_allocate,
            .fd_close = uwvm_wasip1_fd_close,
            .fd_datasync = uwvm_wasip1_fd_datasync,
            .fd_fdstat_get = uwvm_wasip1_fd_fdstat_get,
            .fd_fdstat_set_flags = uwvm_wasip1_fd_fdstat_set_flags,
            .fd_fdstat_set_rights = uwvm_wasip1_fd_fdstat_set_rights,
            .fd_filestat_get = uwvm_wasip1_fd_filestat_get,
            .fd_filestat_set_size = uwvm_wasip1_fd_filestat_set_size,
            .fd_filestat_set_times = uwvm_wasip1_fd_filestat_set_times,
            .fd_pread = uwvm_wasip1_fd_pread,
            .fd_prestat_dir_name = uwvm_wasip1_fd_prestat_dir_name,
            .fd_prestat_get = uwvm_wasip1_fd_prestat_get,
            .fd_pwrite = uwvm_wasip1_fd_pwrite,
            .fd_read = uwvm_wasip1_fd_read,
            .fd_readdir = uwvm_wasip1_fd_readdir,
            .fd_renumber = uwvm_wasip1_fd_renumber,
            .fd_seek = uwvm_wasip1_fd_seek,
            .fd_sync = uwvm_wasip1_fd_sync,
            .fd_tell = uwvm_wasip1_fd_tell,
            .fd_write = uwvm_wasip1_fd_write,
            .path_create_directory = uwvm_wasip1_path_create_directory,
            .path_filestat_get = uwvm_wasip1_path_filestat_get,
            .path_filestat_set_times = uwvm_wasip1_path_filestat_set_times,
            .path_link = uwvm_wasip1_path_link,
            .path_open = uwvm_wasip1_path_open,
            .path_readlink = uwvm_wasip1_path_readlink,
            .path_remove_directory = uwvm_wasip1_path_remove_directory,
            .path_rename = uwvm_wasip1_path_rename,
            .path_symlink = uwvm_wasip1_path_symlink,
            .path_unlink_file = uwvm_wasip1_path_unlink_file,
            .poll_oneoff = uwvm_wasip1_poll_oneoff,
            .proc_exit = uwvm_wasip1_proc_exit,
            .proc_raise = uwvm_wasip1_proc_raise,
            .random_get = uwvm_wasip1_random_get,
            .sched_yield = uwvm_wasip1_sched_yield,
# if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            .sock_accept = uwvm_wasip1_sock_accept,
            .sock_recv = uwvm_wasip1_sock_recv,
            .sock_send = uwvm_wasip1_sock_send,
            .sock_shutdown = uwvm_wasip1_sock_shutdown,
# endif
# if defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
            .args_get_wasm64 = uwvm_wasip1_args_get_wasm64,
            .args_sizes_get_wasm64 = uwvm_wasip1_args_sizes_get_wasm64,
            .clock_res_get_wasm64 = uwvm_wasip1_clock_res_get_wasm64,
            .clock_time_get_wasm64 = uwvm_wasip1_clock_time_get_wasm64,
            .environ_get_wasm64 = uwvm_wasip1_environ_get_wasm64,
            .environ_sizes_get_wasm64 = uwvm_wasip1_environ_sizes_get_wasm64,
            .fd_advise_wasm64 = uwvm_wasip1_fd_advise_wasm64,
            .fd_allocate_wasm64 = uwvm_wasip1_fd_allocate_wasm64,
            .fd_close_wasm64 = uwvm_wasip1_fd_close_wasm64,
            .fd_datasync_wasm64 = uwvm_wasip1_fd_datasync_wasm64,
            .fd_fdstat_get_wasm64 = uwvm_wasip1_fd_fdstat_get_wasm64,
            .fd_fdstat_set_flags_wasm64 = uwvm_wasip1_fd_fdstat_set_flags_wasm64,
            .fd_fdstat_set_rights_wasm64 = uwvm_wasip1_fd_fdstat_set_rights_wasm64,
            .fd_filestat_get_wasm64 = uwvm_wasip1_fd_filestat_get_wasm64,
            .fd_filestat_set_size_wasm64 = uwvm_wasip1_fd_filestat_set_size_wasm64,
            .fd_filestat_set_times_wasm64 = uwvm_wasip1_fd_filestat_set_times_wasm64,
            .fd_pread_wasm64 = uwvm_wasip1_fd_pread_wasm64,
            .fd_prestat_dir_name_wasm64 = uwvm_wasip1_fd_prestat_dir_name_wasm64,
            .fd_prestat_get_wasm64 = uwvm_wasip1_fd_prestat_get_wasm64,
            .fd_pwrite_wasm64 = uwvm_wasip1_fd_pwrite_wasm64,
            .fd_read_wasm64 = uwvm_wasip1_fd_read_wasm64,
            .fd_readdir_wasm64 = uwvm_wasip1_fd_readdir_wasm64,
            .fd_renumber_wasm64 = uwvm_wasip1_fd_renumber_wasm64,
            .fd_seek_wasm64 = uwvm_wasip1_fd_seek_wasm64,
            .fd_sync_wasm64 = uwvm_wasip1_fd_sync_wasm64,
            .fd_tell_wasm64 = uwvm_wasip1_fd_tell_wasm64,
            .fd_write_wasm64 = uwvm_wasip1_fd_write_wasm64,
            .path_create_directory_wasm64 = uwvm_wasip1_path_create_directory_wasm64,
            .path_filestat_get_wasm64 = uwvm_wasip1_path_filestat_get_wasm64,
            .path_filestat_set_times_wasm64 = uwvm_wasip1_path_filestat_set_times_wasm64,
            .path_link_wasm64 = uwvm_wasip1_path_link_wasm64,
            .path_open_wasm64 = uwvm_wasip1_path_open_wasm64,
            .path_readlink_wasm64 = uwvm_wasip1_path_readlink_wasm64,
            .path_remove_directory_wasm64 = uwvm_wasip1_path_remove_directory_wasm64,
            .path_rename_wasm64 = uwvm_wasip1_path_rename_wasm64,
            .path_symlink_wasm64 = uwvm_wasip1_path_symlink_wasm64,
            .path_unlink_file_wasm64 = uwvm_wasip1_path_unlink_file_wasm64,
            .poll_oneoff_wasm64 = uwvm_wasip1_poll_oneoff_wasm64,
            .proc_exit_wasm64 = uwvm_wasip1_proc_exit_wasm64,
            .proc_raise_wasm64 = uwvm_wasip1_proc_raise_wasm64,
            .random_get_wasm64 = uwvm_wasip1_random_get_wasm64,
            .sched_yield_wasm64 = uwvm_wasip1_sched_yield_wasm64,
#  if defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)
            .sock_accept_wasm64 = uwvm_wasip1_sock_accept_wasm64,
            .sock_recv_wasm64 = uwvm_wasip1_sock_recv_wasm64,
            .sock_send_wasm64 = uwvm_wasip1_sock_send_wasm64,
            .sock_shutdown_wasm64 = uwvm_wasip1_sock_shutdown_wasm64,
#  endif
# endif
        };

        return ::std::addressof(wasip1_host_api_v1);
#else
        return nullptr;
#endif
    }
}  // namespace uwvm2::uwvm::wasm::type

namespace uwvm2::uwvm::wasm::type
{
    extern "C" ::std::size_t uwvm_preload_memory_descriptor_count() noexcept { return ::uwvm2::runtime::lib::preload_memory_descriptor_count_host_api(); }

    extern "C" bool uwvm_preload_memory_descriptor_at(::std::size_t descriptor_index, uwvm_preload_memory_descriptor_t* out) noexcept
    { return ::uwvm2::runtime::lib::preload_memory_descriptor_at_host_api(descriptor_index, out); }

    extern "C" bool uwvm_preload_memory_read(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept
    { return ::uwvm2::runtime::lib::preload_memory_read_host_api(memory_index, offset, destination, size); }

    extern "C" bool uwvm_preload_memory_write(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept
    { return ::uwvm2::runtime::lib::preload_memory_write_host_api(memory_index, offset, source, size); }

    extern "C" uwvm_preload_host_api_v1 const* uwvm_get_preload_host_api_v1() noexcept
    {
        static uwvm_preload_host_api_v1 const preload_host_api_v1{
            .struct_size = sizeof(uwvm_preload_host_api_v1),
            .abi_version = preload_host_api_v1_abi_version,
            .memory_descriptor_count = uwvm_preload_memory_descriptor_count,
            .memory_descriptor_at = uwvm_preload_memory_descriptor_at,
            .memory_read = uwvm_preload_memory_read,
            .memory_write = uwvm_preload_memory_write,
        };

        return ::std::addressof(preload_host_api_v1);
    }
}  // namespace uwvm2::uwvm::wasm::type

#include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
