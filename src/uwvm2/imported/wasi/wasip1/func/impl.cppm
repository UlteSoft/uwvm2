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

module;

export module uwvm2.imported.wasi.wasip1.func;

export import :base;
export import :posix;
export import :args_get_wasm64;
export import :args_get;
export import :args_sizes_get_wasm64;
export import :args_sizes_get;
export import :clock_res_get_wasm64;
export import :clock_res_get;
export import :clock_time_get_wasm64;
export import :clock_time_get;
export import :environ_get_wasm64;
export import :environ_get;
export import :environ_sizes_get_wasm64;
export import :environ_sizes_get;
export import :fd_advise_wasm64;
export import :fd_advise;
export import :fd_allocate_wasm64;
export import :fd_allocate;
export import :fd_close_wasm64;
export import :fd_close;
export import :fd_datasync_wasm64;
export import :fd_datasync;
export import :fd_fdstat_get_wasm64;
export import :fd_fdstat_get;
export import :fd_fdstat_set_flags_wasm64;
export import :fd_fdstat_set_flags;
export import :fd_filestat_get_wasm64;
export import :fd_filestat_get;
export import :fd_filestat_set_size_wasm64;
export import :fd_filestat_set_size;
export import :fd_filestat_set_times_wasm64;
export import :fd_filestat_set_times;
export import :fd_prestat_dir_name_wasm64;
export import :fd_prestat_dir_name;
export import :fd_prestat_get_wasm64;
export import :fd_prestat_get;
export import :fd_pwrite_wasm64;
export import :fd_pwrite;
export import :fd_read_wasm64;
export import :fd_read;
export import :fd_readdir_wasm64;
export import :fd_readdir;
export import :fd_renumber_wasm64;
export import :fd_renumber;
export import :fd_seek_wasm64;
export import :fd_seek;
export import :fd_sync_wasm64;
export import :fd_sync;
export import :fd_tell_wasm64;
export import :fd_tell;
export import :fd_write_wasm64;
export import :fd_write;
export import :path_create_directory_wasm64;
export import :path_create_directory;
export import :path_filestat_get_wasm64;
export import :path_filestat_get;
export import :path_filestat_set_times_wasm64;
export import :path_filestat_set_times;
export import :path_link_wasm64;
export import :path_link;
export import :path_open_wasm64;
export import :path_open;
export import :path_readlink_wasm64;
export import :path_readlink;
export import :path_remove_directory_wasm64;
export import :path_remove_directory;
export import :path_rename_wasm64;
export import :path_rename;
export import :path_symlink_wasm64;
export import :path_symlink;
export import :path_unlink_file_wasm64;
export import :path_unlink_file;

#ifndef UWVM_MODULE
# define UWVM_MODULE
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT export
#endif

#include "impl.h"
