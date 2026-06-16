/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        uwvm_color_pop_macro.h
 * @brief       Restores UWVM diagnostic color macros saved by `uwvm_color_push_macro.h`.
 * @details     This header is the required counterpart to `uwvm_color_push_macro.h`.  It pops the `UWVM_COLOR_*` macro
 *              family in reverse definition order, restoring any definitions that were active before the push header was
 *              included.  The reverse order keeps nested include scopes well-behaved and prevents UWVM's temporary color
 *              macros from leaking into unrelated translation units.
 *
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-24
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

// #pragma once

#ifdef UWVM

// Restore UTF-32 color macros first because they were pushed last.
# pragma pop_macro("UWVM_COLOR_U32_RGB")
# pragma pop_macro("UWVM_COLOR_U32_WHITE")
# pragma pop_macro("UWVM_COLOR_U32_LT_CYAN")
# pragma pop_macro("UWVM_COLOR_U32_LT_PURPLE")
# pragma pop_macro("UWVM_COLOR_U32_LT_BLUE")
# pragma pop_macro("UWVM_COLOR_U32_YELLOW")
# pragma pop_macro("UWVM_COLOR_U32_LT_GREEN")
# pragma pop_macro("UWVM_COLOR_U32_LT_RED")
# pragma pop_macro("UWVM_COLOR_U32_DK_GRAY")
# pragma pop_macro("UWVM_COLOR_U32_GRAY")
# pragma pop_macro("UWVM_COLOR_U32_CYAN")
# pragma pop_macro("UWVM_COLOR_U32_PURPLE")
# pragma pop_macro("UWVM_COLOR_U32_BLUE")
# pragma pop_macro("UWVM_COLOR_U32_ORANGE")
# pragma pop_macro("UWVM_COLOR_U32_GREEN")
# pragma pop_macro("UWVM_COLOR_U32_RED")
# pragma pop_macro("UWVM_COLOR_U32_BLACK")
# pragma pop_macro("UWVM_COLOR_U32_RST_ALL_AND_SET_PURPLE")
# pragma pop_macro("UWVM_COLOR_U32_RST_ALL_AND_SET_WHITE")
# pragma pop_macro("UWVM_COLOR_U32_RST_ALL")

// Restore UTF-16 color macros.
# pragma pop_macro("UWVM_COLOR_U16_RGB")
# pragma pop_macro("UWVM_COLOR_U16_WHITE")
# pragma pop_macro("UWVM_COLOR_U16_LT_CYAN")
# pragma pop_macro("UWVM_COLOR_U16_LT_PURPLE")
# pragma pop_macro("UWVM_COLOR_U16_LT_BLUE")
# pragma pop_macro("UWVM_COLOR_U16_YELLOW")
# pragma pop_macro("UWVM_COLOR_U16_LT_GREEN")
# pragma pop_macro("UWVM_COLOR_U16_LT_RED")
# pragma pop_macro("UWVM_COLOR_U16_DK_GRAY")
# pragma pop_macro("UWVM_COLOR_U16_GRAY")
# pragma pop_macro("UWVM_COLOR_U16_CYAN")
# pragma pop_macro("UWVM_COLOR_U16_PURPLE")
# pragma pop_macro("UWVM_COLOR_U16_BLUE")
# pragma pop_macro("UWVM_COLOR_U16_ORANGE")
# pragma pop_macro("UWVM_COLOR_U16_GREEN")
# pragma pop_macro("UWVM_COLOR_U16_RED")
# pragma pop_macro("UWVM_COLOR_U16_BLACK")
# pragma pop_macro("UWVM_COLOR_U16_RST_ALL_AND_SET_PURPLE")
# pragma pop_macro("UWVM_COLOR_U16_RST_ALL_AND_SET_WHITE")
# pragma pop_macro("UWVM_COLOR_U16_RST_ALL")

