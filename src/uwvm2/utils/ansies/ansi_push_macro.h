/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-23
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

/// @brief      The following are the macros used by ansi escape sequences.
/// @details    Use `push_macro` to avoid side effects on existing macros. Please use `pop_macro` in conjunction.

// #pragma once

/// @brief Text color variables:

///
/// CHAR
///

#pragma push_macro("UWVM_AES_BLACK")
#undef UWVM_AES_BLACK
#define UWVM_AES_BLACK "\033[30m"

#pragma push_macro("UWVM_AES_RED")
#undef UWVM_AES_RED
#define UWVM_AES_RED "\033[31m"

#pragma push_macro("UWVM_AES_GREEN")
#undef UWVM_AES_GREEN
#define UWVM_AES_GREEN "\033[32m"

#pragma push_macro("UWVM_AES_ORANGE")
#undef UWVM_AES_ORANGE
#define UWVM_AES_ORANGE "\033[33m"

#pragma push_macro("UWVM_AES_BLUE")
#undef UWVM_AES_BLUE
#define UWVM_AES_BLUE "\033[34m"

#pragma push_macro("UWVM_AES_PURPLE")
#undef UWVM_AES_PURPLE
#define UWVM_AES_PURPLE "\033[35m"

#pragma push_macro("UWVM_AES_CYAN")
#undef UWVM_AES_CYAN
#define UWVM_AES_CYAN "\033[36m"

#pragma push_macro("UWVM_AES_GRAY")
#undef UWVM_AES_GRAY
#define UWVM_AES_GRAY "\033[37m"

#pragma push_macro("UWVM_AES_DK_GRAY")
#undef UWVM_AES_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_DK_GRAY UWVM_AES_BLACK
#else
# define UWVM_AES_DK_GRAY "\033[90m"
#endif

#pragma push_macro("UWVM_AES_LT_RED")
#undef UWVM_AES_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_LT_RED UWVM_AES_RED
#else
# define UWVM_AES_LT_RED "\033[91m"
#endif

#pragma push_macro("UWVM_AES_LT_GREEN")
#undef UWVM_AES_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_LT_GREEN UWVM_AES_GREEN
#else
# define UWVM_AES_LT_GREEN "\033[92m"
#endif

#pragma push_macro("UWVM_AES_YELLOW")
#undef UWVM_AES_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_YELLOW UWVM_AES_ORANGE
#else
# define UWVM_AES_YELLOW "\033[93m"
#endif

#pragma push_macro("UWVM_AES_LT_BLUE")
#undef UWVM_AES_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_LT_BLUE UWVM_AES_BLUE
#else
# define UWVM_AES_LT_BLUE "\033[94m"
#endif

#pragma push_macro("UWVM_AES_LT_PURPLE")
#undef UWVM_AES_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_LT_PURPLE UWVM_AES_PURPLE
#else
# define UWVM_AES_LT_PURPLE "\033[95m"
#endif

#pragma push_macro("UWVM_AES_LT_CYAN")
#undef UWVM_AES_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_LT_CYAN UWVM_AES_CYAN
#else
# define UWVM_AES_LT_CYAN "\033[96m"
#endif

#pragma push_macro("UWVM_AES_WHITE")
#undef UWVM_AES_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_WHITE UWVM_AES_GRAY
#else
# define UWVM_AES_WHITE "\033[97m"
#endif

/// @brief Background color variables:

#pragma push_macro("UWVM_AES_BG_BLACK")
#undef UWVM_AES_BG_BLACK
#define UWVM_AES_BG_BLACK "\033[40m"

#pragma push_macro("UWVM_AES_BG_RED")
#undef UWVM_AES_BG_RED
#define UWVM_AES_BG_RED "\033[41m"

#pragma push_macro("UWVM_AES_BG_GREEN")
#undef UWVM_AES_BG_GREEN
#define UWVM_AES_BG_GREEN "\033[42m"

#pragma push_macro("UWVM_AES_BG_ORANGE")
#undef UWVM_AES_BG_ORANGE
#define UWVM_AES_BG_ORANGE "\033[43m"

#pragma push_macro("UWVM_AES_BG_BLUE")
#undef UWVM_AES_BG_BLUE
#define UWVM_AES_BG_BLUE "\033[44m"

#pragma push_macro("UWVM_AES_BG_PURPLE")
#undef UWVM_AES_BG_PURPLE
#define UWVM_AES_BG_PURPLE "\033[45m"

#pragma push_macro("UWVM_AES_BG_CYAN")
#undef UWVM_AES_BG_CYAN
#define UWVM_AES_BG_CYAN "\033[46m"

#pragma push_macro("UWVM_AES_BG_GRAY")
#undef UWVM_AES_BG_GRAY
#define UWVM_AES_BG_GRAY "\033[47m"

#pragma push_macro("UWVM_AES_BG_DK_GRAY")
#undef UWVM_AES_BG_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_BG_DK_GRAY UWVM_AES_BG_BLACK
#else
# define UWVM_AES_BG_DK_GRAY "\033[100m"
#endif

#pragma push_macro("UWVM_AES_BG_LT_RED")
#undef UWVM_AES_BG_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_BG_LT_RED UWVM_AES_BG_RED
#else
# define UWVM_AES_BG_LT_RED "\033[101m"
#endif

#pragma push_macro("UWVM_AES_BG_LT_GREEN")
#undef UWVM_AES_BG_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_BG_LT_GREEN UWVM_AES_BG_GREEN
#else
# define UWVM_AES_BG_LT_GREEN "\033[102m"
#endif

#pragma push_macro("UWVM_AES_BG_YELLOW")
#undef UWVM_AES_BG_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_BG_YELLOW UWVM_AES_BG_ORANGE
#else
# define UWVM_AES_BG_YELLOW "\033[103m"
#endif

#pragma push_macro("UWVM_AES_BG_LT_BLUE")
#undef UWVM_AES_BG_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_BG_LT_BLUE UWVM_AES_BG_BLUE
#else
# define UWVM_AES_BG_LT_BLUE "\033[104m"
#endif

#pragma push_macro("UWVM_AES_BG_LT_PURPLE")
#undef UWVM_AES_BG_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_BG_LT_PURPLE UWVM_AES_BG_PURPLE
#else
# define UWVM_AES_BG_LT_PURPLE "\033[105m"
#endif

#pragma push_macro("UWVM_AES_BG_LT_CYAN")
#undef UWVM_AES_BG_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_BG_LT_CYAN UWVM_AES_BG_CYAN
#else
# define UWVM_AES_BG_LT_CYAN "\033[106m"
#endif

#pragma push_macro("UWVM_AES_BG_WHITE")
#undef UWVM_AES_BG_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_BG_WHITE UWVM_AES_BG_GRAY
#else
# define UWVM_AES_BG_WHITE "\033[107m"
#endif

/// @brief Bold color variables:
#pragma push_macro("UWVM_AES_BD_BLACK")
#undef UWVM_AES_BD_BLACK
#define UWVM_AES_BD_BLACK "\033[1;30m"

#pragma push_macro("UWVM_AES_BD_RED")
#undef UWVM_AES_BD_RED
#define UWVM_AES_BD_RED "\033[1;31m"

#pragma push_macro("UWVM_AES_BD_GREEN")
#undef UWVM_AES_BD_GREEN
#define UWVM_AES_BD_GREEN "\033[1;32m"

#pragma push_macro("UWVM_AES_BD_ORANGE")
#undef UWVM_AES_BD_ORANGE
#define UWVM_AES_BD_ORANGE "\033[1;33m"

#pragma push_macro("UWVM_AES_BD_BLUE")
#undef UWVM_AES_BD_BLUE
#define UWVM_AES_BD_BLUE "\033[1;34m"

#pragma push_macro("UWVM_AES_BD_PURPLE")
#undef UWVM_AES_BD_PURPLE
#define UWVM_AES_BD_PURPLE "\033[1;35m"

#pragma push_macro("UWVM_AES_BD_CYAN")
#undef UWVM_AES_BD_CYAN
#define UWVM_AES_BD_CYAN "\033[1;36m"

#pragma push_macro("UWVM_AES_BD_GRAY")
#undef UWVM_AES_BD_GRAY
#define UWVM_AES_BD_GRAY "\033[1;37m"

/// @brief Style variables:

#pragma push_macro("UWVM_AES_STY_BOLD")
#undef UWVM_AES_STY_BOLD
#define UWVM_AES_STY_BOLD "\033[1m"

#pragma push_macro("UWVM_AES_STY_FAINT")
#undef UWVM_AES_STY_FAINT
#define UWVM_AES_STY_FAINT "\033[2m"

