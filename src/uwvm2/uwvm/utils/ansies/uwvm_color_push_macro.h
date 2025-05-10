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

#include <uwvm2/utils/ansies/ansi_push_macro.h>
#include <uwvm2/utils/ansies/win32_text_attr_push_macro.h>
///
/// CHAR
///
#pragma push_macro("UWVM_COLOR_RST_ALL")
#undef UWVM_COLOR_RST_ALL
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_RST_ALL ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_RST_ALL, UWVM_WIN32_TEXTATTR_RST_ALL)
#else
# define UWVM_COLOR_RST_ALL UWVM_AES_RST_ALL
#endif

#pragma push_macro("UWVM_COLOR_RST_ALL_AND_SET_WHITE")
#undef UWVM_COLOR_RST_ALL_AND_SET_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_RST_ALL_AND_SET_WHITE                                                                                                                      \
     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_RST_ALL UWVM_AES_WHITE, UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE)
#else
# define UWVM_COLOR_RST_ALL_AND_SET_WHITE UWVM_AES_RST_ALL UWVM_AES_WHITE
#endif

#pragma push_macro("UWVM_COLOR_BLACK")
#undef UWVM_COLOR_BLACK
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_BLACK ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_BLACK, UWVM_WIN32_TEXTATTR_BLACK)
#else
# define UWVM_COLOR_BLACK UWVM_AES_BLACK
#endif

#pragma push_macro("UWVM_COLOR_RED")
#undef UWVM_COLOR_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_RED, UWVM_WIN32_TEXTATTR_RED)
#else
# define UWVM_COLOR_RED UWVM_AES_RED
#endif

#pragma push_macro("UWVM_COLOR_GREEN")
#undef UWVM_COLOR_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_GREEN, UWVM_WIN32_TEXTATTR_GREEN)
#else
# define UWVM_COLOR_GREEN UWVM_AES_GREEN
#endif

#pragma push_macro("UWVM_COLOR_ORANGE")
#undef UWVM_COLOR_ORANGE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_ORANGE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_ORANGE, UWVM_WIN32_TEXTATTR_ORANGE)
#else
# define UWVM_COLOR_ORANGE UWVM_AES_ORANGE
#endif

#pragma push_macro("UWVM_COLOR_BLUE")
#undef UWVM_COLOR_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_BLUE, UWVM_WIN32_TEXTATTR_BLUE)
#else
# define UWVM_COLOR_BLUE UWVM_AES_BLUE
#endif
#pragma push_macro("UWVM_COLOR_PURPLE")
#undef UWVM_COLOR_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_PURPLE, UWVM_WIN32_TEXTATTR_PURPLE)
#else
# define UWVM_COLOR_PURPLE UWVM_AES_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_CYAN")
#undef UWVM_COLOR_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_CYAN, UWVM_WIN32_TEXTATTR_CYAN)
#else
# define UWVM_COLOR_CYAN UWVM_AES_CYAN
#endif

#pragma push_macro("UWVM_COLOR_GRAY")
#undef UWVM_COLOR_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_GRAY, UWVM_WIN32_TEXTATTR_GRAY)
#else
# define UWVM_COLOR_GRAY UWVM_AES_GRAY
#endif

#pragma push_macro("UWVM_COLOR_DK_GRAY")
#undef UWVM_COLOR_DK_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_DK_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_DK_GRAY, UWVM_WIN32_TEXTATTR_DK_GRAY)
#else
# define UWVM_COLOR_DK_GRAY UWVM_AES_DK_GRAY
#endif

#pragma push_macro("UWVM_COLOR_LT_RED")
#undef UWVM_COLOR_LT_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_LT_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_LT_RED, UWVM_WIN32_TEXTATTR_LT_RED)
#else
# define UWVM_COLOR_LT_RED UWVM_AES_LT_RED
#endif

#pragma push_macro("UWVM_COLOR_LT_GREEN")
#undef UWVM_COLOR_LT_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_LT_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_LT_GREEN, UWVM_WIN32_TEXTATTR_LT_GREEN)
#else
# define UWVM_COLOR_LT_GREEN UWVM_AES_LT_GREEN
#endif

#pragma push_macro("UWVM_COLOR_YELLOW")
#undef UWVM_COLOR_YELLOW
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_YELLOW ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_YELLOW, UWVM_WIN32_TEXTATTR_YELLOW)
#else
# define UWVM_COLOR_YELLOW UWVM_AES_YELLOW
#endif

