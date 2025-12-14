
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
# include "base.h"
# include "posix.h"
# include "args_get_wasm64.h"
# include "args_get.h"
# include "args_sizes_get_wasm64.h"
# include "args_sizes_get.h"
# include "clock_res_get_wasm64.h"
# include "clock_res_get.h"
# include "clock_time_get_wasm64.h"
# include "clock_time_get.h"
# include "environ_get_wasm64.h"
# include "environ_get.h"
# include "environ_sizes_get_wasm64.h"
# include "environ_sizes_get.h"
# include "fd_advise_wasm64.h"
# include "fd_advise.h"
# include "fd_allocate_wasm64.h"
# include "fd_allocate.h"
# include "fd_close_wasm64.h"
# include "fd_close.h"
# include "fd_datasync_wasm64.h"
# include "fd_datasync.h"
# include "fd_fdstat_get_wasm64.h"
# include "fd_fdstat_get.h"
# include "fd_fdstat_set_flags_wasm64.h"
# include "fd_fdstat_set_flags.h"
# include "fd_fdstat_set_rights_wasm64.h"
# include "fd_fdstat_set_rights.h"
# include "fd_filestat_get_wasm64.h"
# include "fd_filestat_get.h"
# include "fd_filestat_set_size_wasm64.h"
# include "fd_filestat_set_size.h"
# include "fd_filestat_set_times_wasm64.h"
# include "fd_filestat_set_times.h"
# include "fd_prestat_dir_name_wasm64.h"
# include "fd_prestat_dir_name.h"
# include "fd_prestat_get_wasm64.h"
# include "fd_prestat_get.h"
# include "fd_pread_wasm64.h"
# include "fd_pread.h"
# include "fd_pwrite_wasm64.h"
# include "fd_pwrite.h"
# include "fd_read_wasm64.h"
# include "fd_read.h"
# include "fd_readdir_wasm64.h"
# include "fd_readdir.h"
# include "fd_renumber_wasm64.h"
# include "fd_renumber.h"
# include "fd_seek_wasm64.h"
# include "fd_seek.h"
# include "fd_sync_wasm64.h"
# include "fd_sync.h"
# include "fd_tell_wasm64.h"
# include "fd_tell.h"
# include "fd_write_wasm64.h"
# include "fd_write.h"
# include "path_create_directory_wasm64.h"
# include "path_create_directory.h"
# include "path_filestat_get_wasm64.h"
# include "path_filestat_get.h"
# include "path_filestat_set_times_wasm64.h"
# include "path_filestat_set_times.h"
# include "path_link_wasm64.h"
# include "path_link.h"
# include "path_open_wasm64.h"
# include "path_open.h"
# include "path_readlink_wasm64.h"
# include "path_readlink.h"
# include "path_remove_directory_wasm64.h"
# include "path_remove_directory.h"
# include "path_rename_wasm64.h"
# include "path_rename.h"
# include "path_symlink_wasm64.h"
# include "path_symlink.h"
# include "path_unlink_file_wasm64.h"
# include "path_unlink_file.h"
# include "poll_oneoff_wasm64.h"
# include "poll_oneoff.h"
# include "proc_exit_wasm64.h"
# include "proc_exit.h"
# include "proc_raise_wasm64.h"
# include "proc_raise.h"
# include "random_get_wasm64.h"
# include "random_get.h"
# include "sched_yield_wasm64.h"
# include "sched_yield.h"
# include "sock_accept_wasm64.h"
# include "sock_accept.h"
# include "sock_recv_wasm64.h"
# include "sock_recv.h"
# include "sock_send_wasm64.h"
# include "sock_send.h"
# include "sock_shutdown_wasm64.h"
# include "sock_shutdown.h"
#endif