#pragma push_macro("UWVM_AES_STY_ITALICS")
#undef UWVM_AES_STY_ITALICS
#define UWVM_AES_STY_ITALICS "\033[3m"

#pragma push_macro("UWVM_AES_STY_UNDERLINE")
#undef UWVM_AES_STY_UNDERLINE
#define UWVM_AES_STY_UNDERLINE "\033[4m"

#pragma push_macro("UWVM_AES_STY_BLINK")
#undef UWVM_AES_STY_BLINK
#define UWVM_AES_STY_BLINK "\033[5m"

#pragma push_macro("UWVM_AES_STY_INVERSE")
#undef UWVM_AES_STY_INVERSE
#define UWVM_AES_STY_INVERSE "\033[7m"

#pragma push_macro("UWVM_AES_STY_INVISIABLE")
#undef UWVM_AES_STY_INVISIABLE
#define UWVM_AES_STY_INVISIABLE "\033[8m"

#pragma push_macro("UWVM_AES_STY_CROSSED")
#undef UWVM_AES_STY_CROSSED
#define UWVM_AES_STY_CROSSED "\033[9m"

#pragma push_macro("UWVM_AES_STY_DOUBLE_UNDERLINE")
#undef UWVM_AES_STY_DOUBLE_UNDERLINE
#define UWVM_AES_STY_DOUBLE_UNDERLINE "\033[21m"

/// @brief Reset features commands

#pragma push_macro("UWVM_AES_RST_ALL")
#undef UWVM_AES_RST_ALL
#define UWVM_AES_RST_ALL "\033[0m"

#pragma push_macro("UWVM_AES_RST_COLOR")
#undef UWVM_AES_RST_COLOR
#define UWVM_AES_RST_COLOR "\033[39m"

#pragma push_macro("UWVM_AES_RST_BG_COLOR")
#undef UWVM_AES_RST_BG_COLOR
#define UWVM_AES_RST_BG_COLOR "\033[49m"

#pragma push_macro("UWVM_AES_RST_BD_COLOR")
#undef UWVM_AES_RST_BD_COLOR
#define UWVM_AES_RST_BD_COLOR "\033[22m\033[39m"

#pragma push_macro("UWVM_AES_RST_BD_FT")
#undef UWVM_AES_RST_BD_FT
#define UWVM_AES_RST_BD_FT "\033[22m"

#pragma push_macro("UWVM_AES_RST_ITALICS")
#undef UWVM_AES_RST_ITALICS
#define UWVM_AES_RST_ITALICS "\033[23m"

#pragma push_macro("UWVM_AES_RST_UNDERLINE")
#undef UWVM_AES_RST_UNDERLINE
#define UWVM_AES_RST_UNDERLINE "\033[24m"

#pragma push_macro("UWVM_AES_RST_BLINK")
#undef UWVM_AES_RST_BLINK
#define UWVM_AES_RST_BLINK "\033[25m"

#pragma push_macro("UWVM_AES_RST_INVERSE")
#undef UWVM_AES_RST_INVERSE
#define UWVM_AES_RST_INVERSE "\033[27m"

#pragma push_macro("UWVM_AES_RST_INVISIABLE")
#undef UWVM_AES_RST_INVISIABLE
#define UWVM_AES_RST_INVISIABLE "\033[28m"

#pragma push_macro("UWVM_AES_RST_CROSS")
#undef UWVM_AES_RST_CROSS
#define UWVM_AES_RST_CROSS "\033[29m"

/// @brief Control sequences variables

/// @brief Hide cursor
#pragma push_macro("UWVM_AES_CTRL_HCRS")
#undef UWVM_AES_CTRL_HCRS
#define UWVM_AES_CTRL_HCRS "\033[?25l"

/// @brief Show cursor
#pragma push_macro("UWVM_AES_CTRL_SCRS")
#undef UWVM_AES_CTRL_SCRS
#define UWVM_AES_CTRL_SCRS "\033[?25h"

/// @brief RGB
#pragma push_macro("UWVM_AES_RGB")
#undef UWVM_AES_RGB
#define UWVM_AES_RGB(r, g, b) "\033[38;2;" #r ";" #g ";" #b "m"

///
/// WCHAR
///
#pragma push_macro("UWVM_AES_W_BLACK")
#undef UWVM_AES_W_BLACK
#define UWVM_AES_W_BLACK L"\033[30m"

#pragma push_macro("UWVM_AES_W_RED")
#undef UWVM_AES_W_RED
#define UWVM_AES_W_RED L"\033[31m"

#pragma push_macro("UWVM_AES_W_GREEN")
#undef UWVM_AES_W_GREEN
#define UWVM_AES_W_GREEN L"\033[32m"

#pragma push_macro("UWVM_AES_W_ORANGE")
#undef UWVM_AES_W_ORANGE
#define UWVM_AES_W_ORANGE L"\033[33m"

#pragma push_macro("UWVM_AES_W_BLUE")
#undef UWVM_AES_W_BLUE
#define UWVM_AES_W_BLUE L"\033[34m"

#pragma push_macro("UWVM_AES_W_PURPLE")
#undef UWVM_AES_W_PURPLE
#define UWVM_AES_W_PURPLE L"\033[35m"

#pragma push_macro("UWVM_AES_W_CYAN")
#undef UWVM_AES_W_CYAN
#define UWVM_AES_W_CYAN L"\033[36m"

#pragma push_macro("UWVM_AES_W_GRAY")
#undef UWVM_AES_W_GRAY
#define UWVM_AES_W_GRAY L"\033[37m"

#pragma push_macro("UWVM_AES_W_DK_GRAY")
#undef UWVM_AES_W_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_W_DK_GRAY UWVM_AES_W_BLACK
#else
# define UWVM_AES_W_DK_GRAY L"\033[90m"
#endif

#pragma push_macro("UWVM_AES_W_LT_RED")
#undef UWVM_AES_W_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_W_LT_RED UWVM_AES_W_RED
#else
# define UWVM_AES_W_LT_RED L"\033[91m"
#endif

#pragma push_macro("UWVM_AES_W_LT_GREEN")
#undef UWVM_AES_W_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_W_LT_GREEN UWVM_AES_W_GREEN
#else
# define UWVM_AES_W_LT_GREEN L"\033[92m"
#endif

#pragma push_macro("UWVM_AES_W_YELLOW")
#undef UWVM_AES_W_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_W_YELLOW UWVM_AES_W_ORANGE
#else
# define UWVM_AES_W_YELLOW L"\033[93m"
#endif

#pragma push_macro("UWVM_AES_W_LT_BLUE")
#undef UWVM_AES_W_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_W_LT_BLUE UWVM_AES_W_BLUE
#else
# define UWVM_AES_W_LT_BLUE L"\033[94m"
#endif

#pragma push_macro("UWVM_AES_W_LT_PURPLE")
#undef UWVM_AES_W_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_W_LT_PURPLE UWVM_AES_W_PURPLE
#else
# define UWVM_AES_W_LT_PURPLE L"\033[95m"
#endif

#pragma push_macro("UWVM_AES_W_LT_CYAN")
#undef UWVM_AES_W_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_W_LT_CYAN UWVM_AES_W_CYAN
#else
# define UWVM_AES_W_LT_CYAN L"\033[96m"
#endif

#pragma push_macro("UWVM_AES_W_WHITE")
#undef UWVM_AES_W_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_W_WHITE UWVM_AES_W_GRAY
#else
# define UWVM_AES_W_WHITE L"\033[97m"
#endif

/// @brief Background color variables:

#pragma push_macro("UWVM_AES_W_BG_BLACK")
#undef UWVM_AES_W_BG_BLACK
#define UWVM_AES_W_BG_BLACK L"\033[40m"

#pragma push_macro("UWVM_AES_W_BG_RED")
#undef UWVM_AES_W_BG_RED
#define UWVM_AES_W_BG_RED L"\033[41m"

#pragma push_macro("UWVM_AES_W_BG_GREEN")
#undef UWVM_AES_W_BG_GREEN
#define UWVM_AES_W_BG_GREEN L"\033[42m"

#pragma push_macro("UWVM_AES_W_BG_ORANGE")
#undef UWVM_AES_W_BG_ORANGE
#define UWVM_AES_W_BG_ORANGE L"\033[43m"

#pragma push_macro("UWVM_AES_W_BG_BLUE")
#undef UWVM_AES_W_BG_BLUE
#define UWVM_AES_W_BG_BLUE L"\033[44m"

#pragma push_macro("UWVM_AES_W_BG_PURPLE")
#undef UWVM_AES_W_BG_PURPLE
#define UWVM_AES_W_BG_PURPLE L"\033[45m"