#pragma push_macro("UWVM_COLOR_LT_BLUE")
#undef UWVM_COLOR_LT_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_LT_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_LT_BLUE, UWVM_WIN32_TEXTATTR_LT_BLUE)
#else
# define UWVM_COLOR_LT_BLUE UWVM_AES_LT_BLUE
#endif

#pragma push_macro("UWVM_COLOR_LT_PURPLE")
#undef UWVM_COLOR_LT_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_LT_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_LT_PURPLE, UWVM_WIN32_TEXTATTR_LT_PURPLE)
#else
# define UWVM_COLOR_LT_PURPLE UWVM_AES_LT_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_LT_CYAN")
#undef UWVM_COLOR_LT_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_LT_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_LT_CYAN, UWVM_WIN32_TEXTATTR_LT_CYAN)
#else
# define UWVM_COLOR_LT_CYAN UWVM_AES_LT_CYAN
#endif

#pragma push_macro("UWVM_COLOR_WHITE")
#undef UWVM_COLOR_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_WHITE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_WHITE, UWVM_WIN32_TEXTATTR_WHITE)
#else
# define UWVM_COLOR_WHITE UWVM_AES_WHITE
#endif

#pragma push_macro("UWVM_COLOR_RGB")
#undef UWVM_COLOR_RGB
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_RGB(r, g, b) ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_RGB(r, g, b))
#else
# define UWVM_COLOR_RGB(r, g, b) UWVM_AES_RGB(r, g, b)
#endif

///
/// WCHAR
///
#pragma push_macro("UWVM_COLOR_W_RST_ALL")
#undef UWVM_COLOR_W_RST_ALL
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_RST_ALL ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_RST_ALL, UWVM_WIN32_TEXTATTR_RST_ALL)
#else
# define UWVM_COLOR_W_RST_ALL UWVM_AES_W_RST_ALL
#endif

#pragma push_macro("UWVM_COLOR_W_RST_ALL_AND_SET_WHITE")
#undef UWVM_COLOR_W_RST_ALL_AND_SET_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_RST_ALL_AND_SET_WHITE                                                                                                                    \
     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE, UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE)
#else
# define UWVM_COLOR_W_RST_ALL_AND_SET_WHITE UWVM_AES_W_RST_ALL UWVM_AES_W_WHITE
#endif

#pragma push_macro("UWVM_COLOR_W_BLACK")
#undef UWVM_COLOR_W_BLACK
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_BLACK ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_BLACK, UWVM_WIN32_TEXTATTR_BLACK)
#else
# define UWVM_COLOR_W_BLACK UWVM_AES_W_BLACK
#endif

#pragma push_macro("UWVM_COLOR_W_RED")
#undef UWVM_COLOR_W_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_RED, UWVM_WIN32_TEXTATTR_RED)
#else
# define UWVM_COLOR_W_RED UWVM_AES_W_RED
#endif

#pragma push_macro("UWVM_COLOR_W_GREEN")
#undef UWVM_COLOR_W_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_GREEN, UWVM_WIN32_TEXTATTR_GREEN)
#else
# define UWVM_COLOR_W_GREEN UWVM_AES_W_GREEN
#endif

#pragma push_macro("UWVM_COLOR_W_ORANGE")
#undef UWVM_COLOR_W_ORANGE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_ORANGE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_ORANGE, UWVM_WIN32_TEXTATTR_ORANGE)
#else
# define UWVM_COLOR_W_ORANGE UWVM_AES_W_ORANGE
#endif

#pragma push_macro("UWVM_COLOR_W_BLUE")
#undef UWVM_COLOR_W_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_BLUE, UWVM_WIN32_TEXTATTR_BLUE)
#else
# define UWVM_COLOR_W_BLUE UWVM_AES_W_BLUE
#endif
#pragma push_macro("UWVM_COLOR_W_PURPLE")
#undef UWVM_COLOR_W_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_PURPLE, UWVM_WIN32_TEXTATTR_PURPLE)
#else
# define UWVM_COLOR_W_PURPLE UWVM_AES_W_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_W_CYAN")
#undef UWVM_COLOR_W_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_CYAN, UWVM_WIN32_TEXTATTR_CYAN)
#else
# define UWVM_COLOR_W_CYAN UWVM_AES_W_CYAN
#endif

