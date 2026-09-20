/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        dwarf_eh_frame_registration.h
 * @brief       Platform unwind registration declarations used by the LLVM section manager.
 * @copyright   APL-2.0 License
 */

#pragma once

#if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && !defined(__arm__) && !defined(__thumb__) && __has_include(<unwind.h>)
extern "C" void __register_frame(void const*);
extern "C" void __deregister_frame(void const*);
#endif
