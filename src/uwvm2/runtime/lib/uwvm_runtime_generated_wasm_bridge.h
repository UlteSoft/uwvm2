/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

namespace uwvm2::runtime::lib::details
{
    /// Internal import bridge emitted only into generated Wasm code. This is deliberately separate from the public
    /// host/re-entry API: callers must already own the generated-bridge depth token, canonical LLVM-Wasm FP scope and,
    /// when selected, native-unwind execution gate. Native-provider callbacks suspend that token before invoking host code.
    extern "C++" void llvm_jit_call_raw_from_generated_wasm(void const* runtime_module_ptr,
                                                             ::std::uint_least32_t func_index,
                                                             void* result_buffer,
                                                             ::std::size_t result_bytes,
                                                             void const* param_buffer,
                                                             ::std::size_t param_bytes) noexcept;

    /// Exact machine-level ABI used by generated LLVM code. Keep every integer operand register-wide: some ABIs attach
    /// target-specific extension attributes even to uint32_t parameters/returns. The wrapper validates and narrows the
    /// Wasm function index before forwarding to the typed implementation above.
    extern "C++" void llvm_jit_call_raw_from_generated_wasm_abi_bridge(::std::uintptr_t runtime_module_address,
                                                                        ::std::uintptr_t func_index,
                                                                        ::std::uintptr_t result_buffer_address,
                                                                        ::std::size_t result_bytes,
                                                                        ::std::uintptr_t param_buffer_address,
                                                                        ::std::size_t param_bytes) noexcept;
}
