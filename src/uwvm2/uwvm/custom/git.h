/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-30
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
export import uwvm2.utils.version;  // print defined method should be export
import fast_io;
#else
// std
# include <cstdint>
# include <cstddef>
// import
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <uwvm2/utils/version/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::custom
{

#ifdef UWVM_GIT_COMMIT_ID
    inline constexpr ::fast_io::u8string_view git_commit_id{UWVM_GIT_COMMIT_ID};
#else
    inline constexpr ::fast_io::u8string_view git_commit_id{u8"unknown"};
#endif

#ifdef UWVM_GIT_REMOTE_URL
    inline constexpr ::fast_io::u8string_view git_remote_url{UWVM_GIT_REMOTE_URL};
#else
    inline constexpr ::fast_io::u8string_view git_remote_url{u8"unknown"};
#endif

#ifdef UWVM_GIT_COMMIT_DATA
    inline constexpr ::fast_io::u8string_view git_commit_data{UWVM_GIT_COMMIT_DATA};
#else
    inline constexpr ::fast_io::u8string_view git_commit_data{u8"unknown"};
#endif

#ifdef UWVM_GIT_UPSTREAM_BRANCH
    inline constexpr ::fast_io::u8string_view git_upstream_branch{UWVM_GIT_UPSTREAM_BRANCH};
#else
    inline constexpr ::fast_io::u8string_view git_upstream_branch{u8"unknown"};
#endif

#ifdef UWVM_GIT_HAS_UNCOMMITTED_MODIFICATIONS
    inline constexpr bool git_uncommited_modifications{true};
#else
    inline constexpr bool git_uncommited_modifications{false};
#endif

}  // namespace uwvm2::uwvm::custom