#pragma push_macro("UWVM_AES_W_BG_CYAN")
#undef UWVM_AES_W_BG_CYAN
#define UWVM_AES_W_BG_CYAN L"\033[46m"

#pragma push_macro("UWVM_AES_W_BG_GRAY")
#undef UWVM_AES_W_BG_GRAY
#define UWVM_AES_W_BG_GRAY L"\033[47m"

#pragma push_macro("UWVM_AES_W_BG_DK_GRAY")
#undef UWVM_AES_W_BG_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_DK_GRAY UWVM_AES_W_BG_BLACK
#else
# define UWVM_AES_W_BG_DK_GRAY L"\033[100m"
#endif

#pragma push_macro("UWVM_AES_W_BG_LT_RED")
#undef UWVM_AES_W_BG_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_LT_RED UWVM_AES_W_BG_RED
#else
# define UWVM_AES_W_BG_LT_RED L"\033[101m"
#endif

#pragma push_macro("UWVM_AES_W_BG_LT_GREEN")
#undef UWVM_AES_W_BG_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_LT_GREEN UWVM_AES_W_BG_GREEN
#else
# define UWVM_AES_W_BG_LT_GREEN L"\033[102m"
#endif

#pragma push_macro("UWVM_AES_W_BG_YELLOW")
#undef UWVM_AES_W_BG_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_YELLOW UWVM_AES_W_BG_ORANGE
#else
# define UWVM_AES_W_BG_YELLOW L"\033[103m"
#endif

#pragma push_macro("UWVM_AES_W_BG_LT_BLUE")
#undef UWVM_AES_W_BG_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_LT_BLUE UWVM_AES_W_BG_BLUE
#else
# define UWVM_AES_W_BG_LT_BLUE L"\033[104m"
#endif

#pragma push_macro("UWVM_AES_W_BG_LT_PURPLE")
#undef UWVM_AES_W_BG_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_LT_PURPLE UWVM_AES_W_BG_PURPLE
#else
# define UWVM_AES_W_BG_LT_PURPLE L"\033[105m"
#endif

#pragma push_macro("UWVM_AES_W_BG_LT_CYAN")
#undef UWVM_AES_W_BG_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_LT_CYAN UWVM_AES_W_BG_CYAN
#else
# define UWVM_AES_W_BG_LT_CYAN L"\033[106m"
#endif

#pragma push_macro("UWVM_AES_W_BG_WHITE")
#undef UWVM_AES_W_BG_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_W_BG_WHITE UWVM_AES_W_BG_GRAY
#else
# define UWVM_AES_W_BG_WHITE L"\033[107m"
#endif

/// @brief Bold color variables:
#pragma push_macro("UWVM_AES_W_BD_BLACK")
#undef UWVM_AES_W_BD_BLACK
#define UWVM_AES_W_BD_BLACK L"\033[1;30m"

#pragma push_macro("UWVM_AES_W_BD_RED")
#undef UWVM_AES_W_BD_RED
#define UWVM_AES_W_BD_RED L"\033[1;31m"

#pragma push_macro("UWVM_AES_W_BD_GREEN")
#undef UWVM_AES_W_BD_GREEN
#define UWVM_AES_W_BD_GREEN L"\033[1;32m"

#pragma push_macro("UWVM_AES_W_BD_ORANGE")
#undef UWVM_AES_W_BD_ORANGE
#define UWVM_AES_W_BD_ORANGE L"\033[1;33m"

#pragma push_macro("UWVM_AES_W_BD_BLUE")
#undef UWVM_AES_W_BD_BLUE
#define UWVM_AES_W_BD_BLUE L"\033[1;34m"

#pragma push_macro("UWVM_AES_W_BD_PURPLE")
#undef UWVM_AES_W_BD_PURPLE
#define UWVM_AES_W_BD_PURPLE L"\033[1;35m"

#pragma push_macro("UWVM_AES_W_BD_CYAN")
#undef UWVM_AES_W_BD_CYAN
#define UWVM_AES_W_BD_CYAN L"\033[1;36m"

#pragma push_macro("UWVM_AES_W_BD_GRAY")
#undef UWVM_AES_W_BD_GRAY
#define UWVM_AES_W_BD_GRAY L"\033[1;37m"

/// @brief Style variables:

#pragma push_macro("UWVM_AES_W_STY_BOLD")
#undef UWVM_AES_W_STY_BOLD
#define UWVM_AES_W_STY_BOLD L"\033[1m"

#pragma push_macro("UWVM_AES_W_STY_FAINT")
#undef UWVM_AES_W_STY_FAINT
#define UWVM_AES_W_STY_FAINT L"\033[2m"

#pragma push_macro("UWVM_AES_W_STY_ITALICS")
#undef UWVM_AES_W_STY_ITALICS
#define UWVM_AES_W_STY_ITALICS L"\033[3m"

#pragma push_macro("UWVM_AES_W_STY_UNDERLINE")
#undef UWVM_AES_W_STY_UNDERLINE
#define UWVM_AES_W_STY_UNDERLINE L"\033[4m"

#pragma push_macro("UWVM_AES_W_STY_BLINK")
#undef UWVM_AES_W_STY_BLINK
#define UWVM_AES_W_STY_BLINK L"\033[5m"

#pragma push_macro("UWVM_AES_W_STY_INVERSE")
#undef UWVM_AES_W_STY_INVERSE
#define UWVM_AES_W_STY_INVERSE L"\033[7m"

#pragma push_macro("UWVM_AES_W_STY_INVISIABLE")
#undef UWVM_AES_W_STY_INVISIABLE
#define UWVM_AES_W_STY_INVISIABLE L"\033[8m"

#pragma push_macro("UWVM_AES_W_STY_CROSSED")
#undef UWVM_AES_W_STY_CROSSED
#define UWVM_AES_W_STY_CROSSED L"\033[9m"

#pragma push_macro("UWVM_AES_W_STY_DOUBLE_UNDERLINE")
#undef UWVM_AES_W_STY_DOUBLE_UNDERLINE
#define UWVM_AES_W_STY_DOUBLE_UNDERLINE L"\033[21m"

/// @brief Reset features commands

#pragma push_macro("UWVM_AES_W_RST_ALL")
#undef UWVM_AES_W_RST_ALL
#define UWVM_AES_W_RST_ALL L"\033[0m"

#pragma push_macro("UWVM_AES_W_RST_COLOR")
#undef UWVM_AES_W_RST_COLOR
#define UWVM_AES_W_RST_COLOR L"\033[39m"

#pragma push_macro("UWVM_AES_W_RST_BG_COLOR")
#undef UWVM_AES_W_RST_BG_COLOR
#define UWVM_AES_W_RST_BG_COLOR L"\033[49m"

#pragma push_macro("UWVM_AES_W_RST_BD_COLOR")
#undef UWVM_AES_W_RST_BD_COLOR
#define UWVM_AES_W_RST_BD_COLOR L"\033[22m\033[39m"

#pragma push_macro("UWVM_AES_W_RST_BD_FT")
#undef UWVM_AES_W_RST_BD_FT
#define UWVM_AES_W_RST_BD_FT L"\033[22m"

#pragma push_macro("UWVM_AES_W_RST_ITALICS")
#undef UWVM_AES_W_RST_ITALICS
#define UWVM_AES_W_RST_ITALICS L"\033[23m"

#pragma push_macro("UWVM_AES_W_RST_UNDERLINE")
#undef UWVM_AES_W_RST_UNDERLINE
#define UWVM_AES_W_RST_UNDERLINE L"\033[24m"

#pragma push_macro("UWVM_AES_W_RST_BLINK")
#undef UWVM_AES_W_RST_BLINK
#define UWVM_AES_W_RST_BLINK L"\033[25m"

#pragma push_macro("UWVM_AES_W_RST_INVERSE")
#undef UWVM_AES_W_RST_INVERSE
#define UWVM_AES_W_RST_INVERSE L"\033[27m"

#pragma push_macro("UWVM_AES_W_RST_INVISIABLE")
#undef UWVM_AES_W_RST_INVISIABLE
#define UWVM_AES_W_RST_INVISIABLE L"\033[28m"

#pragma push_macro("UWVM_AES_W_RST_CROSS")
#undef UWVM_AES_W_RST_CROSS
#define UWVM_AES_W_RST_CROSS L"\033[29m"

/// @brief Control sequences variables

/// @brief Hide cursor
#pragma push_macro("UWVM_AES_W_CTRL_HCRS")
#undef UWVM_AES_W_CTRL_HCRS
#define UWVM_AES_W_CTRL_HCRS L"\033[?25l"

