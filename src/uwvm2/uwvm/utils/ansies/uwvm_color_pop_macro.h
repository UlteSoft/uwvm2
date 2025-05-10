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
/// @details    Use `pop_macro` to avoid side effects on existing macros. Please use `pop_macro` in conjunction.

// #pragma once

///
/// UTF-32
///
#pragma pop_macro("UWVM_COLOR_U32_RGB")
#pragma pop_macro("UWVM_COLOR_U32_WHITE")
#pragma pop_macro("UWVM_COLOR_U32_LT_CYAN")
#pragma pop_macro("UWVM_COLOR_U32_LT_PURPLE")
#pragma pop_macro("UWVM_COLOR_U32_LT_BLUE")
#pragma pop_macro("UWVM_COLOR_U32_YELLOW")
#pragma pop_macro("UWVM_COLOR_U32_LT_GREEN")
#pragma pop_macro("UWVM_COLOR_U32_LT_RED")
#pragma pop_macro("UWVM_COLOR_U32_DK_GRAY")
#pragma pop_macro("UWVM_COLOR_U32_GRAY")
#pragma pop_macro("UWVM_COLOR_U32_CYAN")
#pragma pop_macro("UWVM_COLOR_U32_PURPLE")
#pragma pop_macro("UWVM_COLOR_U32_BLUE")
#pragma pop_macro("UWVM_COLOR_U32_ORANGE")
#pragma pop_macro("UWVM_COLOR_U32_GREEN")
#pragma pop_macro("UWVM_COLOR_U32_RED")
#pragma pop_macro("UWVM_COLOR_U32_BLACK")
#pragma pop_macro("UWVM_COLOR_U32_RST_ALL_AND_SET_WHITE")
#pragma pop_macro("UWVM_COLOR_U32_RST_ALL")

///
/// UTF-16
///
#pragma pop_macro("UWVM_COLOR_U16_RGB")
#pragma pop_macro("UWVM_COLOR_U16_WHITE")
#pragma pop_macro("UWVM_COLOR_U16_LT_CYAN")
#pragma pop_macro("UWVM_COLOR_U16_LT_PURPLE")
#pragma pop_macro("UWVM_COLOR_U16_LT_BLUE")
#pragma pop_macro("UWVM_COLOR_U16_YELLOW")
#pragma pop_macro("UWVM_COLOR_U16_LT_GREEN")
#pragma pop_macro("UWVM_COLOR_U16_LT_RED")
#pragma pop_macro("UWVM_COLOR_U16_DK_GRAY")
#pragma pop_macro("UWVM_COLOR_U16_GRAY")
#pragma pop_macro("UWVM_COLOR_U16_CYAN")
#pragma pop_macro("UWVM_COLOR_U16_PURPLE")
#pragma pop_macro("UWVM_COLOR_U16_BLUE")
#pragma pop_macro("UWVM_COLOR_U16_ORANGE")
#pragma pop_macro("UWVM_COLOR_U16_GREEN")
#pragma pop_macro("UWVM_COLOR_U16_RED")
#pragma pop_macro("UWVM_COLOR_U16_BLACK")
#pragma pop_macro("UWVM_COLOR_U16_RST_ALL_AND_SET_WHITE")
#pragma pop_macro("UWVM_COLOR_U16_RST_ALL")

///
/// UTF-8
///
#pragma pop_macro("UWVM_COLOR_U8_RGB")
#pragma pop_macro("UWVM_COLOR_U8_WHITE")
#pragma pop_macro("UWVM_COLOR_U8_LT_CYAN")
#pragma pop_macro("UWVM_COLOR_U8_LT_PURPLE")
#pragma pop_macro("UWVM_COLOR_U8_LT_BLUE")
#pragma pop_macro("UWVM_COLOR_U8_YELLOW")
#pragma pop_macro("UWVM_COLOR_U8_LT_GREEN")
#pragma pop_macro("UWVM_COLOR_U8_LT_RED")
#pragma pop_macro("UWVM_COLOR_U8_DK_GRAY")
#pragma pop_macro("UWVM_COLOR_U8_GRAY")
#pragma pop_macro("UWVM_COLOR_U8_CYAN")
#pragma pop_macro("UWVM_COLOR_U8_PURPLE")
#pragma pop_macro("UWVM_COLOR_U8_BLUE")
#pragma pop_macro("UWVM_COLOR_U8_ORANGE")
#pragma pop_macro("UWVM_COLOR_U8_GREEN")
#pragma pop_macro("UWVM_COLOR_U8_RED")
#pragma pop_macro("UWVM_COLOR_U8_BLACK")
#pragma pop_macro("UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE")
#pragma pop_macro("UWVM_COLOR_U8_RST_ALL")

///
/// WCHAR
///
#pragma pop_macro("UWVM_COLOR_W_RGB")
#pragma pop_macro("UWVM_COLOR_W_WHITE")
#pragma pop_macro("UWVM_COLOR_W_LT_CYAN")
#pragma pop_macro("UWVM_COLOR_W_LT_PURPLE")
#pragma pop_macro("UWVM_COLOR_W_LT_BLUE")
#pragma pop_macro("UWVM_COLOR_W_YELLOW")
#pragma pop_macro("UWVM_COLOR_W_LT_GREEN")
#pragma pop_macro("UWVM_COLOR_W_LT_RED")
#pragma pop_macro("UWVM_COLOR_W_DK_GRAY")
#pragma pop_macro("UWVM_COLOR_W_GRAY")
#pragma pop_macro("UWVM_COLOR_W_CYAN")
#pragma pop_macro("UWVM_COLOR_W_PURPLE")
#pragma pop_macro("UWVM_COLOR_W_BLUE")
#pragma pop_macro("UWVM_COLOR_W_ORANGE")
#pragma pop_macro("UWVM_COLOR_W_GREEN")
#pragma pop_macro("UWVM_COLOR_W_RED")
#pragma pop_macro("UWVM_COLOR_W_BLACK")
#pragma pop_macro("UWVM_COLOR_W_RST_ALL_AND_SET_WHITE")
#pragma pop_macro("UWVM_COLOR_W_RST_ALL")

///
/// CHAR
///
#pragma pop_macro("UWVM_COLOR_RGB")
#pragma pop_macro("UWVM_COLOR_WHITE")
#pragma pop_macro("UWVM_COLOR_LT_CYAN")
#pragma pop_macro("UWVM_COLOR_LT_PURPLE")
#pragma pop_macro("UWVM_COLOR_LT_BLUE")
#pragma pop_macro("UWVM_COLOR_YELLOW")
#pragma pop_macro("UWVM_COLOR_LT_GREEN")
#pragma pop_macro("UWVM_COLOR_LT_RED")
#pragma pop_macro("UWVM_COLOR_DK_GRAY")
#pragma pop_macro("UWVM_COLOR_GRAY")
#pragma pop_macro("UWVM_COLOR_CYAN")
#pragma pop_macro("UWVM_COLOR_PURPLE")
#pragma pop_macro("UWVM_COLOR_BLUE")
#pragma pop_macro("UWVM_COLOR_ORANGE")
#pragma pop_macro("UWVM_COLOR_GREEN")
#pragma pop_macro("UWVM_COLOR_RED")
#pragma pop_macro("UWVM_COLOR_BLACK")
#pragma pop_macro("UWVM_COLOR_RST_ALL_AND_SET_WHITE")
#pragma pop_macro("UWVM_COLOR_RST_ALL")

#include <uwvm2/utils/ansies/win32_text_attr_pop_macro.h>
#include <uwvm2/utils/ansies/ansi_pop_macro.h>
