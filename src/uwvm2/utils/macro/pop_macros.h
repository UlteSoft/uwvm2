/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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

/// @brief      The following are the macros used by uwvm.
/// @details    Use `push_macro` to avoid side effects on existing macros. Please use `pop_macro` in conjunction.

// #pragma once

/// @brief      uwvm's macros
#pragma pop_macro("UWVM_SUPPORT_WEAK_SYMBOL")
#pragma pop_macro("UWVM_SUPPORT_MMAP")
#pragma pop_macro("UWVM_SUPPORT_PRELOAD_DL")
#pragma pop_macro("UWVM_CPP_EXCEPTIONS")
#pragma pop_macro("UWVM_CPP_RTTI")
#pragma pop_macro("UWVM_NO_SANITIZE")
#pragma pop_macro("UWVM_HAS_FEATURE")
#pragma pop_macro("UWVM_MUSTTAIL")
#pragma pop_macro("UWVM_GNU_NODEBUG")
#pragma pop_macro("UWVM_GNU_USED")
#pragma pop_macro("UWVM_SUPPORT_UNIX_PATH_SOCKET")
#pragma pop_macro("UWVM_NOT_SUPPORT_SPECIAL_CHAR")
#pragma pop_macro("UWVM_SUPPORT_INSTALL_PATH")
#pragma pop_macro("UWVM_GNU_MAY_ALIAS")
#pragma pop_macro("UWVM_IF_NOT_CONSTEVAL")
#pragma pop_macro("UWVM_IF_CONSTEVAL")
#pragma pop_macro("UWVM_GNU_HOT")
#pragma pop_macro("UWVM_GNU_COLD")
#pragma pop_macro("UWVM_THROWS")
#pragma pop_macro("UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE")
#pragma pop_macro("UWVM_REPLACEABLE_IF_ELIGIBLE")
#pragma pop_macro("UWVM_HAS_BUILTIN")
#pragma pop_macro("UWVM_ASSERT")
#pragma pop_macro("UWVM_GNU_RETURNS_NONNULL")
#pragma pop_macro("UWVM_GNU_MALLOC")
#pragma pop_macro("UWVM_GNU_ALWAYS_INLINE_ARTIFICIAL_CONST")
#pragma pop_macro("UWVM_GNU_ALWAYS_INLINE_ARTIFICIAL")
#pragma pop_macro("UWVM_GNU_ARTIFICIAL")
#pragma pop_macro("UWVM_ALWAYS_INLINE")
#pragma pop_macro("UWVM_GNU_PURE")
#pragma pop_macro("UWVM_GNU_CONST")
#pragma pop_macro("UWVM_WINFASTCALL_RENAME")
#pragma pop_macro("UWVM_WINFASTCALL")
#pragma pop_macro("UWVM_WINCDECL_RENAME")
#pragma pop_macro("UWVM_WINCDECL")
#pragma pop_macro("UWVM_WINSTDCALL_RENAME")
#pragma pop_macro("UWVM_WINSTDCALL")
#pragma pop_macro("UWVM_STDCALL")
#pragma pop_macro("UWVM_DLL_DLLIMPORT")
#pragma pop_macro("UWVM_DLLIMPORT")
#pragma pop_macro("UWVM_HAS_CPP_ATTRIBUTE")

/// @brief      std and windows.h macros
#pragma pop_macro("refresh")
#pragma pop_macro("new")
#pragma pop_macro("move")
#pragma pop_macro("min")
#pragma pop_macro("max")
#pragma pop_macro("interface")
#pragma pop_macro("erase")
