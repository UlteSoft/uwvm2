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
# include <new>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <uwvm2/runtime/compiler/shared/strict_float.h>
#  include <llvm/Config/llvm-config.h>
#  include <llvm/ADT/SmallPtrSet.h>
#  include <llvm/ADT/DenseMap.h>
#  include <llvm/ADT/SmallVector.h>
#  include <llvm/IR/ValueHandle.h>
#  include <llvm/IR/ValueSymbolTable.h>
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
#  include <llvm/Transforms/Utils/Cloning.h>
#  include <llvm/Transforms/Utils/ModuleUtils.h>
#  include <llvm/Analysis/ValueTracking.h>
#  include <llvm/Support/KnownBits.h>
#  include <llvm/Linker/Linker.h>
#  include <llvm/Support/DynamicLibrary.h>
#  include <llvm/TargetParser/Host.h>
#  include <llvm/TargetParser/Triple.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/IR/LegacyPassManager.h>
#  include <llvm/Pass.h>
#  include <llvm/PassRegistry.h>
#  if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
#   include <llvm/ADT/SmallVector.h>
#   include <llvm/Support/MemoryBuffer.h>
#   include <llvm/Support/raw_ostream.h>
#   include <llvm/Transforms/Utils/Cloning.h>
#  endif
#  include <llvm/InitializePasses.h>
#  include <llvm/Transforms/Scalar/Scalarizer.h>
# endif
# include <uwvm2/runtime/exception/pending_numeric_outer.h>
# include <uwvm2/runtime/compiler/shared/wasm_exception_effect.h>
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
# include <array>
# include <span>
# include <vector>
# include <uwvm2/runtime/compiler/shared/wasm_exception_private_leaf_effect.h>
#endif
# include <uwvm2/validation/standard/wasm3/relaxed_simd.h>
# include <uwvm2/validation/standard/wasm3/threads.h>
# include <uwvm2/validation/standard/wasm3/tail_call.h>
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
# include <uwvm2/validation/standard/wasm3/impl.h>
# include <uwvm2/object/impl.h>
# include <uwvm2/object/memory/flags/impl.h>
# include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
# include <uwvm2/runtime/compiler/shared/wasm_threads.h>
# include <uwvm2/runtime/compiler/shared/wasm_memory64.h>
# include <uwvm2/runtime/wasm_threads/impl.h>
# include <uwvm2/runtime/gc/impl.h>
# include <uwvm2/runtime/checkpoint/materialization.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/uwvm/runtime/validator/validate.h>
# include <uwvm2/runtime/compiler/shared/wasm_exception_control.h>
# include <uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h>
# include <uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h>
# include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#ifndef UWVM_MODULE
# include <uwvm2/runtime/lib/uwvm_runtime_generated_wasm_bridge.h>
# include <uwvm2/runtime/lib/uwvm_runtime_local_imported_provider_callbacks.h>
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
# include "translate/private_host_object_emission_scope.h"
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
        table_out_of_bounds,
        unaligned_atomic,
        atomic_wait_non_shared,
        atomic_wait_cancelled,
        atomic_wait_unavailable,
        atomic_wait_limit,
        null_reference,
        array_out_of_bounds,
        gc_allocation_failure,
        cast_failure
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

    // A successful funcref-table mutation updates only compact views which alias the resolved destination table.
    extern "C++" void llvm_jit_refresh_call_indirect_table_views(
        ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t*,
        ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind,
        ::std::size_t begin,
        ::std::size_t count) noexcept;

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