/// @brief Show cursor
#pragma push_macro("UWVM_AES_W_CTRL_SCRS")
#undef UWVM_AES_W_CTRL_SCRS
#define UWVM_AES_W_CTRL_SCRS L"\033[?25h"

/// @brief RGB
#pragma push_macro("UWVM_AES_W_RGB")
#undef UWVM_AES_W_RGB
#define UWVM_AES_W_RGB(r, g, b) L"\033[38;2;" #r L";" #g L";" #b L"m"

///
/// UTF-8
///
#pragma push_macro("UWVM_AES_U8_BLACK")
#undef UWVM_AES_U8_BLACK
#define UWVM_AES_U8_BLACK u8"\033[30m"

#pragma push_macro("UWVM_AES_U8_RED")
#undef UWVM_AES_U8_RED
#define UWVM_AES_U8_RED u8"\033[31m"

#pragma push_macro("UWVM_AES_U8_GREEN")
#undef UWVM_AES_U8_GREEN
#define UWVM_AES_U8_GREEN u8"\033[32m"

#pragma push_macro("UWVM_AES_U8_ORANGE")
#undef UWVM_AES_U8_ORANGE
#define UWVM_AES_U8_ORANGE u8"\033[33m"

#pragma push_macro("UWVM_AES_U8_BLUE")
#undef UWVM_AES_U8_BLUE
#define UWVM_AES_U8_BLUE u8"\033[34m"

#pragma push_macro("UWVM_AES_U8_PURPLE")
#undef UWVM_AES_U8_PURPLE
#define UWVM_AES_U8_PURPLE u8"\033[35m"

#pragma push_macro("UWVM_AES_U8_CYAN")
#undef UWVM_AES_U8_CYAN
#define UWVM_AES_U8_CYAN u8"\033[36m"

#pragma push_macro("UWVM_AES_U8_GRAY")
#undef UWVM_AES_U8_GRAY
#define UWVM_AES_U8_GRAY u8"\033[37m"

#pragma push_macro("UWVM_AES_U8_DK_GRAY")
#undef UWVM_AES_U8_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_U8_DK_GRAY UWVM_AES_U8_BLACK
#else
# define UWVM_AES_U8_DK_GRAY u8"\033[90m"
#endif

#pragma push_macro("UWVM_AES_U8_LT_RED")
#undef UWVM_AES_U8_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_U8_LT_RED UWVM_AES_U8_RED
#else
# define UWVM_AES_U8_LT_RED u8"\033[91m"
#endif

#pragma push_macro("UWVM_AES_U8_LT_GREEN")
#undef UWVM_AES_U8_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_U8_LT_GREEN UWVM_AES_U8_GREEN
#else
# define UWVM_AES_U8_LT_GREEN u8"\033[92m"
#endif

#pragma push_macro("UWVM_AES_U8_YELLOW")
#undef UWVM_AES_U8_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_U8_YELLOW UWVM_AES_U8_ORANGE
#else
# define UWVM_AES_U8_YELLOW u8"\033[93m"
#endif

#pragma push_macro("UWVM_AES_U8_LT_BLUE")
#undef UWVM_AES_U8_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_U8_LT_BLUE UWVM_AES_U8_BLUE
#else
# define UWVM_AES_U8_LT_BLUE u8"\033[94m"
#endif

#pragma push_macro("UWVM_AES_U8_LT_PURPLE")
#undef UWVM_AES_U8_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_U8_LT_PURPLE UWVM_AES_U8_PURPLE
#else
# define UWVM_AES_U8_LT_PURPLE u8"\033[95m"
#endif

#pragma push_macro("UWVM_AES_U8_LT_CYAN")
#undef UWVM_AES_U8_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_U8_LT_CYAN UWVM_AES_U8_CYAN
#else
# define UWVM_AES_U8_LT_CYAN u8"\033[96m"
#endif

#pragma push_macro("UWVM_AES_U8_WHITE")
#undef UWVM_AES_U8_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_U8_WHITE UWVM_AES_U8_GRAY
#else
# define UWVM_AES_U8_WHITE u8"\033[97m"
#endif

/// @brief Background color variables:

#pragma push_macro("UWVM_AES_U8_BG_BLACK")
#undef UWVM_AES_U8_BG_BLACK
#define UWVM_AES_U8_BG_BLACK u8"\033[40m"

#pragma push_macro("UWVM_AES_U8_BG_RED")
#undef UWVM_AES_U8_BG_RED
#define UWVM_AES_U8_BG_RED u8"\033[41m"

#pragma push_macro("UWVM_AES_U8_BG_GREEN")
#undef UWVM_AES_U8_BG_GREEN
#define UWVM_AES_U8_BG_GREEN u8"\033[42m"

#pragma push_macro("UWVM_AES_U8_BG_ORANGE")
#undef UWVM_AES_U8_BG_ORANGE
#define UWVM_AES_U8_BG_ORANGE u8"\033[43m"

#pragma push_macro("UWVM_AES_U8_BG_BLUE")
#undef UWVM_AES_U8_BG_BLUE
#define UWVM_AES_U8_BG_BLUE u8"\033[44m"

#pragma push_macro("UWVM_AES_U8_BG_PURPLE")
#undef UWVM_AES_U8_BG_PURPLE
#define UWVM_AES_U8_BG_PURPLE u8"\033[45m"

#pragma push_macro("UWVM_AES_U8_BG_CYAN")
#undef UWVM_AES_U8_BG_CYAN
#define UWVM_AES_U8_BG_CYAN u8"\033[46m"

#pragma push_macro("UWVM_AES_U8_BG_GRAY")
#undef UWVM_AES_U8_BG_GRAY
#define UWVM_AES_U8_BG_GRAY u8"\033[47m"

#pragma push_macro("UWVM_AES_U8_BG_DK_GRAY")
#undef UWVM_AES_U8_BG_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_DK_GRAY UWVM_AES_U8_BG_BLACK
#else
# define UWVM_AES_U8_BG_DK_GRAY u8"\033[100m"
#endif

#pragma push_macro("UWVM_AES_U8_BG_LT_RED")
#undef UWVM_AES_U8_BG_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_LT_RED UWVM_AES_U8_BG_RED
#else
# define UWVM_AES_U8_BG_LT_RED u8"\033[101m"
#endif

#pragma push_macro("UWVM_AES_U8_BG_LT_GREEN")
#undef UWVM_AES_U8_BG_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_LT_GREEN UWVM_AES_U8_BG_GREEN
#else
# define UWVM_AES_U8_BG_LT_GREEN u8"\033[102m"
#endif

#pragma push_macro("UWVM_AES_U8_BG_YELLOW")
#undef UWVM_AES_U8_BG_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_YELLOW UWVM_AES_U8_BG_ORANGE
#else
# define UWVM_AES_U8_BG_YELLOW u8"\033[103m"
#endif

#pragma push_macro("UWVM_AES_U8_BG_LT_BLUE")
#undef UWVM_AES_U8_BG_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_LT_BLUE UWVM_AES_U8_BG_BLUE
#else
# define UWVM_AES_U8_BG_LT_BLUE u8"\033[104m"
#endif

#pragma push_macro("UWVM_AES_U8_BG_LT_PURPLE")
#undef UWVM_AES_U8_BG_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_LT_PURPLE UWVM_AES_U8_BG_PURPLE
#else
# define UWVM_AES_U8_BG_LT_PURPLE u8"\033[105m"
#endif

#pragma push_macro("UWVM_AES_U8_BG_LT_CYAN")
#undef UWVM_AES_U8_BG_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_LT_CYAN UWVM_AES_U8_BG_CYAN
#else
# define UWVM_AES_U8_BG_LT_CYAN u8"\033[106m"
#endif

#pragma push_macro("UWVM_AES_U8_BG_WHITE")
#undef UWVM_AES_U8_BG_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_U8_BG_WHITE UWVM_AES_U8_BG_GRAY
#else
# define UWVM_AES_U8_BG_WHITE u8"\033[107m"
#endif

/// @brief Bold color variables:
#pragma push_macro("UWVM_AES_U8_BD_BLACK")
#undef UWVM_AES_U8_BD_BLACK
#define UWVM_AES_U8_BD_BLACK u8"\033[1;30m"

#pragma push_macro("UWVM_AES_U8_BD_RED")
#undef UWVM_AES_U8_BD_RED
#define UWVM_AES_U8_BD_RED u8"\033[1;31m"

#pragma push_macro("UWVM_AES_U8_BD_GREEN")
#undef UWVM_AES_U8_BD_GREEN
#define UWVM_AES_U8_BD_GREEN u8"\033[1;32m"