// Restore UTF-8 color macros.
# pragma pop_macro("UWVM_COLOR_U8_RGB")
# pragma pop_macro("UWVM_COLOR_U8_WHITE")
# pragma pop_macro("UWVM_COLOR_U8_LT_CYAN")
# pragma pop_macro("UWVM_COLOR_U8_LT_PURPLE")
# pragma pop_macro("UWVM_COLOR_U8_LT_BLUE")
# pragma pop_macro("UWVM_COLOR_U8_YELLOW")
# pragma pop_macro("UWVM_COLOR_U8_LT_GREEN")
# pragma pop_macro("UWVM_COLOR_U8_LT_RED")
# pragma pop_macro("UWVM_COLOR_U8_DK_GRAY")
# pragma pop_macro("UWVM_COLOR_U8_GRAY")
# pragma pop_macro("UWVM_COLOR_U8_CYAN")
# pragma pop_macro("UWVM_COLOR_U8_PURPLE")
# pragma pop_macro("UWVM_COLOR_U8_BLUE")
# pragma pop_macro("UWVM_COLOR_U8_ORANGE")
# pragma pop_macro("UWVM_COLOR_U8_GREEN")
# pragma pop_macro("UWVM_COLOR_U8_RED")
# pragma pop_macro("UWVM_COLOR_U8_BLACK")
# pragma pop_macro("UWVM_COLOR_U8_RST_ALL_AND_SET_PURPLE")
# pragma pop_macro("UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE")
# pragma pop_macro("UWVM_COLOR_U8_RST_ALL")

// Restore wide-character color macros.
# pragma pop_macro("UWVM_COLOR_W_RGB")
# pragma pop_macro("UWVM_COLOR_W_WHITE")
# pragma pop_macro("UWVM_COLOR_W_LT_CYAN")
# pragma pop_macro("UWVM_COLOR_W_LT_PURPLE")
# pragma pop_macro("UWVM_COLOR_W_LT_BLUE")
# pragma pop_macro("UWVM_COLOR_W_YELLOW")
# pragma pop_macro("UWVM_COLOR_W_LT_GREEN")
# pragma pop_macro("UWVM_COLOR_W_LT_RED")
# pragma pop_macro("UWVM_COLOR_W_DK_GRAY")
# pragma pop_macro("UWVM_COLOR_W_GRAY")
# pragma pop_macro("UWVM_COLOR_W_CYAN")
# pragma pop_macro("UWVM_COLOR_W_PURPLE")
# pragma pop_macro("UWVM_COLOR_W_BLUE")
# pragma pop_macro("UWVM_COLOR_W_ORANGE")
# pragma pop_macro("UWVM_COLOR_W_GREEN")
# pragma pop_macro("UWVM_COLOR_W_RED")
# pragma pop_macro("UWVM_COLOR_W_BLACK")
# pragma pop_macro("UWVM_COLOR_W_RST_ALL_AND_SET_PURPLE")
# pragma pop_macro("UWVM_COLOR_W_RST_ALL_AND_SET_WHITE")
# pragma pop_macro("UWVM_COLOR_W_RST_ALL")

// Restore narrow-character color macros last because they were pushed first.
# pragma pop_macro("UWVM_COLOR_RGB")
# pragma pop_macro("UWVM_COLOR_WHITE")
# pragma pop_macro("UWVM_COLOR_LT_CYAN")
# pragma pop_macro("UWVM_COLOR_LT_PURPLE")
# pragma pop_macro("UWVM_COLOR_LT_BLUE")
# pragma pop_macro("UWVM_COLOR_YELLOW")
# pragma pop_macro("UWVM_COLOR_LT_GREEN")
# pragma pop_macro("UWVM_COLOR_LT_RED")
# pragma pop_macro("UWVM_COLOR_DK_GRAY")
# pragma pop_macro("UWVM_COLOR_GRAY")
# pragma pop_macro("UWVM_COLOR_CYAN")
# pragma pop_macro("UWVM_COLOR_PURPLE")
# pragma pop_macro("UWVM_COLOR_BLUE")
# pragma pop_macro("UWVM_COLOR_ORANGE")
# pragma pop_macro("UWVM_COLOR_GREEN")
# pragma pop_macro("UWVM_COLOR_RED")
# pragma pop_macro("UWVM_COLOR_BLACK")
# pragma pop_macro("UWVM_COLOR_RST_ALL_AND_SET_PURPLE")
# pragma pop_macro("UWVM_COLOR_RST_ALL_AND_SET_WHITE")
# pragma pop_macro("UWVM_COLOR_RST_ALL")

# include <uwvm2/utils/ansies/win32_text_attr_pop_macro.h>
# include <uwvm2/utils/ansies/ansi_pop_macro.h>

#endif