#pragma push_macro("UWVM_COLOR_W_GRAY")
#undef UWVM_COLOR_W_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_GRAY, UWVM_WIN32_TEXTATTR_GRAY)
#else
# define UWVM_COLOR_W_GRAY UWVM_AES_W_GRAY
#endif

#pragma push_macro("UWVM_COLOR_W_DK_GRAY")
#undef UWVM_COLOR_W_DK_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_DK_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_DK_GRAY, UWVM_WIN32_TEXTATTR_DK_GRAY)
#else
# define UWVM_COLOR_W_DK_GRAY UWVM_AES_W_DK_GRAY
#endif

#pragma push_macro("UWVM_COLOR_W_LT_RED")
#undef UWVM_COLOR_W_LT_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_LT_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_LT_RED, UWVM_WIN32_TEXTATTR_LT_RED)
#else
# define UWVM_COLOR_W_LT_RED UWVM_AES_W_LT_RED
#endif

#pragma push_macro("UWVM_COLOR_W_LT_GREEN")
#undef UWVM_COLOR_W_LT_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_LT_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_LT_GREEN, UWVM_WIN32_TEXTATTR_LT_GREEN)
#else
# define UWVM_COLOR_W_LT_GREEN UWVM_AES_W_LT_GREEN
#endif

#pragma push_macro("UWVM_COLOR_W_YELLOW")
#undef UWVM_COLOR_W_YELLOW
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_YELLOW ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_YELLOW, UWVM_WIN32_TEXTATTR_YELLOW)
#else
# define UWVM_COLOR_W_YELLOW UWVM_AES_W_YELLOW
#endif

#pragma push_macro("UWVM_COLOR_W_LT_BLUE")
#undef UWVM_COLOR_W_LT_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_LT_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_LT_BLUE, UWVM_WIN32_TEXTATTR_LT_BLUE)
#else
# define UWVM_COLOR_W_LT_BLUE UWVM_AES_W_LT_BLUE
#endif

#pragma push_macro("UWVM_COLOR_W_LT_PURPLE")
#undef UWVM_COLOR_W_LT_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_LT_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_LT_PURPLE, UWVM_WIN32_TEXTATTR_LT_PURPLE)
#else
# define UWVM_COLOR_W_LT_PURPLE UWVM_AES_W_LT_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_W_LT_CYAN")
#undef UWVM_COLOR_W_LT_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_LT_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_LT_CYAN, UWVM_WIN32_TEXTATTR_LT_CYAN)
#else
# define UWVM_COLOR_W_LT_CYAN UWVM_AES_W_LT_CYAN
#endif

#pragma push_macro("UWVM_COLOR_W_WHITE")
#undef UWVM_COLOR_W_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_WHITE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_WHITE, UWVM_WIN32_TEXTATTR_WHITE)
#else
# define UWVM_COLOR_W_WHITE UWVM_AES_W_WHITE
#endif

#pragma push_macro("UWVM_COLOR_W_RGB")
#undef UWVM_COLOR_W_RGB
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_W_RGB(r, g, b) ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_W_RGB(r, g, b))
#else
# define UWVM_COLOR_W_RGB(r, g, b) UWVM_AES_W_RGB(r, g, b)
#endif

///
/// UTF-8
///
#pragma push_macro("UWVM_COLOR_U8_RST_ALL")
#undef UWVM_COLOR_U8_RST_ALL
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_RST_ALL ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_RST_ALL, UWVM_WIN32_TEXTATTR_RST_ALL)
#else
# define UWVM_COLOR_U8_RST_ALL UWVM_AES_U8_RST_ALL
#endif

#pragma push_macro("UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE")
#undef UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE                                                                                                                   \
     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE, UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE)
#else
# define UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE UWVM_AES_U8_RST_ALL UWVM_AES_U8_WHITE
#endif

#pragma push_macro("UWVM_COLOR_U8_BLACK")
#undef UWVM_COLOR_U8_BLACK
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_BLACK ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_BLACK, UWVM_WIN32_TEXTATTR_BLACK)
#else
# define UWVM_COLOR_U8_BLACK UWVM_AES_U8_BLACK
#endif

#pragma push_macro("UWVM_COLOR_U8_RED")
#undef UWVM_COLOR_U8_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_RED, UWVM_WIN32_TEXTATTR_RED)
#else
# define UWVM_COLOR_U8_RED UWVM_AES_U8_RED
#endif