#pragma push_macro("UWVM_AES_U8_BD_ORANGE")
#undef UWVM_AES_U8_BD_ORANGE
#define UWVM_AES_U8_BD_ORANGE u8"\033[1;33m"

#pragma push_macro("UWVM_AES_U8_BD_BLUE")
#undef UWVM_AES_U8_BD_BLUE
#define UWVM_AES_U8_BD_BLUE u8"\033[1;34m"

#pragma push_macro("UWVM_AES_U8_BD_PURPLE")
#undef UWVM_AES_U8_BD_PURPLE
#define UWVM_AES_U8_BD_PURPLE u8"\033[1;35m"

#pragma push_macro("UWVM_AES_U8_BD_CYAN")
#undef UWVM_AES_U8_BD_CYAN
#define UWVM_AES_U8_BD_CYAN u8"\033[1;36m"

#pragma push_macro("UWVM_AES_U8_BD_GRAY")
#undef UWVM_AES_U8_BD_GRAY
#define UWVM_AES_U8_BD_GRAY u8"\033[1;37m"

/// @brief Style variables:

#pragma push_macro("UWVM_AES_U8_STY_BOLD")
#undef UWVM_AES_U8_STY_BOLD
#define UWVM_AES_U8_STY_BOLD u8"\033[1m"

#pragma push_macro("UWVM_AES_U8_STY_FAINT")
#undef UWVM_AES_U8_STY_FAINT
#define UWVM_AES_U8_STY_FAINT u8"\033[2m"

#pragma push_macro("UWVM_AES_U8_STY_ITALICS")
#undef UWVM_AES_U8_STY_ITALICS
#define UWVM_AES_U8_STY_ITALICS u8"\033[3m"

#pragma push_macro("UWVM_AES_U8_STY_UNDERLINE")
#undef UWVM_AES_U8_STY_UNDERLINE
#define UWVM_AES_U8_STY_UNDERLINE u8"\033[4m"

#pragma push_macro("UWVM_AES_U8_STY_BLINK")
#undef UWVM_AES_U8_STY_BLINK
#define UWVM_AES_U8_STY_BLINK u8"\033[5m"

#pragma push_macro("UWVM_AES_U8_STY_INVERSE")
#undef UWVM_AES_U8_STY_INVERSE
#define UWVM_AES_U8_STY_INVERSE u8"\033[7m"

#pragma push_macro("UWVM_AES_U8_STY_INVISIABLE")
#undef UWVM_AES_U8_STY_INVISIABLE
#define UWVM_AES_U8_STY_INVISIABLE u8"\033[8m"

#pragma push_macro("UWVM_AES_U8_STY_CROSSED")
#undef UWVM_AES_U8_STY_CROSSED
#define UWVM_AES_U8_STY_CROSSED u8"\033[9m"

#pragma push_macro("UWVM_AES_U8_STY_DOUBLE_UNDERLINE")
#undef UWVM_AES_U8_STY_DOUBLE_UNDERLINE
#define UWVM_AES_U8_STY_DOUBLE_UNDERLINE u8"\033[21m"

/// @brief Reset features commands

#pragma push_macro("UWVM_AES_U8_RST_ALL")
#undef UWVM_AES_U8_RST_ALL
#define UWVM_AES_U8_RST_ALL u8"\033[0m"

#pragma push_macro("UWVM_AES_U8_RST_COLOR")
#undef UWVM_AES_U8_RST_COLOR
#define UWVM_AES_U8_RST_COLOR u8"\033[39m"

#pragma push_macro("UWVM_AES_U8_RST_BG_COLOR")
#undef UWVM_AES_U8_RST_BG_COLOR
#define UWVM_AES_U8_RST_BG_COLOR u8"\033[49m"

#pragma push_macro("UWVM_AES_U8_RST_BD_COLOR")
#undef UWVM_AES_U8_RST_BD_COLOR
#define UWVM_AES_U8_RST_BD_COLOR u8"\033[22m\033[39m"

#pragma push_macro("UWVM_AES_U8_RST_BD_FT")
#undef UWVM_AES_U8_RST_BD_FT
#define UWVM_AES_U8_RST_BD_FT u8"\033[22m"

#pragma push_macro("UWVM_AES_U8_RST_ITALICS")
#undef UWVM_AES_U8_RST_ITALICS
#define UWVM_AES_U8_RST_ITALICS u8"\033[23m"

#pragma push_macro("UWVM_AES_U8_RST_UNDERLINE")
#undef UWVM_AES_U8_RST_UNDERLINE
#define UWVM_AES_U8_RST_UNDERLINE u8"\033[24m"

#pragma push_macro("UWVM_AES_U8_RST_BLINK")
#undef UWVM_AES_U8_RST_BLINK
#define UWVM_AES_U8_RST_BLINK u8"\033[25m"

#pragma push_macro("UWVM_AES_U8_RST_INVERSE")
#undef UWVM_AES_U8_RST_INVERSE
#define UWVM_AES_U8_RST_INVERSE u8"\033[27m"

#pragma push_macro("UWVM_AES_U8_RST_INVISIABLE")
#undef UWVM_AES_U8_RST_INVISIABLE
#define UWVM_AES_U8_RST_INVISIABLE u8"\033[28m"

#pragma push_macro("UWVM_AES_U8_RST_CROSS")
#undef UWVM_AES_U8_RST_CROSS
#define UWVM_AES_U8_RST_CROSS u8"\033[29m"

/// @brief Control sequences variables

/// @brief Hide cursor
#pragma push_macro("UWVM_AES_U8_CTRL_HCRS")
#undef UWVM_AES_U8_CTRL_HCRS
#define UWVM_AES_U8_CTRL_HCRS u8"\033[?25l"

/// @brief Show cursor
#pragma push_macro("UWVM_AES_U8_CTRL_SCRS")
#undef UWVM_AES_U8_CTRL_SCRS
#define UWVM_AES_U8_CTRL_SCRS u8"\033[?25h"

/// @brief RGB
#pragma push_macro("UWVM_AES_U8_RGB")
#undef UWVM_AES_U8_RGB
#define UWVM_AES_U8_RGB(r, g, b) u8"\033[38;2;" #r u8";" #g u8";" #b u8"m"

///
/// UTF-16
///
#pragma push_macro("UWVM_AES_U16_BLACK")
#undef UWVM_AES_U16_BLACK
#define UWVM_AES_U16_BLACK u"\033[30m"

#pragma push_macro("UWVM_AES_U16_RED")
#undef UWVM_AES_U16_RED
#define UWVM_AES_U16_RED u"\033[31m"

#pragma push_macro("UWVM_AES_U16_GREEN")
#undef UWVM_AES_U16_GREEN
#define UWVM_AES_U16_GREEN u"\033[32m"

#pragma push_macro("UWVM_AES_U16_ORANGE")
#undef UWVM_AES_U16_ORANGE
#define UWVM_AES_U16_ORANGE u"\033[33m"

#pragma push_macro("UWVM_AES_U16_BLUE")
#undef UWVM_AES_U16_BLUE
#define UWVM_AES_U16_BLUE u"\033[34m"

#pragma push_macro("UWVM_AES_U16_PURPLE")
#undef UWVM_AES_U16_PURPLE
#define UWVM_AES_U16_PURPLE u"\033[35m"

#pragma push_macro("UWVM_AES_U16_CYAN")
#undef UWVM_AES_U16_CYAN
#define UWVM_AES_U16_CYAN u"\033[36m"

#pragma push_macro("UWVM_AES_U16_GRAY")
#undef UWVM_AES_U16_GRAY
#define UWVM_AES_U16_GRAY u"\033[37m"

#pragma push_macro("UWVM_AES_U16_DK_GRAY")
#undef UWVM_AES_U16_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_U16_DK_GRAY UWVM_AES_U16_BLACK
#else
# define UWVM_AES_U16_DK_GRAY u"\033[90m"
#endif

#pragma push_macro("UWVM_AES_U16_LT_RED")
#undef UWVM_AES_U16_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_U16_LT_RED UWVM_AES_U16_RED
#else
# define UWVM_AES_U16_LT_RED u"\033[91m"
#endif

#pragma push_macro("UWVM_AES_U16_LT_GREEN")
#undef UWVM_AES_U16_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_U16_LT_GREEN UWVM_AES_U16_GREEN
#else
# define UWVM_AES_U16_LT_GREEN u"\033[92m"
#endif

#pragma push_macro("UWVM_AES_U16_YELLOW")
#undef UWVM_AES_U16_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_U16_YELLOW UWVM_AES_U16_ORANGE
#else
# define UWVM_AES_U16_YELLOW u"\033[93m"
#endif

