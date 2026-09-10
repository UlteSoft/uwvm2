/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-30
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
// std
# include <atomic>
# include <bit>
# include <climits>
# include <concepts>
# include <coroutine>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <limits>
# include <memory>
# include <mutex>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/Bitcode/BitcodeReader.h>
#  include <llvm/Bitcode/BitcodeWriter.h>
#  include <llvm/IR/Attributes.h>
#  include <llvm/IR/BasicBlock.h>
#  include <llvm/IR/CallingConv.h>
#  include <llvm/IR/Constants.h>
#  include <llvm/IR/Function.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/InlineAsm.h>
#  include <llvm/IR/Intrinsics.h>
#  include <llvm/IR/LLVMContext.h>
#  include <llvm/IR/Metadata.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/Type.h>
#  include <llvm/IR/Value.h>
#  include <llvm/IR/Verifier.h>
#  include <llvm/Linker/Linker.h>
#  include <llvm/Support/DynamicLibrary.h>
# endif
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/call_indirect_immediate.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/object/memory/flags/impl.h>
# include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifndef UWVM_MODULE
# include <uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h>
# include <uwvm2/runtime/lib/uwvm_runtime_local_imported_provider_callbacks.h>
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    enum class llvm_jit_trap_kind : ::std::uint_least32_t
    {
        unreachable,
        invalid_conversion_to_integer,
        integer_divide_by_zero,
        integer_overflow,
        call_indirect_table_out_of_bounds,
        call_indirect_null_element,
        call_indirect_type_mismatch,
        memory_out_of_bounds,
        runtime_invariant_failure,
        table_out_of_bounds
    };

    extern "C++"
# if UWVM_HAS_CPP_ATTRIBUTE(clang::disable_tail_calls)
        [[clang::disable_tail_calls]]
# endif
        UWVM_NOINLINE void llvm_jit_runtime_trap(llvm_jit_trap_kind,
                                                 [[maybe_unused]] ::std::uintptr_t frame_address,
                                                 [[maybe_unused]] ::std::uintptr_t stack_pointer) noexcept;

    extern "C++"
# if UWVM_HAS_CPP_ATTRIBUTE(clang::disable_tail_calls)
        [[clang::disable_tail_calls]]
# endif
        UWVM_NOINLINE void llvm_jit_memory_out_of_bounds_trap(::std::size_t memory_idx,
                                                              ::std::uint_least64_t memory_static_offset,
                                                              ::std::uint_least64_t memory_offset,
                                                              ::std::uint_least32_t offset_65_bit,
                                                              ::std::uint_least64_t memory_length,
                                                              ::std::size_t memory_type_size,
                                                              [[maybe_unused]] ::std::uintptr_t frame_address,
                                                              [[maybe_unused]] ::std::uintptr_t stack_pointer) noexcept;

    // Table mutation instructions can invalidate the compact call_indirect target snapshots owned by the runtime.
    // Rebuild them after a funcref-table write so generated call_indirect code observes the same table state as uwvm-int.
    extern "C++" void llvm_jit_refresh_call_indirect_table_views() noexcept;

    extern "C++" void llvm_jit_push_call_stack_frame(::std::size_t module_id, ::std::size_t function_index) noexcept;

    extern "C++" void llvm_jit_pop_call_stack_frame() noexcept;

}

UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm
{
# include "translate/single_func.h"
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