#pragma push_macro("UWVM_COLOR_U8_GREEN")
#undef UWVM_COLOR_U8_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_GREEN, UWVM_WIN32_TEXTATTR_GREEN)
#else
# define UWVM_COLOR_U8_GREEN UWVM_AES_U8_GREEN
#endif

#pragma push_macro("UWVM_COLOR_U8_ORANGE")
#undef UWVM_COLOR_U8_ORANGE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_ORANGE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_ORANGE, UWVM_WIN32_TEXTATTR_ORANGE)
#else
# define UWVM_COLOR_U8_ORANGE UWVM_AES_U8_ORANGE
#endif

#pragma push_macro("UWVM_COLOR_U8_BLUE")
#undef UWVM_COLOR_U8_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_BLUE, UWVM_WIN32_TEXTATTR_BLUE)
#else
# define UWVM_COLOR_U8_BLUE UWVM_AES_U8_BLUE
#endif
#pragma push_macro("UWVM_COLOR_U8_PURPLE")
#undef UWVM_COLOR_U8_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_PURPLE, UWVM_WIN32_TEXTATTR_PURPLE)
#else
# define UWVM_COLOR_U8_PURPLE UWVM_AES_U8_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_U8_CYAN")
#undef UWVM_COLOR_U8_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_CYAN, UWVM_WIN32_TEXTATTR_CYAN)
#else
# define UWVM_COLOR_U8_CYAN UWVM_AES_U8_CYAN
#endif

#pragma push_macro("UWVM_COLOR_U8_GRAY")
#undef UWVM_COLOR_U8_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_GRAY, UWVM_WIN32_TEXTATTR_GRAY)
#else
# define UWVM_COLOR_U8_GRAY UWVM_AES_U8_GRAY
#endif

#pragma push_macro("UWVM_COLOR_U8_DK_GRAY")
#undef UWVM_COLOR_U8_DK_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_DK_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_DK_GRAY, UWVM_WIN32_TEXTATTR_DK_GRAY)
#else
# define UWVM_COLOR_U8_DK_GRAY UWVM_AES_U8_DK_GRAY
#endif

#pragma push_macro("UWVM_COLOR_U8_LT_RED")
#undef UWVM_COLOR_U8_LT_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_LT_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_LT_RED, UWVM_WIN32_TEXTATTR_LT_RED)
#else
# define UWVM_COLOR_U8_LT_RED UWVM_AES_U8_LT_RED
#endif

#pragma push_macro("UWVM_COLOR_U8_LT_GREEN")
#undef UWVM_COLOR_U8_LT_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_LT_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_LT_GREEN, UWVM_WIN32_TEXTATTR_LT_GREEN)
#else
# define UWVM_COLOR_U8_LT_GREEN UWVM_AES_U8_LT_GREEN
#endif

#pragma push_macro("UWVM_COLOR_U8_YELLOW")
#undef UWVM_COLOR_U8_YELLOW
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_YELLOW ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_YELLOW, UWVM_WIN32_TEXTATTR_YELLOW)
#else
# define UWVM_COLOR_U8_YELLOW UWVM_AES_U8_YELLOW
#endif

#pragma push_macro("UWVM_COLOR_U8_LT_BLUE")
#undef UWVM_COLOR_U8_LT_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_LT_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_LT_BLUE, UWVM_WIN32_TEXTATTR_LT_BLUE)
#else
# define UWVM_COLOR_U8_LT_BLUE UWVM_AES_U8_LT_BLUE
#endif

#pragma push_macro("UWVM_COLOR_U8_LT_PURPLE")
#undef UWVM_COLOR_U8_LT_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_LT_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_LT_PURPLE, UWVM_WIN32_TEXTATTR_LT_PURPLE)
#else
# define UWVM_COLOR_U8_LT_PURPLE UWVM_AES_U8_LT_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_U8_LT_CYAN")
#undef UWVM_COLOR_U8_LT_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_LT_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_LT_CYAN, UWVM_WIN32_TEXTATTR_LT_CYAN)
#else
# define UWVM_COLOR_U8_LT_CYAN UWVM_AES_U8_LT_CYAN
#endif

#pragma push_macro("UWVM_COLOR_U8_WHITE")
#undef UWVM_COLOR_U8_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_WHITE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_WHITE, UWVM_WIN32_TEXTATTR_WHITE)
#else
# define UWVM_COLOR_U8_WHITE UWVM_AES_U8_WHITE
#endif