#pragma push_macro("UWVM_AES_U16_LT_BLUE")
#undef UWVM_AES_U16_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_U16_LT_BLUE UWVM_AES_U16_BLUE
#else
# define UWVM_AES_U16_LT_BLUE u"\033[94m"
#endif

#pragma push_macro("UWVM_AES_U16_LT_PURPLE")
#undef UWVM_AES_U16_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_U16_LT_PURPLE UWVM_AES_U16_PURPLE
#else
# define UWVM_AES_U16_LT_PURPLE u"\033[95m"
#endif

#pragma push_macro("UWVM_AES_U16_LT_CYAN")
#undef UWVM_AES_U16_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_U16_LT_CYAN UWVM_AES_U16_CYAN
#else
# define UWVM_AES_U16_LT_CYAN u"\033[96m"
#endif

#pragma push_macro("UWVM_AES_U16_WHITE")
#undef UWVM_AES_U16_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_U16_WHITE UWVM_AES_U16_GRAY
#else
# define UWVM_AES_U16_WHITE u"\033[97m"
#endif

/// @brief Background color variables:

#pragma push_macro("UWVM_AES_U16_BG_BLACK")
#undef UWVM_AES_U16_BG_BLACK
#define UWVM_AES_U16_BG_BLACK u"\033[40m"

#pragma push_macro("UWVM_AES_U16_BG_RED")
#undef UWVM_AES_U16_BG_RED
#define UWVM_AES_U16_BG_RED u"\033[41m"

#pragma push_macro("UWVM_AES_U16_BG_GREEN")
#undef UWVM_AES_U16_BG_GREEN
#define UWVM_AES_U16_BG_GREEN u"\033[42m"

#pragma push_macro("UWVM_AES_U16_BG_ORANGE")
#undef UWVM_AES_U16_BG_ORANGE
#define UWVM_AES_U16_BG_ORANGE u"\033[43m"

#pragma push_macro("UWVM_AES_U16_BG_BLUE")
#undef UWVM_AES_U16_BG_BLUE
#define UWVM_AES_U16_BG_BLUE u"\033[44m"

#pragma push_macro("UWVM_AES_U16_BG_PURPLE")
#undef UWVM_AES_U16_BG_PURPLE
#define UWVM_AES_U16_BG_PURPLE u"\033[45m"

#pragma push_macro("UWVM_AES_U16_BG_CYAN")
#undef UWVM_AES_U16_BG_CYAN
#define UWVM_AES_U16_BG_CYAN u"\033[46m"

#pragma push_macro("UWVM_AES_U16_BG_GRAY")
#undef UWVM_AES_U16_BG_GRAY
#define UWVM_AES_U16_BG_GRAY u"\033[47m"

#pragma push_macro("UWVM_AES_U16_BG_DK_GRAY")
#undef UWVM_AES_U16_BG_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_DK_GRAY UWVM_AES_U16_BG_BLACK
#else
# define UWVM_AES_U16_BG_DK_GRAY u"\033[100m"
#endif

#pragma push_macro("UWVM_AES_U16_BG_LT_RED")
#undef UWVM_AES_U16_BG_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_LT_RED UWVM_AES_U16_BG_RED
#else
# define UWVM_AES_U16_BG_LT_RED u"\033[101m"
#endif

#pragma push_macro("UWVM_AES_U16_BG_LT_GREEN")
#undef UWVM_AES_U16_BG_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_LT_GREEN UWVM_AES_U16_BG_GREEN
#else
# define UWVM_AES_U16_BG_LT_GREEN u"\033[102m"
#endif

#pragma push_macro("UWVM_AES_U16_BG_YELLOW")
#undef UWVM_AES_U16_BG_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_YELLOW UWVM_AES_U16_BG_ORANGE
#else
# define UWVM_AES_U16_BG_YELLOW u"\033[103m"
#endif

#pragma push_macro("UWVM_AES_U16_BG_LT_BLUE")
#undef UWVM_AES_U16_BG_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_LT_BLUE UWVM_AES_U16_BG_BLUE
#else
# define UWVM_AES_U16_BG_LT_BLUE u"\033[104m"
#endif

#pragma push_macro("UWVM_AES_U16_BG_LT_PURPLE")
#undef UWVM_AES_U16_BG_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_LT_PURPLE UWVM_AES_U16_BG_PURPLE
#else
# define UWVM_AES_U16_BG_LT_PURPLE u"\033[105m"
#endif

#pragma push_macro("UWVM_AES_U16_BG_LT_CYAN")
#undef UWVM_AES_U16_BG_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_LT_CYAN UWVM_AES_U16_BG_CYAN
#else
# define UWVM_AES_U16_BG_LT_CYAN u"\033[106m"
#endif

#pragma push_macro("UWVM_AES_U16_BG_WHITE")
#undef UWVM_AES_U16_BG_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_U16_BG_WHITE UWVM_AES_U16_BG_GRAY
#else
# define UWVM_AES_U16_BG_WHITE u"\033[107m"
#endif

/// @brief Bold color variables:
#pragma push_macro("UWVM_AES_U16_BD_BLACK")
#undef UWVM_AES_U16_BD_BLACK
#define UWVM_AES_U16_BD_BLACK u"\033[1;30m"

#pragma push_macro("UWVM_AES_U16_BD_RED")
#undef UWVM_AES_U16_BD_RED
#define UWVM_AES_U16_BD_RED u"\033[1;31m"

#pragma push_macro("UWVM_AES_U16_BD_GREEN")
#undef UWVM_AES_U16_BD_GREEN
#define UWVM_AES_U16_BD_GREEN u"\033[1;32m"

#pragma push_macro("UWVM_AES_U16_BD_ORANGE")
#undef UWVM_AES_U16_BD_ORANGE
#define UWVM_AES_U16_BD_ORANGE u"\033[1;33m"

#pragma push_macro("UWVM_AES_U16_BD_BLUE")
#undef UWVM_AES_U16_BD_BLUE
#define UWVM_AES_U16_BD_BLUE u"\033[1;34m"

#pragma push_macro("UWVM_AES_U16_BD_PURPLE")
#undef UWVM_AES_U16_BD_PURPLE
#define UWVM_AES_U16_BD_PURPLE u"\033[1;35m"

#pragma push_macro("UWVM_AES_U16_BD_CYAN")
#undef UWVM_AES_U16_BD_CYAN
#define UWVM_AES_U16_BD_CYAN u"\033[1;36m"

#pragma push_macro("UWVM_AES_U16_BD_GRAY")
#undef UWVM_AES_U16_BD_GRAY
#define UWVM_AES_U16_BD_GRAY u"\033[1;37m"

/// @brief Style variables:

#pragma push_macro("UWVM_AES_U16_STY_BOLD")
#undef UWVM_AES_U16_STY_BOLD
#define UWVM_AES_U16_STY_BOLD u"\033[1m"

#pragma push_macro("UWVM_AES_U16_STY_FAINT")
#undef UWVM_AES_U16_STY_FAINT
#define UWVM_AES_U16_STY_FAINT u"\033[2m"

#pragma push_macro("UWVM_AES_U16_STY_ITALICS")
#undef UWVM_AES_U16_STY_ITALICS
#define UWVM_AES_U16_STY_ITALICS u"\033[3m"

#pragma push_macro("UWVM_AES_U16_STY_UNDERLINE")
#undef UWVM_AES_U16_STY_UNDERLINE
#define UWVM_AES_U16_STY_UNDERLINE u"\033[4m"

#pragma push_macro("UWVM_AES_U16_STY_BLINK")
#undef UWVM_AES_U16_STY_BLINK
#define UWVM_AES_U16_STY_BLINK u"\033[5m"

#pragma push_macro("UWVM_AES_U16_STY_INVERSE")
#undef UWVM_AES_U16_STY_INVERSE
#define UWVM_AES_U16_STY_INVERSE u"\033[7m"

#pragma push_macro("UWVM_AES_U16_STY_INVISIABLE")
#undef UWVM_AES_U16_STY_INVISIABLE
#define UWVM_AES_U16_STY_INVISIABLE u"\033[8m"

#pragma push_macro("UWVM_AES_U16_STY_CROSSED")
#undef UWVM_AES_U16_STY_CROSSED
#define UWVM_AES_U16_STY_CROSSED u"\033[9m"

#pragma push_macro("UWVM_AES_U16_STY_DOUBLE_UNDERLINE")
#undef UWVM_AES_U16_STY_DOUBLE_UNDERLINE
#define UWVM_AES_U16_STY_DOUBLE_UNDERLINE u"\033[21m"

/// @brief Reset features commands

#pragma push_macro("UWVM_AES_U16_RST_ALL")
#undef UWVM_AES_U16_RST_ALL
#define UWVM_AES_U16_RST_ALL u"\033[0m"

