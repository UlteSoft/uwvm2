/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-24
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

/// @brief      The following are the macros used by ansi escape sequences.
/// @details    Use `push_macro` to avoid side effects on existing macros. Please use `pop_macro` in conjunction.

// #pragma once

#include "ansi_push_macro.h"

#pragma push_macro("UWVM_WIN32_TEXTATTR_RST_ALL")
#undef UWVM_WIN32_TEXTATTR_RST_ALL
#define UWVM_WIN32_TEXTATTR_RST_ALL                                                                                                                            \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_rst_all }

#pragma push_macro("UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE")
#undef UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE
#define UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE                                                                                                              \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_white }

#pragma push_macro("UWVM_WIN32_TEXTATTR_BLACK")
#undef UWVM_WIN32_TEXTATTR_BLACK
#define UWVM_WIN32_TEXTATTR_BLACK                                                                                                                              \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_black }

#pragma push_macro("UWVM_WIN32_TEXTATTR_RED")
#undef UWVM_WIN32_TEXTATTR_RED
#define UWVM_WIN32_TEXTATTR_RED                                                                                                                                \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_red }

#pragma push_macro("UWVM_WIN32_TEXTATTR_GREEN")
#undef UWVM_WIN32_TEXTATTR_GREEN
#define UWVM_WIN32_TEXTATTR_GREEN                                                                                                                              \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_green }

#pragma push_macro("UWVM_WIN32_TEXTATTR_ORANGE")
#undef UWVM_WIN32_TEXTATTR_ORANGE
#define UWVM_WIN32_TEXTATTR_ORANGE                                                                                                                             \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_orange }

#pragma push_macro("UWVM_WIN32_TEXTATTR_BLUE")
#undef UWVM_WIN32_TEXTATTR_BLUE
#define UWVM_WIN32_TEXTATTR_BLUE                                                                                                                               \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_blue }

#pragma push_macro("UWVM_WIN32_TEXTATTR_PURPLE")
#undef UWVM_WIN32_TEXTATTR_PURPLE
#define UWVM_WIN32_TEXTATTR_PURPLE                                                                                                                             \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_purple }

#pragma push_macro("UWVM_WIN32_TEXTATTR_CYAN")
#undef UWVM_WIN32_TEXTATTR_CYAN
#define UWVM_WIN32_TEXTATTR_CYAN                                                                                                                               \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_cyan }

#pragma push_macro("UWVM_WIN32_TEXTATTR_GRAY")
#undef UWVM_WIN32_TEXTATTR_GRAY
#define UWVM_WIN32_TEXTATTR_GRAY                                                                                                                               \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_gray }

#pragma push_macro("UWVM_WIN32_TEXTATTR_DK_GRAY")
#undef UWVM_WIN32_TEXTATTR_DK_GRAY
#define UWVM_WIN32_TEXTATTR_DK_GRAY                                                                                                                            \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_dk_gray }

#pragma push_macro("UWVM_WIN32_TEXTATTR_LT_RED")
#undef UWVM_WIN32_TEXTATTR_LT_RED
#define UWVM_WIN32_TEXTATTR_LT_RED                                                                                                                             \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_lt_red }

#pragma push_macro("UWVM_WIN32_TEXTATTR_LT_GREEN")
#undef UWVM_WIN32_TEXTATTR_LT_GREEN
#define UWVM_WIN32_TEXTATTR_LT_GREEN                                                                                                                           \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_lt_green }

#pragma push_macro("UWVM_WIN32_TEXTATTR_YELLOW")
#undef UWVM_WIN32_TEXTATTR_YELLOW
#define UWVM_WIN32_TEXTATTR_YELLOW                                                                                                                             \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_yellow }

#pragma push_macro("UWVM_WIN32_TEXTATTR_LT_BLUE")
#undef UWVM_WIN32_TEXTATTR_LT_BLUE
#define UWVM_WIN32_TEXTATTR_LT_BLUE                                                                                                                            \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_lt_blue }

#pragma push_macro("UWVM_WIN32_TEXTATTR_LT_PURPLE")
#undef UWVM_WIN32_TEXTATTR_LT_PURPLE
#define UWVM_WIN32_TEXTATTR_LT_PURPLE                                                                                                                          \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_lt_purple }

#pragma push_macro("UWVM_WIN32_TEXTATTR_LT_CYAN")
#undef UWVM_WIN32_TEXTATTR_LT_CYAN
#define UWVM_WIN32_TEXTATTR_LT_CYAN                                                                                                                            \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_lt_cyan }

#pragma push_macro("UWVM_WIN32_TEXTATTR_WHITE")
#undef UWVM_WIN32_TEXTATTR_WHITE
#define UWVM_WIN32_TEXTATTR_WHITE                                                                                                                              \
    ::uwvm2::utils::ansies::win32_text_attr { ::uwvm2::utils::ansies::text_attr::foreground_white }