#pragma push_macro("UWVM_COLOR_U8_RGB")
#undef UWVM_COLOR_U8_RGB
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U8_RGB(r, g, b) ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U8_RGB(r, g, b))
#else
# define UWVM_COLOR_U8_RGB(r, g, b) UWVM_AES_U8_RGB(r, g, b)
#endif

///
/// UTF-16
///
#pragma push_macro("UWVM_COLOR_U16_RST_ALL")
#undef UWVM_COLOR_U16_RST_ALL
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_RST_ALL ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_RST_ALL, UWVM_WIN32_TEXTATTR_RST_ALL)
#else
# define UWVM_COLOR_U16_RST_ALL UWVM_AES_U16_RST_ALL
#endif

#pragma push_macro("UWVM_COLOR_U16_RST_ALL_AND_SET_WHITE")
#undef UWVM_COLOR_U16_RST_ALL_AND_SET_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_RST_ALL_AND_SET_WHITE                                                                                                                  \
     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b,                                                                                  \
                          UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE,                                                                                             \
                          UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE)
#else
# define UWVM_COLOR_U16_RST_ALL_AND_SET_WHITE UWVM_AES_U16_RST_ALL UWVM_AES_U16_WHITE
#endif

#pragma push_macro("UWVM_COLOR_U16_BLACK")
#undef UWVM_COLOR_U16_BLACK
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_BLACK ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_BLACK, UWVM_WIN32_TEXTATTR_BLACK)
#else
# define UWVM_COLOR_U16_BLACK UWVM_AES_U16_BLACK
#endif

#pragma push_macro("UWVM_COLOR_U16_RED")
#undef UWVM_COLOR_U16_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_RED, UWVM_WIN32_TEXTATTR_RED)
#else
# define UWVM_COLOR_U16_RED UWVM_AES_U16_RED
#endif

#pragma push_macro("UWVM_COLOR_U16_GREEN")
#undef UWVM_COLOR_U16_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_GREEN, UWVM_WIN32_TEXTATTR_GREEN)
#else
# define UWVM_COLOR_U16_GREEN UWVM_AES_U16_GREEN
#endif

#pragma push_macro("UWVM_COLOR_U16_ORANGE")
#undef UWVM_COLOR_U16_ORANGE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_ORANGE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_ORANGE, UWVM_WIN32_TEXTATTR_ORANGE)
#else
# define UWVM_COLOR_U16_ORANGE UWVM_AES_U16_ORANGE
#endif

#pragma push_macro("UWVM_COLOR_U16_BLUE")
#undef UWVM_COLOR_U16_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_BLUE, UWVM_WIN32_TEXTATTR_BLUE)
#else
# define UWVM_COLOR_U16_BLUE UWVM_AES_U16_BLUE
#endif
#pragma push_macro("UWVM_COLOR_U16_PURPLE")
#undef UWVM_COLOR_U16_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_PURPLE, UWVM_WIN32_TEXTATTR_PURPLE)
#else
# define UWVM_COLOR_U16_PURPLE UWVM_AES_U16_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_U16_CYAN")
#undef UWVM_COLOR_U16_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_CYAN, UWVM_WIN32_TEXTATTR_CYAN)
#else
# define UWVM_COLOR_U16_CYAN UWVM_AES_U16_CYAN
#endif

#pragma push_macro("UWVM_COLOR_U16_GRAY")
#undef UWVM_COLOR_U16_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_GRAY, UWVM_WIN32_TEXTATTR_GRAY)
#else
# define UWVM_COLOR_U16_GRAY UWVM_AES_U16_GRAY
#endif

#pragma push_macro("UWVM_COLOR_U16_DK_GRAY")
#undef UWVM_COLOR_U16_DK_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_DK_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_DK_GRAY, UWVM_WIN32_TEXTATTR_DK_GRAY)
#else
# define UWVM_COLOR_U16_DK_GRAY UWVM_AES_U16_DK_GRAY
#endif

#pragma push_macro("UWVM_COLOR_U16_LT_RED")
#undef UWVM_COLOR_U16_LT_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_LT_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_LT_RED, UWVM_WIN32_TEXTATTR_LT_RED)
#else
# define UWVM_COLOR_U16_LT_RED UWVM_AES_U16_LT_RED
#endif

#pragma push_macro("UWVM_COLOR_U16_LT_GREEN")
#undef UWVM_COLOR_U16_LT_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_LT_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_LT_GREEN, UWVM_WIN32_TEXTATTR_LT_GREEN)
#else
# define UWVM_COLOR_U16_LT_GREEN UWVM_AES_U16_LT_GREEN
#endif