#pragma push_macro("UWVM_AES_U16_RST_COLOR")
#undef UWVM_AES_U16_RST_COLOR
#define UWVM_AES_U16_RST_COLOR u"\033[39m"

#pragma push_macro("UWVM_AES_U16_RST_BG_COLOR")
#undef UWVM_AES_U16_RST_BG_COLOR
#define UWVM_AES_U16_RST_BG_COLOR u"\033[49m"

#pragma push_macro("UWVM_AES_U16_RST_BD_COLOR")
#undef UWVM_AES_U16_RST_BD_COLOR
#define UWVM_AES_U16_RST_BD_COLOR u"\033[22m\033[39m"

#pragma push_macro("UWVM_AES_U16_RST_BD_FT")
#undef UWVM_AES_U16_RST_BD_FT
#define UWVM_AES_U16_RST_BD_FT u"\033[22m"

#pragma push_macro("UWVM_AES_U16_RST_ITALICS")
#undef UWVM_AES_U16_RST_ITALICS
#define UWVM_AES_U16_RST_ITALICS u"\033[23m"

#pragma push_macro("UWVM_AES_U16_RST_UNDERLINE")
#undef UWVM_AES_U16_RST_UNDERLINE
#define UWVM_AES_U16_RST_UNDERLINE u"\033[24m"

#pragma push_macro("UWVM_AES_U16_RST_BLINK")
#undef UWVM_AES_U16_RST_BLINK
#define UWVM_AES_U16_RST_BLINK u"\033[25m"

#pragma push_macro("UWVM_AES_U16_RST_INVERSE")
#undef UWVM_AES_U16_RST_INVERSE
#define UWVM_AES_U16_RST_INVERSE u"\033[27m"

#pragma push_macro("UWVM_AES_U16_RST_INVISIABLE")
#undef UWVM_AES_U16_RST_INVISIABLE
#define UWVM_AES_U16_RST_INVISIABLE u"\033[28m"

#pragma push_macro("UWVM_AES_U16_RST_CROSS")
#undef UWVM_AES_U16_RST_CROSS
#define UWVM_AES_U16_RST_CROSS u"\033[29m"

/// @brief Control sequences variables

/// @brief Hide cursor
#pragma push_macro("UWVM_AES_U16_CTRL_HCRS")
#undef UWVM_AES_U16_CTRL_HCRS
#define UWVM_AES_U16_CTRL_HCRS u"\033[?25l"

/// @brief Show cursor
#pragma push_macro("UWVM_AES_U16_CTRL_SCRS")
#undef UWVM_AES_U16_CTRL_SCRS
#define UWVM_AES_U16_CTRL_SCRS u"\033[?25h"

/// @brief RGB
#pragma push_macro("UWVM_AES_U16_RGB")
#undef UWVM_AES_U16_RGB
#define UWVM_AES_U16_RGB(r, g, b) u"\033[38;2;" #r u";" #g u";" #b u"m"

///
/// UTF-32
///
#pragma push_macro("UWVM_AES_U32_BLACK")
#undef UWVM_AES_U32_BLACK
#define UWVM_AES_U32_BLACK U"\033[30m"

#pragma push_macro("UWVM_AES_U32_RED")
#undef UWVM_AES_U32_RED
#define UWVM_AES_U32_RED U"\033[31m"

#pragma push_macro("UWVM_AES_U32_GREEN")
#undef UWVM_AES_U32_GREEN
#define UWVM_AES_U32_GREEN U"\033[32m"

#pragma push_macro("UWVM_AES_U32_ORANGE")
#undef UWVM_AES_U32_ORANGE
#define UWVM_AES_U32_ORANGE U"\033[33m"

#pragma push_macro("UWVM_AES_U32_BLUE")
#undef UWVM_AES_U32_BLUE
#define UWVM_AES_U32_BLUE U"\033[34m"

#pragma push_macro("UWVM_AES_U32_PURPLE")
#undef UWVM_AES_U32_PURPLE
#define UWVM_AES_U32_PURPLE U"\033[35m"

#pragma push_macro("UWVM_AES_U32_CYAN")
#undef UWVM_AES_U32_CYAN
#define UWVM_AES_U32_CYAN U"\033[36m"

#pragma push_macro("UWVM_AES_U32_GRAY")
#undef UWVM_AES_U32_GRAY
#define UWVM_AES_U32_GRAY U"\033[37m"

#pragma push_macro("UWVM_AES_U32_DK_GRAY")
#undef UWVM_AES_U32_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_U32_DK_GRAY UWVM_AES_U32_BLACK
#else
# define UWVM_AES_U32_DK_GRAY U"\033[90m"
#endif

#pragma push_macro("UWVM_AES_U32_LT_RED")
#undef UWVM_AES_U32_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_U32_LT_RED UWVM_AES_U32_RED
#else
# define UWVM_AES_U32_LT_RED U"\033[91m"
#endif

#pragma push_macro("UWVM_AES_U32_LT_GREEN")
#undef UWVM_AES_U32_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_U32_LT_GREEN UWVM_AES_U32_GREEN
#else
# define UWVM_AES_U32_LT_GREEN U"\033[92m"
#endif

#pragma push_macro("UWVM_AES_U32_YELLOW")
#undef UWVM_AES_U32_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_U32_YELLOW UWVM_AES_U32_ORANGE
#else
# define UWVM_AES_U32_YELLOW U"\033[93m"
#endif

#pragma push_macro("UWVM_AES_U32_LT_BLUE")
#undef UWVM_AES_U32_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_U32_LT_BLUE UWVM_AES_U32_BLUE
#else
# define UWVM_AES_U32_LT_BLUE U"\033[94m"
#endif

#pragma push_macro("UWVM_AES_U32_LT_PURPLE")
#undef UWVM_AES_U32_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_U32_LT_PURPLE UWVM_AES_U32_PURPLE
#else
# define UWVM_AES_U32_LT_PURPLE U"\033[95m"
#endif

#pragma push_macro("UWVM_AES_U32_LT_CYAN")
#undef UWVM_AES_U32_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_U32_LT_CYAN UWVM_AES_U32_CYAN
#else
# define UWVM_AES_U32_LT_CYAN U"\033[96m"
#endif

#pragma push_macro("UWVM_AES_U32_WHITE")
#undef UWVM_AES_U32_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_U32_WHITE UWVM_AES_U32_GRAY
#else
# define UWVM_AES_U32_WHITE U"\033[97m"
#endif

/// @brief Background color variables:

#pragma push_macro("UWVM_AES_U32_BG_BLACK")
#undef UWVM_AES_U32_BG_BLACK
#define UWVM_AES_U32_BG_BLACK U"\033[40m"

#pragma push_macro("UWVM_AES_U32_BG_RED")
#undef UWVM_AES_U32_BG_RED
#define UWVM_AES_U32_BG_RED U"\033[41m"

#pragma push_macro("UWVM_AES_U32_BG_GREEN")
#undef UWVM_AES_U32_BG_GREEN
#define UWVM_AES_U32_BG_GREEN U"\033[42m"

#pragma push_macro("UWVM_AES_U32_BG_ORANGE")
#undef UWVM_AES_U32_BG_ORANGE
#define UWVM_AES_U32_BG_ORANGE U"\033[43m"

#pragma push_macro("UWVM_AES_U32_BG_BLUE")
#undef UWVM_AES_U32_BG_BLUE
#define UWVM_AES_U32_BG_BLUE U"\033[44m"

#pragma push_macro("UWVM_AES_U32_BG_PURPLE")
#undef UWVM_AES_U32_BG_PURPLE
#define UWVM_AES_U32_BG_PURPLE U"\033[45m"

#pragma push_macro("UWVM_AES_U32_BG_CYAN")
#undef UWVM_AES_U32_BG_CYAN
#define UWVM_AES_U32_BG_CYAN U"\033[46m"

#pragma push_macro("UWVM_AES_U32_BG_GRAY")
#undef UWVM_AES_U32_BG_GRAY
#define UWVM_AES_U32_BG_GRAY U"\033[47m"

#pragma push_macro("UWVM_AES_U32_BG_DK_GRAY")
#undef UWVM_AES_U32_BG_DK_GRAY
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_DK_GRAY UWVM_AES_U32_BG_BLACK
#else
# define UWVM_AES_U32_BG_DK_GRAY U"\033[100m"
#endif

#pragma push_macro("UWVM_AES_U32_BG_LT_RED")
#undef UWVM_AES_U32_BG_LT_RED
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_LT_RED UWVM_AES_U32_BG_RED
#else
# define UWVM_AES_U32_BG_LT_RED U"\033[101m"
#endif

#pragma push_macro("UWVM_AES_U32_BG_LT_GREEN")
#undef UWVM_AES_U32_BG_LT_GREEN
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_LT_GREEN UWVM_AES_U32_BG_GREEN
#else
# define UWVM_AES_U32_BG_LT_GREEN U"\033[102m"
#endif

#pragma push_macro("UWVM_AES_U32_BG_YELLOW")
#undef UWVM_AES_U32_BG_YELLOW
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_YELLOW UWVM_AES_U32_BG_ORANGE
#else
# define UWVM_AES_U32_BG_YELLOW U"\033[103m"
#endif

#pragma push_macro("UWVM_AES_U32_BG_LT_BLUE")
#undef UWVM_AES_U32_BG_LT_BLUE
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_LT_BLUE UWVM_AES_U32_BG_BLUE
#else
# define UWVM_AES_U32_BG_LT_BLUE U"\033[104m"
#endif

#pragma push_macro("UWVM_AES_U32_BG_LT_PURPLE")
#undef UWVM_AES_U32_BG_LT_PURPLE
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_LT_PURPLE UWVM_AES_U32_BG_PURPLE
#else
# define UWVM_AES_U32_BG_LT_PURPLE U"\033[105m"
#endif

#pragma push_macro("UWVM_AES_U32_BG_LT_CYAN")
#undef UWVM_AES_U32_BG_LT_CYAN
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_LT_CYAN UWVM_AES_U32_BG_CYAN
#else
# define UWVM_AES_U32_BG_LT_CYAN U"\033[106m"
#endif

#pragma push_macro("UWVM_AES_U32_BG_WHITE")
#undef UWVM_AES_U32_BG_WHITE
#if defined(__DJGPP__)
# define UWVM_AES_U32_BG_WHITE UWVM_AES_U32_BG_GRAY
#else
# define UWVM_AES_U32_BG_WHITE U"\033[107m"
#endif

/// @brief Bold color variables:
#pragma push_macro("UWVM_AES_U32_BD_BLACK")
#undef UWVM_AES_U32_BD_BLACK
#define UWVM_AES_U32_BD_BLACK U"\033[1;30m"

#pragma push_macro("UWVM_AES_U32_BD_RED")
#undef UWVM_AES_U32_BD_RED
#define UWVM_AES_U32_BD_RED U"\033[1;31m"

#pragma push_macro("UWVM_AES_U32_BD_GREEN")
#undef UWVM_AES_U32_BD_GREEN
#define UWVM_AES_U32_BD_GREEN U"\033[1;32m"

#pragma push_macro("UWVM_AES_U32_BD_ORANGE")
#undef UWVM_AES_U32_BD_ORANGE
#define UWVM_AES_U32_BD_ORANGE U"\033[1;33m"

#pragma push_macro("UWVM_AES_U32_BD_BLUE")
#undef UWVM_AES_U32_BD_BLUE
#define UWVM_AES_U32_BD_BLUE U"\033[1;34m"

#pragma push_macro("UWVM_AES_U32_BD_PURPLE")
#undef UWVM_AES_U32_BD_PURPLE
#define UWVM_AES_U32_BD_PURPLE U"\033[1;35m"

#pragma push_macro("UWVM_AES_U32_BD_CYAN")
#undef UWVM_AES_U32_BD_CYAN
#define UWVM_AES_U32_BD_CYAN U"\033[1;36m"

#pragma push_macro("UWVM_AES_U32_BD_GRAY")
#undef UWVM_AES_U32_BD_GRAY
#define UWVM_AES_U32_BD_GRAY U"\033[1;37m"

/// @brief Style variables:

#pragma push_macro("UWVM_AES_U32_STY_BOLD")
#undef UWVM_AES_U32_STY_BOLD
#define UWVM_AES_U32_STY_BOLD U"\033[1m"

#pragma push_macro("UWVM_AES_U32_STY_FAINT")
#undef UWVM_AES_U32_STY_FAINT
#define UWVM_AES_U32_STY_FAINT U"\033[2m"

#pragma push_macro("UWVM_AES_U32_STY_ITALICS")
#undef UWVM_AES_U32_STY_ITALICS
#define UWVM_AES_U32_STY_ITALICS U"\033[3m"

#pragma push_macro("UWVM_AES_U32_STY_UNDERLINE")
#undef UWVM_AES_U32_STY_UNDERLINE
#define UWVM_AES_U32_STY_UNDERLINE U"\033[4m"

#pragma push_macro("UWVM_AES_U32_STY_BLINK")
#undef UWVM_AES_U32_STY_BLINK
#define UWVM_AES_U32_STY_BLINK U"\033[5m"

#pragma push_macro("UWVM_AES_U32_STY_INVERSE")
#undef UWVM_AES_U32_STY_INVERSE
#define UWVM_AES_U32_STY_INVERSE U"\033[7m"

#pragma push_macro("UWVM_AES_U32_STY_INVISIABLE")
#undef UWVM_AES_U32_STY_INVISIABLE
#define UWVM_AES_U32_STY_INVISIABLE U"\033[8m"

#pragma push_macro("UWVM_AES_U32_STY_CROSSED")
#undef UWVM_AES_U32_STY_CROSSED
#define UWVM_AES_U32_STY_CROSSED U"\033[9m"

#pragma push_macro("UWVM_AES_U32_STY_DOUBLE_UNDERLINE")
#undef UWVM_AES_U32_STY_DOUBLE_UNDERLINE
#define UWVM_AES_U32_STY_DOUBLE_UNDERLINE U"\033[21m"

/// @brief Reset features commands

#pragma push_macro("UWVM_AES_U32_RST_ALL")
#undef UWVM_AES_U32_RST_ALL
#define UWVM_AES_U32_RST_ALL U"\033[0m"

#pragma push_macro("UWVM_AES_U32_RST_COLOR")
#undef UWVM_AES_U32_RST_COLOR
#define UWVM_AES_U32_RST_COLOR U"\033[39m"

#pragma push_macro("UWVM_AES_U32_RST_BG_COLOR")
#undef UWVM_AES_U32_RST_BG_COLOR
#define UWVM_AES_U32_RST_BG_COLOR U"\033[49m"

#pragma push_macro("UWVM_AES_U32_RST_BD_COLOR")
#undef UWVM_AES_U32_RST_BD_COLOR
#define UWVM_AES_U32_RST_BD_COLOR U"\033[22m\033[39m"

#pragma push_macro("UWVM_AES_U32_RST_BD_FT")
#undef UWVM_AES_U32_RST_BD_FT
#define UWVM_AES_U32_RST_BD_FT U"\033[22m"

#pragma push_macro("UWVM_AES_U32_RST_ITALICS")
#undef UWVM_AES_U32_RST_ITALICS
#define UWVM_AES_U32_RST_ITALICS U"\033[23m"

#pragma push_macro("UWVM_AES_U32_RST_UNDERLINE")
#undef UWVM_AES_U32_RST_UNDERLINE
#define UWVM_AES_U32_RST_UNDERLINE U"\033[24m"

#pragma push_macro("UWVM_AES_U32_RST_BLINK")
#undef UWVM_AES_U32_RST_BLINK
#define UWVM_AES_U32_RST_BLINK U"\033[25m"

#pragma push_macro("UWVM_AES_U32_RST_INVERSE")
#undef UWVM_AES_U32_RST_INVERSE
#define UWVM_AES_U32_RST_INVERSE U"\033[27m"

#pragma push_macro("UWVM_AES_U32_RST_INVISIABLE")
#undef UWVM_AES_U32_RST_INVISIABLE
#define UWVM_AES_U32_RST_INVISIABLE U"\033[28m"

#pragma push_macro("UWVM_AES_U32_RST_CROSS")
#undef UWVM_AES_U32_RST_CROSS
#define UWVM_AES_U32_RST_CROSS U"\033[29m"

/// @brief Control sequences variables

/// @brief Hide cursor
#pragma push_macro("UWVM_AES_U32_CTRL_HCRS")
#undef UWVM_AES_U32_CTRL_HCRS
#define UWVM_AES_U32_CTRL_HCRS U"\033[?25l"

/// @brief Show cursor
#pragma push_macro("UWVM_AES_U32_CTRL_SCRS")
#undef UWVM_AES_U32_CTRL_SCRS
#define UWVM_AES_U32_CTRL_SCRS U"\033[?25h"

/// @brief RGB
#pragma push_macro("UWVM_AES_U32_RGB")
#undef UWVM_AES_U32_RGB
#define UWVM_AES_U32_RGB(r, g, b) U"\033[38;2;" #r U";" #g U";" #b U"m"