#pragma push_macro("UWVM_COLOR_U16_YELLOW")
#undef UWVM_COLOR_U16_YELLOW
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_YELLOW ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_YELLOW, UWVM_WIN32_TEXTATTR_YELLOW)
#else
# define UWVM_COLOR_U16_YELLOW UWVM_AES_U16_YELLOW
#endif

#pragma push_macro("UWVM_COLOR_U16_LT_BLUE")
#undef UWVM_COLOR_U16_LT_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_LT_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_LT_BLUE, UWVM_WIN32_TEXTATTR_LT_BLUE)
#else
# define UWVM_COLOR_U16_LT_BLUE UWVM_AES_U16_LT_BLUE
#endif

#pragma push_macro("UWVM_COLOR_U16_LT_PURPLE")
#undef UWVM_COLOR_U16_LT_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_LT_PURPLE                                                                                                                              \
     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_LT_PURPLE, UWVM_WIN32_TEXTATTR_LT_PURPLE)
#else
# define UWVM_COLOR_U16_LT_PURPLE UWVM_AES_U16_LT_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_U16_LT_CYAN")
#undef UWVM_COLOR_U16_LT_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_LT_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_LT_CYAN, UWVM_WIN32_TEXTATTR_LT_CYAN)
#else
# define UWVM_COLOR_U16_LT_CYAN UWVM_AES_U16_LT_CYAN
#endif

#pragma push_macro("UWVM_COLOR_U16_WHITE")
#undef UWVM_COLOR_U16_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_WHITE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_WHITE, UWVM_WIN32_TEXTATTR_WHITE)
#else
# define UWVM_COLOR_U16_WHITE UWVM_AES_U16_WHITE
#endif

#pragma push_macro("UWVM_COLOR_U16_RGB")
#undef UWVM_COLOR_U16_RGB
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U16_RGB(r, g, b) ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U16_RGB(r, g, b))
#else
# define UWVM_COLOR_U16_RGB(r, g, b) UWVM_AES_U16_RGB(r, g, b)
#endif

///
/// UTF-32
///
#pragma push_macro("UWVM_COLOR_U32_RST_ALL")
#undef UWVM_COLOR_U32_RST_ALL
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_RST_ALL ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_RST_ALL, UWVM_WIN32_TEXTATTR_RST_ALL)
#else
# define UWVM_COLOR_U32_RST_ALL UWVM_AES_U32_RST_ALL
#endif

#pragma push_macro("UWVM_COLOR_U32_RST_ALL_AND_SET_WHITE")
#undef UWVM_COLOR_U32_RST_ALL_AND_SET_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_RST_ALL_AND_SET_WHITE                                                                                                                  \
     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b,                                                                                  \
                          UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE,                                                                                             \
                          UWVM_WIN32_TEXTATTR_RST_ALL_AND_SET_WHITE)
#else
# define UWVM_COLOR_U32_RST_ALL_AND_SET_WHITE UWVM_AES_U32_RST_ALL UWVM_AES_U32_WHITE
#endif

#pragma push_macro("UWVM_COLOR_U32_BLACK")
#undef UWVM_COLOR_U32_BLACK
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_BLACK ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_BLACK, UWVM_WIN32_TEXTATTR_BLACK)
#else
# define UWVM_COLOR_U32_BLACK UWVM_AES_U32_BLACK
#endif

#pragma push_macro("UWVM_COLOR_U32_RED")
#undef UWVM_COLOR_U32_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_RED, UWVM_WIN32_TEXTATTR_RED)
#else
# define UWVM_COLOR_U32_RED UWVM_AES_U32_RED
#endif

#pragma push_macro("UWVM_COLOR_U32_GREEN")
#undef UWVM_COLOR_U32_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_GREEN, UWVM_WIN32_TEXTATTR_GREEN)
#else
# define UWVM_COLOR_U32_GREEN UWVM_AES_U32_GREEN
#endif

#pragma push_macro("UWVM_COLOR_U32_ORANGE")
#undef UWVM_COLOR_U32_ORANGE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_ORANGE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_ORANGE, UWVM_WIN32_TEXTATTR_ORANGE)
#else
# define UWVM_COLOR_U32_ORANGE UWVM_AES_U32_ORANGE
#endif

#pragma push_macro("UWVM_COLOR_U32_BLUE")
#undef UWVM_COLOR_U32_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_BLUE, UWVM_WIN32_TEXTATTR_BLUE)
#else
# define UWVM_COLOR_U32_BLUE UWVM_AES_U32_BLUE
#endif
#pragma push_macro("UWVM_COLOR_U32_PURPLE")
#undef UWVM_COLOR_U32_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_PURPLE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_PURPLE, UWVM_WIN32_TEXTATTR_PURPLE)
#else
# define UWVM_COLOR_U32_PURPLE UWVM_AES_U32_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_U32_CYAN")
#undef UWVM_COLOR_U32_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_CYAN, UWVM_WIN32_TEXTATTR_CYAN)
#else
# define UWVM_COLOR_U32_CYAN UWVM_AES_U32_CYAN
#endif

#pragma push_macro("UWVM_COLOR_U32_GRAY")
#undef UWVM_COLOR_U32_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_GRAY, UWVM_WIN32_TEXTATTR_GRAY)
#else
# define UWVM_COLOR_U32_GRAY UWVM_AES_U32_GRAY
#endif

#pragma push_macro("UWVM_COLOR_U32_DK_GRAY")
#undef UWVM_COLOR_U32_DK_GRAY
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_DK_GRAY ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_DK_GRAY, UWVM_WIN32_TEXTATTR_DK_GRAY)
#else
# define UWVM_COLOR_U32_DK_GRAY UWVM_AES_U32_DK_GRAY
#endif

#pragma push_macro("UWVM_COLOR_U32_LT_RED")
#undef UWVM_COLOR_U32_LT_RED
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_LT_RED ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_LT_RED, UWVM_WIN32_TEXTATTR_LT_RED)
#else
# define UWVM_COLOR_U32_LT_RED UWVM_AES_U32_LT_RED
#endif

#pragma push_macro("UWVM_COLOR_U32_LT_GREEN")
#undef UWVM_COLOR_U32_LT_GREEN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_LT_GREEN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_LT_GREEN, UWVM_WIN32_TEXTATTR_LT_GREEN)
#else
# define UWVM_COLOR_U32_LT_GREEN UWVM_AES_U32_LT_GREEN
#endif

#pragma push_macro("UWVM_COLOR_U32_YELLOW")
#undef UWVM_COLOR_U32_YELLOW
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_YELLOW ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_YELLOW, UWVM_WIN32_TEXTATTR_YELLOW)
#else
# define UWVM_COLOR_U32_YELLOW UWVM_AES_U32_YELLOW
#endif

#pragma push_macro("UWVM_COLOR_U32_LT_BLUE")
#undef UWVM_COLOR_U32_LT_BLUE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_LT_BLUE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_LT_BLUE, UWVM_WIN32_TEXTATTR_LT_BLUE)
#else
# define UWVM_COLOR_U32_LT_BLUE UWVM_AES_U32_LT_BLUE
#endif

#pragma push_macro("UWVM_COLOR_U32_LT_PURPLE")
#undef UWVM_COLOR_U32_LT_PURPLE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_LT_PURPLE                                                                                                                              \
     ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_LT_PURPLE, UWVM_WIN32_TEXTATTR_LT_PURPLE)
#else
# define UWVM_COLOR_U32_LT_PURPLE UWVM_AES_U32_LT_PURPLE
#endif

#pragma push_macro("UWVM_COLOR_U32_LT_CYAN")
#undef UWVM_COLOR_U32_LT_CYAN
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_LT_CYAN ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_LT_CYAN, UWVM_WIN32_TEXTATTR_LT_CYAN)
#else
# define UWVM_COLOR_U32_LT_CYAN UWVM_AES_U32_LT_CYAN
#endif

#pragma push_macro("UWVM_COLOR_U32_WHITE")
#undef UWVM_COLOR_U32_WHITE
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_WHITE ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_WHITE, UWVM_WIN32_TEXTATTR_WHITE)
#else
# define UWVM_COLOR_U32_WHITE UWVM_AES_U32_WHITE
#endif

#pragma push_macro("UWVM_COLOR_U32_RGB")
#undef UWVM_COLOR_U32_RGB
#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
# define UWVM_COLOR_U32_RGB(r, g, b) ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b, UWVM_AES_U32_RGB(r, g, b))
#else
# define UWVM_COLOR_U32_RGB(r, g, b) UWVM_AES_U32_RGB(r, g, b)
#endif
