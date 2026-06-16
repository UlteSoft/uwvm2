/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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

#ifndef UWVM_MODULE
// This default runtime implementation is intentionally a single aggregation translation unit. It ties together runtime module
// storage, import resolution, interpreter callbacks, optional LLVM JIT materialization, lazy schedulers, WASI context selection,
// and host-facing entry points so every backend observes one coherent module-id space.
//
// Reading map:
// - The anonymous namespace owns all process-local runtime state, caches, schedulers, trap helpers, and backend bridges.
// - `runtime_global_state` is the canonical registry; every function index, type cache, import cache, and JIT address table is
//   keyed from the module ids stored there.
// - Interpreter paths build byte-packed local/operand frames and call generated opfunc streams.
// - LLVM JIT paths materialize either full-module engines or lazy per-function units, then publish raw and typed entry addresses.
// - WASI/preload helpers bind the correct caller memory and environment around imported C API calls.
// - Host API entry points at the end of the file are thin, validated wrappers around those internal mechanisms.
//
// Full coverage guide:
// - Lines near the include block select platform, LLVM, unwind, and ABI integration points.
// - The first anonymous-namespace section declares shared aliases, module records, and process/thread global state.
// - The call-stack section records logical wasm frames and, in tiered mode, snapshots interpreter callers before entering raw JIT.
// - Trap reporting normalizes interpreter traps, mmap memory traps, and LLVM JIT traps into one fatal diagnostic path.
// - The LLVM unwind section records generated code ranges, debug objects, frame-pointer/stack-scan fallbacks, and live probe results.
// - Signature helpers normalize wasm enum signatures and raw C API signature bytes into a common ABI byte model.
// - Import helpers flatten already-initialized import chains and cache final targets for direct/import/table dispatch.
// - Scratch allocation helpers stage host buffers and interpreter frames without returning alloca-backed pointers from helper calls.
// - WASI helpers bind memory[0] and per-module/per-preload environments around local-imported and C API calls.
// - Preload memory helpers expose descriptors, copy read/write APIs, and mmap delivery metadata only for the active preload caller.
// - Interpreter helpers build local/operand frames, dispatch opfunc streams, and bridge direct/call_indirect/import calls.
// - LLVM lazy helpers publish placeholder raw entries that compile, materialize, and replace themselves on first demand.
// - Tiered helpers track hot entries/loops, request urgent T1/T2 compilation, and publish OSR or full-module entries.
// - Full compilation walks every runtime module, emits selected backend artifacts, builds caches, and publishes a one-shot ready flag.
// - Lazy initialization builds validation/materialization metadata and starts background schedulers only when useful.
// - Host entry points validate raw ABI buffers, pick JIT or interpreter execution, then clean up current-thread runtime state.
// std
# include <algorithm>
# include <atomic>
# include <bit>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <functional>
# include <limits>
# include <memory>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>

// platform
// alloca is used for short-lived call-frame and ABI staging buffers. Prefer compiler builtins when available, and include the
// platform header only for toolchains that need an explicit declaration.
# if !UWVM_HAS_BUILTIN(__builtin_alloca) && (defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__))
#  include <malloc.h>
# elif !UWVM_HAS_BUILTIN(__builtin_alloca)
#  include <alloca.h>
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
// Keep LLVM dependencies behind the backend macro so interpreter-only builds do not pay the compile-time or link dependency cost.
#  include <llvm/Analysis/TargetTransformInfo.h>
#  include <llvm/ADT/StringMap.h>
#  include <llvm/Bitcode/BitcodeReader.h>
#  include <llvm/Bitcode/BitcodeWriter.h>
#  include <llvm/DebugInfo/DIContext.h>
#  include <llvm/DebugInfo/DWARF/DWARFContext.h>
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/JITEventListener.h>
#  include <llvm/ExecutionEngine/MCJIT.h>
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#  include <llvm/InitializePasses.h>
#  include <llvm/IR/DIBuilder.h>
#  include <llvm/IR/Constants.h>
#  include <llvm/IR/DebugInfoMetadata.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/Intrinsics.h>
#  include <llvm/IR/LegacyPassManager.h>
#  include <llvm/IR/Metadata.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/PassManager.h>
#  include <llvm/IR/Verifier.h>
#  include <llvm/Linker/Linker.h>
#  include <llvm/Object/ObjectFile.h>
#  include <llvm/PassRegistry.h>
#  include <llvm/Passes/OptimizationLevel.h>
#  include <llvm/Passes/PassBuilder.h>
#  include <llvm/Support/CodeGen.h>
#  include <llvm/Support/DynamicLibrary.h>
#  include <llvm/Support/MemoryBuffer.h>
#  include <llvm/Support/SourceMgr.h>
#  include <llvm/Support/TargetSelect.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/TargetParser/Host.h>
#  include <llvm/Transforms/InstCombine/InstCombine.h>
#  include <llvm/Transforms/Scalar.h>
#  include <llvm/Transforms/Scalar/GVN.h>
#  include <llvm/Transforms/Utils.h>
# endif

# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(_WIN64) &&                                                                                                      \
     ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) && !defined(__CYGWIN__)
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE 1
# else
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE 0
# endif
// LLVM JIT trap reporting can use several native stack-walk backends. The feature macros below separate "backend exists" from
// "backend can safely replace instruction-emitted wasm frames", which lets auto mode fall back without changing generated code.
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(__APPLE__) && !defined(_WIN32) && __has_include(<unwind.h>)
#  include <unwind.h>
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE 0
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE 1
extern "C" void __register_frame(void const*);
extern "C" void __deregister_frame(void const*);
# elif defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && __has_include(<libunwind.h>)
#  ifndef UNW_LOCAL_ONLY
#   define UNW_LOCAL_ONLY
#  endif
#  include <libunwind.h>
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE 1
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE 0
# elif defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && __has_include(<unwind.h>)
#  include <unwind.h>
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE 0
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE 1
# else
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE 0
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE 0
# endif
# if UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE || UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE || UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE 1
# else
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE 0
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(_WIN32) && __has_include(<execinfo.h>)
#  include <execinfo.h>
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_EXECINFO_BACKTRACE 1
# else
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_EXECINFO_BACKTRACE 0
# endif
# ifndef UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES
#  define UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
# endif
# if UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE
#  if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
#   define UWVM2_RUNTIME_LLVM_JIT_LIBUNWIND_FRAME_POINTER_REG UNW_X86_64_RBP
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_SEEDED_LIBUNWIND_BACKTRACE 1
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 1
#  elif defined(__i386__) || defined(_M_IX86)
#   define UWVM2_RUNTIME_LLVM_JIT_LIBUNWIND_FRAME_POINTER_REG UNW_X86_EBP
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_SEEDED_LIBUNWIND_BACKTRACE 1
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 1
#  elif defined(__aarch64__) || defined(_M_ARM64)
#   define UWVM2_RUNTIME_LLVM_JIT_LIBUNWIND_FRAME_POINTER_REG UNW_AARCH64_FP
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_SEEDED_LIBUNWIND_BACKTRACE 1
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 1
#  else
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_SEEDED_LIBUNWIND_BACKTRACE 0
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 0
#  endif
# else
#  define UWVM2_RUNTIME_LLVM_JIT_HAS_SEEDED_LIBUNWIND_BACKTRACE 0
#  if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64) || defined(__i386__) || defined(_M_IX86) || defined(__aarch64__) || defined(_M_ARM64)
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 1
#  else
#   define UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN 0
#  endif
# endif

// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/object/memory/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
# include <uwvm2/runtime/llvm_jit_cache/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/runtime/compiler/uwvm_int/utils/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/uwvm/crtmain/global/process_time.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
#endif

// Generated LLVM raw entries may not use the host platform default ABI. Keep function pointer and function-definition attributes
// paired so a published address can always be called through the exact same convention that created it.
#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI")
#undef UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI
#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR")
#undef UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(_WIN32) &&                                                                                                       \
    ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                       \
    (defined(__GNUC__) || defined(__clang__))
# define UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI __attribute__((__sysv_abi__))
# define UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR [[__gnu__::__sysv_abi__]]
#elif defined(UWVM_RUNTIME_LLVM_JIT) && (defined(__i386__) || defined(_M_IX86))
# define UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI UWVM_FASTCALL
# define UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR UWVM_FASTCALL
#else
# define UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI
# define UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR
#endif

// Interpreter opfuncs call back into these runtime bridges. Their ABI must mirror the translator's opfunc ABI, especially for
// Windows x86_64 GNU/Clang SysV bridges and i386 fastcall builds.
#pragma push_macro("UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR")
#undef UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) && defined(_WIN32) &&                                                                                               \
    ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                       \
    (defined(__GNUC__) || defined(__clang__))
# define UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR [[__gnu__::__sysv_abi__]]
#elif defined(UWVM_RUNTIME_UWVM_INTERPRETER) && (defined(__i386__) || defined(_M_IX86))
# define UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR UWVM_FASTCALL
#else
# define UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR
#endif

namespace uwvm2::runtime::lib
{
    namespace
    {
        // Short aliases keep this runtime glue readable. Most code in this file coordinates already-built runtime storage with
        // compiled backends and host ABI buffers rather than implementing wasm semantics directly.
        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

        using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using runtime_imported_func_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
        using runtime_local_func_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t;
        using runtime_table_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t;
        using runtime_table_elem_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t;
#if defined(UWVM_RUNTIME_LLVM_JIT)
        using runtime_llvm_jit_raw_call_target_t = ::uwvm2::uwvm::runtime::storage::llvm_jit_raw_call_target_t;
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        using win64_context_t = ::fast_io::win32::win64_context;
# endif
#endif
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        using capi_function_t = ::uwvm2::uwvm::wasm::type::capi_function_t;
        using local_imported_t = ::uwvm2::uwvm::wasm::type::local_imported_t;
        using preload_module_memory_attribute_t = ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t;
        using preload_memory_descriptor_t = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t;
        using wasip1_env_type = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_env_type;
        using wasip1_module_override_t = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_override_t;
        using wasip1_module_target_kind_t = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t;

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
        using lazy_compile_scheduler_stats_snapshot_t = ::uwvm2::utils::thread::lazy_compile_scheduler_stats_snapshot;
        using lazy_parser_module_storage_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t;

        // Lazy validation requires parser-level module storage, while execution uses runtime storage. Resolve that parser view by
        // module name so both lazy backends validate against the same parsed module that produced the runtime module.
        struct compiled_module_record;
        [[nodiscard]] inline constexpr lazy_parser_module_storage_t const*
            find_lazy_validator_module_storage(::uwvm2::utils::container::u8string_view module_name) noexcept;
        [[nodiscard]] inline constexpr ::fast_io::unix_timestamp lazy_clock_now() noexcept;
        [[nodiscard]] inline constexpr ::std::size_t lazy_total_function_count() noexcept;
        [[nodiscard]] inline constexpr ::std::size_t lazy_compiled_function_count() noexcept;
        inline constexpr void print_lazy_runtime_compiler_log(::fast_io::unix_timestamp run_start,
                                                              ::fast_io::unix_timestamp exec_start,
                                                              ::fast_io::unix_timestamp exec_end,
                                                              ::fast_io::unix_timestamp stop_end) noexcept;
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        using compiled_module_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
        using compiled_local_func_t = ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t;
        using lazy_compiled_module_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_module_storage_t;
        using lazy_compile_options_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_compile_options;
        using lazy_validation_mode_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_validation_mode;
        using lazy_compile_request_context_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_compile_request_context;

        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tranopt() noexcept;
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tiered_tranopt() noexcept;
# endif
        inline constexpr void prepare_lazy_background_request_contexts(compiled_module_record& rec) noexcept;
        inline constexpr void prioritize_lazy_background_entry(compiled_module_record& rec, ::std::size_t preferred_local_index) noexcept;
        [[nodiscard]] inline constexpr bool lazy_background_refill_callback(void*, ::uwvm2::utils::thread::lazy_compile_scheduler& scheduler) noexcept;
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
        template <typename>
        struct member_function_first_argument;

        // LLVM ownership APIs have changed across releases. Deducing owner argument types from member functions avoids version
        // conditionals while preserving the exact pointer-owner type expected by the installed LLVM headers.
        template <typename R, typename C, typename A0>
        struct member_function_first_argument<R (C::*)(A0)>
        {
            using type = A0;
        };

        using llvm_module_owner_t = typename member_function_first_argument<decltype(&::llvm::ExecutionEngine::addModule)>::type;
        using llvm_jit_memory_manager_owner_t = typename member_function_first_argument<decltype(&::llvm::EngineBuilder::setMCJITMemoryManager)>::type;
        using llvm_jit_function_address_name_t =
            ::std::remove_cvref_t<typename member_function_first_argument<decltype(&::llvm::ExecutionEngine::getFunctionAddress)>::type>;

        using llvm_jit_compiled_module_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::full_function_symbol_t;
        using llvm_jit_lazy_compiled_module_t = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_module_storage_t;
        using llvm_jit_lazy_compile_options_t = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_compile_options;
        using llvm_jit_lazy_validation_mode_t = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_validation_mode;
        using llvm_jit_lazy_compile_request_context_t = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_compile_request_context;
        inline constexpr void prepare_llvm_jit_lazy_background_request_contexts(compiled_module_record& rec) noexcept;
        inline constexpr void prioritize_llvm_jit_lazy_background_entry(compiled_module_record& rec, ::std::size_t preferred_local_index) noexcept;
        [[nodiscard]] inline constexpr bool llvm_jit_lazy_background_refill_callback(void*, ::uwvm2::utils::thread::lazy_compile_scheduler& scheduler) noexcept;
        [[nodiscard]] inline constexpr bool
            ensure_lazy_llvm_jit_defined_function_compiled(::std::size_t module_id, ::std::size_t function_index, bool allow_tiered = false) noexcept;
        [[nodiscard]] inline constexpr bool prepare_lazy_llvm_jit_unwind_native_call_graph(::std::size_t entry_module_id,
                                                                                           ::std::size_t entry_function_index,
                                                                                           bool allow_tiered = false) noexcept;
        [[nodiscard]] inline constexpr ::std::size_t llvm_jit_lazy_compile_unit_code_size(compiled_module_record const& rec,
                                                                                          ::std::size_t local_function_index) noexcept;
        [[nodiscard]] inline constexpr bool
            try_materialize_runtime_module_llvm_jit(compiled_module_record& rec,
                                                    bool publish_full_ready = true,
                                                    ::llvm::CodeGenOptLevel default_codegen_opt_level = ::llvm::CodeGenOptLevel::Aggressive,
                                                    ::std::size_t extra_materialize_threads = ::std::numeric_limits<::std::size_t>::max()) noexcept;
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        [[nodiscard]] inline constexpr bool tiered_runtime_active() noexcept;
        [[nodiscard]] inline constexpr bool tiered_t0_enabled() noexcept;
        [[nodiscard]] inline constexpr bool tiered_t2_enabled() noexcept;
        [[nodiscard]] inline constexpr bool tiered_uses_tiered_targets() noexcept;
        [[nodiscard]] inline constexpr bool tiered_full_ready(compiled_module_record const& rec) noexcept;
        [[nodiscard]] inline constexpr bool tiered_large_module_long_run_active(compiled_module_record const& rec) noexcept;
        [[nodiscard]] inline constexpr ::std::size_t tiered_full_compile_switch_request_threshold(compiled_module_record const& rec) noexcept;
        [[nodiscard]] inline constexpr ::std::uint_least32_t tiered_entry_hot_request_threshold(compiled_module_record const& rec,
                                                                                                ::std::size_t local_index) noexcept;
        [[nodiscard]] inline constexpr ::std::uint_least32_t tiered_entry_hot_probe_stride(compiled_module_record const& rec) noexcept;
        [[nodiscard]] inline constexpr ::std::uint_least32_t tiered_loop_osr_request_threshold(compiled_module_record const& rec,
                                                                                               ::std::size_t local_index) noexcept;
        [[nodiscard]] inline constexpr bool try_publish_tiered_ready_full_llvm_jit_entry(compiled_module_record& rec,
                                                                                         ::std::size_t module_id,
                                                                                         ::std::size_t local_index,
                                                                                         ::std::uintptr_t& raw_entry_address) noexcept;
# endif
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        constexpr ::std::size_t local_slot_size{sizeof(::uwvm2::runtime::compiler::uwvm_int::optable::wasm_stack_top_i32_i64_f32_f64_u)};
        static_assert(local_slot_size == 8uz);
#endif

        // Cached metadata for a local-defined wasm function. It joins runtime storage, backend-specific compiled information, and
        // raw ABI sizes so hot call paths do not need to revisit module type sections.
        struct compiled_defined_func_info
        {
            ::std::size_t module_id{};
            ::std::size_t function_index{};
            runtime_local_func_storage_t const* runtime_func{};
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const* compiled_call_info{};
            compiled_local_func_t const* compiled_func{};
#endif
            ::std::size_t param_bytes{};
            ::std::size_t result_bytes{};
        };

        // Per-module compilation record. Depending on runtime mode, this can hold eager interpreter artifacts, lazy interpreter
        // metadata, full LLVM JIT state, lazy LLVM materialization state, and tiered counters simultaneously.
        struct compiled_module_record
        {
            // module_name is a view into the global module map key; the map owns the storage lifetime.
            ::uwvm2::utils::container::u8string_view module_name{};
            runtime_module_storage_t const* runtime_module{};
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            // Eager and lazy interpreter paths share the same final symbol-table shape so the dispatch bridge does not care which
            // path materialized a function.
            compiled_module_t compiled{};
            lazy_compiled_module_t lazy_compiled{};
            lazy_compile_options_t lazy_compile_options{};
            // One stable background context per local function lets scheduler requests keep raw pointers without allocating per enqueue.
            ::uwvm2::utils::container::vector<lazy_compile_request_context_t> lazy_background_request_contexts{};
            ::uwvm2::utils::container::vector<::uwvm2::validation::error::code_validation_error_impl> lazy_background_errors{};
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
            // Shared prefetch order biases lazy background work toward the selected entry path while still allowing full module coverage.
            ::uwvm2::utils::container::vector<::std::size_t> lazy_prefetch_order{};
            ::std::size_t lazy_prefetch_cursor{};
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
            // Full LLVM JIT state is kept separate from lazy LLVM state so tiered mode can publish lazy T1 entries and later replace
            // them with full-module T2 entries.
            llvm_jit_compiled_module_t llvm_jit_compiled{};
            llvm_jit_lazy_compiled_module_t llvm_jit_lazy_compiled{};
            llvm_jit_lazy_compile_options_t llvm_jit_lazy_compile_options{};
            ::uwvm2::utils::container::vector<llvm_jit_lazy_compile_request_context_t> llvm_jit_lazy_background_request_contexts{};
            ::uwvm2::utils::container::vector<::uwvm2::validation::error::code_validation_error_impl> llvm_jit_lazy_background_errors{};
            // These owners must outlive every function address published into direct-call and call_indirect targets.
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::LLVMContext> llvm_jit_context_holder{};
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> llvm_jit_engine{};
            ::uwvm2::utils::container::vector<::std::uintptr_t> llvm_jit_local_entry_addresses{};
            ::uwvm2::utils::container::vector<::std::uintptr_t> llvm_jit_local_raw_entry_addresses{};
            ::uwvm2::utils::container::vector<runtime_llvm_jit_raw_call_target_t> llvm_jit_lazy_direct_call_targets{};
            ::uwvm2::utils::container::vector<::std::uintptr_t> llvm_jit_lazy_direct_typed_entry_targets{};
            bool llvm_jit_ready{};
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            // Tiered state is updated from execution and scheduler threads. Most counters are policy signals, so relaxed atomic_ref
            // sampling is enough and keeps the record movable.
            ::uwvm2::utils::thread::lazy_compile_unit_state tiered_full_compile_state{};
            ::std::uint_least8_t tiered_full_ready{};
            ::std::size_t tiered_switch_count{};
            ::std::size_t tiered_interpreter_fallback_count{};
            ::std::size_t tiered_entry_miss_count{};
            ::std::size_t tiered_large_loop_sample_count{};
            ::std::uint_least8_t tiered_large_long_run_ready{};
            ::uwvm2::utils::container::vector<::std::uint_least32_t> tiered_entry_hot_counters{};
            ::uwvm2::utils::container::vector<::std::uint_least32_t> tiered_osr_request_counters{};
#endif

            // Canonical type-index table for fast call_indirect signature checks.
            // Maps each type index to its canonical representative (deduplicated by {params, results}).
            ::uwvm2::utils::container::vector<::std::size_t> type_canon_index{};
#if defined(UWVM_RUNTIME_LLVM_JIT)
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<runtime_llvm_jit_raw_call_target_t>> llvm_jit_call_indirect_targets{};
#endif

            inline constexpr compiled_module_record() = default;
            inline constexpr compiled_module_record(compiled_module_record const&) = delete;
            inline constexpr compiled_module_record& operator= (compiled_module_record const&) = delete;
            inline constexpr compiled_module_record(compiled_module_record&&) = default;
            inline constexpr compiled_module_record& operator= (compiled_module_record&&) = default;
            inline constexpr ~compiled_module_record() noexcept;
        };

        // Maps an address range of local-defined function storage back to a runtime module. Import aliases and table elements often
        // contain only a storage pointer, so this avoids expensive pointer maps on the hot path.
        struct defined_func_ptr_range
        {
            ::std::uintptr_t begin{};
            ::std::uintptr_t end{};
            ::std::size_t module_id{};
        };

#if defined(UWVM_RUNTIME_LLVM_JIT)
        // Associates a generated native address with a wasm function identity for trap backtraces.
        struct llvm_jit_unwind_entry
        {
            ::std::uintptr_t address{};
            ::std::size_t module_id{};
            ::std::size_t function_index{};
            bool raw_entry{};
        };

        // Sorted merged native code ranges cheaply filter unrelated host frames before looking up JIT unwind entries.
        struct llvm_jit_code_range
        {
            ::std::uintptr_t begin{};
            ::std::uintptr_t end{};
        };

        // Minimal section relocation record retained beside the copied JIT object.  DWARF lookup sometimes needs both the
        // loaded native address and the original object-section address to recover inline call chains reliably.
        struct llvm_jit_debug_loaded_section
        {
            ::std::uint64_t section_index{};
            ::std::uint64_t object_address{};
            ::std::uint64_t load_address{};
            ::std::uint64_t size{};
            bool text{};
        };

        // Fallback LoadedObjectInfo for MCJIT platforms whose getObjectForDebug returns no object.  Section indices are stable
        // across a byte-for-byte object-buffer copy, so they are sufficient to answer DWARF relocation address queries.
        class llvm_jit_copied_loaded_object_info final : public ::llvm::LoadedObjectInfoHelper<llvm_jit_copied_loaded_object_info>
        {
        public:
            ::uwvm2::utils::container::vector<llvm_jit_debug_loaded_section> sections{};

            [[nodiscard]] inline constexpr ::std::uint64_t getSectionLoadAddress(::llvm::object::SectionRef const& section) const override
            {
                auto const section_index{section.getIndex()};
                for(auto const& record: sections)
                {
                    if(record.section_index == section_index) { return record.load_address; }
                }

                return 0u;
            }
        };

        // Retain debug objects because DWARF inline-frame queries need the object and relocation view to remain alive after MCJIT finalization.
        struct llvm_jit_debug_object
        {
            ::llvm::object::OwningBinary<::llvm::object::ObjectFile> object{};
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::LoadedObjectInfo> loaded_info{};
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::DWARFContext> dwarf_context{};
            ::uwvm2::utils::container::vector<llvm_jit_debug_loaded_section> loaded_sections{};
        };
#endif

        // Global runtime registry. It is centralized intentionally: full compile, lazy compile, host APIs, bridges, trap reporting,
        // import calls, and WASI binding all have to agree on the same module ids and cached function metadata.
        struct runtime_global_state
        {
            ::uwvm2::utils::container::vector<compiled_module_record> modules{};
            ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string_view, ::std::size_t> module_name_to_id{};

            // Full-compile: keep the hot local-call path O(1) by indexing local funcs with vectors (not hash maps).
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<compiled_defined_func_info>> defined_func_cache{};

            // For indirect calls / import-alias resolution that only has `local_defined_function_storage_t*`,
            // map pointer-address to {module_id, local_index} via a sorted range table.
            ::uwvm2::utils::container::vector<defined_func_ptr_range> defined_func_ptr_ranges{};

#if defined(UWVM_RUNTIME_LLVM_JIT)
            // JIT unwind/debug metadata is published during materialization and read during trap reporting. Urgent schedulers are
            // separated from normal lazy background work so demand compilation is not starved.
            ::uwvm2::utils::container::vector<llvm_jit_unwind_entry> llvm_jit_unwind_entries{};
            ::uwvm2::utils::container::vector<llvm_jit_code_range> llvm_jit_code_ranges{};
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::delete_owned_ptr<llvm_jit_debug_object>> llvm_jit_debug_objects{};
            ::uwvm2::utils::thread::lazy_compile_scheduler llvm_jit_urgent_scheduler{};
            ::std::atomic_flag llvm_jit_urgent_start_lock = ATOMIC_FLAG_INIT;
            ::std::atomic_size_t llvm_jit_urgent_request_count{};
#endif

            ::std::atomic_bool bridges_initialized{false};
            // compiled_all means eager-mode module records, import caches, function maps, and optional interpreter bridges are ready.
            ::std::atomic_bool compiled_all{false};
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
            // Lazy initialization is separate from full compilation: metadata may be ready while individual functions remain uncompiled.
            ::uwvm2::utils::thread::lazy_compile_scheduler lazy_scheduler{};
            ::std::atomic_bool lazy_initialized{false};
            bool lazy_compile_active{};
            ::std::atomic_flag lazy_prefetch_lock = ATOMIC_FLAG_INIT;
            ::std::size_t lazy_prefetch_module_id{SIZE_MAX};
            ::std::size_t lazy_prefetch_local_function_index{SIZE_MAX};
            ::std::atomic_size_t lazy_runtime_miss_count{};
            ::std::atomic_size_t lazy_runtime_compiled_hit_count{};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            // Tiered counters feed adaptive scheduling and runtime logs. Exact counts are not correctness-critical, so relaxed atomics
            // are used where the hot path only needs approximate pressure signals.
            ::uwvm2::utils::thread::lazy_compile_scheduler tiered_urgent_scheduler{};
            ::std::atomic_flag tiered_scheduler_start_lock = ATOMIC_FLAG_INIT;
            ::std::atomic_bool tiered_schedulers_deferred{};
            ::std::size_t tiered_deferred_worker_count{};
            ::std::atomic_size_t tiered_osr_callback_count{};
            ::std::atomic_size_t tiered_osr_ready_count{};
            ::std::atomic_size_t tiered_osr_miss_count{};
            ::std::atomic_size_t tiered_osr_deferred_count{};
            ::std::atomic_size_t tiered_osr_compile_request_count{};
            ::std::atomic_size_t tiered_osr_urgent_request_count{};
            ::std::atomic_size_t tiered_full_compile_request_count{};
            ::std::atomic_size_t tiered_full_compile_ready_count{};
            ::std::atomic_size_t tiered_full_compile_failed_count{};
            ::std::atomic_size_t tiered_full_publish_count{};
# endif
#endif
        };

        inline runtime_global_state g_runtime{};  // [global]

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
# if defined(UWVM_USE_THREAD_LOCAL)
// Entry hotness probing is per-thread to avoid a global cache-line bounce on every interpreted function entry.
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#   ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#   else
        [[__gnu__::__tls_model__("local-dynamic")]]
#   endif
#  endif
        inline thread_local ::std::uint_least32_t tiered_entry_hot_probe_tick{};  // [global] [thread-local]
# endif

# if defined(UWVM_USE_THREAD_LOCAL)
// Keep the counter-sampling phase separate from entry probing so tiered heuristics do not accidentally synchronize.
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#   ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#   else
        [[__gnu__::__tls_model__("local-dynamic")]]
#   endif
#  endif
        inline thread_local ::std::uint_least32_t tiered_counter_sample_tick{};  // [global] [thread-local]
# endif
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr lazy_parser_module_storage_t const*
            find_lazy_validator_module_storage(::uwvm2::utils::container::u8string_view module_name) noexcept
        {
            // The lazy compiler validates from parser storage but executes from runtime storage; name lookup is the stable bridge.
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]] { return nullptr; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) [[unlikely]] { return nullptr; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr || wf->binfmt_ver != 1u) [[unlikely]] { return nullptr; }
            return ::std::addressof(wf->wasm_module_storage.wasm_binfmt_ver1_storage);
        }
#endif

        // Keep the exit flag in a trivial namespace-scope object, not as a
        // member of runtime_process_exit_state. A scalar with constant
        // initialization is zero-initialized with the static storage image
        // before any .init_array dynamic-initialization code runs, and it has
        // no destructor, so no __cxa_atexit entry is registered for it.
        //
        // runtime_process_exit_state is intentionally a non-trivially
        // destructible sentinel. Its destructor is registered through the
        // static-initialization path (__cxa_atexit on the Itanium C++ ABI).
        // Because the sentinel is defined after g_runtime in this translation
        // unit, its atexit destructor runs before g_runtime is destroyed. If
        // the flag were a sentinel member, that same destructor would set the
        // member and then end the sentinel object's lifetime; g_runtime's later
        // destructor would read a member of an already-destroyed object, which
        // is undefined behavior and can be optimized incorrectly in release
        // builds. The separate trivial bool stays valid while static
        // destructors run and lets compiled_module_record avoid LLVM MCJIT
        // teardown during process exit.
        inline bool g_runtime_process_exiting{};  // [global]

        struct runtime_process_exit_state
        {
            ~runtime_process_exit_state() noexcept { g_runtime_process_exiting = true; }
        };

        inline runtime_process_exit_state g_runtime_process_exit_state{};  // [global]

        inline constexpr compiled_module_record::~compiled_module_record() noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT)
            if(g_runtime_process_exiting)
            {
                // Avoid late LLVM MCJIT teardown during static destruction. The process is exiting, so releasing ownership is safer
                // than running destructors that may touch already-destroyed LLVM globals.
                static_cast<void>(llvm_jit_compiled.llvm_jit_module.llvm_module.release());
                static_cast<void>(llvm_jit_compiled.llvm_jit_module.llvm_context_holder.release());
                for(auto& materialized_func: llvm_jit_lazy_compiled.materialized_functions)
                {
                    static_cast<void>(materialized_func.llvm_jit_engine.release());
                    static_cast<void>(materialized_func.llvm_context_holder.release());
                }
                static_cast<void>(llvm_jit_engine.release());
                static_cast<void>(llvm_jit_context_holder.release());
            }
#endif
        }

        struct cached_import_target;

        struct call_stack_frame
        {
            // Store logical wasm identity instead of native return addresses so interpreter and JIT trap output share one format.
            ::std::size_t module_id{};
            ::std::size_t function_index{};
        };

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        struct tiered_jit_entry_call_stack_snapshot_t
        {
            // Tiered execution can jump from an interpreter frame directly into a T1/T2 raw JIT entry. Native unwind can describe
            // the generated callee, but the still-live interpreter callers are represented only in the logical TLS stack. Keep a
            // bounded snapshot at the exact mixed-stack boundary so trap reporting can recover those callers even if an optimized
            // helper frame or tail path makes the live vector harder to observe at the fatal trap point.
            // This snapshot is deliberately a fallback source for caller frames, not a replacement for DWARF/native unwind. The
            // native unwinder still owns generated leaf frames, including inlined LLVM frames when debug/unwind metadata exposes
            // them; the snapshot only preserves the interpreter side that native unwind has no architectural reason to know about.
            // Keep the bound small and allocation-free because fatal traps may arrive from signal/terminate paths where allocation,
            // locking, or walking large diagnostic structures would make the original trap harder to report deterministically.
            inline static constexpr ::std::size_t max_frames{64uz};

            ::uwvm2::utils::container::array<call_stack_frame, max_frames> frames{};
            ::std::size_t size{};
            bool active{};
        };
#endif

        struct printed_call_stack_frame_tracker
        {
            // Trap reports are intentionally small. This fixed tracker lets hybrid tiered reports merge native unwind frames with
            // interpreter frames without heap allocation or duplicate logical wasm frames.
            inline static constexpr ::std::size_t max_frames{64uz};

            ::uwvm2::utils::container::array<call_stack_frame, max_frames> frames{};
            ::std::size_t size{};

            [[nodiscard]] inline constexpr bool contains(::std::size_t module_id, ::std::size_t function_index) const noexcept
            {
                for(::std::size_t i{}; i != size; ++i)
                {
                    auto const& fr{frames.index_unchecked(i)};
                    if(fr.module_id == module_id && fr.function_index == function_index) { return true; }
                }

                return false;
            }

            inline constexpr void record(::std::size_t module_id, ::std::size_t function_index) noexcept
            {
                if(size == max_frames || contains(module_id, function_index)) [[unlikely]] { return; }
                frames.index_unchecked(size++) = call_stack_frame{module_id, function_index};
            }
        };

        // Per-thread execution state: logical wasm stack frames plus a tiny call_indirect inline cache. Keeping this state thread-local
        // prevents re-entrant host calls from corrupting another thread's trap report or indirect-call cache.
        struct call_stack_tls_state
        {
            inline static constexpr ::std::size_t kCallStackMaxDepth{4096uz};
            inline static constexpr ::std::size_t kCallIndirectCacheEntries{8uz};

            using thread_local_allocator = ::fast_io::native_thread_local_allocator;
            ::uwvm2::utils::container::vector<call_stack_frame, thread_local_allocator> frames{};

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            // Active only while a tiered raw JIT entry is executing below an interpreter caller. It lives in the same TLS object as
            // the logical stack so a trap can merge both views without consulting shared runtime state during unwinding.
            tiered_jit_entry_call_stack_snapshot_t tiered_jit_entry_snapshot{};
#endif

            struct call_indirect_cache_entry
            {
                // The cache key includes table identity, backing element storage, selector, and expected type. The backing pointer is
                // important because table growth/reallocation must invalidate stale entries.
                runtime_table_storage_t const* table{};
                void const* elems_data{};
                ::std::uint_least32_t selector{};
                ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* expected_ft_ptr{};

                ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t elem_type{};
                void const* target_ptr{};

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const* defined_info{};
#else
                compiled_defined_func_info const* defined_info{};
#endif
                cached_import_target const* imported_tgt{};
            };

            static_assert((kCallIndirectCacheEntries & (kCallIndirectCacheEntries - 1uz)) == 0uz, "cache size must be power-of-two.");
            ::uwvm2::utils::container::array<call_indirect_cache_entry, kCallIndirectCacheEntries> call_indirect_cache{};

            inline constexpr call_stack_tls_state() noexcept { frames.reserve(kCallStackMaxDepth); }

            inline constexpr void push(call_stack_frame fr) noexcept
            {
                // Reserve the common maximum depth up front, but allow slow-path growth instead of failing on diagnostic-heavy stacks.
                if(frames.size() < frames.capacity()) [[likely]] { frames.push_back_unchecked(fr); }
                else
                {
                    frames.push_back(fr);
                }
            }

            inline constexpr void pop() noexcept
            {
                if(!frames.empty()) [[likely]] { frames.pop_back_unchecked(); }
            }
        };

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        struct tiered_jit_entry_call_stack_snapshot_guard
        {
            call_stack_tls_state* tls{};
            tiered_jit_entry_call_stack_snapshot_t saved{};

            inline constexpr explicit tiered_jit_entry_call_stack_snapshot_guard(call_stack_tls_state& s) noexcept :
                tls{::std::addressof(s)}, saved{s.tiered_jit_entry_snapshot}
            {
                // Save/restore the previous snapshot because tiered execution can re-enter generated code recursively through
                // imports or host callbacks. A nested raw-entry trap must see the innermost mixed-stack boundary, while returning
                // from that nested call must restore the outer boundary for any later trap in the outer JIT activation.
                auto& snapshot{s.tiered_jit_entry_snapshot};
                auto const frame_count{s.frames.size()};
                // Retain the most recent logical frames because they are the only interpreter callers that can appear directly
                // below the generated activation. Older frames are less useful in a bounded fatal-trap report and would only
                // increase work on already fragile signal/terminate paths.
                auto const skip{
                    frame_count > tiered_jit_entry_call_stack_snapshot_t::max_frames ? frame_count - tiered_jit_entry_call_stack_snapshot_t::max_frames : 0uz};
                snapshot.size = frame_count - skip;
                for(::std::size_t i{}; i != snapshot.size; ++i) { snapshot.frames.index_unchecked(i) = s.frames.index_unchecked(skip + i); }
                // Set active last so an interrupted observer never treats a partially initialized buffer as a valid fallback stack.
                snapshot.active = true;
            }

            tiered_jit_entry_call_stack_snapshot_guard(tiered_jit_entry_call_stack_snapshot_guard const&) = delete;
            tiered_jit_entry_call_stack_snapshot_guard& operator= (tiered_jit_entry_call_stack_snapshot_guard const&) = delete;

            inline constexpr ~tiered_jit_entry_call_stack_snapshot_guard()
            {
                // Restoring also clears active for the common non-nested case, preventing a later interpreter-only trap from
                // accidentally appending stale tiered boundary frames.
                if(tls != nullptr) [[likely]] { tls->tiered_jit_entry_snapshot = saved; }
            }
        };
#endif

        struct preload_call_context_t
        {
            inline static constexpr ::std::size_t invalid_module_id{::std::numeric_limits<::std::size_t>::max()};

            // Preload C APIs need the current wasm caller to select the correct memory descriptor and WASI environment.
            ::std::size_t module_id{invalid_module_id};
            preload_module_memory_attribute_t const* preload_module_memory_attribute{};
            capi_function_t const* capi_function{};
        };

        struct suppressed_call_stack_frame_t
        {
            inline static constexpr ::std::size_t invalid_id{::std::numeric_limits<::std::size_t>::max()};

            // Used when a trap is raised before normal dispatch, for example call_indirect signature mismatch, to avoid printing the
            // same logical frame twice.
            ::std::size_t module_id{invalid_id};
            ::std::size_t function_index{invalid_id};
        };

#if defined(UWVM_USE_THREAD_LOCAL)
// Direct thread_local storage is the fast path. The TLS model attribute selects local-exec for static runtime builds and
// local-dynamic for shared-library style builds without changing semantics.
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local call_stack_tls_state g_call_stack{};  // [global] [thread_local]

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local preload_call_context_t g_preload_call_context{};  // [global] [thread_local]

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local suppressed_call_stack_frame_t g_suppressed_call_stack_frame{};  // [global] [thread_local]

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr call_stack_tls_state& get_call_stack() noexcept { return g_call_stack; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr preload_call_context_t& get_preload_call_context() noexcept { return g_preload_call_context; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr suppressed_call_stack_frame_t& get_suppressed_call_stack_frame() noexcept
        { return g_suppressed_call_stack_frame; }

        inline constexpr void erase_current_thread_state() noexcept {}
#else
        // Keep the UWVM_USE_THREAD_LOCAL branch as direct thread_local storage.
        // This map exists only for toolchains/platforms where C++ thread_local is disabled.
        using os_thread_id_t =
# if defined(__SINGLE_THREAD__)
            ::std::size_t;
# else
            decltype(::fast_io::this_thread::get_id());
# endif

        struct runtime_thread_state
        {
            // Map-backed fallback for environments where C++ thread_local is disabled.
            call_stack_tls_state call_stack{};
            preload_call_context_t preload_call_context{};
            suppressed_call_stack_frame_t suppressed_call_stack_frame{};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            ::std::uintptr_t llvm_jit_trap_return_address{};
            ::std::uintptr_t llvm_jit_trap_frame_address{};
            ::std::uintptr_t llvm_jit_trap_stack_pointer{};
            llvm_jit_trap_kind llvm_jit_last_trap_kind{};
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            win64_context_t llvm_jit_win64_trap_caller_context{};
            bool llvm_jit_win64_trap_caller_context_valid{};
#  endif
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            ::std::uint_least32_t tiered_entry_hot_probe_tick{};
            ::std::uint_least32_t tiered_counter_sample_tick{};
# endif
        };

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr os_thread_id_t current_thread_id() noexcept
        {
# if defined(__SINGLE_THREAD__)
            return 0uz;
# else
            return ::fast_io::this_thread::get_id();
# endif
        }

        inline ::uwvm2::utils::container::concurrent_node_map<os_thread_id_t, runtime_thread_state> g_thread_states{};  // [global]

        /// @warning `g_thread_states` entries MUST be cleaned up on thread exit.
        ///          Currently only the main thread is used; we clear `g_thread_states` at the end of
        ///          `full_compile_and_run_main_module()` (by erasing the current thread state).
        ///          When implementing `wasi-thread`, every created thread must erase its own state on exit to avoid
        ///          unbounded growth and possible thread-id reuse issues.

        struct thread_states_reserve_guard
        {
            // Reserve enough nodes for common runtime usage so fallback thread-state insertion avoids early rehashing.
            inline constexpr thread_states_reserve_guard() noexcept { g_thread_states.reserve(256uz); }
        };

        inline thread_states_reserve_guard g_thread_states_reserve_guard{};  // [global]

        [[nodiscard]] inline constexpr runtime_thread_state& get_thread_state() noexcept
        {
            // try_emplace_and_visit gives a stable node address with one map operation.
            auto const id{current_thread_id()};
            runtime_thread_state* st{};

            g_thread_states.try_emplace_and_visit(
                id,
                [&](auto& kv) constexpr noexcept { st = ::std::addressof(kv.second); },
                [&](auto& kv) constexpr noexcept { st = ::std::addressof(kv.second); });

            if(st == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            return *st;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr call_stack_tls_state& get_call_stack() noexcept { return get_thread_state().call_stack; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr preload_call_context_t& get_preload_call_context() noexcept
        { return get_thread_state().preload_call_context; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr suppressed_call_stack_frame_t& get_suppressed_call_stack_frame() noexcept
        { return get_thread_state().suppressed_call_stack_frame; }

        inline constexpr void erase_current_thread_state() noexcept { g_thread_states.erase(current_thread_id()); }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(UWVM_USE_THREAD_LOCAL)
        // Non-TLS fallback accessors. Do not route the thread_local build through these;
        // the direct TLS variables are the fast path and should stay visible in preprocessed code.
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::uintptr_t& get_llvm_jit_trap_return_address() noexcept
        { return get_thread_state().llvm_jit_trap_return_address; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::uintptr_t& get_llvm_jit_trap_frame_address() noexcept
        { return get_thread_state().llvm_jit_trap_frame_address; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::uintptr_t& get_llvm_jit_trap_stack_pointer() noexcept
        { return get_thread_state().llvm_jit_trap_stack_pointer; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr llvm_jit_trap_kind& get_llvm_jit_last_trap_kind() noexcept
        { return get_thread_state().llvm_jit_last_trap_kind; }

# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr win64_context_t& get_llvm_jit_win64_trap_caller_context() noexcept
        { return get_thread_state().llvm_jit_win64_trap_caller_context; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool& get_llvm_jit_win64_trap_caller_context_valid() noexcept
        { return get_thread_state().llvm_jit_win64_trap_caller_context_valid; }
# endif
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED) && !defined(UWVM_USE_THREAD_LOCAL)
        // Tiered sampling counters are direct thread_local variables when available.
        // The container-backed state below is strictly the compatibility fallback.
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::uint_least32_t& get_tiered_entry_hot_probe_tick() noexcept
        { return get_thread_state().tiered_entry_hot_probe_tick; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::uint_least32_t& get_tiered_counter_sample_tick() noexcept
        { return get_thread_state().tiered_counter_sample_tick; }
#endif

        struct preload_call_context_guard
        {
            preload_call_context_t* ctx{};
            preload_call_context_t saved{};

            // Preload calls may nest. Save/restore the active context so each C API observes its immediate wasm caller.
            inline explicit constexpr preload_call_context_guard(preload_module_memory_attribute_t const* attribute,
                                                                 capi_function_t const* function = nullptr,
                                                                 ::std::size_t caller_module_id = preload_call_context_t::invalid_module_id) noexcept :
                ctx{::std::addressof(get_preload_call_context())}, saved{*ctx}
            {
                if(caller_module_id != preload_call_context_t::invalid_module_id) { ctx->module_id = caller_module_id; }
                else
                {
                    auto& call_stack{get_call_stack()};
                    if(!call_stack.frames.empty()) [[likely]] { ctx->module_id = call_stack.frames.back().module_id; }
                    else
                    {
                        ctx->module_id = preload_call_context_t::invalid_module_id;
                    }
                }
                ctx->preload_module_memory_attribute = attribute;
                ctx->capi_function = function;
            }

            inline constexpr preload_call_context_guard(preload_call_context_guard const&) noexcept = delete;
            inline constexpr preload_call_context_guard& operator= (preload_call_context_guard const&) noexcept = delete;

            inline constexpr ~preload_call_context_guard()
            {
                if(this->ctx != nullptr) [[likely]] { *this->ctx = this->saved; }
            }
        };

        [[nodiscard]] inline constexpr compiled_defined_func_info const* find_defined_func_info(runtime_local_func_storage_t const* f) noexcept
        {
            // Resolve a local function storage pointer back to compiled metadata through sorted address ranges. This supports import
            // aliases and table elements without putting a hash lookup on every call.
            if(f == nullptr) [[unlikely]] { return nullptr; }
            if(g_runtime.defined_func_ptr_ranges.empty()) [[unlikely]] { return nullptr; }

            ::std::uintptr_t const addr{reinterpret_cast<::std::uintptr_t>(f)};
            auto const it{::std::upper_bound(g_runtime.defined_func_ptr_ranges.begin(),
                                             g_runtime.defined_func_ptr_ranges.end(),
                                             addr,
                                             [](auto const a, defined_func_ptr_range const& r) constexpr noexcept { return a < r.begin; })};
            if(it == g_runtime.defined_func_ptr_ranges.begin()) [[unlikely]] { return nullptr; }

            auto const& r{*(it - 1u)};
            if(addr < r.begin || addr >= r.end) [[unlikely]] { return nullptr; }

            constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
            static_assert(elem_size != 0uz);

            ::std::uintptr_t const off_bytes{addr - r.begin};
            if((off_bytes % elem_size) != 0u) [[unlikely]] { return nullptr; }
            auto const local_idx{static_cast<::std::size_t>(off_bytes / elem_size)};

            if(r.module_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { return nullptr; }
            auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(r.module_id)};
            if(local_idx >= mod_cache.size()) [[unlikely]] { return nullptr; }

            auto const info{::std::addressof(mod_cache.index_unchecked(local_idx))};
            if(info->runtime_func != f) [[unlikely]] { return nullptr; }
            return info;
        }

        struct call_stack_guard
        {
            call_stack_tls_state* tls{};

            // RAII keeps logical call-stack frames balanced across normal returns and fast_io exceptions converted into traps.
            inline constexpr explicit call_stack_guard(call_stack_tls_state& s, ::std::size_t module_id, ::std::size_t function_index) noexcept : tls{&s}
            { tls->push(call_stack_frame{module_id, function_index}); }

            call_stack_guard(call_stack_guard const&) = delete;
            call_stack_guard& operator= (call_stack_guard const&) = delete;

            inline constexpr ~call_stack_guard()
            {
                if(tls) [[likely]] { tls->pop(); }
            }
        };

        enum class trap_kind : unsigned
        {
            // Backend-neutral trap categories. Interpreter callbacks, mmap signal handlers, and LLVM trap thunks all funnel through
            // this single reporting path.
            // opfunc
            unreachable,
            invalid_conversion_to_integer,
            integer_divide_by_zero,
            integer_overflow,
            // call_indirect (wasm1.0 MVP)
            call_indirect_table_out_of_bounds,
            call_indirect_null_element,
            call_indirect_type_mismatch,
            memory_out_of_bounds,
            runtime_invariant_failure,
            // uncatched int error (wasm 3.0, exception)
            uncatched_int_tag

        };

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view trap_kind_name(trap_kind k) noexcept
        {
            switch(k)
            {
                case trap_kind::unreachable:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"catch unreachable"};
                }
                case trap_kind::invalid_conversion_to_integer:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"invalid conversion to integer"};
                }
                case trap_kind::integer_divide_by_zero:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"integer divide by zero"};
                }
                case trap_kind::integer_overflow:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"integer overflow"};
                }
                case trap_kind::call_indirect_table_out_of_bounds:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: table index out of bounds"};
                }
                case trap_kind::call_indirect_null_element:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: uninitialized element"};
                }
                case trap_kind::call_indirect_type_mismatch:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"call_indirect: signature mismatch"};
                }
                case trap_kind::memory_out_of_bounds:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"memory access out of bounds"};
                }
                case trap_kind::runtime_invariant_failure:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"runtime invariant failure"};
                }
                case trap_kind::uncatched_int_tag:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"tag: uncatched wasm exception"};
                }
                [[unlikely]] default:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"unknown trap"};
                }
            }
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            resolve_module_display_name(::uwvm2::utils::container::u8string_view module_name) noexcept
        {
            // Prefer the wasm custom-name section when present, but keep the storage key as a reliable fallback.
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) { return module_name; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) { return module_name; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr) { return module_name; }

            auto const n{wf->wasm_custom_name.module_name};
            if(n.empty()) { return module_name; }
            return n;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view resolve_func_display_name(::uwvm2::utils::container::u8string_view module_name,
                                                                                                          ::std::size_t function_index) noexcept
        {
            // Function names are optional debug metadata; missing names must not affect trap reporting.
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) { return {}; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) { return {}; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr) { return {}; }

            using wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
            constexpr auto wasm_u32_max{static_cast<::std::size_t>(::std::numeric_limits<wasm_u32>::max())};
            if(function_index > wasm_u32_max) { return {}; }

            auto const key{static_cast<wasm_u32>(function_index)};
            auto const fn_it{wf->wasm_custom_name.function_name.find(key)};
            if(fn_it == wf->wasm_custom_name.function_name.end()) { return {}; }
            return fn_it->second;
        }

        template <typename Output>
        [[nodiscard]] inline constexpr bool
            dump_call_stack_frame_for_trap(Output& u8log_output_ul, ::std::size_t frame_index, ::std::size_t module_id, ::std::size_t function_index) noexcept
        {
            // Print one logical wasm frame. Native unwind and interpreter frame dumping both converge here for consistent formatting.
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto const& mod_rec{g_runtime.modules.index_unchecked(module_id)};
            auto const mod_name{resolve_module_display_name(mod_rec.module_name)};
            auto const fn_name{resolve_func_display_name(mod_rec.module_name, function_index)};

            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"#",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                frame_index,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" module=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                mod_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" func_idx=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                function_index);

            if(!fn_name.empty())
            {
                ::fast_io::io::perr(u8log_output_ul,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8" func_name=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    fn_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\"");
            }

            ::fast_io::io::perrln(u8log_output_ul, ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            return true;
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
# if defined(UWVM_USE_THREAD_LOCAL)
// The last JIT trap context is thread-local because generated code can trap while another thread is executing unrelated wasm.
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#   ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#   else
        [[__gnu__::__tls_model__("local-dynamic")]]
#   endif
#  endif
        inline thread_local ::std::uintptr_t llvm_jit_trap_return_address{};  // [global] [thread-local]
# endif
# if defined(UWVM_USE_THREAD_LOCAL)
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#   ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#   else
        [[__gnu__::__tls_model__("local-dynamic")]]
#   endif
#  endif
        inline thread_local ::std::uintptr_t llvm_jit_trap_frame_address{};  // [global] [thread-local]
# endif
# if defined(UWVM_USE_THREAD_LOCAL)
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#   ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#   else
        [[__gnu__::__tls_model__("local-dynamic")]]
#   endif
#  endif
        inline thread_local ::std::uintptr_t llvm_jit_trap_stack_pointer{};  // [global] [thread-local]
# endif
# if defined(UWVM_USE_THREAD_LOCAL)
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#   ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#   else
        [[__gnu__::__tls_model__("local-dynamic")]]
#   endif
#  endif
        inline thread_local llvm_jit_trap_kind llvm_jit_last_trap_kind{};  // [global] [thread-local]
# endif
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE && defined(UWVM_USE_THREAD_LOCAL)
        inline thread_local win64_context_t llvm_jit_win64_trap_caller_context{};  // [global] [thread-local]
        inline thread_local bool llvm_jit_win64_trap_caller_context_valid{};       // [global] [thread-local]
# endif

        [[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_call_stack_t
            get_runtime_llvm_jit_effective_call_stack_mode() noexcept;
        [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_call_stack_requested() noexcept;
        [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_check_requested() noexcept;
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] inline constexpr bool llvm_jit_win64_virtual_unwind_once(win64_context_t& context) noexcept;
        // Keep this helper as a real call boundary.  Win64 SEH backtracing uses it as the first stable host frame
        // after a generated-code trap; inlining it into a trap entry can make RtlCaptureContext/return-address
        // matching observe the trap entry's own layout and truncate the recovered Wasm stack.  The C++ `inline`
        // keyword is still intentional here for ODR-friendly header-style linkage; UWVM_NOINLINE preserves the
        // required unwind anchor.
        UWVM_NOINLINE inline constexpr void store_llvm_jit_win64_trap_caller_context(::std::uintptr_t expected_return_address,
                                                                                     ::std::uintptr_t trap_frame_address,
                                                                                     ::std::uintptr_t trap_stack_pointer) noexcept;
        [[nodiscard]] inline constexpr bool load_llvm_jit_win64_trap_caller_context(win64_context_t& context) noexcept;
# endif

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_call_stack_applies_to_current_compiler() noexcept
        {
            // Only JIT-capable runtime modes should consult native unwind policy. Interpreter-only execution reports instruction frames.
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            auto const compiler{runtime_mode::global_runtime_compiler};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            if(compiler == runtime_mode::runtime_compiler_t::llvm_jit_only) { return true; }
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(compiler == runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered) { return true; }
# endif
            return false;
        }

        inline constexpr void
            record_llvm_jit_unwind_entry(::std::size_t module_id, ::std::size_t function_index, ::std::uintptr_t address, bool raw_entry) noexcept
        {
            // Re-materialization can publish the same address again. Update in place so the sorted table remains unique by address.
            if(address == 0u) [[unlikely]] { return; }

            auto& entries{g_runtime.llvm_jit_unwind_entries};
            for(auto& entry: entries)
            {
                if(entry.address != address) { continue; }
                entry.module_id = module_id;
                entry.function_index = function_index;
                entry.raw_entry = raw_entry;
                return;
            }

            entries.push_back(llvm_jit_unwind_entry{address, module_id, function_index, raw_entry});
            ::std::sort(entries.begin(), entries.end(), [](auto const& lhs, auto const& rhs) constexpr noexcept { return lhs.address < rhs.address; });
        }

        inline constexpr void record_llvm_jit_code_range(::std::uintptr_t begin, ::std::uintptr_t size) noexcept
        {
            // Keep ranges merged. Trap reporting first tests whether an IP belongs to any generated section before doing heavier
            // wasm-frame resolution or DWARF inline lookup.
            if(begin == 0u || size == 0u) [[unlikely]] { return; }

            auto const end{begin + size};
            if(end <= begin) [[unlikely]] { return; }

            auto& ranges{g_runtime.llvm_jit_code_ranges};
            ranges.push_back(llvm_jit_code_range{begin, end});
            ::std::sort(ranges.begin(), ranges.end(), [](auto const& lhs, auto const& rhs) constexpr noexcept { return lhs.begin < rhs.begin; });

            ::std::size_t write_index{};
            for(::std::size_t read_index{}; read_index != ranges.size(); ++read_index)
            {
                auto const range{ranges.index_unchecked(read_index)};
                if(write_index == 0uz || range.begin > ranges.index_unchecked(write_index - 1uz).end)
                {
                    ranges.index_unchecked(write_index++) = range;
                    continue;
                }

                auto& previous{ranges.index_unchecked(write_index - 1uz)};
                if(range.end > previous.end) { previous.end = range.end; }
            }
            ranges.resize(write_index);
        }

        [[nodiscard]] inline constexpr bool llvm_jit_ip_in_code_range(::std::uintptr_t ip) noexcept
        {
            // Upper-bound lookup works because record_llvm_jit_code_range maintains a sorted non-overlapping range vector.
            auto const& ranges{g_runtime.llvm_jit_code_ranges};
            if(ranges.empty()) [[unlikely]] { return false; }

            auto const it{::std::upper_bound(ranges.begin(),
                                             ranges.end(),
                                             ip,
                                             [](auto const address, llvm_jit_code_range const& range) constexpr noexcept { return address < range.begin; })};
            if(it == ranges.begin()) [[unlikely]] { return false; }

            auto const& range{*(it - 1u)};
            return ip >= range.begin && ip < range.end;
        }

        [[nodiscard]] inline constexpr bool
            parse_llvm_jit_inline_frame_name(::uwvm2::utils::container::u8string_view name, ::std::size_t& module_id, ::std::size_t& function_index) noexcept
        {
            // The LLVM translator encodes wasm inline-frame identity in synthetic function names. Parsing it here lets trap reports
            // recover inlined wasm calls from DWARF instead of showing only the outer native entry.
            constexpr decltype(auto) prefix{u8"uwvm-inline:m="};
            constexpr decltype(auto) middle{u8":f="};
            constexpr ::std::size_t prefix_size{sizeof(prefix) - 1uz};
            constexpr ::std::size_t middle_size{sizeof(middle) - 1uz};
            if(name.size() <= prefix_size || ::std::memcmp(name.data(), prefix, prefix_size) != 0) { return false; }

            auto const name_begin{name.data()};
            auto const name_end{name_begin + name.size()};
            auto const middle_begin{::std::search(name_begin + prefix_size, name_end, middle, middle + middle_size)};
            if(middle_begin == name_end) { return false; }

            auto const parse_size{[](char8_t const* begin, char8_t const* end, ::std::size_t& out) constexpr noexcept -> bool
                                  {
                                      if(begin == end) { return false; }
                                      ::std::size_t value{};
                                      auto const [next, err]{::fast_io::parse_by_scan(begin, end, ::fast_io::mnp::dec_get<true, false>(value))};
                                      if(err != ::fast_io::parse_code::ok || next != end) { return false; }
                                      out = value;
                                      return true;
                                  }};

            ::std::size_t parsed_module_id{};
            ::std::size_t parsed_function_index{};
            if(!parse_size(name_begin + prefix_size, middle_begin, parsed_module_id)) { return false; }
            if(!parse_size(middle_begin + middle_size, name_end, parsed_function_index)) { return false; }

            module_id = parsed_module_id;
            function_index = parsed_function_index;
            return true;
        }

        [[nodiscard]] inline constexpr ::llvm::object::OwningBinary<::llvm::object::ObjectFile>
            copy_llvm_jit_object_for_debug(::llvm::object::ObjectFile const& obj,
                                           ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded,
                                           ::std::unique_ptr<::llvm::LoadedObjectInfo>& loaded_info) noexcept
        {
            // Some MCJIT/RuntimeDyld object formats, notably Mach-O in current LLVM builds, do not synthesize a separate
            // debug object for listeners.  Copy the original object buffer and pair it with a cloneable section-load map so
            // DWARFContext can still process relocations after the listener callback returns.
            auto copied_loaded_info{::std::make_unique<llvm_jit_copied_loaded_object_info>()};
            if(copied_loaded_info == nullptr) [[unlikely]] { return {}; }

            for(auto const& section: obj.sections())
            {
                auto const load_address{loaded.getSectionLoadAddress(section)};
                if(load_address == 0u) { continue; }

                copied_loaded_info->sections.push_back(llvm_jit_debug_loaded_section{.section_index = section.getIndex(),
                                                                                     .object_address = section.getAddress(),
                                                                                     .load_address = load_address,
                                                                                     .size = section.getSize(),
                                                                                     .text = section.isText()});
            }

            auto const object_buffer_ref{obj.getMemoryBufferRef()};
            auto copied_buffer{::llvm::MemoryBuffer::getMemBufferCopy(object_buffer_ref.getBuffer(), object_buffer_ref.getBufferIdentifier())};
            if(copied_buffer == nullptr) [[unlikely]] { return {}; }

            auto copied_object_expected{::llvm::object::ObjectFile::createObjectFile(copied_buffer->getMemBufferRef())};
            if(!copied_object_expected)
            {
                ::llvm::consumeError(copied_object_expected.takeError());
                return {};
            }

            loaded_info = ::std::move(copied_loaded_info);
            return ::llvm::object::OwningBinary<::llvm::object::ObjectFile>{::std::move(*copied_object_expected), ::std::move(copied_buffer)};
        }

        class uwvm_llvm_jit_debug_listener final : public ::llvm::JITEventListener
        {
        public:
            constexpr void
                notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& obj, ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
            {
                // MCJIT reports loaded sections through this listener. Capture text ranges for fast IP filtering and retain debug
                // objects so DWARF inline-frame lookup remains valid after finalization.
                for(auto const& section: obj.sections())
                {
                    if(!section.isText()) { continue; }

                    auto const load_address{static_cast<::std::uintptr_t>(loaded.getSectionLoadAddress(section))};
                    auto const size{static_cast<::std::uintptr_t>(section.getSize())};
                    record_llvm_jit_code_range(load_address, size);
                }

                auto debug_object{loaded.getObjectForDebug(obj)};
                auto loaded_info{loaded.clone()};
                if(debug_object.getBinary() == nullptr || loaded_info == nullptr)
                {
                    // Keep optimized inline recovery available on platforms where LLVM reports code ranges but not a
                    // ready-made debug object.  Without this fallback the unwind path can see only native entry frames.
                    loaded_info.reset();
                    debug_object = copy_llvm_jit_object_for_debug(obj, loaded, loaded_info);
                }
                if(debug_object.getBinary() == nullptr || loaded_info == nullptr) { return; }

                auto dwarf_context{::llvm::DWARFContext::create(
                    *debug_object.getBinary(),
                    ::llvm::DWARFContext::ProcessDebugRelocations::Process,
                    loaded_info.get(),
                    {},
                    [](auto) constexpr noexcept {},
                    [](auto) constexpr noexcept {},
                    true)};
                if(dwarf_context == nullptr) { return; }

                auto record{::uwvm2::utils::container::make_delete_owned<llvm_jit_debug_object>()};
                if(record == nullptr) { return; }

                for(auto const& section: obj.sections())
                {
                    auto const load_address{loaded.getSectionLoadAddress(section)};
                    if(load_address == 0u) { continue; }

                    record->loaded_sections.push_back(llvm_jit_debug_loaded_section{.section_index = section.getIndex(),
                                                                                    .object_address = section.getAddress(),
                                                                                    .load_address = load_address,
                                                                                    .size = section.getSize(),
                                                                                    .text = section.isText()});
                }

                record->object = ::std::move(debug_object);
                record->loaded_info.reset(loaded_info.release());
                record->dwarf_context.reset(dwarf_context.release());
                g_runtime.llvm_jit_debug_objects.push_back(::std::move(record));
            }
        };

        [[nodiscard]] inline constexpr uwvm_llvm_jit_debug_listener& get_uwvm_llvm_jit_debug_listener() noexcept
        {
            static uwvm_llvm_jit_debug_listener listener{};
            return listener;
        }

        struct resolved_llvm_jit_unwind_entry
        {
            llvm_jit_unwind_entry const* entry{};
            ::std::uintptr_t offset{};
        };

        [[nodiscard]] inline constexpr resolved_llvm_jit_unwind_entry resolve_llvm_jit_unwind_entry(::std::uintptr_t ip) noexcept
        {
            // Resolve to the nearest entry at or before the IP, after confirming the IP belongs to generated code.
            auto const& entries{g_runtime.llvm_jit_unwind_entries};
            if(entries.empty()) [[unlikely]] { return {}; }
            if(!llvm_jit_ip_in_code_range(ip)) [[unlikely]] { return {}; }

            auto const it{::std::upper_bound(entries.begin(),
                                             entries.end(),
                                             ip,
                                             [](auto const address, llvm_jit_unwind_entry const& entry) constexpr noexcept
                                             { return address < entry.address; })};
            if(it == entries.begin()) [[unlikely]] { return {}; }

            auto const entry_it{it - 1u};
            auto const offset{ip - entry_it->address};
            return resolved_llvm_jit_unwind_entry{::std::addressof(*entry_it), offset};
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view runtime_llvm_jit_unwind_backend_name() noexcept
        {
            // Diagnostic label for the selected native unwind implementation.
# if UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE
            return u8"libunwind";
# elif UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            return u8"win64-seh";
# elif UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE
            return u8"unwind.h";
# else
            return u8"unavailable";
# endif
        }

# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
        struct llvm_jit_unwind_backtrace_storage
        {
            // Keep trap output bounded. A trap should be diagnostic, not an unbounded stack walk.
            inline static constexpr ::std::size_t max_frames{64uz};

            ::std::uintptr_t frames[max_frames]{};
            ::std::size_t size{};
            ::std::size_t omit{};
        };

        struct llvm_jit_trap_frame_context
        {
            ::std::uintptr_t return_address{};
            ::std::uintptr_t trap_frame_address{};
            ::std::uintptr_t frame_pointer{};
            ::std::uintptr_t stack_pointer{};
        };

        [[nodiscard]] inline constexpr bool llvm_jit_frame_record_address_aligned(::std::uintptr_t address) noexcept
        { return (address % alignof(::std::uintptr_t)) == 0u; }

        [[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_load_frame_record_word(::std::uintptr_t address) noexcept
        {
            ::std::uintptr_t value{};
            ::std::memcpy(::std::addressof(value), reinterpret_cast<void const*>(address), sizeof(value));
            return value;
        }

        [[nodiscard]] inline constexpr bool llvm_jit_frame_pointer_link_plausible(::std::uintptr_t current_frame_pointer,
                                                                                  ::std::uintptr_t next_frame_pointer) noexcept
        {
            // A conservative plausibility check protects trap reporting from following corrupted or non-frame-pointer chains.
            constexpr ::std::uintptr_t max_frame_chain_delta{64u << 20u};

            if(next_frame_pointer == 0u || !llvm_jit_frame_record_address_aligned(next_frame_pointer)) { return false; }
            if(next_frame_pointer <= current_frame_pointer) { return false; }
            return next_frame_pointer - current_frame_pointer <= max_frame_chain_delta;
        }

        [[nodiscard]] inline constexpr llvm_jit_trap_frame_context get_llvm_jit_trap_frame_context(::std::uintptr_t return_address,
                                                                                                   ::std::uintptr_t trap_frame_address,
                                                                                                   ::std::uintptr_t trap_stack_pointer = 0u) noexcept
        {
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN
            if(return_address == 0u) [[unlikely]] { return {}; }

#   if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            if(trap_frame_address == 0u || !llvm_jit_frame_record_address_aligned(trap_frame_address)) [[unlikely]] { return {}; }
            // On Win64 the generated code passes the current JIT frame pointer explicitly.  The helper's own frame is
            // intentionally ignored: crossing the Win64 host-call bridge from the private Wasm ABI makes that frame an
            // unreliable place to recover the generated caller chain.
            auto const next_frame_pointer{llvm_jit_load_frame_record_word(trap_frame_address)};
            if(next_frame_pointer != 0u && !llvm_jit_frame_pointer_link_plausible(trap_frame_address, next_frame_pointer)) [[unlikely]] { return {}; }

            auto const stack_pointer{trap_stack_pointer == 0u ? trap_frame_address + 2u * sizeof(::std::uintptr_t) : trap_stack_pointer};
            if(stack_pointer <= trap_frame_address) [[unlikely]] { return {}; }
            return llvm_jit_trap_frame_context{.return_address = return_address,
                                               .trap_frame_address = trap_frame_address,
                                               .frame_pointer = trap_frame_address,
                                               .stack_pointer = stack_pointer};
#   else
            // The trap bridge now prefers the generated frame pointer when available.  Treat the supplied address as
            // the current frame record and let the frame walk/unwinder step from there; starting at the caller frame
            // would skip the first Wasm caller for detailed memory traps.
            ::std::uintptr_t frame_pointer{};
            if(trap_frame_address != 0u)
            {
                if(llvm_jit_frame_record_address_aligned(trap_frame_address))
                {
                    auto const next_frame_pointer{llvm_jit_load_frame_record_word(trap_frame_address)};
                    if(next_frame_pointer == 0u || llvm_jit_frame_pointer_link_plausible(trap_frame_address, next_frame_pointer))
                    {
                        frame_pointer = trap_frame_address;
                    }
                }
                if(frame_pointer == 0u && trap_stack_pointer == 0u) [[unlikely]] { return {}; }
            }
            else if(trap_stack_pointer == 0u) [[unlikely]] { return {}; }

            auto const stack_pointer{trap_stack_pointer == 0u ? frame_pointer + 2u * sizeof(::std::uintptr_t) : trap_stack_pointer};
            if(stack_pointer == 0u) [[unlikely]] { return {}; }
            return llvm_jit_trap_frame_context{.return_address = return_address,
                                               .trap_frame_address = trap_frame_address,
                                               .frame_pointer = frame_pointer,
                                               .stack_pointer = stack_pointer};
#   endif
#  else
            static_cast<void>(return_address);
            static_cast<void>(trap_frame_address);
            static_cast<void>(trap_stack_pointer);
            return {};
#  endif
        }

        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage
            capture_llvm_jit_frame_pointer_backtrace(llvm_jit_trap_frame_context const& trap_context, ::std::size_t omit) noexcept
        {
            // Frame-pointer walking is a fallback when platform unwind APIs cannot start from the generated trap frame.
            llvm_jit_unwind_backtrace_storage storage{};
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN
            auto frame_pointer{trap_context.frame_pointer};
            for(::std::size_t frame_index{}; storage.size != llvm_jit_unwind_backtrace_storage::max_frames; ++frame_index)
            {
                if(frame_pointer == 0u || !llvm_jit_frame_record_address_aligned(frame_pointer)) [[unlikely]] { break; }

                auto const return_address{llvm_jit_load_frame_record_word(frame_pointer + sizeof(::std::uintptr_t))};
                if(return_address == 0u) { break; }

                auto ip{return_address};
                if(ip != 0u) { --ip; }
                if(resolve_llvm_jit_unwind_entry(ip).entry == nullptr) { break; }

                if(frame_index >= omit) { storage.frames[storage.size++] = ip; }

                auto const next_frame_pointer{llvm_jit_load_frame_record_word(frame_pointer)};
                if(!llvm_jit_frame_pointer_link_plausible(frame_pointer, next_frame_pointer)) { break; }
                frame_pointer = next_frame_pointer;
            }
#  else
            static_cast<void>(trap_context);
            static_cast<void>(omit);
#  endif
            return storage;
        }

        [[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_align_stack_scan_address(::std::uintptr_t address) noexcept
        {
            constexpr auto alignment{static_cast<::std::uintptr_t>(alignof(::std::uintptr_t))};
            auto const mask{alignment - 1u};
            return (address + mask) & ~mask;
        }

        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage capture_llvm_jit_stack_scan_backtrace(llvm_jit_trap_frame_context const& trap_context,
                                                                                                               ::std::size_t omit) noexcept
        {
            // Async mmap traps can arrive without a reusable platform unwinder cursor.  If the interrupted FP chain cannot be
            // walked, scan a bounded window of the interrupted stack for frame records whose saved LR resolves to registered JIT code.
            llvm_jit_unwind_backtrace_storage storage{};
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN
            if(trap_context.stack_pointer == 0u) [[unlikely]] { return storage; }

            auto const scan_begin{llvm_jit_align_stack_scan_address(trap_context.stack_pointer)};
            if(scan_begin < trap_context.stack_pointer) [[unlikely]] { return storage; }

            constexpr ::std::uintptr_t max_stack_scan_bytes{256u << 10u};
            ::std::uintptr_t scan_end{};
            if(trap_context.frame_pointer != 0u && trap_context.frame_pointer >= scan_begin)
            {
                if(trap_context.frame_pointer > (::std::numeric_limits<::std::uintptr_t>::max)() - 2u * sizeof(::std::uintptr_t)) [[unlikely]]
                {
                    return storage;
                }
                scan_end = trap_context.frame_pointer + 2u * sizeof(::std::uintptr_t);
            }
            else
            {
                if(scan_begin > (::std::numeric_limits<::std::uintptr_t>::max)() - max_stack_scan_bytes) [[unlikely]] { return storage; }
                scan_end = scan_begin + max_stack_scan_bytes;
            }

            for(auto candidate{scan_begin}; candidate <= scan_end - 2u * sizeof(::std::uintptr_t); candidate += sizeof(::std::uintptr_t))
            {
                if(!llvm_jit_frame_record_address_aligned(candidate)) { continue; }

                auto const return_address{llvm_jit_load_frame_record_word(candidate + sizeof(::std::uintptr_t))};
                if(return_address == 0u) { continue; }

                auto ip{return_address};
                if(ip != 0u) { --ip; }
                if(resolve_llvm_jit_unwind_entry(ip).entry == nullptr) { continue; }

                auto const next_frame_pointer{llvm_jit_load_frame_record_word(candidate)};
                if(next_frame_pointer != 0u && !llvm_jit_frame_pointer_link_plausible(candidate, next_frame_pointer)) { continue; }

                auto frame_pointer{candidate};
                for(::std::size_t frame_index{}; storage.size != llvm_jit_unwind_backtrace_storage::max_frames; ++frame_index)
                {
                    auto const frame_return_address{llvm_jit_load_frame_record_word(frame_pointer + sizeof(::std::uintptr_t))};
                    if(frame_return_address == 0u) { break; }

                    auto frame_ip{frame_return_address};
                    if(frame_ip != 0u) { --frame_ip; }
                    if(resolve_llvm_jit_unwind_entry(frame_ip).entry == nullptr) { break; }

                    if(frame_index >= omit) { storage.frames[storage.size++] = frame_ip; }

                    auto const next_frame{llvm_jit_load_frame_record_word(frame_pointer)};
                    if(!llvm_jit_frame_pointer_link_plausible(frame_pointer, next_frame)) { break; }
                    frame_pointer = next_frame;
                }

                if(storage.size != 0uz) { break; }
            }
#  else
            static_cast<void>(trap_context);
            static_cast<void>(omit);
#  endif
            return storage;
        }

#  if UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE
        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage capture_llvm_jit_unwind_backtrace(::std::size_t omit) noexcept
        {
            llvm_jit_unwind_backtrace_storage storage{};

            unw_context_t context;
            if(unw_getcontext(::std::addressof(context)) != 0) [[unlikely]] { return storage; }

            unw_cursor_t cursor;
            if(unw_init_local(::std::addressof(cursor), ::std::addressof(context)) != 0) [[unlikely]] { return storage; }

            for(::std::size_t frame_index{}; storage.size != llvm_jit_unwind_backtrace_storage::max_frames; ++frame_index)
            {
                auto const step_result{unw_step(::std::addressof(cursor))};
                if(step_result <= 0) { break; }
                if(frame_index < omit) { continue; }

                unw_word_t ip{};
                if(unw_get_reg(::std::addressof(cursor), UNW_REG_IP, ::std::addressof(ip)) != 0) [[unlikely]] { break; }

                auto frame_ip{static_cast<::std::uintptr_t>(ip)};
                if(frame_index != 0uz && frame_ip != 0u) { --frame_ip; }
                storage.frames[storage.size++] = frame_ip;
            }

            return storage;
        }

        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage
            capture_llvm_jit_seeded_unwind_backtrace(llvm_jit_trap_frame_context const& trap_context, ::std::size_t omit) noexcept
        {
            llvm_jit_unwind_backtrace_storage storage{};
#   if UWVM2_RUNTIME_LLVM_JIT_HAS_SEEDED_LIBUNWIND_BACKTRACE
            if(trap_context.return_address == 0u || trap_context.stack_pointer == 0u || trap_context.frame_pointer == 0u) [[unlikely]] { return storage; }

            unw_context_t context;
            if(unw_getcontext(::std::addressof(context)) != 0) [[unlikely]] { return storage; }

            unw_cursor_t cursor;
            if(unw_init_local(::std::addressof(cursor), ::std::addressof(context)) != 0) [[unlikely]] { return storage; }

            auto ip{trap_context.return_address};
            if(ip != 0u) { --ip; }

            if(unw_set_reg(::std::addressof(cursor), UNW_REG_IP, static_cast<unw_word_t>(ip)) != 0) [[unlikely]] { return storage; }
            if(unw_set_reg(::std::addressof(cursor), UNW_REG_SP, static_cast<unw_word_t>(trap_context.stack_pointer)) != 0) [[unlikely]] { return storage; }
            if(unw_set_reg(::std::addressof(cursor), UWVM2_RUNTIME_LLVM_JIT_LIBUNWIND_FRAME_POINTER_REG, static_cast<unw_word_t>(trap_context.frame_pointer)) !=
               0) [[unlikely]]
            {
                return storage;
            }

            for(::std::size_t frame_index{}; storage.size != llvm_jit_unwind_backtrace_storage::max_frames; ++frame_index)
            {
                auto const step_result{unw_step(::std::addressof(cursor))};
                if(step_result <= 0) { break; }
                if(frame_index < omit) { continue; }

                unw_word_t frame_ip_word{};
                if(unw_get_reg(::std::addressof(cursor), UNW_REG_IP, ::std::addressof(frame_ip_word)) != 0) [[unlikely]] { break; }

                auto frame_ip{static_cast<::std::uintptr_t>(frame_ip_word)};
                if(frame_ip != 0u) { --frame_ip; }
                storage.frames[storage.size++] = frame_ip;
            }
#   else
            static_cast<void>(trap_context);
            static_cast<void>(omit);
#   endif
            return storage;
        }
#  elif UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage capture_llvm_jit_win64_seeded_unwind_backtrace(::std::size_t omit) noexcept
        {
            llvm_jit_unwind_backtrace_storage storage{};
            win64_context_t context{};
            if(!load_llvm_jit_win64_trap_caller_context(context)) [[unlikely]] { return storage; }

            for(::std::size_t frame_index{}; storage.size != llvm_jit_unwind_backtrace_storage::max_frames; ++frame_index)
            {
                if(!llvm_jit_win64_virtual_unwind_once(context)) { break; }
                if(frame_index < omit) { continue; }

                auto frame_ip{static_cast<::std::uintptr_t>(context.Rip)};
                if(frame_ip != 0u) { --frame_ip; }
                if(resolve_llvm_jit_unwind_entry(frame_ip).entry == nullptr) { break; }
                storage.frames[storage.size++] = frame_ip;
            }

            return storage;
        }

        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage capture_llvm_jit_unwind_backtrace(::std::size_t omit) noexcept
        {
            llvm_jit_unwind_backtrace_storage storage{};
            if(omit > static_cast<::std::size_t>((::std::numeric_limits<::std::uint32_t>::max)())) [[unlikely]] { return storage; }

            void* frames[llvm_jit_unwind_backtrace_storage::max_frames]{};
            auto const captured{::fast_io::win32::nt::RtlCaptureStackBackTrace(static_cast<::std::uint32_t>(omit),
                                                                               static_cast<::std::uint32_t>(llvm_jit_unwind_backtrace_storage::max_frames),
                                                                               frames,
                                                                               nullptr)};
            storage.size = static_cast<::std::size_t>(captured);
            for(::std::size_t i{}; i != storage.size; ++i)
            {
                auto frame_ip{reinterpret_cast<::std::uintptr_t>(frames[i])};
                if(i != 0uz && frame_ip != 0u) { --frame_ip; }
                storage.frames[i] = frame_ip;
            }

            return storage;
        }
#  else
        inline constexpr _Unwind_Reason_Code capture_llvm_jit_unwind_backtrace_frame(_Unwind_Context* context, void* opaque) noexcept
        {
            auto storage{static_cast<llvm_jit_unwind_backtrace_storage*>(opaque)};
            if(storage == nullptr) [[unlikely]] { return _URC_END_OF_STACK; }

            if(storage->omit != 0uz)
            {
                --storage->omit;
                return _URC_NO_REASON;
            }

            if(storage->size >= llvm_jit_unwind_backtrace_storage::max_frames) { return _URC_END_OF_STACK; }

            auto ip{static_cast<::std::uintptr_t>(_Unwind_GetIP(context))};
            if(ip != 0u) { --ip; }
            storage->frames[storage->size++] = ip;
            return _URC_NO_REASON;
        }

        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage capture_llvm_jit_unwind_backtrace(::std::size_t omit) noexcept
        {
            llvm_jit_unwind_backtrace_storage storage{};
            storage.omit = omit;
            static_cast<void>(_Unwind_Backtrace(capture_llvm_jit_unwind_backtrace_frame, ::std::addressof(storage)));
            return storage;
        }
#  endif
# endif

        [[maybe_unused]] UWVM_NOINLINE inline constexpr bool runtime_llvm_jit_unwind_probe_leaf() noexcept
        {
            // Keep this as a real call boundary so the probe verifies that the unwind backend can observe at least one caller frame.
# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
            auto const backtrace{capture_llvm_jit_unwind_backtrace(0uz)};
            auto const usable{backtrace.size >= 2uz};
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
            return usable;
# else
            return false;
# endif
        }

        [[maybe_unused]] UWVM_NOINLINE inline constexpr bool runtime_llvm_jit_unwind_probe_root() noexcept
        {
            auto const usable{runtime_llvm_jit_unwind_probe_leaf()};
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
            return usable;
        }

        [[maybe_unused]] [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_backend_usable() noexcept
        {
            // Cache the result because probing may instantiate a tiny JIT module and should not run repeatedly on every trap.
# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
            static bool const usable{runtime_llvm_jit_unwind_probe_root()};
            return usable;
# else
            return false;
# endif
        }

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_can_replace_instruction_frames() noexcept
        {
            // JIT unwind mode only requires generated code to carry registered native unwind tables.
            // Host uwvm2 frames may still be built without unwind tables; trap reporting captures
            // the JIT caller frame at the runtime trap boundary and seeds the JIT-only unwind from there.
# if UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES
            return true;
# else
            return false;
# endif
        }

        inline constexpr bool ensure_llvm_jit_native_target_initialized() noexcept;

# if UWVM2_RUNTIME_LLVM_JIT_HAS_EXECINFO_BACKTRACE || UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        struct runtime_llvm_jit_live_unwind_probe_state
        {
            inline static constexpr int max_frames{64};

            void* frames[max_frames]{};
            int frame_count{};
            ::std::uintptr_t function_address{};
        };

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_live_unwind_probe_saw_jit_frame(runtime_llvm_jit_live_unwind_probe_state const& state) noexcept
        {
            // The probe only needs to prove that a captured native backtrace contains the generated function's address range.
            constexpr ::std::uintptr_t probe_function_span{4096u};
            auto const begin{state.function_address};
            auto const end{begin + probe_function_span};
            if(begin == 0u) [[unlikely]] { return false; }

            auto const frame_count{state.frame_count < runtime_llvm_jit_live_unwind_probe_state::max_frames
                                       ? state.frame_count
                                       : runtime_llvm_jit_live_unwind_probe_state::max_frames};
            for(int i{}; i != frame_count; ++i)
            {
                auto ip{reinterpret_cast<::std::uintptr_t>(state.frames[i])};
                if(ip != 0u) { --ip; }
                if(ip >= begin && ip < end) { return true; }
            }

            return false;
        }

#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        UWVM_NOINLINE inline constexpr void runtime_llvm_jit_live_unwind_probe_capture(runtime_llvm_jit_live_unwind_probe_state* state,
                                                                                       ::std::uintptr_t frame_address,
                                                                                       ::std::uintptr_t stack_pointer) noexcept
        {
            if(state == nullptr) [[unlikely]] { return; }

#   if UWVM_HAS_BUILTIN(__builtin_return_address)
            auto const return_address{reinterpret_cast<::std::uintptr_t>(__builtin_return_address(0))};
#   else
            constexpr ::std::uintptr_t return_address{};
#   endif

            auto const trap_context{get_llvm_jit_trap_frame_context(return_address, frame_address)};
            if(trap_context.return_address != 0u && state->frame_count != runtime_llvm_jit_live_unwind_probe_state::max_frames)
            {
                state->frames[state->frame_count++] = reinterpret_cast<void*>(trap_context.return_address);
                win64_context_t context{};
                context.Rip = static_cast<::std::uint64_t>(trap_context.return_address);
                context.Rsp = static_cast<::std::uint64_t>(stack_pointer);
                context.Rbp = static_cast<::std::uint64_t>(frame_address);

                while(state->frame_count != runtime_llvm_jit_live_unwind_probe_state::max_frames)
                {
                    if(!llvm_jit_win64_virtual_unwind_once(context)) { break; }
                    state->frames[state->frame_count++] = reinterpret_cast<void*>(static_cast<::std::uintptr_t>(context.Rip));
                }
            }
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
        }
#  endif

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_live_unwind_probe() noexcept
        {
            // Build and execute a minimal generated function to test the actual runtime unwind environment, not just compile-time
            // availability of headers or libraries.
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]] { return false; }

            ::llvm::LLVMContext context{};
            auto module{::uwvm2::utils::container::make_delete_owned<::llvm::Module>(
                ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_llvm_string_ref(u8"uwvm2_runtime_llvm_jit_unwind_probe"),
                context)};

            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{target_builder.selectTarget()};
            if(target_machine == nullptr) [[unlikely]] { return false; }
            target_machine->setFastISel(true);

            module->setTargetTriple(target_machine->getTargetTriple());
            module->setDataLayout(target_machine->createDataLayout());

            auto const i32_type{::llvm::Type::getInt32Ty(context)};
            auto const void_type{::llvm::Type::getVoidTy(context)};
            auto const ptr_type{::llvm::PointerType::getUnqual(context)};
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_EXECINFO_BACKTRACE
            auto const backtrace_type{::llvm::FunctionType::get(i32_type, {ptr_type, i32_type}, false)};
            auto const backtrace_function{::llvm::Function::Create(backtrace_type,
                                                                   ::llvm::GlobalValue::ExternalLinkage,
                                                                   llvm_jit_translate_details::get_llvm_string_ref(u8"backtrace"),
                                                                   *module)};

            auto const probe_type{::llvm::FunctionType::get(void_type, {ptr_type, ptr_type}, false)};
#  else
            static_cast<void>(i32_type);
            auto const intptr_type{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto const capture_type{::llvm::FunctionType::get(void_type, {ptr_type, intptr_type, intptr_type}, false)};
            auto const capture_function{
                ::llvm::Function::Create(capture_type,
                                         ::llvm::GlobalValue::ExternalLinkage,
                                         llvm_jit_translate_details::get_llvm_string_ref(u8"uwvm2_runtime_llvm_jit_live_unwind_probe_capture"),
                                         *module)};

            auto const probe_type{::llvm::FunctionType::get(void_type, {ptr_type}, false)};
#  endif
            auto const probe_function{::llvm::Function::Create(probe_type,
                                                               ::llvm::GlobalValue::ExternalLinkage,
                                                               llvm_jit_translate_details::get_llvm_string_ref(u8"uwvm2_runtime_llvm_jit_live_unwind_probe"),
                                                               *module)};
            probe_function->setUWTableKind(::llvm::UWTableKind::Async);
            probe_function->addFnAttr(llvm_jit_translate_details::get_llvm_string_ref(u8"disable-tail-calls"),
                                      llvm_jit_translate_details::get_llvm_string_ref(u8"true"));
            probe_function->addFnAttr(llvm_jit_translate_details::get_llvm_string_ref(u8"frame-pointer"),
                                      llvm_jit_translate_details::get_llvm_string_ref(u8"all"));

            auto const entry_block{::llvm::BasicBlock::Create(context, llvm_jit_translate_details::get_llvm_string_ref(u8"entry"), probe_function)};
            ::llvm::IRBuilder<> builder{entry_block};
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_EXECINFO_BACKTRACE
            auto const frames_arg{probe_function->getArg(0)};
            auto const count_arg{probe_function->getArg(1)};
            auto const count_value{
                builder.CreateCall(backtrace_function, {frames_arg, ::llvm::ConstantInt::get(i32_type, runtime_llvm_jit_live_unwind_probe_state::max_frames)})};
            builder.CreateStore(count_value, count_arg);
#  else
            auto const state_arg{probe_function->getArg(0)};
            auto const register_name{::llvm::MDString::get(context, llvm_jit_translate_details::get_llvm_string_ref(u8"rbp"))};
            auto const register_metadata{::llvm::MDNode::get(context, {register_name})};
            auto const frame_address{
                builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {intptr_type}, {::llvm::MetadataAsValue::get(context, register_metadata)})};
            auto const stack_register_name{::llvm::MDString::get(context, llvm_jit_translate_details::get_llvm_string_ref(u8"rsp"))};
            auto const stack_register_metadata{::llvm::MDNode::get(context, {stack_register_name})};
            auto const stack_pointer{
                builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {intptr_type}, {::llvm::MetadataAsValue::get(context, stack_register_metadata)})};
            builder.CreateCall(capture_function, {state_arg, frame_address, stack_pointer});
#  endif
            builder.CreateRetVoid();

            if(::llvm::verifyModule(*module)) [[unlikely]] { return false; }

#  if UWVM2_RUNTIME_LLVM_JIT_HAS_EXECINFO_BACKTRACE
            ::llvm::sys::DynamicLibrary::AddSymbol(::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_llvm_string_ref(u8"backtrace"),
                                                   reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(&backtrace)));
#  else
            ::llvm::sys::DynamicLibrary::AddSymbol(
                ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_llvm_string_ref(u8"uwvm2_runtime_llvm_jit_live_unwind_probe_capture"),
                reinterpret_cast<void*>(reinterpret_cast<::std::uintptr_t>(&runtime_llvm_jit_live_unwind_probe_capture)));
#  endif

            auto raw_engine{
                ::llvm::EngineBuilder(llvm_module_owner_t{module.release()})
                    .setEngineKind(::llvm::EngineKind::JIT)
                    .setOptLevel(::llvm::CodeGenOptLevel::None)
                    .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{
                        ::uwvm2::utils::container::make_delete_owned<::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()
                            .release()})
                    .create(target_machine.get())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }
            static_cast<void>(target_machine.release());

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};
            engine->finalizeObject();

            auto const probe_address{reinterpret_cast<::std::uintptr_t>(engine->getPointerToFunction(probe_function))};
            if(probe_address == 0u) [[unlikely]] { return false; }

            runtime_llvm_jit_live_unwind_probe_state state{};
            state.function_address = probe_address;

#  if UWVM2_RUNTIME_LLVM_JIT_HAS_EXECINFO_BACKTRACE
            using probe_func_t = void (*)(void**, int*) noexcept;
            auto const probe_func{reinterpret_cast<probe_func_t>(probe_address)};
            probe_func(state.frames, ::std::addressof(state.frame_count));
#  else
            using probe_func_t = void (*)(runtime_llvm_jit_live_unwind_probe_state*) noexcept;
            auto const probe_func{reinterpret_cast<probe_func_t>(probe_address)};
            probe_func(::std::addressof(state));
#  endif

            return runtime_llvm_jit_live_unwind_probe_saw_jit_frame(state);
        }
# else
        [[nodiscard]] inline constexpr bool runtime_llvm_jit_live_unwind_probe() noexcept { return false; }
# endif

        enum class runtime_llvm_jit_unwind_probe_status : unsigned
        {
            ok,
            no_backend,
            no_frame_replacement,
            live_probe_failed
        };

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            runtime_llvm_jit_unwind_probe_status_reason(runtime_llvm_jit_unwind_probe_status st) noexcept
        {
            switch(st)
            {
                case runtime_llvm_jit_unwind_probe_status::ok: return u8"ok";
                case runtime_llvm_jit_unwind_probe_status::no_backend: return u8"no unwind backend was compiled into this build";
                case runtime_llvm_jit_unwind_probe_status::no_frame_replacement: return u8"generated-code unwind-frame replacement is not verified";
                case runtime_llvm_jit_unwind_probe_status::live_probe_failed: return u8"generated-code live unwind probe failed";
            }

            return u8"unknown unwind probe failure";
        }

        struct runtime_llvm_jit_unwind_probe_result
        {
            runtime_llvm_jit_unwind_probe_status status{};
            ::fast_io::unix_timestamp elapsed{};
        };

        [[nodiscard]] inline constexpr ::fast_io::unix_timestamp runtime_llvm_jit_unwind_probe_verbose_now() noexcept
        {
            ::fast_io::unix_timestamp ts{};
            if(!::uwvm2::uwvm::io::show_verbose) [[likely]] { return ts; }

# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                ts = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                // keep default timestamp
            }
# endif

            return ts;
        }

        inline constexpr void runtime_llvm_jit_unwind_probe_verbose_info(runtime_llvm_jit_unwind_probe_result const& result) noexcept
        {
            if(!::uwvm2::uwvm::io::show_verbose) [[likely]] { return; }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"LLVM JIT unwind self-check status=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                runtime_llvm_jit_unwind_probe_status_reason(result.status),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" backend=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                runtime_llvm_jit_unwind_backend_name(),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" time=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                result.elapsed,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"s. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                u8"[",
                                ::uwvm2::uwvm::io::get_local_realtime(),
                                u8"] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(verbose)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }

        [[nodiscard]] inline constexpr runtime_llvm_jit_unwind_probe_result runtime_llvm_jit_checked_unwind_probe_result() noexcept
        {
            // Static initialization makes the expensive live probe run once, while callers still receive a plain value.
            static runtime_llvm_jit_unwind_probe_result const result{[]() constexpr noexcept
                                                                     {
                                                                         auto const start_time{runtime_llvm_jit_unwind_probe_verbose_now()};

                                                                         runtime_llvm_jit_unwind_probe_status status{};
# if !UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
                                                                         status = runtime_llvm_jit_unwind_probe_status::no_backend;
# else
                                                                         if(!runtime_llvm_jit_unwind_can_replace_instruction_frames()) [[unlikely]]
                                                                         {
                                                                             status = runtime_llvm_jit_unwind_probe_status::no_frame_replacement;
                                                                         }
                                                                         else if(!runtime_llvm_jit_live_unwind_probe()) [[unlikely]]
                                                                         {
                                                                             status = runtime_llvm_jit_unwind_probe_status::live_probe_failed;
                                                                         }
                                                                         else
                                                                         {
                                                                             status = runtime_llvm_jit_unwind_probe_status::ok;
                                                                         }
# endif

                                                                         runtime_llvm_jit_unwind_probe_result ret{.status = status};
                                                                         if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                                                                         {
                                                                             ret.elapsed = runtime_llvm_jit_unwind_probe_verbose_now() - start_time;
                                                                         }
                                                                         runtime_llvm_jit_unwind_probe_verbose_info(ret);
                                                                         return ret;
                                                                     }()};
            return result;
        }

        [[nodiscard]] inline constexpr runtime_llvm_jit_unwind_probe_status runtime_llvm_jit_checked_unwind_probe_status() noexcept
        { return runtime_llvm_jit_checked_unwind_probe_result().status; }

        [[maybe_unused]] [[nodiscard]] inline constexpr runtime_llvm_jit_unwind_probe_status runtime_llvm_jit_static_unwind_probe_status() noexcept
        {
# if !UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
            return runtime_llvm_jit_unwind_probe_status::no_backend;
# else
            if(!runtime_llvm_jit_unwind_can_replace_instruction_frames()) { return runtime_llvm_jit_unwind_probe_status::no_frame_replacement; }
            return runtime_llvm_jit_unwind_probe_status::ok;
# endif
        }

        [[maybe_unused]] inline constexpr void runtime_llvm_jit_auto_unwind_fallback_warning_once(runtime_llvm_jit_unwind_probe_status st) noexcept
        {
            // Warn once so auto mode is transparent without flooding logs when many traps or entries consult the policy.
            static bool warned{};
            if(warned) { return; }
            warned = true;

            if(!::uwvm2::uwvm::io::show_runtime_warning) { return; }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"LLVM JIT call-stack auto mode could not use generated-code unwind (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                runtime_llvm_jit_unwind_probe_status_reason(st),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"); falling back to instruction call-stack frames.",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8" (runtime)\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            if(::uwvm2::uwvm::io::runtime_warning_fatal) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Convert warnings to fatal errors. ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(runtime)\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }
        }

        [[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_call_stack_t
            get_runtime_llvm_jit_effective_call_stack_mode() noexcept
        {
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;
            using runtime_llvm_jit_call_stack_t = runtime_mode::runtime_llvm_jit_call_stack_t;

            auto const requested{runtime_mode::global_runtime_llvm_jit_call_stack};
            if(requested != runtime_llvm_jit_call_stack_t::auto_policy) { return requested; }
            if(!runtime_llvm_jit_call_stack_applies_to_current_compiler()) { return runtime_llvm_jit_call_stack_t::instruction; }

# if !UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
            return runtime_llvm_jit_call_stack_t::instruction;
# else
            // Auto mode is on the startup hot path; avoid materializing a throwaway MCJIT probe here.
            // An explicit `-Rllvm-call-stack unwind` still performs the live check before committing to unwind mode.
            auto const st{runtime_llvm_jit_static_unwind_probe_status()};
            if(st == runtime_llvm_jit_unwind_probe_status::ok) { return runtime_llvm_jit_call_stack_t::unwind; }
            runtime_llvm_jit_auto_unwind_fallback_warning_once(st);
            return runtime_llvm_jit_call_stack_t::instruction;
# endif
        }

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_call_stack_requested() noexcept
        {
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;
            if(!runtime_llvm_jit_call_stack_applies_to_current_compiler()) { return false; }
            auto const mode{get_runtime_llvm_jit_effective_call_stack_mode()};
            return mode == runtime_mode::runtime_llvm_jit_call_stack_t::unwind || mode == runtime_mode::runtime_llvm_jit_call_stack_t::unwind_uncheck;
        }

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_check_requested() noexcept
        {
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;
            auto const requested{runtime_mode::global_runtime_llvm_jit_call_stack};
            return requested == runtime_mode::runtime_llvm_jit_call_stack_t::unwind;
        }

        [[noreturn]] inline constexpr void runtime_llvm_jit_unwind_call_stack_fatal(::uwvm2::utils::container::u8string_view reason) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"LLVM JIT unwind call-stack mode cannot be used: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                reason,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8". Use ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"-Rllvm-call-stack instruction",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" or ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"-Rllvm-call-stack none",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8".\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }

        inline constexpr void validate_runtime_llvm_jit_unwind_call_stack_or_fatal() noexcept
        {
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;
            if(runtime_mode::global_runtime_llvm_jit_call_stack != runtime_mode::runtime_llvm_jit_call_stack_t::unwind) { return; }

            auto const st{runtime_llvm_jit_checked_unwind_probe_status()};
            if(st != runtime_llvm_jit_unwind_probe_status::ok) [[unlikely]]
            {
                runtime_llvm_jit_unwind_call_stack_fatal(runtime_llvm_jit_unwind_probe_status_reason(st));
            }
        }

        template <typename Output>
        inline constexpr void dump_llvm_jit_unwind_call_stack_frames_for_trap(Output& u8log_output_ul,
                                                                              ::std::size_t& printed_frame_count,
                                                                              trap_kind current_trap_kind,
                                                                              printed_call_stack_frame_tracker* printed_frames = nullptr) noexcept
        {
# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
            ::std::size_t last_printed_module_id{::std::numeric_limits<::std::size_t>::max()};
            ::std::size_t last_printed_function_index{::std::numeric_limits<::std::size_t>::max()};
            auto const suppressed_frame{get_suppressed_call_stack_frame()};

            auto const print_wasm_frame{
                [&](::std::size_t module_id, ::std::size_t function_index) constexpr noexcept
                {
                    if(module_id == suppressed_frame.module_id && function_index == suppressed_frame.function_index) { return; }

                    if(printed_frames != nullptr && printed_frames->contains(module_id, function_index)) { return; }

                    if(printed_frame_count != 0uz && module_id == last_printed_module_id && function_index == last_printed_function_index) { return; }

                    if(dump_call_stack_frame_for_trap(u8log_output_ul, printed_frame_count, module_id, function_index))
                    {
                        if(printed_frames != nullptr) { printed_frames->record(module_id, function_index); }
                        last_printed_module_id = module_id;
                        last_printed_function_index = function_index;
                        ++printed_frame_count;
                    }
                }};

            auto const print_debug_inline_frames{
                [&](::std::uintptr_t ip) constexpr noexcept -> bool
                {
                    // Full JIT and tier-2 JIT can inline multiple Wasm functions into one native frame.  Native unwind
                    // entries alone would then report only the outer entry/raw wrapper, so prefer DWARF inline frames when
                    // they are available and print only the parseable Wasm frame records.
                    //
                    // The IP available from a trap is a native return address normalized by the unwinder.  On targets such
                    // as AArch64 that address can still land at the end of the trap call instruction, while DWARF inline
                    // ranges are often keyed at the instruction start.  Probe a small bounded window around the IP before
                    // falling back to the coarse native unwind entry.
                    bool printed{};
                    ::llvm::DILineInfoSpecifier specifier{::llvm::DILineInfoSpecifier::FileLineInfoKind::RawValue,
                                                          ::llvm::DILineInfoSpecifier::FunctionNameKind::ShortName};

                    auto const try_print_address{
                        [&](::llvm::object::SectionedAddress address) constexpr noexcept -> bool
                        {
                            auto const try_print_debug_object_address{
                                [&](llvm_jit_debug_object const& debug_object, ::llvm::object::SectionedAddress candidate_address) constexpr noexcept -> bool
                                {
                                    auto const inline_info{debug_object.dwarf_context->getInliningInfoForAddress(candidate_address, specifier)};
                                    auto const frame_count{inline_info.getNumberOfFrames()};
                                    if(frame_count == 0u) { return false; }

                                    bool matched{};
                                    for(unsigned i{}; i != frame_count; ++i)
                                    {
                                        ::std::size_t module_id{};
                                        ::std::size_t function_index{};
                                        auto const& frame_name{inline_info.getFrame(i).FunctionName};
                                        auto const frame_name_view{::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_uwvm_u8string_view(
                                            ::llvm::StringRef{frame_name.data(), frame_name.size()})};
                                        if(!parse_llvm_jit_inline_frame_name(frame_name_view, module_id, function_index)) { continue; }
                                        print_wasm_frame(module_id, function_index);
                                        printed = true;
                                        matched = true;
                                    }

                                    return matched;
                                }};

                            for(auto const& debug_object: g_runtime.llvm_jit_debug_objects)
                            {
                                if(debug_object == nullptr || debug_object->dwarf_context == nullptr) { continue; }
                                if(try_print_debug_object_address(*debug_object, address)) { return true; }

                                for(auto const& section: debug_object->loaded_sections)
                                {
                                    if(!section.text || section.load_address == 0u || section.size == 0u) { continue; }
                                    if(address.Address < section.load_address) { continue; }

                                    auto const section_offset{address.Address - section.load_address};
                                    if(section_offset >= section.size ||
                                       section_offset > ::std::numeric_limits<::std::uint64_t>::max() - section.object_address)
                                    {
                                        continue;
                                    }

                                    // Some object formats keep line-table rows section-relative even when a LoadedObjectInfo is
                                    // available.  Convert the native PC back to the original object section address and carry
                                    // the section index so DWARF inline lookup can find inlined Wasm callees after LLVM inlining.
                                    auto const object_address{section.object_address + section_offset};
                                    if(try_print_debug_object_address(*debug_object, {object_address, section.section_index})) { return true; }
                                }
                            }

                            return false;
                        }};

                    constexpr ::std::uintptr_t probe_stride{4u};
                    constexpr ::std::uintptr_t backward_probe_limit{64u};
                    for(::std::uintptr_t delta{}; delta <= backward_probe_limit; delta += probe_stride)
                    {
                        if(ip >= delta && try_print_address({ip - delta, ::llvm::object::SectionedAddress::UndefSection})) { return true; }
                    }

                    if(ip != ::std::numeric_limits<::std::uintptr_t>::max() && try_print_address({ip + 1u, ::llvm::object::SectionedAddress::UndefSection}))
                    {
                        return true;
                    }

                    return printed;
                }};

            auto const print_resolved_unwind_ip{[&](::std::uintptr_t ip) constexpr noexcept
                                                {
                                                    auto const resolved{resolve_llvm_jit_unwind_entry(ip)};
                                                    if(current_trap_kind == trap_kind::call_indirect_type_mismatch && printed_frame_count != 0uz &&
                                                       resolved.entry != nullptr && resolved.entry->raw_entry)
                                                    {
                                                        return;
                                                    }
                                                    if(print_debug_inline_frames(ip)) { return; }

                                                    if(resolved.entry == nullptr) { return; }
                                                    print_wasm_frame(resolved.entry->module_id, resolved.entry->function_index);
                                                }};

#  if !defined(UWVM_USE_THREAD_LOCAL)
            auto const llvm_jit_trap_return_address{get_llvm_jit_trap_return_address()};
            auto const llvm_jit_trap_frame_address{get_llvm_jit_trap_frame_address()};
            auto const llvm_jit_trap_stack_pointer{get_llvm_jit_trap_stack_pointer()};
            auto const llvm_jit_last_trap_kind{get_llvm_jit_last_trap_kind()};
#  endif

            if(llvm_jit_trap_return_address != 0u)
            {
                auto trap_ip{llvm_jit_trap_return_address};
                if(trap_ip != 0u) { --trap_ip; }
                print_resolved_unwind_ip(trap_ip);
            }

            auto const trap_frame_context{
                get_llvm_jit_trap_frame_context(llvm_jit_trap_return_address, llvm_jit_trap_frame_address, llvm_jit_trap_stack_pointer)};
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_LIBUNWIND_BACKTRACE
            auto backtrace{capture_llvm_jit_seeded_unwind_backtrace(trap_frame_context, 0uz)};
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_frame_pointer_backtrace(trap_frame_context, 0uz); }
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_stack_scan_backtrace(trap_frame_context, 0uz); }
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_unwind_backtrace(0uz); }
#  elif UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            auto backtrace{capture_llvm_jit_win64_seeded_unwind_backtrace(0uz)};
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_frame_pointer_backtrace(trap_frame_context, 0uz); }
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_stack_scan_backtrace(trap_frame_context, 0uz); }
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_unwind_backtrace(0uz); }
#  else
            auto backtrace{capture_llvm_jit_frame_pointer_backtrace(trap_frame_context, 0uz)};
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_stack_scan_backtrace(trap_frame_context, 0uz); }
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_unwind_backtrace(0uz); }
#  endif
            if(printed_frame_count > 1uz &&
               (llvm_jit_last_trap_kind == llvm_jit_trap_kind::call_indirect_type_mismatch || current_trap_kind == trap_kind::call_indirect_type_mismatch))
            {
                return;
            }
            for(::std::size_t i{}; i != backtrace.size; ++i)
            {
                auto const ip{backtrace.frames[i]};
                if(printed_frame_count != 0uz && llvm_jit_trap_return_address != 0u &&
                   (ip == llvm_jit_trap_return_address || ip == llvm_jit_trap_return_address - 1u))
                {
                    continue;
                }
                if(printed_frame_count != 0uz && resolve_llvm_jit_unwind_entry(ip).entry == nullptr) { continue; }
                print_resolved_unwind_ip(ip);
            }
# endif
        }
#endif

        inline constexpr void dump_call_stack_for_trap([[maybe_unused]] trap_kind current_trap_kind) noexcept
        {
            // Trap output is serialized on the log stream so concurrent traps do not interleave frame lines.
            // No copies will be made here.
            auto u8log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
            // Add raii locks while unlocking operations
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_osr)};
            // No copies will be made here.
            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_osr)};

            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Call stack:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            auto const& frames{get_call_stack().frames};
            auto const suppressed_frame{get_suppressed_call_stack_frame()};
            auto const n{frames.size()};
            ::std::size_t printed_frame_count{};
            printed_call_stack_frame_tracker printed_frames{};

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            auto const& tiered_snapshot_for_trap{get_call_stack().tiered_jit_entry_snapshot};
            auto const tiered_snapshot_active_for_native_order{tiered_snapshot_for_trap.active};
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
            auto const use_llvm_jit_unwind_call_stack{runtime_llvm_jit_unwind_call_stack_requested()};
            auto const defer_native_jit_frames{use_llvm_jit_unwind_call_stack && n != 0uz && current_trap_kind == trap_kind::call_indirect_type_mismatch
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                                               && !tiered_snapshot_active_for_native_order
# endif
            };
            if(use_llvm_jit_unwind_call_stack && !defer_native_jit_frames)
            {
                // In tiered OSR traps the current faulting frame may be generated code while its callers are still
                // interpreter frames. Print the native/DWARF-derived JIT frames first so the report starts at the leaf,
                // then append the remaining interpreter frames below.
                // Memory-OOB is allowed to use native unwind first because this helper prints only resolved wasm frames and the
                // duplicate tracker removes any frame later seen through TLS. This is important for OSR/inline traps where the
                // live logical stack can miss an optimized host-entry frame that DWARF/native unwind can still recover.
                // call_indirect type mismatch stays logical-first because the runtime already knows the exact mismatched table
                // target, and native raw-wrapper frames can otherwise make the diagnostic less precise.
                // The exception is an active tiered OSR snapshot: in that mixed stack the logical side starts at the interpreter
                // caller below the generated OSR body, while native/DWARF unwind is the only source for the optimized JIT leaf.
                dump_llvm_jit_unwind_call_stack_frames_for_trap(u8log_output_ul, printed_frame_count, current_trap_kind, ::std::addressof(printed_frames));
            }
#endif

            for(::std::size_t i{}; i != n; ++i)
            {
                auto const frame_index{n - 1uz - i};
                auto const& fr{frames.index_unchecked(frame_index)};
                if(fr.module_id == suppressed_frame.module_id && fr.function_index == suppressed_frame.function_index) { continue; }
                if(printed_frames.contains(fr.module_id, fr.function_index)) { continue; }
                if(dump_call_stack_frame_for_trap(u8log_output_ul, printed_frame_count, fr.module_id, fr.function_index))
                {
                    printed_frames.record(fr.module_id, fr.function_index);
                    ++printed_frame_count;
                }
            }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            auto const& tiered_snapshot{tiered_snapshot_for_trap};
            if(tiered_snapshot.active)
            {
                // Mixed tiered traps need one extra logical source: the interpreter stack captured at the T0-to-JIT boundary.
                // Native/DWARF frames still decide the generated leaf frames; this pass only fills caller frames that cannot be
                // discovered by the native unwinder after optimized raw-entry replacement or OSR.
                // Do this after the live logical stack so ordinary interpreter frames keep their natural order, but use the
                // duplicate tracker for every source. Without that deduplication, a trap taken just after an OSR/raw-entry handoff
                // can print the same wasm function once from the native unwind map and again from the saved boundary snapshot.
                for(::std::size_t i{}; i != tiered_snapshot.size; ++i)
                {
                    auto const frame_index{tiered_snapshot.size - 1uz - i};
                    auto const& fr{tiered_snapshot.frames.index_unchecked(frame_index)};
                    if(fr.module_id == suppressed_frame.module_id && fr.function_index == suppressed_frame.function_index) { continue; }
                    if(printed_frames.contains(fr.module_id, fr.function_index)) { continue; }
                    if(dump_call_stack_frame_for_trap(u8log_output_ul, printed_frame_count, fr.module_id, fr.function_index))
                    {
                        printed_frames.record(fr.module_id, fr.function_index);
                        ++printed_frame_count;
                    }
                }
            }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
            if(printed_frame_count == 0uz && use_llvm_jit_unwind_call_stack)
            {
                // If all logical sources were filtered out, make one final native unwind attempt so optimized JIT-only traps still
                // produce useful output instead of an empty call stack. The tracker remains shared so fallback frames cannot repeat
                // anything printed by an earlier source.
                dump_llvm_jit_unwind_call_stack_frames_for_trap(u8log_output_ul, printed_frame_count, current_trap_kind, ::std::addressof(printed_frames));
            }
#endif

            ::fast_io::io::perrln(u8log_output_ul);
        }

#if UWVM_HAS_CPP_ATTRIBUTE(clang::disable_tail_calls)
        [[clang::disable_tail_calls]]
#endif
        inline constexpr void print_trap_fatal_message(trap_kind k) noexcept
        {
            // Print the fatal headline separately from the call stack so memory traps can reuse the same formatting after printing
            // detailed memory diagnostics.
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Runtime crash (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                trap_kind_name(k),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8")\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }

#if UWVM_HAS_CPP_ATTRIBUTE(clang::disable_tail_calls)
        [[clang::disable_tail_calls]]
#endif
        UWVM_NOINLINE inline constexpr void trap_fatal(trap_kind k) noexcept
        {
            // Keep termination behind a volatile function pointer and signal fence so compilers do not fold away diagnostic work
            // before noreturn termination.
            print_trap_fatal_message(k);

            dump_call_stack_for_trap(k);

            using terminate_func_t = void (*)() noexcept;
            terminate_func_t volatile terminate_func{::fast_io::fast_terminate};
            terminate_func();
            ::std::atomic_signal_fence(::std::memory_order_seq_cst);
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        inline constexpr void store_llvm_jit_trap_context(llvm_jit_trap_kind k,
                                                          ::std::uintptr_t return_address,
                                                          ::std::uintptr_t frame_address = 0u,
                                                          ::std::uintptr_t stack_pointer = 0u) noexcept
        {
            // Generated code passes just enough native context for later trap reporting. Store it before the generic fatal path runs.
# if defined(UWVM_USE_THREAD_LOCAL)
            llvm_jit_last_trap_kind = k;
            llvm_jit_trap_return_address = return_address;
            llvm_jit_trap_frame_address = frame_address;
            llvm_jit_trap_stack_pointer = stack_pointer;
# else
            get_llvm_jit_last_trap_kind() = k;
            get_llvm_jit_trap_return_address() = return_address;
            get_llvm_jit_trap_frame_address() = frame_address;
            get_llvm_jit_trap_stack_pointer() = stack_pointer;
# endif
        }

# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] inline constexpr bool llvm_jit_win64_virtual_unwind_once(win64_context_t& context) noexcept
        {
            if(context.Rip == 0u) [[unlikely]] { return false; }

            // RtlLookupFunctionEntry expects a control PC inside the call instruction's containing function.  Trap
            // helpers usually give us a return address, so bias by one byte to make the lookup land in the caller.
            auto control_pc{context.Rip};
            if(control_pc != 0u) { --control_pc; }

            ::std::uint64_t image_base{};
            auto const function_entry{::fast_io::win32::nt::RtlLookupFunctionEntry(control_pc, ::std::addressof(image_base), nullptr)};
            if(function_entry == nullptr)
            {
                // Leaf functions may have no .pdata entry on Win64.  In that case the architectural convention is a
                // simple return-address pop from RSP, which is the same fallback used by the platform unwinder.
                if(context.Rsp == 0u || !llvm_jit_frame_record_address_aligned(static_cast<::std::uintptr_t>(context.Rsp))) [[unlikely]] { return false; }

                auto const return_address{llvm_jit_load_frame_record_word(static_cast<::std::uintptr_t>(context.Rsp))};
                if(return_address == 0u) [[unlikely]] { return false; }
                context.Rip = static_cast<::std::uint64_t>(return_address);
                context.Rsp += sizeof(::std::uintptr_t);
                return true;
            }

            void* handler_data{};
            ::std::uint64_t establisher_frame{};
            // RtlVirtualUnwind applies the registered .pdata/.xdata metadata to mutate CONTEXT in place from the current
            // frame to its caller.  The image base must match the base used when the JIT registered the function table.
            static_cast<void>(::fast_io::win32::nt::RtlVirtualUnwind(::fast_io::win32::win64_unwind_flag_nhandler,
                                                                     image_base,
                                                                     control_pc,
                                                                     function_entry,
                                                                     ::std::addressof(context),
                                                                     ::std::addressof(handler_data),
                                                                     ::std::addressof(establisher_frame),
                                                                     nullptr));
            return context.Rip != 0u;
        }

        UWVM_NOINLINE inline constexpr void store_llvm_jit_win64_trap_caller_context(::std::uintptr_t expected_return_address,
                                                                                     ::std::uintptr_t trap_frame_address,
                                                                                     ::std::uintptr_t trap_stack_pointer) noexcept
        {
            win64_context_t context{};
            bool valid{};

            // Start from the generated caller's architectural state when the trap helper was entered.  Win64 uses
            // frame-pointer offsets in SEH unwind metadata, so RSP must come from generated code instead of being
            // reconstructed from RBP as a simple frame-record address.  This path is preferred because it avoids using
            // the helper's mixed-ABI frame as the root of the JIT stack walk.
            if(expected_return_address != 0u && trap_frame_address != 0u && trap_stack_pointer != 0u)
            {
                context.Rip = static_cast<::std::uint64_t>(expected_return_address);
                context.Rsp = static_cast<::std::uint64_t>(trap_stack_pointer);
                context.Rbp = static_cast<::std::uint64_t>(trap_frame_address);
                valid = true;
            }
            else if(expected_return_address != 0u)
            {
                // Best-effort fallback for older generated code or partially available trap context: capture the helper
                // context and unwind a few frames until the expected JIT return address is recovered.
                ::fast_io::win32::nt::RtlCaptureContext(::std::addressof(context));
                for(unsigned i{}; i != 4u; ++i)
                {
                    if(static_cast<::std::uintptr_t>(context.Rip) == expected_return_address)
                    {
                        valid = true;
                        break;
                    }
                    if(!llvm_jit_win64_virtual_unwind_once(context)) { break; }
                }
            }
#  if defined(UWVM_USE_THREAD_LOCAL)
            llvm_jit_win64_trap_caller_context = valid ? context : win64_context_t{};
            llvm_jit_win64_trap_caller_context_valid = valid;
#  else
            get_llvm_jit_win64_trap_caller_context() = valid ? context : win64_context_t{};
            get_llvm_jit_win64_trap_caller_context_valid() = valid;
#  endif
        }

        [[nodiscard]] inline constexpr bool load_llvm_jit_win64_trap_caller_context(win64_context_t& context) noexcept
        {
#  if defined(UWVM_USE_THREAD_LOCAL)
            if(!llvm_jit_win64_trap_caller_context_valid) { return false; }
            context = llvm_jit_win64_trap_caller_context;
#  else
            if(!get_llvm_jit_win64_trap_caller_context_valid()) { return false; }
            context = get_llvm_jit_win64_trap_caller_context();
#  endif
            return context.Rip != 0u;
        }
# endif

        [[nodiscard, maybe_unused]] inline constexpr ::std::uintptr_t llvm_jit_signal_trap_return_address(::std::uintptr_t instruction_address) noexcept
        {
            if(instruction_address == 0u) { return 0u; }
            if(instruction_address == (::std::numeric_limits<::std::uintptr_t>::max)()) [[unlikely]] { return instruction_address; }
            return instruction_address + 1u;
        }
#endif

        inline constexpr void print_memory_out_of_bounds_trap(::uwvm2::object::memory::error::memory_error_t const& memerr) noexcept
        {
            // Memory traps include structured memory-error details before the generic runtime trap headline and wasm call stack.
            ::uwvm2::object::memory::error::output_memory_error_line(memerr);
            print_trap_fatal_message(trap_kind::memory_out_of_bounds);
            dump_call_stack_for_trap(trap_kind::memory_out_of_bounds);
        }

#if defined(UWVM_SUPPORT_MMAP)
        inline constexpr void print_mmap_memory_out_of_bounds_trap(::uwvm2::object::memory::error::mmap_memory_error_t const& memerr) noexcept
        {
# if defined(UWVM_RUNTIME_LLVM_JIT)
            store_llvm_jit_trap_context(::uwvm2::runtime::lib::llvm_jit_trap_kind::memory_out_of_bounds,
                                        llvm_jit_signal_trap_return_address(memerr.instruction_address),
                                        memerr.frame_address,
                                        memerr.stack_pointer);
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            store_llvm_jit_win64_trap_caller_context(llvm_jit_signal_trap_return_address(memerr.instruction_address),
                                                     memerr.frame_address,
                                                     memerr.stack_pointer);
#  endif
# endif
            ::uwvm2::object::memory::error::output_mmap_memory_error_line(memerr);
            print_trap_fatal_message(trap_kind::memory_out_of_bounds);
            dump_call_stack_for_trap(trap_kind::memory_out_of_bounds);
        }

        inline constexpr void ensure_memory_signal_trap_bridge_initialized() noexcept
        { ::uwvm2::object::memory::signal::set_mmap_memory_out_of_bounds_handler(print_mmap_memory_out_of_bounds_trap); }
#else
        // Non-mmap builds cannot receive guard-page memory traps, so the bridge initializer is intentionally a no-op.
        inline constexpr void ensure_memory_signal_trap_bridge_initialized() noexcept {}
#endif

#ifdef UWVM_CPP_EXCEPTIONS
        [[noreturn]] inline constexpr void
            print_and_terminate_compile_validation_error(::uwvm2::utils::container::u8string_view module_name,
                                                         ::uwvm2::validation::error::code_validation_error_impl const& v_err) noexcept
        {
            // Compilation may run after parsing, especially in lazy modes. Reconstruct validator diagnostics from parser module
            // storage so users see the same source indication as eager validation.
            // Try to print detailed validator diagnostics (same format as `uwvm/runtime/validator/validate.h`).
            auto const fallback_and_terminate{
                [&]() constexpr noexcept
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Validation error during compilation (module=\"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        module_name,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\").\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }};

            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]] { fallback_and_terminate(); }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) [[unlikely]] { fallback_and_terminate(); }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr || wf->binfmt_ver != 1u) [[unlikely]] { fallback_and_terminate(); }

            auto const file_name{wf->file_name};
            auto const& module_storage{wf->wasm_module_storage.wasm_binfmt_ver1_storage};

            auto const module_begin{module_storage.module_span.module_begin};
            auto const module_end{module_storage.module_span.module_end};
            if(module_begin == nullptr || module_end == nullptr) [[unlikely]] { fallback_and_terminate(); }

            ::uwvm2::uwvm::utils::memory::print_memory const memory_printer{module_begin, v_err.err_curr, module_end};

            ::uwvm2::validation::error::error_output_t errout{};
            errout.module_begin = module_begin;
            errout.err = v_err;
            errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(::uwvm2::uwvm::utils::ansies::put_color);
# if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
            errout.flag.win32_use_text_attr = static_cast<::std::uint_least8_t>(!::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b);
# endif

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                // 1
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validation error in WebAssembly Code (module=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\", file=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                file_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\").\n",
                                // 2
                                errout,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\n"
                                // 3
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Validator Memory Indication: ",
                                memory_printer,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n\n");

            ::fast_io::fast_terminate();
        }
#endif

        enum class valtype_kind : unsigned
        {
            // Function signatures can come from wasm enum arrays or from C ABI raw u8 type arrays. The view normalizes both.
            wasm_enum,
            raw_u8
        };

        struct valtype_vec_view
        {
            valtype_kind kind{};
            void const* data{};
            ::std::size_t size{};

            inline constexpr ::std::uint_least8_t at(::std::size_t i) const noexcept
            {
                // Bounds and null checks make signature comparison robust even for malformed preload metadata.
                if(i >= size) [[unlikely]] { return 0u; }

                if(kind == valtype_kind::raw_u8)
                {
                    auto const p{static_cast<::std::uint_least8_t const*>(data)};
                    if(p == nullptr) [[unlikely]] { return 0u; }
                    return p[i];
                }
                else
                {
                    using wasm_value_type_const_ptr_t_may_alias UWVM_GNU_MAY_ALIAS = wasm_value_type const*;

                    auto const p{static_cast<wasm_value_type_const_ptr_t_may_alias>(data)};
                    if(p == nullptr) [[unlikely]] { return 0u; }
                    return static_cast<::std::uint_least8_t>(p[i]);
                }
            }
        };

        struct func_sig_view
        {
            // Lightweight non-owning signature view used by import caches, call_indirect checks, and host ABI validation.
            valtype_vec_view params{};
            valtype_vec_view results{};
        };

        // Precomputed import dispatch table for O(1) imported calls.
        // This is built once before execution (after uwvm runtime initialization + compilation).
        struct cached_import_target
        {
            // Imports are fully resolved before execution. The cache stores the leaf target and ABI byte sizes so call bridges do
            // not chase import chains or recompute signatures at every call.
            enum class kind : unsigned
            {
                defined,
                local_imported,
                dl,
                weak_symbol
            };

            kind k{};
            call_stack_frame frame{};
            func_sig_view sig{};
            ::std::size_t param_bytes{};
            ::std::size_t result_bytes{};
            preload_module_memory_attribute_t const* preload_module_memory_attribute{};

            union
            {
                struct
                {
                    runtime_local_func_storage_t const* runtime_func{};
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    compiled_local_func_t const* compiled_func{};
#endif
                } defined;

                runtime_imported_func_storage_t::local_imported_target_t local_imported;

                capi_function_t const* capi_ptr;
            } u{};
        };

        inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<cached_import_target>> g_import_call_cache{};  // [global]

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        struct cached_wasip1_runtime_module_context
        {
            // Per-runtime-module WASI context cache. This makes the normal no-override path fast while still honoring per-module
            // override configuration for preloaded modules.
            wasip1_env_type* env{};
            native_memory_t* memory0{};
            bool import_visible{true};
        };

        inline ::uwvm2::utils::container::vector<cached_wasip1_runtime_module_context> g_wasip1_runtime_module_context_cache{};  // [global]
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // =========================================================================
        // Lazy interpreter scheduling
        // -------------------------------------------------------------------------
        // This section does not execute wasm directly. It prepares lazy compile-unit
        // metadata, orders background prefetch work, and records runtime statistics.
        // The actual execution path later calls ensure_lazy_defined_function_compiled
        // before entering an interpreter function body.
        //
        // Coverage invariants:
        // - Request contexts must outlive scheduler queue entries.
        // - Prefetch order is a hint; demand compilation remains authoritative.
        // - Compile-unit state is read atomically because workers and execution threads race intentionally.
        // - Validation errors are stored per local function so background failures can report the right module/function.
        // - Entry-biased prefetch is temporary and must not prevent eventual module-wide coverage.
        // - Statistics are best-effort and must not add synchronization to hot interpreter paths.
        // =========================================================================
        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_lazy_background_target_tranopt() noexcept
        {
            // Background lazy compilation uses the same ABI-aware register-cache policy as foreground interpreter compilation so
            // lazily materialized functions are indistinguishable from eagerly translated ones.
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t res{};

# if !(defined(__pdp11) || (defined(__wasm__) && !defined(__wasm_tail_call__)))
            res.is_tail_call = true;
# endif

# if defined(__ARM_PCS_AAPCS64) || defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
            // AArch64 has enough integer and SIMD argument registers to cache several stack-top values without heavy spilling.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
# elif defined(__arm__) || defined(_M_ARM)
            // ARM32 keeps the background interpreter cache disabled because the ABI has too few spare argument registers.
# elif ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                    \
     (!defined(_WIN32) || (defined(__GNUC__) || defined(__clang__)))
            // SysV x86_64, including GNU/Clang SysV-on-Windows builds, leaves enough argument registers after the three fixed args.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 14uz;
# elif defined(_WIN32) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                 \
     !(defined(__GNUC__) || defined(__clang__))
            // Microsoft x64 has only four integer argument registers and shadow-space costs, so this experimental cache stays off.
#  if 0
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 4uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 4uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 4uz;
#  endif
# elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
            // PowerPC64 ABIs provide wide integer/floating argument windows that can carry stack-top cache values efficiently.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
# elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
#  if defined(__riscv_float_abi_soft) || defined(__riscv_float_abi_single)
            // RV64 soft/single-float ABIs route more scalar values through integer registers, so fp cache slots share that window.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  else
            // RV64 hard-float ABIs can split integer and floating stack-top cache ranges across their natural register classes.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
#  endif
# elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
            // RV32 does not reserve a cache window here; register pressure usually outweighs the byte-stack savings.
# elif defined(__loongarch__) && defined(__loongarch64)
#  if defined(__loongarch_soft_float) || defined(__loongarch_single_float)
            // LoongArch64 soft/single-float follows the integer-window strategy used by other reduced-fp ABIs.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  else
            // LoongArch64 hard-float has separate integer and fp argument windows suitable for stack-top caching.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
#  endif
# elif defined(__loongarch__)
            // 32-bit LoongArch leaves caching disabled for the same register-pressure reason as other 32-bit targets.
# elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
#  if defined(__mips_n32) || defined(__mips_n64)
#   if defined(__mips_soft_float)
            // MIPS n32/n64 soft-float keeps scalar caches in the available general-purpose argument registers.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#   else
            // MIPS n32/n64 hard-float has a narrower practical cache window, so only a few stack-top values are assigned.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#   endif
#  endif
# elif defined(__s390x__)
            // s390x has enough call argument slots for a small integer/fp stack-top cache after the fixed interpreter arguments.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
# elif defined(__wasm__)
            // WebAssembly hosts do not benefit from this native-register cache model.
# endif

            return res;
        }

        [[nodiscard]] inline constexpr ::std::size_t lazy_compile_unit_code_size(compiled_module_record const& rec, ::std::size_t local_function_index) noexcept
        {
            // Code size is a scheduling heuristic only. Invalid metadata returns zero so sorting remains total and non-throwing.
            if(local_function_index >= rec.lazy_compiled.functions.size()) [[unlikely]] { return 0uz; }
            auto const& fn{rec.lazy_compiled.functions.index_unchecked(local_function_index)};
            if(fn.primary_cu_index >= rec.lazy_compiled.compile_units.size()) [[unlikely]] { return 0uz; }
            return rec.lazy_compiled.compile_units.index_unchecked(fn.primary_cu_index).code_size;
        }

        inline constexpr void prepare_lazy_background_request_contexts(compiled_module_record& rec) noexcept
        {
            // Store one request context per local function. Scheduler requests only carry a pointer, so the context vector must stay
            // stable after initialization.
            auto const local_n{rec.runtime_module == nullptr ? 0uz : rec.runtime_module->local_defined_function_vec_storage.size()};
            rec.lazy_background_errors.clear();
            rec.lazy_background_errors.resize(local_n);
            rec.lazy_background_request_contexts.clear();
            rec.lazy_background_request_contexts.resize(local_n);
            rec.lazy_prefetch_order.clear();
            rec.lazy_prefetch_order.resize(local_n);
            rec.lazy_prefetch_cursor = 0uz;

            if(local_n == 0uz || rec.runtime_module == nullptr) { return; }

            for(::std::size_t i{}; i != local_n; ++i)
            {
                auto const& fn{rec.lazy_compiled.functions.index_unchecked(i)};
                auto& ctx{rec.lazy_background_request_contexts.index_unchecked(i)};
                ctx.curr_module = rec.runtime_module;
                ctx.lazy_storage = ::std::addressof(rec.lazy_compiled);
                ctx.options = rec.lazy_compile_options;
                ctx.compile_unit_index = fn.primary_cu_index;
                ctx.err = ::std::addressof(rec.lazy_background_errors.index_unchecked(i));
                ctx.module_name = rec.module_name;
                rec.lazy_prefetch_order.index_unchecked(i) = i;
            }

            ::std::sort(rec.lazy_prefetch_order.begin(),
                        rec.lazy_prefetch_order.end(),
                        [&](::std::size_t a, ::std::size_t b) constexpr noexcept
                        {
                            // Small functions first reduce time-to-first-background-progress and avoid monopolizing workers with
                            // one large function before easy wins are compiled.
                            auto const size_a{lazy_compile_unit_code_size(rec, a)};
                            auto const size_b{lazy_compile_unit_code_size(rec, b)};
                            if(size_a != size_b) { return size_a < size_b; }
                            return a < b;
                        });
        }

        inline constexpr void prioritize_lazy_background_entry(compiled_module_record& rec, ::std::size_t preferred_local_index) noexcept
        {
            // Move the likely entry target to the front without rebuilding the whole order. This keeps cold-start behavior responsive.
            if(preferred_local_index >= rec.lazy_prefetch_order.size()) [[unlikely]] { return; }

            auto const it{::std::find(rec.lazy_prefetch_order.begin(), rec.lazy_prefetch_order.end(), preferred_local_index)};
            if(it == rec.lazy_prefetch_order.end()) [[unlikely]] { return; }

            ::std::rotate(rec.lazy_prefetch_order.begin(), it, it + 1uz);
            rec.lazy_prefetch_order.index_unchecked(0) = preferred_local_index;
            rec.lazy_prefetch_cursor = 0uz;
        }

        [[nodiscard]] inline constexpr bool enqueue_lazy_background_requests_for_module(compiled_module_record& rec,
                                                                                        ::uwvm2::utils::thread::lazy_compile_scheduler& scheduler) noexcept
        {
            // Refill lazily: each pass advances a cursor and skips functions already claimed by demand compilation.
            auto const local_n{rec.lazy_prefetch_order.size()};
            if(local_n == 0uz || rec.runtime_module == nullptr) [[unlikely]] { return false; }

            constexpr auto curr_target_tranopt{get_lazy_background_target_tranopt()};
            bool queued{};

            for(; rec.lazy_prefetch_cursor < local_n; ++rec.lazy_prefetch_cursor)
            {
                auto const local_index{rec.lazy_prefetch_order.index_unchecked(rec.lazy_prefetch_cursor)};
                if(local_index >= rec.lazy_compiled.functions.size()) [[unlikely]] { continue; }
                auto& fn{rec.lazy_compiled.functions.index_unchecked(local_index)};
                auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
                if(st != ::uwvm2::utils::thread::lazy_compile_state::uncompiled) { continue; }

                auto& ctx{rec.lazy_background_request_contexts.index_unchecked(local_index)};
                auto request{::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::make_lazy_compile_request<curr_target_tranopt>(ctx, 0u)};
                if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { continue; }

                if(!scheduler.try_request(request)) [[unlikely]] { break; }
                queued = true;
            }

            return queued;
        }

        inline constexpr bool lazy_background_refill_callback(void*, ::uwvm2::utils::thread::lazy_compile_scheduler& scheduler) noexcept
        {
            // The prefetch lock serializes cursor updates across scheduler workers without requiring each module record to own a lock.
            if(g_runtime.lazy_prefetch_lock.test_and_set(::std::memory_order_acquire)) { return false; }

            bool queued{};
            auto const module_count{g_runtime.modules.size()};
            auto const preferred_module_id{g_runtime.lazy_prefetch_module_id};

            if(preferred_module_id < module_count)
            {
                auto& preferred_rec{g_runtime.modules.index_unchecked(preferred_module_id)};
                if(enqueue_lazy_background_requests_for_module(preferred_rec, scheduler)) { queued = true; }
            }

            for(::std::size_t module_id{}; module_id != module_count; ++module_id)
            {
                if(module_id == preferred_module_id) { continue; }
                auto& rec{g_runtime.modules.index_unchecked(module_id)};
                if(enqueue_lazy_background_requests_for_module(rec, scheduler)) { queued = true; }
            }

            g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);
            return queued;
        }
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr ::fast_io::unix_timestamp lazy_clock_now() noexcept
        {
            // Runtime logging should be best-effort; clock failures simply return the default timestamp.
            ::fast_io::unix_timestamp ts{};
# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                ts = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
            }
# endif
            return ts;
        }

        [[nodiscard]] inline constexpr ::std::size_t lazy_total_function_count() noexcept
        {
            // Count local-defined functions across all runtime modules for end-of-run lazy statistics.
            ::std::size_t total{};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
               || ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                      ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
#  endif
            )
            {
                for(auto const& rec: g_runtime.modules) { total += rec.llvm_jit_lazy_compiled.functions.size(); }
                return total;
            }
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            for(auto const& rec: g_runtime.modules) { total += rec.lazy_compiled.functions.size(); }
# endif
            return total;
        }

        [[nodiscard]] inline constexpr ::std::size_t lazy_compiled_function_count() noexcept
        {
            // Count terminal compiled states. Lazy failure is intentionally excluded so the summary reflects usable functions.
            ::std::size_t total{};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
               || ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                      ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
#  endif
            )
            {
                for(auto const& rec: g_runtime.modules)
                {
                    for(auto const& fn: rec.llvm_jit_lazy_compiled.functions)
                    {
                        if(fn.materialization_state.state.load(::std::memory_order_acquire) == ::uwvm2::utils::thread::lazy_compile_state::compiled)
                        {
                            ++total;
                        }
                    }
                }
                return total;
            }
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            for(auto const& rec: g_runtime.modules)
            {
                for(auto const& fn: rec.lazy_compiled.functions)
                {
                    if(fn.materialization_state.state.load(::std::memory_order_acquire) == ::uwvm2::utils::thread::lazy_compile_state::compiled) { ++total; }
                }
            }
# endif
            return total;
        }

        inline constexpr void print_lazy_runtime_compiler_log(::fast_io::unix_timestamp run_start,
                                                              ::fast_io::unix_timestamp exec_start,
                                                              ::fast_io::unix_timestamp exec_end,
                                                              ::fast_io::unix_timestamp stop_end) noexcept
        {
            // Emit one compact summary after lazy execution so benchmark runs can correlate demand misses, background work, and
            // scheduler behavior without tracing every individual function.
            if(!::uwvm2::uwvm::io::enable_runtime_log) { return; }

            auto const stats{g_runtime.lazy_scheduler.snapshot_stats()};
            auto const worker_count{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved};
            auto const scheduling_size{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_size};
            auto const total_functions{lazy_total_function_count()};
            auto const compiled_functions{lazy_compiled_function_count()};
            auto const miss_count{g_runtime.lazy_runtime_miss_count.load(::std::memory_order_relaxed)};
            auto const compiled_hit_count{g_runtime.lazy_runtime_compiled_hit_count.load(::std::memory_order_relaxed)};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            auto const llvm_jit_urgent_requests{g_runtime.llvm_jit_urgent_request_count.load(::std::memory_order_relaxed)};
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            ::std::size_t tiered_switches{};
            ::std::size_t tiered_interpreter_fallbacks_sampled{};
            ::std::size_t tiered_entry_misses{};
            ::std::size_t tiered_large_loop_samples{};
            ::std::size_t tiered_large_long_run_modules{};
            ::std::size_t tiered_osr_callbacks{};
            ::std::size_t tiered_osr_ready{};
            ::std::size_t tiered_osr_misses{};
            ::std::size_t tiered_osr_deferred{};
            ::std::size_t tiered_osr_compile_requests{};
            ::std::size_t tiered_osr_urgent_requests{};
            ::std::size_t tiered_full_compile_requests{};
            ::std::size_t tiered_full_compile_ready{};
            ::std::size_t tiered_full_compile_failed{};
            ::std::size_t tiered_full_publishes{};
            auto const tiered_backend{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                      ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered};
            if(tiered_backend)
            {
                for(auto const& rec: g_runtime.modules)
                {
                    tiered_switches += rec.tiered_switch_count;
                    auto& fallback_counter{const_cast<::std::size_t&>(rec.tiered_interpreter_fallback_count)};
                    auto& entry_miss_counter{const_cast<::std::size_t&>(rec.tiered_entry_miss_count)};
                    auto& large_loop_counter{const_cast<::std::size_t&>(rec.tiered_large_loop_sample_count)};
                    tiered_interpreter_fallbacks_sampled += ::std::atomic_ref<::std::size_t>{fallback_counter}.load(::std::memory_order_relaxed);
                    tiered_entry_misses += ::std::atomic_ref<::std::size_t>{entry_miss_counter}.load(::std::memory_order_relaxed);
                    tiered_large_loop_samples += ::std::atomic_ref<::std::size_t>{large_loop_counter}.load(::std::memory_order_relaxed);
                    if(tiered_large_module_long_run_active(rec)) { ++tiered_large_long_run_modules; }
                }
                tiered_osr_callbacks = g_runtime.tiered_osr_callback_count.load(::std::memory_order_relaxed);
                tiered_osr_ready = g_runtime.tiered_osr_ready_count.load(::std::memory_order_relaxed);
                tiered_osr_misses = g_runtime.tiered_osr_miss_count.load(::std::memory_order_relaxed);
                tiered_osr_deferred = g_runtime.tiered_osr_deferred_count.load(::std::memory_order_relaxed);
                tiered_osr_compile_requests = g_runtime.tiered_osr_compile_request_count.load(::std::memory_order_relaxed);
                tiered_osr_urgent_requests = g_runtime.tiered_osr_urgent_request_count.load(::std::memory_order_relaxed);
                tiered_full_compile_requests = g_runtime.tiered_full_compile_request_count.load(::std::memory_order_relaxed);
                tiered_full_compile_ready = g_runtime.tiered_full_compile_ready_count.load(::std::memory_order_relaxed);
                tiered_full_compile_failed = g_runtime.tiered_full_compile_failed_count.load(::std::memory_order_relaxed);
                tiered_full_publishes = g_runtime.tiered_full_publish_count.load(::std::memory_order_relaxed);
            }
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
            auto const log_prefix{
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                tiered_backend ? ::uwvm2::utils::container::u8string_view{u8"[tiered-lazy] "} :
#  endif
                               (::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only
                                    ? ::uwvm2::utils::container::u8string_view{u8"[llvm-jit-lazy] "}
                                    : ::uwvm2::utils::container::u8string_view{u8"[uwvm-int-lazy] "})};
# else
            auto const log_prefix{::uwvm2::utils::container::u8string_view{u8"[uwvm-int-lazy] "}};
# endif

            ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                 log_prefix,
                                 u8"summary workers=",
                                 worker_count,
                                 u8" scheduling_size=",
                                 scheduling_size,
                                 u8" functions=",
                                 total_functions,
                                 u8" compiled=",
                                 compiled_functions,
                                 u8" demand_misses=",
                                 miss_count,
                                 u8" compiled_hits=",
                                 compiled_hit_count
# if defined(UWVM_RUNTIME_LLVM_JIT)
                                 ,
                                 u8" llvm_jit_urgent_requests=",
                                 llvm_jit_urgent_requests
# endif
            );

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(tiered_backend)
            {
                ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                     u8" tiered_switches=",
                                     tiered_switches,
                                     u8" tiered_int_fallbacks_sampled=",
                                     tiered_interpreter_fallbacks_sampled,
                                     u8" tiered_entry_misses=",
                                     tiered_entry_misses,
                                     u8" tiered_large_loop_samples=",
                                     tiered_large_loop_samples,
                                     u8" tiered_large_long_run_modules=",
                                     tiered_large_long_run_modules,
                                     u8" tiered_osr_callbacks=",
                                     tiered_osr_callbacks,
                                     u8" tiered_osr_ready=",
                                     tiered_osr_ready,
                                     u8" tiered_osr_misses=",
                                     tiered_osr_misses,
                                     u8" tiered_osr_deferred=",
                                     tiered_osr_deferred,
                                     u8" tiered_osr_compile_requests=",
                                     tiered_osr_compile_requests,
                                     u8" tiered_osr_urgent_requests=",
                                     tiered_osr_urgent_requests,
                                     u8" tiered_full_requests=",
                                     tiered_full_compile_requests,
                                     u8" tiered_full_ready=",
                                     tiered_full_compile_ready,
                                     u8" tiered_full_failed=",
                                     tiered_full_compile_failed,
                                     u8" tiered_full_publishes=",
                                     tiered_full_publishes);
            }
# endif

            ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                 u8" total_time=",
                                 stop_end - run_start,
                                 u8" exec_time=",
                                 exec_end - exec_start,
                                 u8" stop_time=",
                                 stop_end - exec_end,
                                 u8"\n");

            ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                 log_prefix,
                                 u8"scheduler enqueued=",
                                 stats.enqueued_requests,
                                 u8" enqueue_failures=",
                                 stats.enqueue_failures,
                                 u8" duplicate_requests=",
                                 stats.duplicate_requests,
                                 u8" inline_compiles=",
                                 stats.inline_compiles,
                                 u8" worker_compiles=",
                                 stats.worker_compiles,
                                 u8" helper_compiles=",
                                 stats.helper_compiles,
                                 u8" passive_waits=",
                                 stats.passive_waits,
                                 u8" worker_waits=",
                                 stats.worker_queue_waits,
                                 u8" refill_calls=",
                                 stats.refill_calls,
                                 u8" refill_successes=",
                                 stats.refill_successes,
                                 u8" queue_depth=",
                                 g_runtime.lazy_scheduler.queued_count.load(::std::memory_order_relaxed),
                                 u8" queue_capacity=",
                                 g_runtime.lazy_scheduler.queue_capacity,
                                 u8"\n");

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(tiered_backend)
            {
                auto const urgent_stats{g_runtime.tiered_urgent_scheduler.snapshot_stats()};
                ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                     log_prefix,
                                     u8"urgent-scheduler enqueued=",
                                     urgent_stats.enqueued_requests,
                                     u8" enqueue_failures=",
                                     urgent_stats.enqueue_failures,
                                     u8" duplicate_requests=",
                                     urgent_stats.duplicate_requests,
                                     u8" inline_compiles=",
                                     urgent_stats.inline_compiles,
                                     u8" worker_compiles=",
                                     urgent_stats.worker_compiles,
                                     u8" helper_compiles=",
                                     urgent_stats.helper_compiles,
                                     u8" passive_waits=",
                                     urgent_stats.passive_waits,
                                     u8" worker_waits=",
                                     urgent_stats.worker_queue_waits,
                                     u8" refill_calls=",
                                     urgent_stats.refill_calls,
                                     u8" refill_successes=",
                                     urgent_stats.refill_successes,
                                     u8" queue_depth=",
                                     g_runtime.tiered_urgent_scheduler.queued_count.load(::std::memory_order_relaxed),
                                     u8" queue_capacity=",
                                     g_runtime.tiered_urgent_scheduler.queue_capacity,
                                     u8"\n");
            }
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
            if(g_runtime.llvm_jit_urgent_scheduler.running() || llvm_jit_urgent_requests != 0uz)
            {
                auto const urgent_stats{g_runtime.llvm_jit_urgent_scheduler.snapshot_stats()};
                ::fast_io::io::print(::uwvm2::uwvm::io::u8runtime_log_output,
                                     log_prefix,
                                     u8"llvm-urgent-scheduler enqueued=",
                                     urgent_stats.enqueued_requests,
                                     u8" enqueue_failures=",
                                     urgent_stats.enqueue_failures,
                                     u8" duplicate_requests=",
                                     urgent_stats.duplicate_requests,
                                     u8" inline_compiles=",
                                     urgent_stats.inline_compiles,
                                     u8" worker_compiles=",
                                     urgent_stats.worker_compiles,
                                     u8" helper_compiles=",
                                     urgent_stats.helper_compiles,
                                     u8" passive_waits=",
                                     urgent_stats.passive_waits,
                                     u8" worker_waits=",
                                     urgent_stats.worker_queue_waits,
                                     u8" refill_calls=",
                                     urgent_stats.refill_calls,
                                     u8" refill_successes=",
                                     urgent_stats.refill_successes,
                                     u8" queue_depth=",
                                     g_runtime.llvm_jit_urgent_scheduler.queued_count.load(::std::memory_order_relaxed),
                                     u8" queue_capacity=",
                                     g_runtime.llvm_jit_urgent_scheduler.queue_capacity,
                                     u8"\n");
            }
# endif
        }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
        // =========================================================================
        // LLVM lazy publication and tiered runtime policy
        // -------------------------------------------------------------------------
        // Lazy JIT starts with placeholder raw entries. The first demand compile
        // materializes a function, records debug/unwind metadata, publishes direct
        // and call_indirect targets, and optionally feeds tiered hotness counters.
        //
        // Coverage invariants:
        // - Raw entry slots are the synchronization boundary between generated callers and runtime publication.
        // - Typed entry slots are separate because direct wasm calls and raw host buffers use different ABIs.
        // - Unwind registration happens for both typed and raw entries so diagnostics can identify either wrapper.
        // - Background prefetch is opportunistic; demand compilation must still be correct with zero workers.
        // - call_indirect table targets are updated after publication so lazy and eager callers converge.
        // - Tiered mode must never publish a full-module entry before the full artifact is marked ready.
        // =========================================================================
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view get_runtime_llvm_jit_call_stack_mode_name() noexcept
        {
            // Return a stable label for logs and diagnostics; keep it in sync with the effective runtime policy, not the raw flag.
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            using runtime_llvm_jit_call_stack_t = runtime_mode::runtime_llvm_jit_call_stack_t;
            switch(get_runtime_llvm_jit_effective_call_stack_mode())
            {
                case runtime_llvm_jit_call_stack_t::auto_policy: return u8"auto";
                case runtime_llvm_jit_call_stack_t::instruction: return u8"instruction";
                case runtime_llvm_jit_call_stack_t::none: return u8"none";
                case runtime_llvm_jit_call_stack_t::unwind: return u8"unwind";
                case runtime_llvm_jit_call_stack_t::unwind_uncheck: return u8"unwind-uncheck";
            }

            return u8"instruction";
        }

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_uses_instruction_call_stack_frames() noexcept
        {
            // Decide whether generated code should maintain lightweight logical wasm frames in TLS in addition to native unwind data.
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            using runtime_llvm_jit_call_stack_t = runtime_mode::runtime_llvm_jit_call_stack_t;
            switch(get_runtime_llvm_jit_effective_call_stack_mode())
            {
                case runtime_llvm_jit_call_stack_t::auto_policy: [[fallthrough]];
                case runtime_llvm_jit_call_stack_t::instruction: return true;
                case runtime_llvm_jit_call_stack_t::none: return false;
                case runtime_llvm_jit_call_stack_t::unwind: [[fallthrough]];
                case runtime_llvm_jit_call_stack_t::unwind_uncheck: return false;
            }

            return true;
        }

        inline constexpr void
            configure_runtime_llvm_jit_call_stack_policy(::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option& opt) noexcept
        {
            // Convert the runtime call-stack policy into codegen switches before IR is emitted.
            validate_runtime_llvm_jit_unwind_call_stack_or_fatal();
            opt.emit_call_stack_frames = runtime_llvm_jit_uses_instruction_call_stack_frames();
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            // Tiered/no-T0 has no interpreter caller stack to merge with native unwind. Runtime-helper traps such as
            // call_indirect type mismatch can be reached after LLVM has inlined or folded the generated call chain, leaving
            // platform unwind with only the raw entry frame. Keep the lightweight logical frame instrumentation enabled in this
            // specific unwind configuration so the fatal reporter can recover the wasm caller chain through TLS.
            // Do not use tiered_runtime_active() here: compile options are configured during lazy initialization before
            // g_runtime.lazy_compile_active is set, so the execution-time predicate would incorrectly disable this fallback.
            auto const tiered_backend_for_call_stack{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                                     ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered};
            opt.emit_call_stack_frames =
                opt.emit_call_stack_frames || (runtime_llvm_jit_unwind_call_stack_requested() && tiered_backend_for_call_stack && !tiered_t0_enabled());
# endif
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            // Some Win64 trap sites are synthetic pre-dispatch checks or detailed runtime bridges where SEH can only
            // recover the JIT leaf. Keep logical frames available so diagnostics can avoid reporting a partial stack.
            opt.emit_call_stack_frames = opt.emit_call_stack_frames || runtime_llvm_jit_unwind_call_stack_requested();
# endif
            opt.emit_unwind_call_stack_frames = runtime_llvm_jit_unwind_call_stack_requested();
        }

        inline constexpr void publish_llvm_jit_call_indirect_defined_entry_targets(compiled_defined_func_info const* info,
                                                                                   ::std::uintptr_t raw_entry_address,
                                                                                   ::std::uintptr_t typed_entry_address) noexcept
        {
            // call_indirect tables may already contain placeholder lazy targets. Publish new raw/typed addresses with release
            // ordering so readers never observe a partially installed generated entry.
            if(info == nullptr) { return; }

            auto const info_address{reinterpret_cast<::std::uintptr_t>(info)};
            for(auto& caller_rec: g_runtime.modules)
            {
                for(auto& target_vec: caller_rec.llvm_jit_call_indirect_targets)
                {
                    for(auto& target: target_vec)
                    {
                        auto const current_context{::std::atomic_ref<::std::uintptr_t>{target.context_address}.load(::std::memory_order_acquire)};
                        auto const current_entry{::std::atomic_ref<::std::uintptr_t>{target.entry_address}.load(::std::memory_order_acquire)};
                        if(current_context != info_address && (raw_entry_address == 0u || current_entry != raw_entry_address)) { continue; }

                        if(raw_entry_address != 0u)
                        {
                            ::std::atomic_ref<::std::uintptr_t>{target.entry_address}.store(raw_entry_address, ::std::memory_order_release);
                        }
                        if(typed_entry_address != 0u)
                        {
                            ::std::atomic_ref<::std::uintptr_t>{target.typed_entry_address}.store(typed_entry_address, ::std::memory_order_release);
                        }
                    }
                }
            }
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        [[nodiscard]] inline constexpr bool tiered_local_function_direct_supported(compiled_module_record const& rec,
                                                                                   ::std::size_t local_function_index) noexcept
        {
            // Central policy gate for direct raw-entry replacement. It currently accepts every local function, but keeping the helper
            // makes it cheap to disable individual functions later for ABI, validation, or code-shape reasons.
            static_cast<void>(rec);
            static_cast<void>(local_function_index);
            return true;
        }

        [[nodiscard]] inline constexpr bool publish_tiered_llvm_jit_entry_targets(compiled_module_record& rec,
                                                                                  ::std::size_t module_id,
                                                                                  ::std::size_t local_function_index,
                                                                                  ::std::uintptr_t raw_entry_address,
                                                                                  ::std::uintptr_t typed_entry_address) noexcept
        {
            // Tiered mode updates direct-call slots and indirect-call tables together. The raw entry is mandatory because tiered
            // bridges enter generated code through the raw ABI wrapper.
            if(raw_entry_address == 0u) [[unlikely]] { return false; }
            if(!tiered_local_function_direct_supported(rec, local_function_index)) { return false; }

            if(local_function_index < rec.llvm_jit_lazy_direct_call_targets.size())
            {
                auto& target{rec.llvm_jit_lazy_direct_call_targets.index_unchecked(local_function_index)};
                ::std::atomic_ref<::std::uintptr_t>{target.entry_address}.store(raw_entry_address, ::std::memory_order_release);
            }

            if(typed_entry_address != 0u && local_function_index < rec.llvm_jit_lazy_direct_typed_entry_targets.size())
            {
                auto& target{rec.llvm_jit_lazy_direct_typed_entry_targets.index_unchecked(local_function_index)};
                ::std::atomic_ref<::std::uintptr_t>{target}.store(typed_entry_address, ::std::memory_order_release);
            }

            compiled_defined_func_info const* info_for_targets{};
            if(module_id < g_runtime.defined_func_cache.size()) [[likely]]
            {
                auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(module_id)};
                if(local_function_index < mod_cache.size()) [[likely]] { info_for_targets = ::std::addressof(mod_cache.index_unchecked(local_function_index)); }
            }

            if(info_for_targets == nullptr) { return true; }
            publish_llvm_jit_call_indirect_defined_entry_targets(info_for_targets, raw_entry_address, typed_entry_address);

            return true;
        }
# endif

        inline constexpr void publish_llvm_jit_lazy_materialized_function(void* user_data, ::std::size_t local_function_index) noexcept
        {
            // Lazy LLVM materialization callback: once MCJIT finalizes a function, publish all addresses that can reach it.
            auto const rec{static_cast<compiled_module_record*>(user_data)};
            if(rec == nullptr || rec->runtime_module == nullptr) [[unlikely]] { return; }
            if(local_function_index >= rec->llvm_jit_lazy_compiled.materialized_functions.size()) [[unlikely]] { return; }

            ::std::uintptr_t raw_entry_address{};
            ::std::uintptr_t typed_entry_address{};
            if(!::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_raw_entry_address(rec->llvm_jit_lazy_compiled,
                                                                                                                     local_function_index,
                                                                                                                     raw_entry_address) ||
               !::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_entry_address(rec->llvm_jit_lazy_compiled,
                                                                                                                 local_function_index,
                                                                                                                 typed_entry_address))
            {
                return;
            }

            auto const module_id{rec->llvm_jit_lazy_compile_options.compile_options.curr_wasm_id};
            auto const function_index{rec->runtime_module->imported_function_vec_storage.size() + local_function_index};
            record_llvm_jit_unwind_entry(module_id, function_index, typed_entry_address, false);
            record_llvm_jit_unwind_entry(module_id, function_index, raw_entry_address, true);

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(tiered_runtime_active() && tiered_uses_tiered_targets())
            {
                if(tiered_full_ready(*rec))
                {
                    ::std::uintptr_t full_raw_entry_address{};
                    static_cast<void>(try_publish_tiered_ready_full_llvm_jit_entry(*rec,
                                                                                   rec->llvm_jit_lazy_compile_options.compile_options.curr_wasm_id,
                                                                                   local_function_index,
                                                                                   full_raw_entry_address));
                    return;
                }

                static_cast<void>(publish_tiered_llvm_jit_entry_targets(*rec,
                                                                        rec->llvm_jit_lazy_compile_options.compile_options.curr_wasm_id,
                                                                        local_function_index,
                                                                        raw_entry_address,
                                                                        typed_entry_address));
                return;
            }
# endif

            if(local_function_index < rec->llvm_jit_lazy_direct_call_targets.size())
            {
                auto& target{rec->llvm_jit_lazy_direct_call_targets.index_unchecked(local_function_index)};
                target.entry_address = raw_entry_address;
                target.context_address = 0u;
            }

            if(local_function_index < rec->llvm_jit_lazy_direct_typed_entry_targets.size())
            {
                auto& target{rec->llvm_jit_lazy_direct_typed_entry_targets.index_unchecked(local_function_index)};
                target = typed_entry_address;
            }

            compiled_defined_func_info const* info_for_targets{};
            if(module_id < g_runtime.defined_func_cache.size())
            {
                auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(module_id)};
                if(local_function_index < mod_cache.size()) { info_for_targets = ::std::addressof(mod_cache.index_unchecked(local_function_index)); }
            }
            publish_llvm_jit_call_indirect_defined_entry_targets(info_for_targets, raw_entry_address, typed_entry_address);
        }

        [[nodiscard]] inline constexpr ::std::size_t llvm_jit_lazy_compile_unit_code_size(compiled_module_record const& rec,
                                                                                          ::std::size_t local_function_index) noexcept
        {
            // Return the primary compile-unit size used by background ordering, urgent scheduling, and tiered hotness thresholds.
            if(local_function_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]] { return 0uz; }
            auto const& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_function_index)};
            if(fn.primary_cu_index >= rec.llvm_jit_lazy_compiled.compile_units.size()) [[unlikely]] { return 0uz; }
            return rec.llvm_jit_lazy_compiled.compile_units.index_unchecked(fn.primary_cu_index).code_size;
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
# endif

        inline constexpr void prepare_llvm_jit_lazy_background_request_contexts(compiled_module_record& rec) noexcept
        {
            // LLVM lazy requests carry a publish callback so materialized functions can replace placeholder direct-call targets
            // immediately after finalization.
            auto const local_n{rec.runtime_module == nullptr ? 0uz : rec.runtime_module->local_defined_function_vec_storage.size()};
            rec.llvm_jit_lazy_background_errors.clear();
            rec.llvm_jit_lazy_background_errors.resize(local_n);
            rec.llvm_jit_lazy_background_request_contexts.clear();
            rec.llvm_jit_lazy_background_request_contexts.resize(local_n);
            rec.lazy_prefetch_order.clear();
            rec.lazy_prefetch_order.resize(local_n);
            rec.lazy_prefetch_cursor = 0uz;

            if(local_n == 0uz || rec.runtime_module == nullptr) { return; }

            for(::std::size_t i{}; i != local_n; ++i)
            {
                auto const& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(i)};
                auto& ctx{rec.llvm_jit_lazy_background_request_contexts.index_unchecked(i)};
                ctx.curr_module = rec.runtime_module;
                ctx.lazy_storage = ::std::addressof(rec.llvm_jit_lazy_compiled);
                ctx.options = rec.llvm_jit_lazy_compile_options;
                ctx.compile_unit_index = fn.primary_cu_index;
                ctx.err = ::std::addressof(rec.llvm_jit_lazy_background_errors.index_unchecked(i));
                ctx.module_name = rec.module_name;
                ctx.publish_materialized_function = publish_llvm_jit_lazy_materialized_function;
                ctx.publish_user_data = ::std::addressof(rec);
                rec.lazy_prefetch_order.index_unchecked(i) = i;
            }

            ::std::sort(rec.lazy_prefetch_order.begin(),
                        rec.lazy_prefetch_order.end(),
                        [&](::std::size_t a, ::std::size_t b) constexpr noexcept
                        {
                            // Smaller functions are queued first by default to reduce background latency and produce early wins.
                            auto const size_a{llvm_jit_lazy_compile_unit_code_size(rec, a)};
                            auto const size_b{llvm_jit_lazy_compile_unit_code_size(rec, b)};
                            if(size_a != size_b) { return size_a < size_b; }
                            return a < b;
                        });
        }

        inline constexpr void prioritize_llvm_jit_lazy_background_entry(compiled_module_record& rec, ::std::size_t preferred_local_index) noexcept
        {
            // Move the selected entry function to the front of the existing prefetch order without disturbing the rest of the queue.
            if(preferred_local_index >= rec.lazy_prefetch_order.size()) [[unlikely]] { return; }

            auto const it{::std::find(rec.lazy_prefetch_order.begin(), rec.lazy_prefetch_order.end(), preferred_local_index)};
            if(it == rec.lazy_prefetch_order.end()) [[unlikely]] { return; }

            ::std::rotate(rec.lazy_prefetch_order.begin(), it, it + 1uz);
            rec.lazy_prefetch_order.index_unchecked(0) = preferred_local_index;
            rec.lazy_prefetch_cursor = 0uz;
        }

        [[nodiscard]] inline constexpr bool collect_llvm_jit_lazy_entry_direct_graph_order(runtime_module_storage_t const& runtime_module,
                                                                                           ::std::size_t entry_local_function_index,
                                                                                           ::std::size_t graph_budget,
                                                                                           ::uwvm2::utils::container::vector<::std::size_t>& out) noexcept
        {
            // Seed lazy JIT around the entry function's direct-call graph. This improves startup locality without committing to
            // eager full-module compilation.
            out.clear();
            auto const local_n{runtime_module.local_defined_function_vec_storage.size()};
            if(graph_budget == 0uz || entry_local_function_index >= local_n) [[unlikely]] { return false; }

            ::uwvm2::utils::container::vector<::std::uint_least8_t> seen{};
            seen.resize(local_n);

            out.reserve(graph_budget);
            ::uwvm2::utils::container::vector<::std::size_t> stack{};
            stack.reserve(graph_budget);
            seen.index_unchecked(entry_local_function_index) = 1u;
            stack.push_back(entry_local_function_index);
            ::std::size_t seen_count{1uz};

            ::uwvm2::utils::container::vector<::std::size_t> callees{};
            while(!stack.empty() && out.size() < graph_budget)
            {
                auto const local_index{stack.back()};
                stack.pop_back();
                out.push_back(local_index);

                callees.clear();
                if(!::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::collect_direct_defined_callees(runtime_module, local_index, callees))
                {
                    continue;
                }

                for(auto remaining{callees.size()}; remaining != 0uz;)
                {
                    --remaining;
                    auto const callee_local_index{callees.index_unchecked(remaining)};
                    if(callee_local_index >= local_n) [[unlikely]] { continue; }
                    if(seen.index_unchecked(callee_local_index) != 0u) { continue; }
                    if(seen_count == graph_budget) { break; }

                    seen.index_unchecked(callee_local_index) = 1u;
                    stack.push_back(callee_local_index);
                    ++seen_count;
                }
            }

            return true;
        }

        [[nodiscard]] inline constexpr bool seed_llvm_jit_lazy_background_entry_direct_graph(compiled_module_record& rec,
                                                                                             ::std::size_t entry_local_function_index) noexcept
        {
            // Replace the default size-sorted order with a bounded direct-call graph rooted at the chosen entry function.
            if(rec.runtime_module == nullptr) [[unlikely]] { return false; }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            auto const tiered_backend{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                      ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered};
            auto const tiered_t0_backend{tiered_backend && tiered_t0_enabled()};
            auto const background_graph_budget{tiered_t0_backend ? 16uz : 96uz};
# else
            constexpr ::std::size_t background_graph_budget{96uz};
# endif

            ::uwvm2::utils::container::vector<::std::size_t> graph_order{};
            if(!collect_llvm_jit_lazy_entry_direct_graph_order(*rec.runtime_module, entry_local_function_index, background_graph_budget, graph_order))
            {
                return false;
            }

            if(graph_order.empty()) [[unlikely]] { return false; }

            rec.lazy_prefetch_order.clear();
            rec.lazy_prefetch_order.reserve(graph_order.size());
            for(auto const local_index: graph_order) { rec.lazy_prefetch_order.push_back(local_index); }
            rec.lazy_prefetch_cursor = 0uz;
            return true;
        }

        [[nodiscard]] inline constexpr bool
            enqueue_llvm_jit_lazy_background_requests_for_module(compiled_module_record& rec,
                                                                 ::uwvm2::utils::thread::lazy_compile_scheduler& scheduler) noexcept
        {
            // Queue a bounded batch per refill so one module cannot monopolize the scheduler when many functions remain cold.
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            auto const tiered_backend{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                      ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered};
            auto const tiered_t0_backend{tiered_backend && tiered_t0_enabled()};
            auto const refill_request_budget{tiered_t0_backend ? 8uz : 32uz};
# else
            constexpr ::std::size_t refill_request_budget{32uz};
# endif
            auto const local_n{rec.lazy_prefetch_order.size()};
            if(local_n == 0uz || rec.runtime_module == nullptr) [[unlikely]] { return false; }

            bool queued{};
            ::std::size_t queued_this_refill{};
            for(; rec.lazy_prefetch_cursor < local_n; ++rec.lazy_prefetch_cursor)
            {
                if(queued_this_refill == refill_request_budget) { break; }

                auto const local_index{rec.lazy_prefetch_order.index_unchecked(rec.lazy_prefetch_cursor)};
                if(local_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]] { continue; }
                auto& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
                auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
                if(st != ::uwvm2::utils::thread::lazy_compile_state::uncompiled) { continue; }

                auto& ctx{rec.llvm_jit_lazy_background_request_contexts.index_unchecked(local_index)};
                auto request{::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::make_lazy_compile_request(ctx, 0u)};
                if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { continue; }

                if(!scheduler.try_request(request)) [[unlikely]] { break; }
                queued = true;
                ++queued_this_refill;
            }

            return queued;
        }

        inline constexpr bool llvm_jit_lazy_background_refill_callback(void*, ::uwvm2::utils::thread::lazy_compile_scheduler& scheduler) noexcept
        {
            // Prefer the module/function selected from the entry path, then scan all modules for eventual background coverage. The
            // atomic flag serializes refills because each record owns a mutable prefetch cursor.
            if(g_runtime.lazy_prefetch_lock.test_and_set(::std::memory_order_acquire)) { return false; }

            bool queued{};
            auto const module_count{g_runtime.modules.size()};
            auto const preferred_module_id{g_runtime.lazy_prefetch_module_id};

            if(preferred_module_id < module_count)
            {
                auto& preferred_rec{g_runtime.modules.index_unchecked(preferred_module_id)};
                if(enqueue_llvm_jit_lazy_background_requests_for_module(preferred_rec, scheduler)) { queued = true; }
            }

            for(::std::size_t module_id{}; module_id != module_count; ++module_id)
            {
                if(module_id == preferred_module_id) { continue; }
                auto& rec{g_runtime.modules.index_unchecked(module_id)};
                if(enqueue_llvm_jit_lazy_background_requests_for_module(rec, scheduler)) { queued = true; }
            }

            g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);
            return queued;
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        // =========================================================================
        // Tiered execution policy and hotness tracking
        // -------------------------------------------------------------------------
        // Tiered mode starts in the interpreter, promotes selected functions to lazy
        // LLVM entries, and can eventually request a full-module LLVM artifact. The
        // helpers below keep those choices explicit so entry calls, loop OSR, logging,
        // and trap reporting all observe the same state transitions.
        //
        // Coverage invariants:
        // - T0 fallback must remain correct even when every LLVM scheduler is stopped.
        // - T1 lazy entries are installed function-by-function and may coexist with T2 full entries.
        // - T2 readiness is a release/acquire byte because worker threads publish it to execution threads.
        // - Entry hotness counters are sampled, so thresholds must account for stride amplification.
        // - Loop OSR requests are more conservative than direct-call promotion because they cross frame layouts.
        // - Trap reporting preserves mixed interpreter/JIT stacks through explicit boundary snapshots.
        // =========================================================================
        [[nodiscard]] inline constexpr bool tiered_runtime_active() noexcept
        {
            // Tiered helpers are meaningful only during a lazy tiered run; initialization code may build tiered metadata earlier.
            return g_runtime.lazy_compile_active && ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                                        ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered;
        }

        [[nodiscard]] inline constexpr bool tiered_t0_enabled() noexcept
        {
            // T0 is the lazy UWVM interpreter tier used for immediate execution before LLVM code is ready.
            return !::uwvm2::uwvm::runtime::runtime_mode::runtime_tiered_disable_uwvm_int_lazy_interpreter;
        }

        [[nodiscard]] inline constexpr bool tiered_t2_enabled() noexcept
        {
            // T2 is the optional full-module LLVM tier requested only after runtime evidence justifies the compile cost.
            return !::uwvm2::uwvm::runtime::runtime_mode::runtime_tiered_disable_llvm_full_jit;
        }

        [[nodiscard]] inline constexpr bool tiered_uses_tiered_targets() noexcept
        {
            // Direct-call slots become atomic tiered targets when either the interpreter tier or full LLVM tier can replace entries.
            return tiered_t0_enabled() || tiered_t2_enabled();
        }

        constexpr ::std::size_t tiered_urgent_scheduler_queue_capacity{64uz};

        inline constexpr void verbose_log_registered_tiered_urgent_scheduler(::std::size_t worker_count) noexcept
        {
            if(::uwvm2::uwvm::io::show_verbose && worker_count != 0uz) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Registered tiered urgent JIT scheduler (workers=",
                                    worker_count,
                                    u8", queue_capacity=",
                                    tiered_urgent_scheduler_queue_capacity,
                                    u8"). ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                    u8"[",
                                    ::uwvm2::uwvm::io::get_local_realtime(),
                                    u8"] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(verbose)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }
        }

        inline constexpr void ensure_tiered_jit_schedulers_started() noexcept
        {
            // Tiered mode can defer JIT worker startup until hot entry or OSR evidence appears. This avoids spinning up workers for
            // short-lived programs that finish happily in the lazy interpreter tier.
            if(!tiered_runtime_active()) { return; }
            if(g_runtime.lazy_scheduler.running()) { return; }
            if(!g_runtime.tiered_schedulers_deferred.load(::std::memory_order_acquire)) { return; }

            while(g_runtime.tiered_scheduler_start_lock.test_and_set(::std::memory_order_acquire)) { ::uwvm2::utils::thread::lazy_compile_thread_yield(); }

            if(!g_runtime.lazy_scheduler.running() && g_runtime.tiered_schedulers_deferred.load(::std::memory_order_relaxed))
            {
                auto const worker_count{g_runtime.tiered_deferred_worker_count};
                auto const jit_worker_count{worker_count == 0uz ? 0uz : 1uz};
                g_runtime.lazy_scheduler.start(
                    {.worker_count = jit_worker_count, .queue_capacity = 0uz, .refill_callback = nullptr, .refill_user_data = nullptr});
                g_runtime.tiered_urgent_scheduler.start({.worker_count = jit_worker_count,
                                                         .queue_capacity = jit_worker_count == 0uz ? 0uz : tiered_urgent_scheduler_queue_capacity,
                                                         .refill_callback = nullptr,
                                                         .refill_user_data = nullptr});
                g_runtime.tiered_schedulers_deferred.store(false, ::std::memory_order_release);
                verbose_log_registered_tiered_urgent_scheduler(jit_worker_count);
            }

            g_runtime.tiered_scheduler_start_lock.clear(::std::memory_order_release);
        }

        [[nodiscard]] inline constexpr ::std::size_t module_id_from_record(compiled_module_record const& rec) noexcept
        {
            // The record normally lives inside g_runtime.modules. Pointer arithmetic is the fast path, with name lookup as a fallback
            // for unusual move/debug states.
            auto const module_count{g_runtime.modules.size()};
            auto const module_data{g_runtime.modules.data()};
            auto const rec_address{reinterpret_cast<::std::uintptr_t>(::std::addressof(rec))};
            if(module_data != nullptr && module_count != 0uz)
            {
                auto const base_address{reinterpret_cast<::std::uintptr_t>(module_data)};
                auto const record_size{sizeof(compiled_module_record)};
                if(record_size != 0uz && module_count <= (::std::numeric_limits<::std::uintptr_t>::max() / record_size))
                {
                    auto const bytes{static_cast<::std::uintptr_t>(module_count * record_size)};
                    if(base_address <= ::std::numeric_limits<::std::uintptr_t>::max() - bytes && rec_address >= base_address &&
                       rec_address < base_address + bytes)
                    {
                        return static_cast<::std::size_t>((rec_address - base_address) / record_size);
                    }
                }
            }
            auto const it{g_runtime.module_name_to_id.find(rec.module_name)};
            return it == g_runtime.module_name_to_id.end() ? SIZE_MAX : it->second;
        }

        enum class tiered_llvm_jit_demand_state : unsigned
        {
            unavailable,  // No usable tiered LLVM entry exists and no request should be made from this path.
            ready,        // A raw LLVM entry was published and can be entered immediately.
            busy,         // Compilation is queued/in progress or the hotness gate has not opened yet.
            failed        // Compilation failed; callers should fall back or terminate according to their policy.
        };

        // Tiered thresholds are intentionally centralized. They balance startup responsiveness against compile cost for tiny hot
        // loops, medium numerical kernels, and extremely large modules such as language-runtime eval loops.
        constexpr ::std::size_t tiered_full_compile_switch_threshold{65536uz};
        constexpr ::std::size_t tiered_large_long_run_local_function_threshold{8192uz};
        constexpr ::std::size_t tiered_large_long_run_entry_counter_threshold{1048576uz};
        constexpr ::std::size_t tiered_large_long_run_switch_counter_threshold{262144uz};
        constexpr ::std::size_t tiered_large_long_run_loop_sample_threshold{1uz};
        constexpr ::std::size_t tiered_large_loop_compile_entry_counter_threshold{16777216uz};
        constexpr ::std::size_t tiered_large_loop_compile_fallback_counter_threshold{67108864uz};
        constexpr ::std::size_t tiered_large_loop_compile_switch_counter_threshold{8388608uz};
        // Huge CPython-style eval artifacts need an early counter-only urgent Tier 1 request.
        // The bytecode-local loop poll already filters by iteration count; this runtime gate
        // remains earlier than the old 262144-signal gate, but avoids handing the main
        // CPython eval frame to a huge Tier 1 artifact before the loop has proven long enough.
        constexpr ::std::uint_least32_t tiered_large_loop_osr_request_threshold{131072u};

        [[nodiscard]] inline constexpr ::std::size_t tiered_full_compile_switch_request_threshold(compiled_module_record const& rec) noexcept
        {
            // Large modules need much stronger evidence before full compilation; huge modules leave the decision to other signals.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n >= 1024uz) { return ::std::numeric_limits<::std::size_t>::max(); }
            if(local_n >= 512uz) { return 1048576uz; }
            return tiered_full_compile_switch_threshold;
        }

        [[nodiscard]] inline constexpr bool tiered_t1_schedulers_stable_for_full_compile() noexcept
        {
            // Avoid starting a full-module build while T1 demand queues still contain latency-sensitive function requests.
            if(g_runtime.lazy_scheduler.queued_count.load(::std::memory_order_acquire) != 0uz) { return false; }
            if(g_runtime.tiered_urgent_scheduler.running() && g_runtime.tiered_urgent_scheduler.queued_count.load(::std::memory_order_acquire) != 0uz)
            {
                return false;
            }
            return true;
        }

        [[nodiscard]] inline constexpr bool tiered_full_ready(compiled_module_record const& rec) noexcept
        {
            // The ready byte is accessed atomically because background compilation publishes it while execution threads probe it.
            auto& ready{const_cast<::std::uint_least8_t&>(rec.tiered_full_ready)};
            return ::std::atomic_ref<::std::uint_least8_t>{ready}.load(::std::memory_order_acquire) != 0u;
        }

        inline constexpr void store_tiered_full_ready(compiled_module_record& rec, bool ready) noexcept
        {
            // Publish or clear full-tier readiness with release ordering so entry arrays written before it are visible to readers.
            ::std::atomic_ref<::std::uint_least8_t>{rec.tiered_full_ready}.store(static_cast<::std::uint_least8_t>(ready ? 1u : 0u),
                                                                                 ::std::memory_order_release);
        }

        [[nodiscard]] inline constexpr ::std::uint_least32_t tiered_large_long_run_counter_sample_stride(compiled_module_record const& rec) noexcept
        {
            // Sampling keeps per-entry accounting cheap in large modules while still accumulating enough evidence for long runs.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n >= 8192uz) { return 16u; }
            if(local_n >= 1024uz) { return 16u; }
            if(local_n >= 512uz) { return 8u; }
            return 16u;
        }

        [[nodiscard]] inline constexpr bool tiered_large_long_run_counter_sampled(::std::size_t local_index, ::std::uint_least32_t stride) noexcept
        {
            // Mix the thread-local tick with the function index so adjacent hot functions do not sample in lockstep.
            if(stride <= 1u) { return true; }
#  if defined(UWVM_USE_THREAD_LOCAL)
            auto const tick{++tiered_counter_sample_tick};
#  else
            auto const tick{++get_tiered_counter_sample_tick()};
#  endif
            auto const phase{static_cast<::std::uint_least32_t>((local_index + 1uz) * 0x85ebca6buz)};
            return ((tick ^ phase) & (stride - 1u)) == 0u;
        }

        [[nodiscard]] inline constexpr bool tiered_large_module_long_run_active(compiled_module_record const& rec) noexcept
        {
            // Large modules need runtime evidence before aggressive JIT scheduling. Once the threshold is crossed, publish a sticky
            // ready bit so future checks stay cheap.
            auto& ready{const_cast<::std::uint_least8_t&>(rec.tiered_large_long_run_ready)};
            ::std::atomic_ref<::std::uint_least8_t> ready_ref{ready};
            if(ready_ref.load(::std::memory_order_acquire) != 0u) { return true; }

            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            auto& large_loop_sample_counter{const_cast<::std::size_t&>(rec.tiered_large_loop_sample_count)};
            auto const large_loop_sample_count{::std::atomic_ref<::std::size_t>{large_loop_sample_counter}.load(::std::memory_order_relaxed)};
            if(local_n < tiered_large_long_run_local_function_threshold && large_loop_sample_count < tiered_large_long_run_loop_sample_threshold)
            {
                return false;
            }

            auto& fallback_counter{const_cast<::std::size_t&>(rec.tiered_interpreter_fallback_count)};
            auto& entry_miss_counter{const_cast<::std::size_t&>(rec.tiered_entry_miss_count)};
            auto& switch_counter{const_cast<::std::size_t&>(rec.tiered_switch_count)};
            auto const fallback_count{::std::atomic_ref<::std::size_t>{fallback_counter}.load(::std::memory_order_relaxed)};
            auto const entry_miss_count{::std::atomic_ref<::std::size_t>{entry_miss_counter}.load(::std::memory_order_relaxed)};
            auto const switch_count{::std::atomic_ref<::std::size_t>{switch_counter}.load(::std::memory_order_relaxed)};
            if(fallback_count < tiered_large_long_run_entry_counter_threshold && entry_miss_count < tiered_large_long_run_entry_counter_threshold &&
               switch_count < tiered_large_long_run_switch_counter_threshold && large_loop_sample_count < tiered_large_long_run_loop_sample_threshold)
            {
                return false;
            }

            ::std::uint_least8_t expected{};
            if(ready_ref.compare_exchange_strong(expected, static_cast<::std::uint_least8_t>(1u), ::std::memory_order_acq_rel, ::std::memory_order_acquire) &&
               ::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
            {
                ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(u8"tiered-large-long-run module=\"",
                                                                                                             rec.module_name,
                                                                                                             u8"\" module_id=",
                                                                                                             module_id_from_record(rec),
                                                                                                             u8" local_functions=",
                                                                                                             local_n,
                                                                                                             u8" fallback_count=",
                                                                                                             fallback_count,
                                                                                                             u8" entry_miss_count=",
                                                                                                             entry_miss_count,
                                                                                                             u8" switch_count=",
                                                                                                             switch_count,
                                                                                                             u8" large_loop_samples=",
                                                                                                             large_loop_sample_count,
                                                                                                             u8" entry_threshold=",
                                                                                                             tiered_large_long_run_entry_counter_threshold,
                                                                                                             u8" switch_threshold=",
                                                                                                             tiered_large_long_run_switch_counter_threshold,
                                                                                                             u8" large_loop_threshold=",
                                                                                                             tiered_large_long_run_loop_sample_threshold);
            }
            return true;
        }

        [[nodiscard]] inline constexpr bool tiered_large_loop_sentinel_compile_active(compiled_module_record const& rec) noexcept
        {
            // The sentinel path opens only after the module has proven it is long-running or after a large-loop sample is seen.
            if(!tiered_large_module_long_run_active(rec)) { return false; }

            auto& large_loop_sample_counter{const_cast<::std::size_t&>(rec.tiered_large_loop_sample_count)};
            auto const large_loop_sample_count{::std::atomic_ref<::std::size_t>{large_loop_sample_counter}.load(::std::memory_order_relaxed)};
            if(large_loop_sample_count >= tiered_large_long_run_loop_sample_threshold) { return true; }

            auto& fallback_counter{const_cast<::std::size_t&>(rec.tiered_interpreter_fallback_count)};
            auto& entry_miss_counter{const_cast<::std::size_t&>(rec.tiered_entry_miss_count)};
            auto& switch_counter{const_cast<::std::size_t&>(rec.tiered_switch_count)};
            auto const fallback_count{::std::atomic_ref<::std::size_t>{fallback_counter}.load(::std::memory_order_relaxed)};
            auto const entry_miss_count{::std::atomic_ref<::std::size_t>{entry_miss_counter}.load(::std::memory_order_relaxed)};
            auto const switch_count{::std::atomic_ref<::std::size_t>{switch_counter}.load(::std::memory_order_relaxed)};

            return (switch_count >= tiered_large_loop_compile_switch_counter_threshold && switch_count >= fallback_count) ||
                   fallback_count >= tiered_large_loop_compile_fallback_counter_threshold ||
                   entry_miss_count >= tiered_large_loop_compile_entry_counter_threshold ||
                   switch_count >= (tiered_large_loop_compile_switch_counter_threshold * 4uz);
        }

        [[nodiscard]] inline constexpr bool tiered_small_module_long_run_active(compiled_module_record const& rec) noexcept
        {
            // Small modules can promote aggressively once repeated interpreter entries or tier switches show the run is not short.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n == 0uz || local_n >= 128uz) { return false; }

            auto& entry_miss_counter{const_cast<::std::size_t&>(rec.tiered_entry_miss_count)};
            auto& switch_counter{const_cast<::std::size_t&>(rec.tiered_switch_count)};
            auto& fallback_counter{const_cast<::std::size_t&>(rec.tiered_interpreter_fallback_count)};
            auto const entry_miss_count{::std::atomic_ref<::std::size_t>{entry_miss_counter}.load(::std::memory_order_relaxed)};
            auto const switch_count{::std::atomic_ref<::std::size_t>{switch_counter}.load(::std::memory_order_relaxed)};
            auto const fallback_count{::std::atomic_ref<::std::size_t>{fallback_counter}.load(::std::memory_order_relaxed)};

            return entry_miss_count >= 4096uz || switch_count >= 2048uz || fallback_count >= 4096uz;
        }

        inline constexpr void mark_tiered_full_compile_failed(compiled_module_record& rec) noexcept
        {
            // Publish failure to all future tiered probes so they stop waiting for a full-module artifact.
            store_tiered_full_ready(rec, false);
            rec.tiered_full_compile_state.state.store(::uwvm2::utils::thread::lazy_compile_state::failed, ::std::memory_order_release);
            g_runtime.tiered_full_compile_failed_count.fetch_add(1uz, ::std::memory_order_relaxed);
        }

        [[nodiscard]] inline constexpr ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config
            make_tiered_full_compile_split_config() noexcept
        {
            // Full tier-2 compilation reuses the runtime scheduling knob, but disables default adjustment because this is a deliberate
            // tiered policy decision rather than startup full-compile heuristics.
            using runtime_scheduling_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_t;
            using compile_task_split_policy_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t;

            auto const runtime_scheduling_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_policy};
            auto const runtime_scheduling_size{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_size};
            return runtime_scheduling_policy == runtime_scheduling_policy_t::function_count
                       ? ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config{.policy =
                                                                                                                    compile_task_split_policy_t::function_count,
                                                                                                                .split_size = runtime_scheduling_size,
                                                                                                                .adjust_for_default_policy = false}
                       : ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config{.policy =
                                                                                                                    compile_task_split_policy_t::code_size,
                                                                                                                .split_size = runtime_scheduling_size,
                                                                                                                .adjust_for_default_policy = false};
        }

        [[nodiscard]] inline constexpr bool publish_tiered_full_module_entries(compiled_module_record& rec, ::std::size_t module_id) noexcept
        {
            // Once a full-module JIT artifact exists, publish every local function entry so direct calls and call_indirect targets
            // switch consistently to the T2 code.
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const local_func_count{runtime_module->local_defined_function_vec_storage.size()};
            if(local_func_count > rec.llvm_jit_local_raw_entry_addresses.size()) [[unlikely]] { return false; }

            for(::std::size_t local_index{}; local_index != local_func_count; ++local_index)
            {
                auto const raw_entry_address{rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local_index)};
                auto const typed_entry_address{
                    local_index < rec.llvm_jit_local_entry_addresses.size() ? rec.llvm_jit_local_entry_addresses.index_unchecked(local_index) : 0u};
                if(!publish_tiered_llvm_jit_entry_targets(rec, module_id, local_index, raw_entry_address, typed_entry_address)) [[unlikely]] { return false; }
            }

            return true;
        }

        [[nodiscard]] inline constexpr bool try_publish_tiered_ready_full_llvm_jit_entry(compiled_module_record& rec,
                                                                                         ::std::size_t module_id,
                                                                                         ::std::size_t local_index,
                                                                                         ::std::uintptr_t& raw_entry_address) noexcept
        {
            raw_entry_address = 0u;
            if(!tiered_t2_enabled()) { return false; }
            if(!tiered_full_ready(rec)) { return false; }
            if(local_index >= rec.llvm_jit_local_raw_entry_addresses.size()) [[unlikely]] { return false; }

            raw_entry_address = rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local_index);
            auto const typed_entry_address{
                local_index < rec.llvm_jit_local_entry_addresses.size() ? rec.llvm_jit_local_entry_addresses.index_unchecked(local_index) : 0u};
            if(!publish_tiered_llvm_jit_entry_targets(rec, module_id, local_index, raw_entry_address, typed_entry_address))
            {
                raw_entry_address = 0u;
                return false;
            }
            return true;
        }

        inline constexpr void tiered_full_compile_request_entry(void* user_data) noexcept
        {
            // Background job for producing the full LLVM tier. It validates, emits, materializes, and publishes all entries as one
            // state transition so execution threads never observe a half-ready full module.
            auto const rec{static_cast<compiled_module_record*>(user_data)};
            if(rec == nullptr || rec->runtime_module == nullptr) [[unlikely]] { return; }
            if(!tiered_runtime_active()) [[unlikely]]
            {
                mark_tiered_full_compile_failed(*rec);
                return;
            }

            auto const module_id{module_id_from_record(*rec)};
            auto const runtime_module{rec->runtime_module};
            auto const local_func_count{runtime_module->local_defined_function_vec_storage.size()};
            ::uwvm2::validation::error::code_validation_error_impl err{};
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option opt{};
            opt.curr_wasm_id = module_id;
            opt.verify_llvm_jit_ir = !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_disable_ir_verifaction;
            configure_runtime_llvm_jit_call_stack_policy(opt);

            bool compiled_ok{};
#  ifdef UWVM_CPP_EXCEPTIONS
            try
#  endif
            {
                rec->llvm_jit_compiled =
                    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_all_from_uwvm(*runtime_module,
                                                                                                       opt,
                                                                                                       err,
                                                                                                       0uz,
                                                                                                       make_tiered_full_compile_split_config());
                compiled_ok = rec->llvm_jit_compiled.local_funcs.size() == local_func_count;
            }
#  ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                compiled_ok = false;
            }
#  endif
            if(!compiled_ok) [[unlikely]]
            {
                mark_tiered_full_compile_failed(*rec);
                return;
            }

            if(!try_materialize_runtime_module_llvm_jit(*rec, false, ::llvm::CodeGenOptLevel::Less, 0uz)) [[unlikely]]
            {
                mark_tiered_full_compile_failed(*rec);
                return;
            }

            if(!publish_tiered_full_module_entries(*rec, module_id)) [[unlikely]]
            {
                mark_tiered_full_compile_failed(*rec);
                return;
            }

            store_tiered_full_ready(*rec, true);
            if(!publish_tiered_full_module_entries(*rec, module_id)) [[unlikely]]
            {
                mark_tiered_full_compile_failed(*rec);
                return;
            }

            g_runtime.tiered_full_compile_ready_count.fetch_add(1uz, ::std::memory_order_relaxed);
            g_runtime.tiered_full_publish_count.fetch_add(local_func_count, ::std::memory_order_relaxed);

            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
            {
                ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(u8"tiered-full-ready module=\"",
                                                                                                             rec->module_name,
                                                                                                             u8"\" module_id=",
                                                                                                             module_id,
                                                                                                             u8" functions=",
                                                                                                             local_func_count);
            }
        }

        inline constexpr void maybe_request_tiered_full_compile(compiled_module_record& rec,
                                                                ::uwvm2::utils::container::u8string_view reason = u8"switch") noexcept
        {
            // Request full tier-2 compilation only after tiered execution has produced enough evidence that the compile cost will
            // amortize over a long-running workload.
            if(!tiered_t2_enabled()) { return; }
            if(tiered_full_ready(rec)) { return; }
            ensure_tiered_jit_schedulers_started();
            if(!g_runtime.lazy_scheduler.running()) { return; }
            if(!tiered_t1_schedulers_stable_for_full_compile()) { return; }
            if(rec.tiered_full_compile_state.state.load(::std::memory_order_acquire) != ::uwvm2::utils::thread::lazy_compile_state::uncompiled) { return; }

            ::uwvm2::utils::thread::lazy_compile_request request{.unit = ::std::addressof(rec.tiered_full_compile_state),
                                                                 .compile = tiered_full_compile_request_entry,
                                                                 .user_data = ::std::addressof(rec),
                                                                 .priority = 0u};
            if(!g_runtime.lazy_scheduler.try_request(request)) { return; }

            g_runtime.tiered_full_compile_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
            {
                ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(u8"tiered-full-request module=\"",
                                                                                                             rec.module_name,
                                                                                                             u8"\" module_id=",
                                                                                                             module_id_from_record(rec),
                                                                                                             u8" priority=0 reason=",
                                                                                                             reason);
            }
        }

        inline constexpr void record_tiered_lazy_compiled_hit() noexcept
        {
            // Count only when runtime logging is enabled; the hot path should not pay for metrics in quiet runs.
            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]] { g_runtime.lazy_runtime_compiled_hit_count.fetch_add(1uz, ::std::memory_order_relaxed); }
        }

        inline constexpr void record_tiered_lazy_miss() noexcept
        {
            // Misses include both entry-demand compilation and loop-OSR requests, giving one aggregate view of tier pressure.
            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]] { g_runtime.lazy_runtime_miss_count.fetch_add(1uz, ::std::memory_order_relaxed); }
        }

        [[nodiscard]] inline constexpr bool tiered_large_loop_sentinel_candidate(compiled_module_record const& rec, ::std::size_t local_index) noexcept
        {
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]] { return false; }
            // A true result here means the normal OSR thresholds are not enough.  Huge
            // CPython-style eval functions need the large-loop sentinel path so the execution
            // thread can request an urgent artifact without forcing synchronous compilation.
            return llvm_jit_lazy_compile_unit_code_size(rec, local_index) >=
                   ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_large_loop_sentinel_function_size;
        }

        inline constexpr void
            record_tiered_large_loop_sample(compiled_module_record& rec, ::std::size_t local_index, ::std::size_t loop_wasm_code_offset) noexcept
        {
            static_cast<void>(local_index);
            auto const sample_count{::std::atomic_ref<::std::size_t>{rec.tiered_large_loop_sample_count}.fetch_add(1uz, ::std::memory_order_relaxed) + 1uz};
            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
            {
                auto const log_periodic_sample{tiered_large_long_run_loop_sample_threshold > 1uz &&
                                               sample_count > tiered_large_long_run_loop_sample_threshold &&
                                               (sample_count % tiered_large_long_run_loop_sample_threshold) == 0uz};
                auto const log_power_of_two_sample{tiered_large_long_run_loop_sample_threshold <= 1uz && (sample_count & (sample_count - 1uz)) == 0uz};
                if(sample_count == 1uz || sample_count == tiered_large_long_run_loop_sample_threshold || log_periodic_sample || log_power_of_two_sample)
                {
                    ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(u8"tiered-large-loop-sample module=\"",
                                                                                                                 rec.module_name,
                                                                                                                 u8"\" module_id=",
                                                                                                                 module_id_from_record(rec),
                                                                                                                 u8" local_fn=",
                                                                                                                 local_index,
                                                                                                                 u8" loop_offset=",
                                                                                                                 loop_wasm_code_offset,
                                                                                                                 u8" samples=",
                                                                                                                 sample_count,
                                                                                                                 u8" threshold=",
                                                                                                                 tiered_large_long_run_loop_sample_threshold);
                }
            }
            static_cast<void>(tiered_large_module_long_run_active(rec));
        }

        inline constexpr void record_tiered_llvm_jit_switch(compiled_module_record& rec) noexcept
        {
            // Each successful switch into LLVM code is evidence that a full T2 artifact may eventually amortize.
            auto const switch_count{::std::atomic_ref<::std::size_t>{rec.tiered_switch_count}.fetch_add(1uz, ::std::memory_order_relaxed) + 1uz};
            auto const full_compile_switch_threshold{tiered_full_compile_switch_request_threshold(rec)};
            if(tiered_t2_enabled() && switch_count >= full_compile_switch_threshold &&
               rec.tiered_full_compile_state.state.load(::std::memory_order_relaxed) == ::uwvm2::utils::thread::lazy_compile_state::uncompiled)
            {
                maybe_request_tiered_full_compile(rec, u8"switch");
            }
        }

        inline constexpr void record_tiered_entry_miss(compiled_module_record& rec, ::std::size_t local_index, ::std::uint_least32_t stride) noexcept
        {
            // Add the sampling stride rather than one so the counter approximates unsampled misses.
            static_cast<void>(local_index);
            if(stride == 0u) [[unlikely]] { stride = 1u; }
            ::std::atomic_ref<::std::size_t>{rec.tiered_entry_miss_count}.fetch_add(static_cast<::std::size_t>(stride), ::std::memory_order_relaxed);
            static_cast<void>(tiered_large_module_long_run_active(rec));
        }

        inline constexpr void record_tiered_interpreter_entry(compiled_module_record& rec, ::std::size_t local_index) noexcept
        {
            // Large-module fallback entries are sampled separately from failed LLVM demands because they indicate interpreter load.
            if(rec.llvm_jit_lazy_compiled.functions.size() < tiered_large_long_run_local_function_threshold) { return; }
            auto const stride{tiered_large_long_run_counter_sample_stride(rec)};
            if(tiered_large_long_run_counter_sampled(local_index, stride))
            {
                ::std::atomic_ref<::std::size_t>{rec.tiered_interpreter_fallback_count}.fetch_add(static_cast<::std::size_t>(stride),
                                                                                                  ::std::memory_order_relaxed);
                static_cast<void>(tiered_large_module_long_run_active(rec));
            }
        }

        inline constexpr void record_tiered_interpreter_entry(::std::size_t module_id, ::std::size_t function_index) noexcept
        {
            // Public-facing recorder accepts wasm function indices and maps them back to local indices when possible.
            if(module_id < g_runtime.modules.size())
            {
                auto& rec{g_runtime.modules.index_unchecked(module_id)};
                auto const runtime_module{rec.runtime_module};
                if(runtime_module != nullptr)
                {
                    auto const import_n{runtime_module->imported_function_vec_storage.size()};
                    if(function_index >= import_n)
                    {
                        auto const local_index{function_index - import_n};
                        if(local_index < rec.llvm_jit_lazy_compiled.functions.size())
                        {
                            record_tiered_interpreter_entry(rec, local_index);
                            return;
                        }
                    }
                }
            }
        }

        inline constexpr void record_tiered_osr_compile_request() noexcept
        {
            // OSR request counts are log-only metrics, not scheduling state.
            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]] { g_runtime.tiered_osr_compile_request_count.fetch_add(1uz, ::std::memory_order_relaxed); }
        }

        inline constexpr void reset_tiered_runtime_log_metrics() noexcept
        {
            // Reset counters when a new lazy run starts so the runtime log describes the current execution window only.
            g_runtime.tiered_osr_callback_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_osr_ready_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_osr_miss_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_osr_deferred_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_osr_compile_request_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_osr_urgent_request_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_full_compile_request_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_full_compile_ready_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_full_compile_failed_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.tiered_full_publish_count.store(0uz, ::std::memory_order_relaxed);
        }

        [[nodiscard]] inline constexpr ::std::uint_least32_t tiered_entry_hot_request_threshold(compiled_module_record const& rec,
                                                                                                ::std::size_t local_index) noexcept
        {
            // Entry thresholds scale with module size and function size. Small modules can afford earlier JIT, while very large or
            // very large-code functions avoid demand JIT until stronger evidence appears.
            ::std::uint_least32_t threshold{262144u};
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n < 128uz) { threshold = 8192u; }
            else if(local_n < 512uz) { threshold = 16384u; }
            else if(local_n >= 8192uz) { threshold = 262144u; }
            else if(local_n >= 512uz) { threshold = 65536u; }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            if(local_n < 128uz)
            {
                if(code_size <= 128uz) { threshold = (threshold > 2048u) ? 2048u : threshold; }
                else if(code_size <= 512uz) { threshold = (threshold > 4096u) ? 4096u : threshold; }
                else if(code_size <= 1024uz) { threshold = (threshold > 8192u) ? 8192u : threshold; }
                else if(code_size <= 4096uz) { threshold = (threshold > 16384u) ? 16384u : threshold; }
            }
            else if(local_n < 512uz)
            {
                if(code_size <= 128uz) { threshold = (threshold > 4096u) ? 4096u : threshold; }
                else if(code_size <= 512uz) { threshold = (threshold > 8192u) ? 8192u : threshold; }
                else if(code_size <= 4096uz) { threshold = (threshold > 32768u) ? 32768u : threshold; }
            }
            else if(local_n < 4096uz)
            {
                if(code_size <= 128uz) { threshold = (threshold > 8192u) ? 8192u : threshold; }
                else if(code_size <= 512uz) { threshold = (threshold > 16384u) ? 16384u : threshold; }
                else if(code_size <= 4096uz) { threshold = (threshold > 65536u) ? 65536u : threshold; }
            }
            else if(local_n >= tiered_large_long_run_local_function_threshold && tiered_large_module_long_run_active(rec))
            {
                if(code_size <= 128uz) { threshold = (threshold > 8192u) ? 8192u : threshold; }
                else if(code_size <= 512uz) { threshold = (threshold > 16384u) ? 16384u : threshold; }
                else if(code_size <= 1024uz) { threshold = (threshold > 32768u) ? 32768u : threshold; }
                else if(code_size <= 4096uz) { threshold = (threshold > 65536u) ? 65536u : threshold; }
            }

            if(code_size >= 32768uz) { threshold = ::std::numeric_limits<::std::uint_least32_t>::max(); }
            else if(code_size >= 8192uz) { threshold = (threshold < 131072u) ? 131072u : threshold; }
            else if(code_size >= 4096uz) { threshold = (threshold < 32768u) ? 32768u : threshold; }
            return threshold;
        }

        [[nodiscard]] inline constexpr bool tiered_entry_hot_tracking_enabled(compiled_module_record const& rec) noexcept
        {
            // Empty modules have no per-entry counters to update and should stay on the cheap no-tracking path.
            return !rec.llvm_jit_lazy_compiled.functions.empty();
        }

        [[nodiscard]] inline constexpr ::std::uint_least32_t tiered_entry_hot_probe_stride(compiled_module_record const& rec) noexcept
        {
            // Probe less often as module size grows; every sampled miss adds the stride back to approximate true pressure.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n >= 8192uz) { return 16u; }
            if(local_n >= 1024uz) { return 16u; }
            if(local_n >= 512uz) { return 8u; }
            if(local_n >= 128uz) { return 8u; }
            return 4u;
        }

        [[nodiscard]] inline constexpr bool tiered_entry_hot_probe_sampled(::std::size_t local_index, ::std::uint_least32_t stride) noexcept
        {
            // Function-index phasing spreads samples across local functions instead of sampling all hot entries on the same tick.
            if(stride <= 1u) { return true; }
#  if defined(UWVM_USE_THREAD_LOCAL)
            auto const tick{++tiered_entry_hot_probe_tick};
#  else
            auto const tick{++get_tiered_entry_hot_probe_tick()};
#  endif
            auto const phase{static_cast<::std::uint_least32_t>(local_index * 0x9e3779b1uz)};
            return ((tick ^ phase) & (stride - 1u)) == 0u;
        }

        [[nodiscard]] inline constexpr bool tiered_function_code_has_loop(compiled_module_record const& rec, ::std::size_t local_index) noexcept
        {
            // A cheap bytecode scan detects loop presence for tiny hot functions that benefit from inline demand compilation.
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]] { return false; }

            auto const& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
            if(fn.primary_cu_index >= rec.llvm_jit_lazy_compiled.compile_units.size()) [[unlikely]] { return false; }

            auto const& cu{rec.llvm_jit_lazy_compiled.compile_units.index_unchecked(fn.primary_cu_index)};
            auto code_curr{cu.code_begin};
            auto const code_end{cu.code_end};
            if(code_curr == nullptr || code_end == nullptr || code_curr > code_end) [[unlikely]] { return false; }

            namespace llvm_lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
            using wasm1_code = llvm_lazy::details::all_details::wasm1_code;
            while(code_curr < code_end)
            {
                wasm1_code op{};
                ::std::memcpy(::std::addressof(op), code_curr, sizeof(op));
                if(op == wasm1_code::loop) { return true; }
                if(!llvm_lazy::details::skip_wasm_instruction_for_direct_call_scan(code_curr, code_end)) [[unlikely]] { return false; }
            }
            return false;
        }

        [[nodiscard]] inline constexpr bool tiered_function_code_has_fp_kernel_shape(compiled_module_record const& rec, ::std::size_t local_index) noexcept
        {
            // FP-heavy kernels can be expensive to compile and may not benefit from early OSR in medium modules, so classify them
            // with a lightweight opcode count rather than full analysis.
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]] { return false; }

            auto const& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
            if(fn.primary_cu_index >= rec.llvm_jit_lazy_compiled.compile_units.size()) [[unlikely]] { return false; }

            auto const& cu{rec.llvm_jit_lazy_compiled.compile_units.index_unchecked(fn.primary_cu_index)};
            auto code_curr{cu.code_begin};
            auto const code_end{cu.code_end};
            if(code_curr == nullptr || code_end == nullptr || code_curr > code_end) [[unlikely]] { return false; }

            namespace llvm_lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
            using wasm1_code = llvm_lazy::details::all_details::wasm1_code;
            ::std::size_t fp_ops{};
            while(code_curr < code_end)
            {
                wasm1_code op{};
                ::std::memcpy(::std::addressof(op), code_curr, sizeof(op));
                auto const op_value{static_cast<unsigned>(op)};
                if(op_value == 0x2au || op_value == 0x2bu || op_value == 0x38u || op_value == 0x39u || (op_value >= 0x8bu && op_value <= 0xa6u) ||
                   (op_value >= 0xb2u && op_value <= 0xbbu))
                {
                    if(++fp_ops >= 8uz) { return true; }
                }
                if(!llvm_lazy::details::skip_wasm_instruction_for_direct_call_scan(code_curr, code_end)) [[unlikely]] { return false; }
            }
            return false;
        }

        [[nodiscard]] inline constexpr bool tiered_loop_osr_should_suppress_medium_fp_kernel(compiled_module_record const& rec,
                                                                                             ::std::size_t local_index) noexcept
        {
            // Medium floating-point kernels often compile slowly; suppress OSR unless other heuristics identify a better target.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n < 16uz || local_n >= 128uz) { return false; }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            return code_size >= 1024uz && tiered_function_code_has_fp_kernel_shape(rec, local_index);
        }

        [[nodiscard]] inline constexpr bool tiered_entry_should_compile_inline_for_small_hot_loop(compiled_module_record const& rec,
                                                                                                  ::std::size_t local_index) noexcept
        {
            // Very small loop-heavy modules can finish fastest by compiling synchronously on the demanding execution thread.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n == 0uz || local_n > 8uz) { return false; }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            return code_size >= 96uz && code_size <= 640uz && !tiered_function_code_has_fp_kernel_shape(rec, local_index) &&
                   tiered_function_code_has_loop(rec, local_index);
        }

        [[nodiscard]] inline constexpr bool tiered_entry_hot_should_use_urgent_scheduler(compiled_module_record& rec, ::std::size_t local_index) noexcept
        {
            // Once hot enough to request LLVM, pick the urgent lane when latency matters more than background throughput.
            if(!g_runtime.tiered_urgent_scheduler.running()) { return false; }

            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n < 512uz) { return true; }
            if(tiered_large_module_long_run_active(rec)) { return true; }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            return code_size >= 1024uz;
        }

        [[nodiscard]] inline constexpr bool tiered_entry_is_hot_enough_to_request_llvm(compiled_module_record& rec, ::std::size_t local_index) noexcept
        {
            // Sample entry misses instead of counting every miss exactly. This keeps the interpreter entry path cheap while still
            // converging on hot functions.
            if(local_index >= rec.tiered_entry_hot_counters.size()) [[unlikely]] { return true; }
            auto const probe_stride{tiered_entry_hot_probe_stride(rec)};
            auto const sampled{tiered_entry_hot_probe_sampled(local_index, probe_stride)};
            if(sampled) { record_tiered_entry_miss(rec, local_index, probe_stride); }

            auto const request_threshold{tiered_entry_hot_request_threshold(rec, local_index)};
            if(request_threshold == (::std::numeric_limits<::std::uint_least32_t>::max)()) { return false; }

            auto& counter{rec.tiered_entry_hot_counters.index_unchecked(local_index)};
            ::std::atomic_ref<::std::uint_least32_t> counter_ref{counter};
            if(counter_ref.load(::std::memory_order_relaxed) >= request_threshold) { return true; }

            if(!sampled) { return false; }

            auto const observed{counter_ref.fetch_add(probe_stride, ::std::memory_order_relaxed)};
            return observed >= request_threshold || observed + probe_stride >= request_threshold;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool tiered_entry_miss_should_enter_prepare(compiled_module_record& rec,
                                                                                                      ::std::size_t local_index) noexcept
        {
            // Cheap pre-filter for interpreter entry stubs: skip the heavier prepare path until a function is ready or hot enough.
            if(tiered_full_ready(rec)) { return true; }
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]] { return true; }

            auto& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st != ::uwvm2::utils::thread::lazy_compile_state::uncompiled) { return true; }
            if(tiered_entry_should_compile_inline_for_small_hot_loop(rec, local_index)) { return true; }
            if(!tiered_entry_hot_tracking_enabled(rec)) { return false; }

            return tiered_entry_is_hot_enough_to_request_llvm(rec, local_index);
        }

        [[nodiscard]] inline constexpr bool tiered_loop_osr_can_request_llvm(compiled_module_record const& rec, ::std::size_t local_index) noexcept
        {
            // Loop OSR is restricted for cold modules and medium FP-heavy kernels so compilation does not dominate execution time.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            if(code_size >= ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_large_loop_sentinel_function_size)
            {
                return tiered_large_loop_sentinel_compile_active(rec);
            }
            if(!::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_osr_poll_enabled_for_module_local_function_count(local_n)) { return false; }
            if(tiered_loop_osr_should_suppress_medium_fp_kernel(rec, local_index)) { return false; }
            return true;
        }

        [[nodiscard]] inline constexpr ::std::uint_least32_t tiered_loop_osr_request_threshold(compiled_module_record const& rec,
                                                                                               ::std::size_t local_index) noexcept
        {
            // OSR thresholds model "loop work performed" rather than raw callbacks. Small modules wait longer unless runtime evidence
            // shows they are long-running.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(tiered_large_loop_sentinel_candidate(rec, local_index)) { return tiered_large_loop_osr_request_threshold; }
            if(!::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_osr_poll_enabled_for_module_local_function_count(local_n))
            {
                return (::std::numeric_limits<::std::uint_least32_t>::max)();
            }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            if(local_n >= 16uz && local_n < 128uz)
            {
                if(tiered_small_module_long_run_active(rec))
                {
                    if(code_size >= 4096uz) { return 4u; }
                    if(code_size >= 1024uz) { return 2u; }
                    return 4u;
                }
                if(code_size >= 4096uz) { return 4u; }
                if(code_size >= 1536uz) { return 2u; }

                constexpr ::std::size_t cold_small_module_osr_work_threshold{8uz * 1024uz * 1024uz};
                auto threshold{code_size == 0uz ? cold_small_module_osr_work_threshold : (cold_small_module_osr_work_threshold + code_size - 1uz) / code_size};
                auto const min_threshold{code_size >= 4096uz ? 256uz : 512uz};
                auto const max_threshold{65535uz};
                if(threshold < min_threshold) { threshold = min_threshold; }
                if(threshold > max_threshold) { threshold = max_threshold; }
                return static_cast<::std::uint_least32_t>(threshold);
            }

            return 1u;
        }

        [[nodiscard]] inline constexpr bool tiered_loop_osr_is_hot_enough_to_request_llvm(compiled_module_record& rec, ::std::size_t local_index) noexcept
        {
            // Loop OSR increments exact per-function counters because loop polls are already coarser than entry misses.
            if(local_index >= rec.tiered_osr_request_counters.size()) [[unlikely]] { return true; }
            auto const request_threshold{tiered_loop_osr_request_threshold(rec, local_index)};
            if(request_threshold == (::std::numeric_limits<::std::uint_least32_t>::max)()) { return false; }

            auto& counter{rec.tiered_osr_request_counters.index_unchecked(local_index)};
            ::std::atomic_ref<::std::uint_least32_t> counter_ref{counter};
            auto const observed{counter_ref.fetch_add(1u, ::std::memory_order_relaxed)};
            return observed >= request_threshold || observed + 1u >= request_threshold;
        }

        [[nodiscard]] inline constexpr bool try_publish_tiered_ready_llvm_jit_entry(compiled_module_record& rec,
                                                                                    ::std::size_t module_id,
                                                                                    ::std::size_t local_index,
                                                                                    ::std::uintptr_t& raw_entry_address) noexcept
        {
            // Publish the best available tier for a single function, preferring full T2 entries over lazy T1 entries.
            raw_entry_address = 0u;
            if(try_publish_tiered_ready_full_llvm_jit_entry(rec, module_id, local_index, raw_entry_address)) { return true; }

            if(!::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_raw_entry_address(rec.llvm_jit_lazy_compiled,
                                                                                                                     local_index,
                                                                                                                     raw_entry_address))
            {
                return false;
            }

            ::std::uintptr_t typed_entry_address{};
            static_cast<void>(::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_entry_address(rec.llvm_jit_lazy_compiled,
                                                                                                                               local_index,
                                                                                                                               typed_entry_address));
            if(!publish_tiered_llvm_jit_entry_targets(rec, module_id, local_index, raw_entry_address, typed_entry_address))
            {
                raw_entry_address = 0u;
                return false;
            }
            return true;
        }

        [[nodiscard]] inline constexpr tiered_llvm_jit_demand_state
            prepare_tiered_llvm_jit_defined_function(::std::size_t module_id, ::std::size_t function_index, ::std::uintptr_t& raw_entry_address) noexcept
        {
            // Demand entry preparation first tries already-published T2/T1 code, then decides whether to request a new lazy JIT
            // artifact or leave execution in the interpreter tier.
            raw_entry_address = 0u;
            if(!tiered_runtime_active()) { return tiered_llvm_jit_demand_state::unavailable; }
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return tiered_llvm_jit_demand_state::unavailable; }

            auto& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return tiered_llvm_jit_demand_state::unavailable; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) [[unlikely]] { return tiered_llvm_jit_demand_state::unavailable; }
            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size() || local_index >= rec.llvm_jit_lazy_background_request_contexts.size()) [[unlikely]]
            {
                return tiered_llvm_jit_demand_state::unavailable;
            }
            if(!tiered_local_function_direct_supported(rec, local_index)) { return tiered_llvm_jit_demand_state::unavailable; }

            if(try_publish_tiered_ready_full_llvm_jit_entry(rec, module_id, local_index, raw_entry_address))
            {
                record_tiered_lazy_compiled_hit();
                return tiered_llvm_jit_demand_state::ready;
            }

            if(try_publish_tiered_ready_llvm_jit_entry(rec, module_id, local_index, raw_entry_address))
            {
                record_tiered_lazy_compiled_hit();
                return tiered_llvm_jit_demand_state::ready;
            }

            auto& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled)
            {
                return try_publish_tiered_ready_llvm_jit_entry(rec, module_id, local_index, raw_entry_address) ? tiered_llvm_jit_demand_state::ready
                                                                                                               : tiered_llvm_jit_demand_state::unavailable;
            }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]] { return tiered_llvm_jit_demand_state::failed; }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::queued || st == ::uwvm2::utils::thread::lazy_compile_state::compiling)
            {
                return tiered_llvm_jit_demand_state::busy;
            }
            auto const use_inline_compile{tiered_entry_should_compile_inline_for_small_hot_loop(rec, local_index)};
            if(!use_inline_compile && !tiered_entry_is_hot_enough_to_request_llvm(rec, local_index)) { return tiered_llvm_jit_demand_state::busy; }

            if(fn.primary_cu_index >= rec.llvm_jit_lazy_compiled.compile_units.size()) [[unlikely]] { return tiered_llvm_jit_demand_state::unavailable; }

            auto& ctx{rec.llvm_jit_lazy_background_request_contexts.index_unchecked(local_index)};
            auto request{::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::make_lazy_compile_request(ctx, 1u)};
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return tiered_llvm_jit_demand_state::unavailable; }

            ensure_tiered_jit_schedulers_started();
            auto const use_urgent_scheduler{!use_inline_compile && tiered_entry_hot_should_use_urgent_scheduler(rec, local_index)};
            record_tiered_lazy_miss();
            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
            {
                auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
                auto const entry_threshold{tiered_entry_hot_request_threshold(rec, local_index)};
                auto const entry_probe_stride{tiered_entry_hot_probe_stride(rec)};
                ::std::uint_least32_t entry_counter{};
                if(local_index < rec.tiered_entry_hot_counters.size())
                {
                    auto& counter{rec.tiered_entry_hot_counters.index_unchecked(local_index)};
                    entry_counter = ::std::atomic_ref<::std::uint_least32_t>{counter}.load(::std::memory_order_relaxed);
                }
                ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(
                    u8"tiered-demand-request module=\"",
                    rec.module_name,
                    u8"\" module_id=",
                    module_id,
                    u8" fn=",
                    function_index,
                    u8" local_fn=",
                    local_index,
                    u8" cu=",
                    fn.primary_cu_index,
                    u8" size=",
                    code_size,
                    u8" local_functions=",
                    rec.llvm_jit_lazy_compiled.functions.size(),
                    u8" entry_threshold=",
                    entry_threshold,
                    u8" entry_probe_stride=",
                    entry_probe_stride,
                    u8" entry_counter=",
                    entry_counter,
                    u8" state=",
                    ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::compile_state_name(st),
                    u8" priority=1 lane=",
                    use_inline_compile ? u8"inline" : (use_urgent_scheduler ? u8"urgent" : u8"normal"));
            }

            if(use_inline_compile)
            {
                if(!g_runtime.lazy_scheduler.ensure_ready(request))
                {
                    return request.unit->state.load(::std::memory_order_acquire) == ::uwvm2::utils::thread::lazy_compile_state::failed
                               ? tiered_llvm_jit_demand_state::failed
                               : tiered_llvm_jit_demand_state::busy;
                }
                return try_publish_tiered_ready_llvm_jit_entry(rec, module_id, local_index, raw_entry_address) ? tiered_llvm_jit_demand_state::ready
                                                                                                               : tiered_llvm_jit_demand_state::unavailable;
            }

            if(use_urgent_scheduler)
            {
                if(g_runtime.tiered_urgent_scheduler.try_request_or_shadow_queued(request))
                {
                    g_runtime.tiered_osr_urgent_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
                    return tiered_llvm_jit_demand_state::busy;
                }
            }

            if(g_runtime.lazy_scheduler.running()) { static_cast<void>(g_runtime.lazy_scheduler.try_request(request)); }
            return request.unit->state.load(::std::memory_order_acquire) == ::uwvm2::utils::thread::lazy_compile_state::failed
                       ? tiered_llvm_jit_demand_state::failed
                       : tiered_llvm_jit_demand_state::busy;
        }

        [[nodiscard]] inline constexpr bool try_get_tiered_ready_loop_reentry(compiled_module_record& rec,
                                                                              ::std::size_t module_id,
                                                                              ::std::size_t local_index,
                                                                              ::std::size_t loop_wasm_code_offset,
                                                                              ::std::uintptr_t& reentry_address) noexcept
        {
            // ---------------------------------------------------------------------
            // Loop OSR readiness check.
            // ---------------------------------------------------------------------
            // This helper is intentionally split out from the request path: callers
            // can ask whether a precise loop reentry exists without starting new work.
            // If it finds a reentry, it also publishes the owning function target so
            // subsequent direct calls skip the placeholder bridge.
            //
            // The full-tier branch refreshes direct-call targets even if the specific
            // loop reentry came from lazy metadata, keeping T1 and T2 publication order
            // coherent for mixed entry/OSR workloads.
            // Loop reentry is more specific than a normal raw function entry, so publish the owning function target as a side effect
            // whenever a matching loop body is already materialized.
            reentry_address = 0u;
            auto const full_ready{tiered_full_ready(rec)};
            if(::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_tiered_loop_reentry_raw_entry_address(
                   rec.llvm_jit_lazy_compiled,
                   local_index,
                   loop_wasm_code_offset,
                   reentry_address))
            {
                if(!full_ready)
                {
                    ::std::uintptr_t raw_entry_address{};
                    ::std::uintptr_t typed_entry_address{};
                    if(::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_raw_entry_address(rec.llvm_jit_lazy_compiled,
                                                                                                                            local_index,
                                                                                                                            raw_entry_address))
                    {
                        static_cast<void>(
                            ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_entry_address(rec.llvm_jit_lazy_compiled,
                                                                                                                             local_index,
                                                                                                                             typed_entry_address));
                        static_cast<void>(publish_tiered_llvm_jit_entry_targets(rec, module_id, local_index, raw_entry_address, typed_entry_address));
                    }
                }
                else
                {
                    ::std::uintptr_t full_raw_entry_address{};
                    static_cast<void>(try_publish_tiered_ready_full_llvm_jit_entry(rec, module_id, local_index, full_raw_entry_address));
                }

                return reentry_address != 0u;
            }

            if(full_ready)
            {
                ::std::uintptr_t full_raw_entry_address{};
                static_cast<void>(try_publish_tiered_ready_full_llvm_jit_entry(rec, module_id, local_index, full_raw_entry_address));
            }

            return false;
        }

        [[nodiscard]] inline constexpr bool tiered_loop_osr_should_use_urgent_scheduler(compiled_module_record& rec, ::std::size_t local_index) noexcept
        {
            // Urgent OSR requests are reserved for large modules, proven long runs, or sizable loop bodies.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n >= 512uz) { return true; }
            if(tiered_large_module_long_run_active(rec)) { return true; }
            if(local_n < 128uz) { return tiered_small_module_long_run_active(rec); }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            return code_size >= 4096uz;
        }

        [[nodiscard]] inline constexpr bool tiered_loop_osr_should_compile_inline_after_deferred_sample(compiled_module_record const& rec,
                                                                                                        ::std::size_t local_index) noexcept
        {
            // Inline OSR compilation is allowed only for medium-sized loops where the wait is likely cheaper than another fallback.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n >= 512uz) { return false; }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            if(local_n < 128uz && !tiered_small_module_long_run_active(rec) && code_size < 1536uz) { return false; }

            return code_size >= 1024uz && code_size < 4096uz;
        }

        [[nodiscard]] inline constexpr bool tiered_loop_osr_should_wait_for_urgent_compile(compiled_module_record const& rec,
                                                                                           ::std::size_t local_index) noexcept
        {
            // Waiting is useful for moderate functions where the urgent worker can plausibly finish before much more interpreter work.
            auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
            if(local_n >= 512uz) { return false; }

            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            return code_size < 8192uz;
        }

        [[nodiscard]] inline constexpr bool try_request_tiered_loop_reentry_compile(compiled_module_record& rec, ::std::size_t local_index) noexcept
        {
            // ---------------------------------------------------------------------
            // Loop OSR request path.
            // ---------------------------------------------------------------------
            // Request priority is higher than normal entry-demand compilation because
            // the interpreter has already reached a hot loop poll. The lane selection
            // below keeps synchronous compilation for bounded small cases and routes
            // larger loops through urgent/background schedulers.
            // Route loop reentry compilation through the cheapest lane likely to finish soon: inline for small urgent cases, urgent
            // scheduler for hot/large cases, and the normal lazy scheduler otherwise.
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size() || local_index >= rec.llvm_jit_lazy_background_request_contexts.size()) [[unlikely]]
            {
                return false;
            }

            auto& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]] { return false; }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled) { return true; }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiling) { return true; }
            if(fn.primary_cu_index >= rec.llvm_jit_lazy_compiled.compile_units.size()) [[unlikely]] { return false; }

            auto& ctx{rec.llvm_jit_lazy_background_request_contexts.index_unchecked(local_index)};
            auto request{::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::make_lazy_compile_request(ctx, 2u)};
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return false; }

            ensure_tiered_jit_schedulers_started();
            auto const use_inline_compile{tiered_loop_osr_should_compile_inline_after_deferred_sample(rec, local_index)};
            auto const use_urgent_scheduler{tiered_loop_osr_should_use_urgent_scheduler(rec, local_index) && g_runtime.tiered_urgent_scheduler.running()};
            record_tiered_lazy_miss();
            if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
            {
                auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
                auto const osr_threshold{tiered_loop_osr_request_threshold(rec, local_index)};
                ::std::uint_least32_t osr_counter{};
                if(local_index < rec.tiered_osr_request_counters.size())
                {
                    auto& counter{rec.tiered_osr_request_counters.index_unchecked(local_index)};
                    osr_counter = ::std::atomic_ref<::std::uint_least32_t>{counter}.load(::std::memory_order_relaxed);
                }
                ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(
                    u8"tiered-osr-request module=\"",
                    rec.module_name,
                    u8"\" module_id=",
                    module_id_from_record(rec),
                    u8" fn=",
                    fn.function_index,
                    u8" local_fn=",
                    local_index,
                    u8" cu=",
                    fn.primary_cu_index,
                    u8" size=",
                    code_size,
                    u8" local_functions=",
                    rec.llvm_jit_lazy_compiled.functions.size(),
                    u8" osr_threshold=",
                    osr_threshold,
                    u8" osr_counter=",
                    osr_counter,
                    u8" state=",
                    ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::compile_state_name(st),
                    u8" priority=2 lane=",
                    use_inline_compile ? u8"inline" : (use_urgent_scheduler ? u8"urgent" : u8"normal"));
            }

            if(use_inline_compile)
            {
                record_tiered_osr_compile_request();
                return g_runtime.lazy_scheduler.ensure_ready(request);
            }

            if(use_urgent_scheduler)
            {
                if(g_runtime.tiered_urgent_scheduler.try_request_or_shadow_queued(request))
                {
                    record_tiered_osr_compile_request();
                    g_runtime.tiered_osr_urgent_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
                    if(tiered_loop_osr_should_wait_for_urgent_compile(rec, local_index))
                    {
                        return g_runtime.tiered_urgent_scheduler.wait_until_ready_passive(*request.unit);
                    }
                    return true;
                }
            }

            if(g_runtime.lazy_scheduler.running())
            {
                if(g_runtime.lazy_scheduler.try_request(request)) { record_tiered_osr_compile_request(); }
            }
            return true;
        }

        [[nodiscard]] UWVM_NOINLINE bool tiered_try_enter_loop_osr_impl(::std::size_t module_id,
                                                                        ::std::size_t function_index,
                                                                        ::std::size_t loop_wasm_code_offset,
                                                                        ::std::byte* result_buffer,
                                                                        ::std::size_t result_bytes,
                                                                        ::std::byte const* local_base,
                                                                        ::std::size_t local_bytes,
                                                                        ::std::uintptr_t* compile_state_address_ptr) noexcept
        {
            // Interpreter loop polls arrive here when execution might jump into generated code. Keep this noinline so profiler and
            // trap diagnostics see a clear boundary between interpreter execution and runtime OSR glue.
            if(!tiered_runtime_active()) { return false; }
            auto const log_enabled{::uwvm2::uwvm::io::enable_runtime_log};
            if(log_enabled) [[unlikely]] { g_runtime.tiered_osr_callback_count.fetch_add(1uz, ::std::memory_order_relaxed); }
            // If this OSR site proves unusable, write the disabled sentinel back into the interpreter-side poll state.
            auto const disable_poll{[&]() constexpr noexcept
                                    {
                                        if(compile_state_address_ptr != nullptr)
                                        {
                                            *compile_state_address_ptr =
                                                ::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_loop_osr_disabled_state_address;
                                        }
                                    }};
            auto const record_miss{[&]() constexpr noexcept -> bool
                                   {
                                       if(log_enabled) [[unlikely]] { g_runtime.tiered_osr_miss_count.fetch_add(1uz, ::std::memory_order_relaxed); }
                                       return false;
                                   }};
            // Validate the raw buffers supplied by the interpreter loop before touching module metadata.
            if((result_bytes != 0uz && result_buffer == nullptr) || (local_bytes != 0uz && local_base == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(module_id >= g_runtime.modules.size()) [[unlikely]]
            {
                disable_poll();
                return record_miss();
            }

            auto& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]]
            {
                disable_poll();
                return record_miss();
            }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) [[unlikely]]
            {
                disable_poll();
                return record_miss();
            }
            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]]
            {
                disable_poll();
                return record_miss();
            }
            auto const large_loop_sentinel{tiered_large_loop_sentinel_candidate(rec, local_index)};
            // Large-loop sentinels record evidence even when the normal OSR gate is not open yet.
            if(large_loop_sentinel) { record_tiered_large_loop_sample(rec, local_index, loop_wasm_code_offset); }
            if(!tiered_loop_osr_can_request_llvm(rec, local_index))
            {
                if(large_loop_sentinel) { return record_miss(); }
                disable_poll();
                return record_miss();
            }
            auto& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
            if(compile_state_address_ptr != nullptr && *compile_state_address_ptr == 0u)
            {
                // Hand the interpreter a stable atomic state address so future loop polls can cheaply observe compile progress.
                *compile_state_address_ptr = reinterpret_cast<::std::uintptr_t>(::std::addressof(fn.materialization_state.state));
            }
            if(!tiered_local_function_direct_supported(rec, local_index))
            {
                disable_poll();
                return record_miss();
            }

            ::std::uintptr_t reentry_address{};
            if(!try_get_tiered_ready_loop_reentry(rec, module_id, local_index, loop_wasm_code_offset, reentry_address))
            {
                // No reentry is ready yet. Decide whether this poll is hot enough to request compilation and try one more lookup.
                auto const st_before_request{fn.materialization_state.state.load(::std::memory_order_acquire)};
                if(st_before_request == ::uwvm2::utils::thread::lazy_compile_state::compiled ||
                   st_before_request == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]]
                {
                    disable_poll();
                    return record_miss();
                }
                if(st_before_request == ::uwvm2::utils::thread::lazy_compile_state::uncompiled &&
                   !tiered_loop_osr_is_hot_enough_to_request_llvm(rec, local_index))
                {
                    if(log_enabled) [[unlikely]] { g_runtime.tiered_osr_deferred_count.fetch_add(1uz, ::std::memory_order_relaxed); }
                    return record_miss();
                }
                static_cast<void>(try_request_tiered_loop_reentry_compile(rec, local_index));
                if(!try_get_tiered_ready_loop_reentry(rec, module_id, local_index, loop_wasm_code_offset, reentry_address))
                {
                    // Terminal states disable future polling at this loop because another request cannot produce a useful reentry.
                    auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
                    if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled || st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]]
                    {
                        disable_poll();
                    }
                    return record_miss();
                }
            }

            // Tiered OSR reentry wrappers are generated as raw Wasm-entry targets.  Keep this pointer ABI in lockstep
            // with the LLVM raw-entry calling convention and uwvm-int's opfunc ABI policy: Windows x86_64 is SysV,
            // every i686 target is fastcall, and the rest use the platform default.
            using entry_fn_t =
                void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t, ::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::size_t);
            auto const entry_fn{reinterpret_cast<entry_fn_t>(reentry_address)};
            auto& call_stack{get_call_stack()};
            // OSR transfers control from an interpreter loop into a generated loop body without creating a normal wasm native-call
            // edge for the interpreter callers. Capture the boundary before the raw entry so a trap inside an inlined or optimized
            // loop body can still report the interpreter caller chain below the generated frame.
            tiered_jit_entry_call_stack_snapshot_guard snapshot_guard{call_stack};
            entry_fn(0u, reinterpret_cast<::std::uintptr_t>(result_buffer), result_bytes, reinterpret_cast<::std::uintptr_t>(local_base), local_bytes);
            if(log_enabled) [[unlikely]] { g_runtime.tiered_osr_ready_count.fetch_add(1uz, ::std::memory_order_relaxed); }
            record_tiered_llvm_jit_switch(rec);
            return true;
        }

        [[nodiscard]] UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr bool
            tiered_try_enter_loop_osr(::std::size_t module_id,
                                      ::std::size_t function_index,
                                      ::std::size_t loop_wasm_code_offset,
                                      ::std::byte* result_buffer,
                                      ::std::size_t result_bytes,
                                      ::std::byte const* local_base,
                                      ::std::size_t local_bytes,
                                      ::std::uintptr_t* compile_state_address_ptr) noexcept
        {
            // Thin ABI-stable callback used by interpreter opfuncs; the noinline implementation owns diagnostics and scheduling.
            return tiered_try_enter_loop_osr_impl(module_id,
                                                  function_index,
                                                  loop_wasm_code_offset,
                                                  result_buffer,
                                                  result_bytes,
                                                  local_base,
                                                  local_bytes,
                                                  compile_state_address_ptr);
        }

# endif

        [[nodiscard]] inline constexpr bool has_llvm_jit_lazy_background_work() noexcept
        {
            // Scan prefetch orders rather than all compile units so the scheduler observes the same priority order it will enqueue.
            for(auto const& rec: g_runtime.modules)
            {
                if(rec.runtime_module == nullptr) [[unlikely]] { continue; }
                auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
                for(auto const local_index: rec.lazy_prefetch_order)
                {
                    if(local_index >= local_n) [[unlikely]] { continue; }
                    auto const& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
                    if(fn.materialization_state.state.load(::std::memory_order_acquire) == ::uwvm2::utils::thread::lazy_compile_state::uncompiled)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        inline constexpr ::std::size_t llvm_jit_urgent_scheduler_queue_capacity{64uz};

        [[nodiscard]] inline constexpr bool ensure_llvm_jit_urgent_scheduler_started() noexcept
        {
            // LLVM lazy demand compilation has an urgent lane independent from normal background prefetching.
            if(g_runtime.llvm_jit_urgent_scheduler.running()) { return true; }

            while(g_runtime.llvm_jit_urgent_start_lock.test_and_set(::std::memory_order_acquire))
            {
                if(g_runtime.llvm_jit_urgent_scheduler.running()) { return true; }
                ::uwvm2::utils::thread::lazy_compile_thread_yield();
            }

            if(!g_runtime.llvm_jit_urgent_scheduler.running())
            {
                g_runtime.llvm_jit_urgent_scheduler.start(
                    {.worker_count = 1uz, .queue_capacity = llvm_jit_urgent_scheduler_queue_capacity, .refill_callback = nullptr, .refill_user_data = nullptr});
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Registered LLVM JIT lazy urgent scheduler (workers=1, queue_capacity=",
                                        llvm_jit_urgent_scheduler_queue_capacity,
                                        u8"). ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }
            }

            g_runtime.llvm_jit_urgent_start_lock.clear(::std::memory_order_release);
            return g_runtime.llvm_jit_urgent_scheduler.running();
        }

        [[nodiscard]] inline constexpr bool llvm_jit_lazy_demand_should_use_urgent_scheduler(compiled_module_record const& rec,
                                                                                             ::std::size_t local_index,
                                                                                             ::uwvm2::utils::thread::lazy_compile_state current_state) noexcept
        {
            // Large cold functions should not wait behind background work once demanded directly.
            auto const code_size{llvm_jit_lazy_compile_unit_code_size(rec, local_index)};
            if(current_state != ::uwvm2::utils::thread::lazy_compile_state::queued && current_state != ::uwvm2::utils::thread::lazy_compile_state::uncompiled)
            {
                return false;
            }
            if(code_size < 4096uz) { return false; }
            return ensure_llvm_jit_urgent_scheduler_started();
        }
#endif

        [[nodiscard]] inline constexpr ::std::size_t valtype_size(::std::uint_least8_t code) noexcept
        {
            // Host ABI staging uses byte-packed scalar values. Unsupported value types return zero and are rejected by callers.
            switch(static_cast<wasm_value_type>(code))
            {
                case wasm_value_type::i32:
                {
                    return 4uz;
                }
                case wasm_value_type::i64:
                {
                    return 8uz;
                }
                case wasm_value_type::f32:
                {
                    return 4uz;
                }
                case wasm_value_type::f64:
                {
                    return 8uz;
                }
                default:
                {
                    if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::v128)) { return 16uz; }
                    return 0uz;
                }
            }
        }

        [[nodiscard]] inline constexpr bool func_sig_equal(func_sig_view const& a, func_sig_view const& b) noexcept
        {
            // call_indirect and import validation compare normalized signatures, not pointer identity, because canonicalization can
            // cross imported/local-defined boundaries.
            if(a.params.size != b.params.size || a.results.size != b.results.size) { return false; }
            for(::std::size_t i{}; i != a.params.size; ++i)
            {
                if(a.params.at(i) != b.params.at(i)) { return false; }
            }
            for(::std::size_t i{}; i != a.results.size; ++i)
            {
                if(a.results.at(i) != b.results.at(i)) { return false; }
            }
            return true;
        }

        [[nodiscard]] inline constexpr ::std::size_t total_abi_bytes(valtype_vec_view const& v) noexcept
        {
            // Convert a wasm/C-ABI value-type vector into the exact byte count used by raw entry buffers and interpreter stacks.
            ::std::size_t total{};
            for(::std::size_t i{}; i != v.size; ++i)
            {
                auto const sz{valtype_size(v.at(i))};
                if(sz == 0uz) [[unlikely]] { return 0uz; }
                if(total > (::std::numeric_limits<::std::size_t>::max() - sz)) [[unlikely]] { return 0uz; }
                total += sz;
            }
            return total;
        }

        inline constexpr void validate_entry_run_buffers(::uwvm2::utils::container::u8string_view main_module_name,
                                                         ::std::size_t param_count,
                                                         ::std::size_t param_bytes,
                                                         ::std::size_t result_count,
                                                         ::std::size_t result_bytes,
                                                         ::std::byte const* param_buffer,
                                                         ::std::size_t configured_param_bytes,
                                                         ::std::byte* result_buffer,
                                                         ::std::size_t configured_result_bytes) noexcept
        {
            // Entry buffers are supplied by the host. Validate both type support and exact byte sizes before the runtime copies data
            // into interpreter/JIT call frames.
            if((param_count != 0uz && param_bytes == 0uz) || (result_count != 0uz && result_bytes == 0uz)) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function signature contains value types unsupported by the runtime entry ABI (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            if(configured_param_bytes != param_bytes || configured_result_bytes != result_bytes || (param_bytes != 0uz && param_buffer == nullptr) ||
               (result_bytes != 0uz && result_buffer == nullptr)) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function ABI buffers do not match the function signature (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\", expected-param-bytes=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    param_bytes,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8", configured-param-bytes=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    configured_param_bytes,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8", expected-result-bytes=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    result_bytes,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8", configured-result-bytes=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    configured_result_bytes,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }
        }

        [[nodiscard]] inline constexpr func_sig_view func_sig_from_defined(runtime_local_func_storage_t const* f) noexcept
        {
            // Defined wasm functions already point at the canonical runtime function type.
            auto const ft{f->function_type_ptr};
            return {
                {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
            };
        }

        [[nodiscard]] inline constexpr func_sig_view func_sig_from_local_imported(local_imported_t const* m, ::std::size_t idx) noexcept
        {
            // Local-imported modules expose signature metadata through their import adapter rather than wasm runtime storage.
            auto const info{m->get_function_information_from_index(idx)};
            if(!info.successed) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& ft{info.function_type};
            return {
                {valtype_kind::wasm_enum, ft.parameter.begin, static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)},
                {valtype_kind::wasm_enum, ft.result.begin,    static_cast<::std::size_t>(ft.result.end - ft.result.begin)      }
            };
        }

        [[nodiscard]] inline constexpr func_sig_view func_sig_from_capi(capi_function_t const* f) noexcept
        {
            // C API preload functions describe signatures as raw value-type bytes; keep them non-owning for cache entries.
            return {
                {valtype_kind::raw_u8, f->para_type_vec_begin, f->para_type_vec_size},
                {valtype_kind::raw_u8, f->res_type_vec_begin,  f->res_type_vec_size }
            };
        }

        [[nodiscard]] inline constexpr preload_module_memory_attribute_t const* find_preload_module_memory_attribute(capi_function_t const* f) noexcept
        {
            // The preload registry owns memory access policy; runtime calls only attach it to resolved C API targets.
            return ::uwvm2::uwvm::wasm::storage::find_loaded_preload_module_memory_attribute(f);
        }

        struct resolved_func
        {
            // Normalized leaf import target after alias chains are resolved.
            enum class kind : unsigned
            {
                defined,
                local_imported,
                dl,
                weak_symbol
            };

            kind k{};

            union
            {
                runtime_local_func_storage_t const* defined_ptr;
                runtime_imported_func_storage_t::local_imported_target_t local_imported;
                capi_function_t const* capi_ptr;
            } u{};
        };

        // Import resolution is performed by uwvm runtime initializer.
        // This runtime only consumes the initialized link_kind/target fields and never performs on-demand linking.
        [[nodiscard]] inline constexpr runtime_imported_func_storage_t const*
            resolve_import_leaf_assuming_initialized(runtime_imported_func_storage_t const* f) noexcept
        {
            // Runtime initialization resolves imports ahead of execution. This helper only follows already-initialized aliases and
            // guards against accidental cycles introduced by internal bugs.
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            auto curr{f};
            for(::std::size_t steps{};; ++steps)
            {
                // The initializer guarantees import-alias chains are finite and acyclic. This guard is for internal bugs.
                if(steps > 8192uz) [[unlikely]] { return nullptr; }
                if(curr == nullptr) [[unlikely]] { return nullptr; }

                switch(curr->link_kind)
                {
                    case link_kind::imported:
                    {
                        curr = curr->target.imported_ptr;
                        continue;
                    }
                    case link_kind::defined: [[fallthrough]];
                    case link_kind::local_imported:
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                    case link_kind::dl:
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                    case link_kind::weak_symbol:
#endif
                    {
                        return curr;
                    }
                    case link_kind::unresolved:
                    {
                        return nullptr;
                    }
                    default:
                    {
                        return nullptr;
                    }
                }
            }
        }

        [[nodiscard]] inline constexpr resolved_func resolve_func_from_import_assuming_initialized(runtime_imported_func_storage_t const* f) noexcept
        {
            // Convert the resolved runtime import leaf into a compact tagged union consumed by import-cache construction.
            using link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
            auto const leaf{resolve_import_leaf_assuming_initialized(f)};
            if(leaf == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            switch(leaf->link_kind)
            {
                case link_kind::defined:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::defined;
                    r.u.defined_ptr = leaf->target.defined_ptr;
                    return r;
                }
                case link_kind::local_imported:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::local_imported;
                    r.u.local_imported = leaf->target.local_imported;
                    return r;
                }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
                case link_kind::dl:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::dl;
                    r.u.capi_ptr = leaf->target.dl_ptr;
                    return r;
                }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                case link_kind::weak_symbol:
                {
                    resolved_func r{};
                    r.k = resolved_func::kind::weak_symbol;
                    r.u.capi_ptr = leaf->target.weak_symbol_ptr;
                    return r;
                }
#endif
                case link_kind::imported: [[fallthrough]];
                default:
                {
                    [[unlikely]] ::fast_io::fast_terminate();
                }
            }
        }

        using byte_allocator =
#if defined(UWVM_USE_THREAD_LOCAL)
            ::fast_io::native_thread_local_allocator;
#else
            ::fast_io::native_global_allocator;
#endif

        struct heap_buf_guard
        {
            void* ptr{};

            // Simple RAII for fallback heap allocations used when alloca would be too large or unavailable.
            inline constexpr heap_buf_guard() noexcept = default;

            heap_buf_guard(heap_buf_guard const&) = delete;
            heap_buf_guard& operator= (heap_buf_guard const&) = delete;

            inline constexpr ~heap_buf_guard()
            {
                if(ptr) { byte_allocator::deallocate(ptr); }
            }
        };

        struct thread_local_bump_allocator
        {
            // Reusable per-thread scratch allocator for large interpreter frames. It avoids repeated heap allocate/free when deep or
            // large wasm calls exceed the safe alloca threshold.
            struct block
            {
                ::std::byte* base{};
                ::std::size_t cap{};
                ::std::size_t sp{};
                block* prev{};
                block* next_free{};
            };

            struct mark_t
            {
                block* curr{};
                ::std::size_t sp{};
            };

            using block_allocator =
#if defined(UWVM_USE_THREAD_LOCAL)
                ::fast_io::native_typed_thread_local_allocator<block>;
#else
                ::fast_io::native_typed_global_allocator<block>;
#endif

            block* curr{};
            block* free_list{};

            thread_local_bump_allocator(thread_local_bump_allocator const&) = delete;
            thread_local_bump_allocator& operator= (thread_local_bump_allocator const&) = delete;

            inline constexpr thread_local_bump_allocator() noexcept = default;

            inline constexpr ~thread_local_bump_allocator()
            {
                destroy_chain(curr);
                destroy_chain(free_list);
            }

            [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr mark_t mark() const noexcept { return {.curr = curr, .sp = curr == nullptr ? 0uz : curr->sp}; }

            UWVM_ALWAYS_INLINE inline constexpr void release(mark_t m) noexcept
            {
                // Release back to a saved mark and recycle whole blocks onto a free list for the next large frame.
                while(curr != m.curr)
                {
                    if(curr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const old{curr};
                    curr = curr->prev;
                    old->sp = 0uz;
                    old->prev = nullptr;
                    old->next_free = free_list;
                    free_list = old;
                }

                if(curr != nullptr)
                {
                    if(m.sp > curr->sp) [[unlikely]] { ::fast_io::fast_terminate(); }
                    curr->sp = m.sp;
                }
                else if(m.sp != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
            }

            [[nodiscard]] UWVM_ALWAYS_INLINE static inline constexpr ::std::size_t align_up(::std::size_t v, ::std::size_t a) noexcept
            { return (v + (a - 1uz)) & ~(a - 1uz); }

            UWVM_ALWAYS_INLINE inline constexpr void push_block(::std::size_t min_cap) noexcept
            {
                // Blocks grow geometrically from 4 KiB so small bursts stay compact while large frames still need only one block.
                auto b{take_free_block(min_cap)};
                if(b == nullptr)
                {
                    auto new_cap{4096uz};
                    while(new_cap < min_cap) { new_cap <<= 1; }

                    auto const mem{static_cast<::std::byte*>(byte_allocator::allocate(new_cap))};
                    if(mem == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    b = block_allocator::allocate(1uz);
                    if(b == nullptr) [[unlikely]]
                    {
                        byte_allocator::deallocate(mem);
                        ::fast_io::fast_terminate();
                    }

                    ::std::construct_at(b, block{.base = mem, .cap = new_cap});
                }

                b->sp = 0uz;
                b->prev = curr;
                b->next_free = nullptr;
                curr = b;
            }

            [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::byte* allocate_bytes(::std::size_t n, ::std::size_t align = 16uz) noexcept
            {
                // The interpreter frame layout expects explicit alignment; invalid alignment is a runtime invariant failure.
                if(n == 0uz) [[unlikely]] { n = 1uz; }

                if(align == 0uz || (align & (align - 1uz)) != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                if(n > (::std::numeric_limits<::std::size_t>::max() - (align - 1uz))) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const min_cap{n + (align - 1uz)};
                if(curr == nullptr) { push_block(min_cap); }

                auto aligned_sp{align_up(curr->sp, align)};
                if(aligned_sp > curr->cap || n > (curr->cap - aligned_sp))
                {
                    push_block(min_cap);
                    aligned_sp = align_up(curr->sp, align);
                    if(aligned_sp > curr->cap || n > (curr->cap - aligned_sp)) [[unlikely]] { ::fast_io::fast_terminate(); }
                }

                auto const p{curr->base + aligned_sp};
                curr->sp = aligned_sp + n;
                return p;
            }

            [[nodiscard]] inline constexpr block* take_free_block(::std::size_t min_cap) noexcept
            {
                block* prev{};
                auto it{free_list};
                while(it != nullptr)
                {
                    if(it->cap >= min_cap)
                    {
                        if(prev == nullptr) { free_list = it->next_free; }
                        else
                        {
                            prev->next_free = it->next_free;
                        }
                        it->next_free = nullptr;
                        return it;
                    }
                    prev = it;
                    it = it->next_free;
                }

                return nullptr;
            }

            inline static constexpr void destroy_chain(block* b) noexcept
            {
                while(b != nullptr)
                {
                    auto const next{b->prev != nullptr ? b->prev : b->next_free};
                    if(b->base != nullptr) { byte_allocator::deallocate(b->base); }
                    ::std::destroy_at(b);
                    block_allocator::deallocate_n(b, 1uz);
                    b = next;
                }
            }
        };

#if defined(UWVM_USE_THREAD_LOCAL)
        // Hot interpreter scratch path: keep direct thread_local access when enabled.
        // The fallback map below is intentionally not used by TLS builds.
# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local thread_local_bump_allocator g_call_scratch{};  // [global] [thread_local]
#else
        inline ::uwvm2::utils::container::concurrent_node_map<os_thread_id_t, thread_local_bump_allocator> g_call_scratch_states{};  // [global]

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr thread_local_bump_allocator& get_call_scratch() noexcept
        {
            auto const id{current_thread_id()};
            thread_local_bump_allocator* scratch{};

            g_call_scratch_states.try_emplace_and_visit(
                id,
                [&](auto& kv) constexpr noexcept { scratch = ::std::addressof(kv.second); },
                [&](auto& kv) constexpr noexcept { scratch = ::std::addressof(kv.second); });

            if(scratch == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            return *scratch;
        }
#endif

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        inline constexpr ::uwvm2::object::memory::linear::native_memory_t const* resolve_memory0_ptr(runtime_module_storage_t const& rt) noexcept
        {
            // WASI preview1 conventionally uses memory[0]. Follow import aliases to locate the actual native memory object.
            using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
            using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

            auto const import_n{rt.imported_memory_vec_storage.size()};
            if(import_n == 0uz)
            {
                if(rt.local_defined_memory_vec_storage.empty()) { return nullptr; }
                return ::std::addressof(rt.local_defined_memory_vec_storage.index_unchecked(0uz).memory);
            }

            constexpr ::std::size_t kMaxChain{4096uz};
            auto curr{::std::addressof(rt.imported_memory_vec_storage.index_unchecked(0uz))};
            for(::std::size_t steps{}; steps != kMaxChain; ++steps)
            {
                if(curr == nullptr) { return nullptr; }

                switch(curr->link_kind)
                {
                    case memory_link_kind::imported:
                    {
                        curr = curr->target.imported_ptr;
                        break;
                    }
                    case memory_link_kind::defined:
                    {
                        auto const def{curr->target.defined_ptr};
                        if(def == nullptr) { return nullptr; }
                        return ::std::addressof(def->memory);
                    }
                    case memory_link_kind::local_imported: [[fallthrough]];
                    case memory_link_kind::unresolved: [[fallthrough]];
                    default:
                    {
                        return nullptr;
                    }
                }
            }
            return nullptr;
        }
#endif

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        inline constexpr ::std::size_t invalid_default_wasip1_memory_runtime_module_id{preload_call_context_t::invalid_module_id};
        inline ::std::size_t default_wasip1_memory_runtime_module_id{invalid_default_wasip1_memory_runtime_module_id};

        [[nodiscard]] inline constexpr bool has_configured_wasip1_module_override_fast_path() noexcept
        { return !::uwvm2::uwvm::imported::wasi::wasip1::storage::configured_wasip1_groups.empty(); }

        inline constexpr void bind_default_wasip1_memory(runtime_module_storage_t const& rt,
                                                         ::std::size_t module_id = invalid_default_wasip1_memory_runtime_module_id) noexcept
        {
            // Best-effort binding: WASI functions will trap/return errors if a caller without memory[0] invokes them.
            // Always overwrite the pointer to avoid using a stale memory from a previous run.
            auto const mem0{resolve_memory0_ptr(rt)};
            ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.wasip1_memory =
                const_cast<::uwvm2::object::memory::linear::native_memory_t*>(mem0);
            default_wasip1_memory_runtime_module_id = module_id;
        }

        inline constexpr void bind_wasip1_memory_for_runtime_module(wasip1_env_type& env, runtime_module_storage_t const& rt) noexcept
        {
            // Bind the resolved memory[0] directly into the selected WASI environment for the duration of an import call.
            auto const mem0{resolve_memory0_ptr(rt)};
            env.wasip1_memory = const_cast<native_memory_t*>(mem0);
        }

        inline constexpr void bind_wasip1_memory_for_runtime_module_id(wasip1_env_type& env, ::std::size_t module_id) noexcept
        {
            // Prefer the per-module cache; fall back to live runtime storage if the cache has not been rebuilt for this module id.
            if(module_id < g_wasip1_runtime_module_context_cache.size()) [[likely]]
            {
                env.wasip1_memory = g_wasip1_runtime_module_context_cache.index_unchecked(module_id).memory0;
                return;
            }

            if(module_id >= g_runtime.modules.size()) [[unlikely]]
            {
                env.wasip1_memory = nullptr;
                return;
            }

            auto const runtime_module{g_runtime.modules.index_unchecked(module_id).runtime_module};
            if(runtime_module == nullptr) [[unlikely]]
            {
                env.wasip1_memory = nullptr;
                return;
            }

            bind_wasip1_memory_for_runtime_module(env, *runtime_module);
        }

        inline constexpr void bind_default_wasip1_memory_for_runtime_module_id(::std::size_t module_id) noexcept
        {
            // Update the process-wide default WASI environment to match the calling runtime module.
            if(module_id >= g_runtime.modules.size()) [[unlikely]]
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.wasip1_memory = nullptr;
                default_wasip1_memory_runtime_module_id = invalid_default_wasip1_memory_runtime_module_id;
                return;
            }

            auto const runtime_module{g_runtime.modules.index_unchecked(module_id).runtime_module};
            if(runtime_module == nullptr) [[unlikely]]
            {
                ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.wasip1_memory = nullptr;
                default_wasip1_memory_runtime_module_id = invalid_default_wasip1_memory_runtime_module_id;
                return;
            }

            bind_default_wasip1_memory(*runtime_module, module_id);
        }

        inline constexpr void bind_wasip1_memory_for_selected_env(wasip1_env_type& env, ::std::size_t module_id) noexcept
        {
            // Keep the global default environment cache coherent for the common path, but bind override environments directly.
            if(::std::addressof(env) == ::std::addressof(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env)) [[likely]]
            {
                bind_default_wasip1_memory_for_runtime_module_id(module_id);
                return;
            }
            bind_wasip1_memory_for_runtime_module_id(env, module_id);
        }

        [[nodiscard]] inline constexpr bool is_current_wasip1_env_selected(wasip1_env_type& env) noexcept
        {
            // Avoid installing a nested scoped environment when the requested env is already active for this thread/call chain.
# if defined(UWVM_USE_THREAD_LOCAL)
            auto const current_env_ptr{::uwvm2::uwvm::imported::wasi::wasip1::storage::current_wasip1_env_ptr};
# else
            auto const current_env_ptr{::uwvm2::uwvm::imported::wasi::wasip1::storage::current_wasip1_env_ptr_ref()};
# endif
            return current_env_ptr == ::std::addressof(env) ||
                   (current_env_ptr == nullptr &&
                    ::std::addressof(env) == ::std::addressof(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env));
        }

        [[nodiscard]] inline constexpr bool try_prepare_default_global_wasip1_env_fast_path(::std::size_t module_id) noexcept
        {
            // With no module-specific overrides, all WASI imports can use the default env after refreshing its memory binding.
            if(has_configured_wasip1_module_override_fast_path()) [[unlikely]] { return false; }

            if(default_wasip1_memory_runtime_module_id != module_id) [[unlikely]] { bind_default_wasip1_memory_for_runtime_module_id(module_id); }
            return true;
        }

        [[nodiscard]] inline constexpr wasip1_module_override_t* find_wasip1_override_for_runtime_module_id_slow(::std::size_t module_id) noexcept
        {
            // Slow path: resolve the module's configured WASI override from the original wasm/preload module registry.
            if(!has_configured_wasip1_module_override_fast_path()) [[likely]] { return nullptr; }
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return nullptr; }

            auto const module_name{g_runtime.modules.index_unchecked(module_id).module_name};
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]] { return nullptr; }

            switch(it->second.type)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm:
                {
                    return ::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_module_override(wasip1_module_target_kind_t::main_wasm, module_name);
                }
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    return ::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_module_override(wasip1_module_target_kind_t::preload_wasm, module_name);
                }
                default:
                {
                    return nullptr;
                }
            }
        }

        [[nodiscard]] inline constexpr wasip1_env_type& resolve_wasip1_env_for_runtime_module_id(::std::size_t module_id) noexcept
        {
            // Return the caller-specific WASI environment, falling back to the default global env when no override applies.
            if(module_id < g_wasip1_runtime_module_context_cache.size()) [[likely]]
            {
                if(auto const env{g_wasip1_runtime_module_context_cache.index_unchecked(module_id).env}; env != nullptr) [[likely]] { return *env; }
            }

            if(auto const state{find_wasip1_override_for_runtime_module_id_slow(module_id)}; state != nullptr) [[likely]] { return state->env; }
            return ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env;
        }

        [[nodiscard]] inline constexpr bool is_wasip1_import_visible_for_runtime_module_id_slow(::std::size_t module_id) noexcept
        {
            // Determine whether a WASI import is allowed for this module, honoring per-module `enabled` overrides.
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return true; }
            if(!has_configured_wasip1_module_override_fast_path()) [[likely]] { return ::uwvm2::uwvm::wasm::storage::local_preload_wasip1; }

            auto const module_name{g_runtime.modules.index_unchecked(module_id).module_name};
            auto const module_it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(module_it == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]] { return true; }

            switch(module_it->second.type)
            {
                case ::uwvm2::uwvm::wasm::type::module_type_t::exec_wasm: [[fallthrough]];
                case ::uwvm2::uwvm::wasm::type::module_type_t::preloaded_wasm:
                {
                    if(auto const state{find_wasip1_override_for_runtime_module_id_slow(module_id)}; state != nullptr && state->enabled_is_set) [[unlikely]]
                    {
                        return state->enabled;
                    }
                    return ::uwvm2::uwvm::wasm::storage::local_preload_wasip1;
                }
                default:
                {
                    return true;
                }
            }
        }

        [[nodiscard]] inline constexpr bool is_wasip1_import_visible_for_runtime_module_id(::std::size_t module_id) noexcept
        {
            // Import visibility is read frequently while constructing import caches, so consult the rebuilt cache first.
            if(module_id < g_wasip1_runtime_module_context_cache.size()) [[likely]]
            {
                return g_wasip1_runtime_module_context_cache.index_unchecked(module_id).import_visible;
            }

            return is_wasip1_import_visible_for_runtime_module_id_slow(module_id);
        }

        inline constexpr void rebuild_wasip1_runtime_module_context_cache() noexcept
        {
            // Cache per-module WASI environment, visibility, and memory[0] so import calls do not repeatedly inspect module metadata.
            g_wasip1_runtime_module_context_cache.clear();
            g_wasip1_runtime_module_context_cache.resize(g_runtime.modules.size());

            for(::std::size_t module_id{}; module_id != g_runtime.modules.size(); ++module_id)
            {
                auto& ctx{g_wasip1_runtime_module_context_cache.index_unchecked(module_id)};
                ctx.env = ::std::addressof(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env);
                if(auto const state{find_wasip1_override_for_runtime_module_id_slow(module_id)}; state != nullptr) { ctx.env = ::std::addressof(state->env); }

                ctx.import_visible = is_wasip1_import_visible_for_runtime_module_id_slow(module_id);

                if(auto const runtime_module{g_runtime.modules.index_unchecked(module_id).runtime_module}; runtime_module != nullptr) [[likely]]
                {
                    ctx.memory0 = const_cast<native_memory_t*>(resolve_memory0_ptr(*runtime_module));
                }
            }
        }

        [[nodiscard]] inline constexpr wasip1_module_override_t* find_wasip1_override_for_preload_capi_function(capi_function_t const* function) noexcept
        {
            // Preloaded C APIs are owned by preload-DL or weak-symbol registries, not by runtime module ids; resolve that owner first.
            if(!has_configured_wasip1_module_override_fast_path()) [[likely]] { return nullptr; }
            auto const owner{::uwvm2::uwvm::wasm::storage::find_preload_capi_function_owner(function)};
            if(owner == nullptr) [[unlikely]] { return nullptr; }

            switch(owner->kind)
            {
# if defined(UWVM_SUPPORT_PRELOAD_DL)
                case ::uwvm2::uwvm::wasm::storage::preload_capi_function_owner_kind_t::preloaded_dl:
                {
                    if(owner->module_index >= ::uwvm2::uwvm::wasm::storage::preloaded_dl.size()) [[unlikely]] { return nullptr; }
                    auto const module_name{::uwvm2::uwvm::wasm::storage::preloaded_dl.index_unchecked(owner->module_index).module_name};
                    return ::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_module_override(wasip1_module_target_kind_t::preloaded_dl, module_name);
                }
# endif
# if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                case ::uwvm2::uwvm::wasm::storage::preload_capi_function_owner_kind_t::weak_symbol:
                {
                    if(owner->module_index >= ::uwvm2::uwvm::wasm::storage::weak_symbol.size()) [[unlikely]] { return nullptr; }
                    auto const module_name{::uwvm2::uwvm::wasm::storage::weak_symbol.index_unchecked(owner->module_index).module_name};
                    return ::uwvm2::uwvm::imported::wasi::wasip1::storage::find_wasip1_module_override(wasip1_module_target_kind_t::weak_symbol, module_name);
                }
# endif
                default:
                {
                    return nullptr;
                }
            }
        }

        [[nodiscard]] inline constexpr wasip1_env_type& resolve_wasip1_env_for_preload_capi_function(capi_function_t const* function) noexcept
        {
            // C API functions can have their own WASI override separate from the wasm caller module.
            if(auto const state{find_wasip1_override_for_preload_capi_function(function)}; state != nullptr) [[likely]] { return state->env; }
            return ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env;
        }

        [[nodiscard]] inline constexpr bool try_get_wasip1_target_for_preload_capi_function(capi_function_t const* function,
                                                                                            wasip1_module_target_kind_t& target_kind,
                                                                                            ::uwvm2::utils::container::u8string_view& module_name) noexcept
        {
            // Recover the logical preload target for scoped WASI target reporting while invoking a C API function.
            if(!has_configured_wasip1_module_override_fast_path()) [[likely]] { return false; }
            auto const owner{::uwvm2::uwvm::wasm::storage::find_preload_capi_function_owner(function)};
            if(owner == nullptr) [[unlikely]] { return false; }

            switch(owner->kind)
            {
# if defined(UWVM_SUPPORT_PRELOAD_DL)
                case ::uwvm2::uwvm::wasm::storage::preload_capi_function_owner_kind_t::preloaded_dl:
                {
                    if(owner->module_index >= ::uwvm2::uwvm::wasm::storage::preloaded_dl.size()) [[unlikely]] { return false; }
                    target_kind = wasip1_module_target_kind_t::preloaded_dl;
                    module_name = ::uwvm2::uwvm::wasm::storage::preloaded_dl.index_unchecked(owner->module_index).module_name;
                    return true;
                }
# endif
# if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                case ::uwvm2::uwvm::wasm::storage::preload_capi_function_owner_kind_t::weak_symbol:
                {
                    if(owner->module_index >= ::uwvm2::uwvm::wasm::storage::weak_symbol.size()) [[unlikely]] { return false; }
                    target_kind = wasip1_module_target_kind_t::weak_symbol;
                    module_name = ::uwvm2::uwvm::wasm::storage::weak_symbol.index_unchecked(owner->module_index).module_name;
                    return true;
                }
# endif
                default:
                {
                    return false;
                }
            }
        }

        inline constexpr void call_local_imported_with_wasip1_env(local_imported_t const& module,
                                                                  ::std::size_t function_index,
                                                                  ::std::byte* result_buffer,
                                                                  ::std::byte* param_buffer,
                                                                  ::std::size_t caller_module_id) noexcept
        {
            // Select the caller-specific WASI environment around local-imported calls only when overrides make the default fast path
            // insufficient.
            if(try_prepare_default_global_wasip1_env_fast_path(caller_module_id)) [[likely]]
            {
                module.call_func_index(function_index, result_buffer, param_buffer);
                return;
            }

            auto& wasip1_env{resolve_wasip1_env_for_runtime_module_id(caller_module_id)};
            bind_wasip1_memory_for_selected_env(wasip1_env, caller_module_id);

            if(is_current_wasip1_env_selected(wasip1_env)) [[likely]]
            {
                module.call_func_index(function_index, result_buffer, param_buffer);
                return;
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_env_t wasip1_env_guard{wasip1_env};
            module.call_func_index(function_index, result_buffer, param_buffer);
        }

        inline constexpr void call_capi_with_wasip1_env(capi_function_t const& function,
                                                        preload_module_memory_attribute_t const* preload_module_memory_attribute,
                                                        ::std::byte* result_buffer,
                                                        ::std::byte* param_buffer,
                                                        ::std::size_t caller_module_id) noexcept
        {
            // Preloaded C APIs may need both caller memory metadata and a WASI target identity. The guard scopes both pieces of
            // ambient state to this call.
            preload_call_context_guard preload_guard{preload_module_memory_attribute, ::std::addressof(function), caller_module_id};

            if(try_prepare_default_global_wasip1_env_fast_path(caller_module_id)) [[likely]]
            {
                function.func_ptr(result_buffer, param_buffer);
                return;
            }

            auto& wasip1_env{resolve_wasip1_env_for_preload_capi_function(::std::addressof(function))};
            bind_wasip1_memory_for_selected_env(wasip1_env, caller_module_id);

            auto const invoke_function{
                [&]() constexpr noexcept
                {
                    wasip1_module_target_kind_t target_kind{};
                    ::uwvm2::utils::container::u8string_view target_module_name{};
                    auto const has_target{try_get_wasip1_target_for_preload_capi_function(::std::addressof(function), target_kind, target_module_name)};
                    if(has_target)
                    {
                        ::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_target_t wasip1_target_guard{target_kind, target_module_name};
                        function.func_ptr(result_buffer, param_buffer);
                    }
                    else
                    {
                        function.func_ptr(result_buffer, param_buffer);
                    }
                }};

            if(is_current_wasip1_env_selected(wasip1_env)) [[likely]]
            {
                invoke_function();
                return;
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_env_t wasip1_env_guard{wasip1_env};
            invoke_function();
        }
#endif

        [[nodiscard]] inline constexpr runtime_module_storage_t const* get_active_preload_runtime_module() noexcept
        {
            // Preload host APIs are context-sensitive: they operate on the wasm module that is currently calling the C API.
            auto const& ctx{get_preload_call_context()};
            if(ctx.module_id == preload_call_context_t::invalid_module_id) [[unlikely]] { return nullptr; }
            if(ctx.module_id >= g_runtime.modules.size()) [[unlikely]] { return nullptr; }
            return g_runtime.modules.index_unchecked(ctx.module_id).runtime_module;
        }

        [[nodiscard]] inline constexpr preload_module_memory_attribute_t const* get_active_preload_memory_attribute() noexcept
        {
            // The active C API call decides which memories, if any, the preload module is allowed to inspect.
            return get_preload_call_context().preload_module_memory_attribute;
        }

        [[nodiscard]] inline constexpr bool preload_memory_index_is_selected(preload_module_memory_attribute_t const* attribute,
                                                                             ::std::size_t memory_index) noexcept
        {
            // Apply the preload module's memory allow-list before exposing descriptors or permitting copy access.
            if(attribute == nullptr) [[unlikely]] { return false; }
            if(attribute->apply_to_all_memories) [[likely]] { return true; }

            return attribute->memory_index_set.find(memory_index) != attribute->memory_index_set.cend();
        }

        struct preload_memory_rights_t
        {
            // `allow_access` gates all host APIs; `prefer_mmap` asks descriptor creation to expose a direct view when safe.
            bool allow_access{};
            bool prefer_mmap{};
        };

        [[nodiscard]] inline constexpr preload_memory_rights_t requested_preload_memory_rights(preload_module_memory_attribute_t const* attribute,
                                                                                               ::std::size_t memory_index) noexcept
        {
            // Memory attributes decide whether a preload module can inspect a memory and whether mmap exposure is preferred over copy.
            using access_mode = ::uwvm2::uwvm::wasm::type::preload_module_memory_access_mode_t;

            if(!preload_memory_index_is_selected(attribute, memory_index)) [[unlikely]] { return {}; }

            switch(attribute->memory_access_mode)
            {
                case access_mode::copy:
                {
                    return {true, false};
                }
                case access_mode::mmap:
                {
                    return {true, true};
                }
                case access_mode::none: [[fallthrough]];
                default:
                {
                    return {};
                }
            }
        }

        struct resolved_preload_memory_t
        {
            // Runtime-normalized view of a memory selected for preload exposure.
            enum class target_kind : unsigned
            {
                native_defined,
                local_imported
            };

            target_kind kind{target_kind::native_defined};
            native_memory_t const* native_memory{};
            local_imported_t* local_imported{};
            ::std::size_t local_imported_index{};
            ::std::byte* memory_begin{};
            ::std::uint_least64_t page_count{};
            ::std::uint_least64_t page_size_bytes{};
            ::std::uint_least64_t byte_length{};
            unsigned backend_kind{::uwvm2::uwvm::wasm::type::uwvm_preload_memory_backend_native_defined};
            unsigned mmap_delivery_state{::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none};
            ::std::uint_least64_t partial_protection_limit_bytes{};
            void const* dynamic_length_atomic_object{};
        };

        struct preload_memory_delivery_t
        {
            // Encodes how the selected memory is exposed to host code: no access, copy-only, or mmap-backed view.
            unsigned delivery_state{::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none};
        };

        [[nodiscard]] inline constexpr bool
            compute_preload_byte_length(::std::uint_least64_t page_count, ::std::uint_least64_t page_size_bytes, ::std::uint_least64_t& byte_length) noexcept
        {
            // Compute memory byte length without wrapping when local-imported memories report page counts directly.
            if(page_size_bytes != 0u && page_count > (::std::numeric_limits<::std::uint_least64_t>::max() / page_size_bytes)) [[unlikely]] { return false; }
            byte_length = page_count * page_size_bytes;
            return true;
        }

#if defined(UWVM_SUPPORT_MMAP)
        [[nodiscard]] inline constexpr ::std::uint_least64_t preload_partial_protection_limit_bytes(native_memory_t const& memory) noexcept
        {

            // Partial guard-page mmap exposure needs the same architecture-sized limit used by the native linear-memory backend.
            if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
            {
                if(memory.status == ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64)
                {
                    return ::uwvm2::object::memory::linear::max_partial_protection_wasm64_length;
                }
                return ::uwvm2::object::memory::linear::max_partial_protection_wasm32_length;
            }
            else
            {
                static_cast<void>(memory);
                return ::uwvm2::object::memory::linear::max_partial_protection_wasm32_length;
            }
        }
#endif

        [[nodiscard]] inline constexpr bool resolve_defined_preload_memory(native_memory_t const& memory, resolved_preload_memory_t& resolved) noexcept
        {
            // Take a snapshot so descriptor fields describe one coherent memory view even if the memory can grow later.
            resolved.kind = resolved_preload_memory_t::target_kind::native_defined;
            resolved.native_memory = ::std::addressof(memory);
            resolved.local_imported = nullptr;
            resolved.local_imported_index = 0uz;
            resolved.page_size_bytes = static_cast<::std::uint_least64_t>(1u) << memory.custom_page_size_log2;
            resolved.backend_kind = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_backend_native_defined;
            resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none;
            resolved.partial_protection_limit_bytes = 0u;
            resolved.dynamic_length_atomic_object = nullptr;

            ::std::size_t snapshot_byte_length{};
            if(!::uwvm2::object::memory::linear::with_memory_access_snapshot(memory,
                                                                             [&](::std::byte* memory_begin, ::std::size_t byte_length) constexpr noexcept
                                                                             {
                                                                                 resolved.memory_begin = memory_begin;
                                                                                 snapshot_byte_length = byte_length;
                                                                                 return true;
                                                                             })) [[unlikely]]
            {
                return false;
            }

            resolved.byte_length = static_cast<::std::uint_least64_t>(snapshot_byte_length);
            resolved.page_count = static_cast<::std::uint_least64_t>(snapshot_byte_length >> memory.custom_page_size_log2);
            if(resolved.memory_begin == nullptr && resolved.byte_length != 0u) [[unlikely]] { return false; }

#if defined(UWVM_SUPPORT_MMAP)
            if constexpr(native_memory_t::can_mmap)
            {
                if(memory.require_dynamic_determination_memory_size())
                {
                    resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_dynamic_bounds;
                    resolved.dynamic_length_atomic_object = memory.memory_length_p;
                }
                else if(memory.is_full_page_protection())
                {
                    resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_full_protection;
                }
                else
                {
                    resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_partial_protection;
                    resolved.partial_protection_limit_bytes = preload_partial_protection_limit_bytes(memory);
                }
            }
#endif

            return true;
        }

        [[nodiscard]] inline constexpr bool resolve_local_imported_preload_memory(local_imported_t* local_imported,
                                                                                  ::std::size_t local_imported_index,
                                                                                  resolved_preload_memory_t& resolved) noexcept
        {
            // Local-imported memories are owned by native modules. Query their snapshot API instead of assuming UWVM native memory layout.
            if(local_imported == nullptr) [[unlikely]] { return false; }

            ::uwvm2::uwvm::wasm::type::memory_access_snapshot_result_t snapshot{};
            if(!local_imported->memory_access_snapshot_from_index(local_imported_index, snapshot)) [[unlikely]] { return false; }

            resolved.kind = resolved_preload_memory_t::target_kind::local_imported;
            resolved.native_memory = nullptr;
            resolved.local_imported = local_imported;
            resolved.local_imported_index = local_imported_index;
            resolved.memory_begin = snapshot.memory_begin;
            resolved.page_count = snapshot.page_count;
            resolved.page_size_bytes = local_imported->memory_page_size_from_index(local_imported_index);
            resolved.backend_kind = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_backend_local_imported;
            resolved.mmap_delivery_state = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none;
            resolved.partial_protection_limit_bytes = 0u;
            resolved.dynamic_length_atomic_object = nullptr;

            if(!compute_preload_byte_length(resolved.page_count, resolved.page_size_bytes, resolved.byte_length)) [[unlikely]] { return false; }
            if(resolved.memory_begin == nullptr && resolved.byte_length != 0u) [[unlikely]] { return false; }

            return true;
        }

        [[nodiscard]] inline constexpr bool
            resolve_preload_memory(runtime_module_storage_t const& rt, ::std::size_t memory_index, resolved_preload_memory_t& resolved) noexcept
        {
            // Preload-visible memories can be local, imported-from-wasm, or local-imported from a native module. Normalize all of them
            // into one descriptor shape for host APIs.
            using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
            using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

            auto const imported_count{rt.imported_memory_vec_storage.size()};
            if(memory_index < imported_count)
            {
                constexpr ::std::size_t kMaxChain{4096uz};
                auto curr{::std::addressof(rt.imported_memory_vec_storage.index_unchecked(memory_index))};

                for(::std::size_t steps{}; steps != kMaxChain; ++steps)
                {
                    if(curr == nullptr) [[unlikely]] { return false; }

                    switch(curr->link_kind)
                    {
                        case memory_link_kind::imported:
                        {
                            curr = curr->target.imported_ptr;
                            break;
                        }
                        case memory_link_kind::defined:
                        {
                            auto const defined{curr->target.defined_ptr};
                            if(defined == nullptr) [[unlikely]] { return false; }
                            return resolve_defined_preload_memory(defined->memory, resolved);
                        }
                        case memory_link_kind::local_imported:
                        {
                            return resolve_local_imported_preload_memory(curr->target.local_imported.module_ptr, curr->target.local_imported.index, resolved);
                        }
                        case memory_link_kind::unresolved: [[fallthrough]];
                        default:
                        {
                            return false;
                        }
                    }
                }

                return false;
            }

            auto const local_index{memory_index - imported_count};
            if(local_index >= rt.local_defined_memory_vec_storage.size()) [[unlikely]] { return false; }
            return resolve_defined_preload_memory(rt.local_defined_memory_vec_storage.index_unchecked(local_index).memory, resolved);
        }

        [[nodiscard]] inline constexpr preload_memory_delivery_t determine_preload_memory_delivery(preload_memory_rights_t rights,
                                                                                                   resolved_preload_memory_t const& resolved) noexcept
        {
            // Honor the preload policy preference, but fall back to copy when mmap metadata is unavailable for this memory backend.
            if(!rights.allow_access) [[unlikely]] { return {}; }

            if(rights.prefer_mmap && resolved.mmap_delivery_state != ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none)
            {
                return {resolved.mmap_delivery_state};
            }

            return {::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_copy};
        }

        [[nodiscard]] inline constexpr bool
            validate_preload_copy_range(::std::uint_least64_t byte_length, ::std::uint_least64_t offset, ::std::size_t size, ::std::size_t& offset_out) noexcept
        {
            // Re-check each read/write range against the current snapshot length before copying bytes into or out of wasm memory.
            if(byte_length > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max())) [[unlikely]] { return false; }
            if(offset > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max())) [[unlikely]] { return false; }

            auto const host_byte_length{static_cast<::std::size_t>(byte_length)};
            offset_out = static_cast<::std::size_t>(offset);

            if(offset_out > host_byte_length) [[unlikely]] { return false; }
            if(size > (host_byte_length - offset_out)) [[unlikely]] { return false; }

            return true;
        }

        template <typename Fn>
        [[nodiscard]] inline constexpr bool with_native_preload_copy_access(native_memory_t const& memory, Fn&& fn) noexcept
        {
            // Copy-style host APIs use the memory backend's snapshot helper so growth/protection state is synchronized consistently.
            return ::uwvm2::object::memory::linear::with_memory_access_snapshot(memory, ::std::forward<Fn>(fn));
        }

        [[nodiscard]] inline constexpr ::std::size_t active_preload_total_memory_count(runtime_module_storage_t const& rt) noexcept
        {
            // Runtime memory indices list imported memories first, followed by locally defined memories.
            return rt.imported_memory_vec_storage.size() + rt.local_defined_memory_vec_storage.size();
        }

        [[nodiscard]] inline constexpr bool try_build_preload_memory_descriptor(::std::size_t memory_index,
                                                                                preload_memory_descriptor_t* out,
                                                                                resolved_preload_memory_t* resolved_out,
                                                                                preload_memory_delivery_t* delivery_out) noexcept
        {
            // Build descriptors lazily from the active preload call context so a single host API can serve many caller modules.
            auto const rt{get_active_preload_runtime_module()};
            if(rt == nullptr) [[unlikely]] { return false; }

            resolved_preload_memory_t resolved{};
            if(!resolve_preload_memory(*rt, memory_index, resolved)) [[unlikely]] { return false; }

            auto const rights{requested_preload_memory_rights(get_active_preload_memory_attribute(), memory_index)};
            auto const delivery{determine_preload_memory_delivery(rights, resolved)};
            if(delivery.delivery_state == ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_none) [[unlikely]] { return false; }

            if(out != nullptr)
            {
                *out = preload_memory_descriptor_t{
                    .memory_index = memory_index,
                    .delivery_state = delivery.delivery_state,
                    .backend_kind = resolved.backend_kind,
                    .reserved0 = 0u,
                    .reserved1 = 0u,
                    .page_count = resolved.page_count,
                    .page_size_bytes = resolved.page_size_bytes,
                    .byte_length = resolved.byte_length,
                    .partial_protection_limit_bytes = resolved.partial_protection_limit_bytes,
                    .mmap_view_begin =
                        delivery.delivery_state == ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_copy ? nullptr : resolved.memory_begin,
                    .dynamic_length_atomic_object = delivery.delivery_state == ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_delivery_mmap_dynamic_bounds
                                                        ? resolved.dynamic_length_atomic_object
                                                        : nullptr};
            }

            if(resolved_out != nullptr) [[likely]] { *resolved_out = resolved; }
            if(delivery_out != nullptr) [[likely]] { *delivery_out = delivery; }
            return true;
        }

        [[nodiscard]] inline constexpr ::std::size_t preload_memory_descriptor_count_impl() noexcept
        {
            // Count only memories that both resolve successfully and are allowed by the active preload memory policy.
            auto const rt{get_active_preload_runtime_module()};
            if(rt == nullptr) [[unlikely]] { return 0uz; }

            auto const total{active_preload_total_memory_count(*rt)};
            ::std::size_t count{};
            for(::std::size_t i{}; i != total; ++i) { count += static_cast<::std::size_t>(try_build_preload_memory_descriptor(i, nullptr, nullptr, nullptr)); }
            return count;
        }

        [[nodiscard]] inline constexpr bool preload_memory_descriptor_at_impl(::std::size_t descriptor_index, preload_memory_descriptor_t* out) noexcept
        {
            // Iterate physical runtime memory indices and expose the Nth policy-visible descriptor to the host.
            if(out == nullptr) [[unlikely]] { return false; }

            auto const rt{get_active_preload_runtime_module()};
            if(rt == nullptr) [[unlikely]] { return false; }

            auto const total{active_preload_total_memory_count(*rt)};
            ::std::size_t current_descriptor_index{};
            preload_memory_descriptor_t descriptor{};
            for(::std::size_t memory_index{}; memory_index != total; ++memory_index)
            {
                if(!try_build_preload_memory_descriptor(memory_index, ::std::addressof(descriptor), nullptr, nullptr)) { continue; }
                if(current_descriptor_index == descriptor_index) [[likely]]
                {
                    *out = descriptor;
                    return true;
                }
                ++current_descriptor_index;
            }

            return false;
        }

        [[nodiscard]] inline constexpr bool
            preload_memory_read_impl(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept
        {
            // Read through either the native UWVM memory snapshot path or the local-imported module's own memory access API.
            if(size != 0uz && destination == nullptr) [[unlikely]] { return false; }

            resolved_preload_memory_t resolved{};
            if(!try_build_preload_memory_descriptor(memory_index, nullptr, ::std::addressof(resolved), nullptr)) [[unlikely]] { return false; }
            if(size == 0uz) [[unlikely]] { return true; }

            switch(resolved.kind)
            {
                case resolved_preload_memory_t::target_kind::native_defined:
                {
                    auto const memory{resolved.native_memory};
                    if(memory == nullptr) [[unlikely]] { return false; }
                    return with_native_preload_copy_access(
                        *memory,
                        [&](::std::byte* memory_begin, ::std::size_t byte_length) constexpr noexcept
                        {
                            if(memory_begin == nullptr) [[unlikely]] { return false; }

                            ::std::size_t host_offset{};
                            if(!validate_preload_copy_range(static_cast<::std::uint_least64_t>(byte_length), offset, size, host_offset)) [[unlikely]]
                            {
                                return false;
                            }

                            ::std::memcpy(destination, memory_begin + host_offset, size);
                            return true;
                        });
                }
                case resolved_preload_memory_t::target_kind::local_imported:
                {
                    auto const local_imported{resolved.local_imported};
                    if(local_imported == nullptr) [[unlikely]] { return false; }
                    return local_imported->memory_read_from_index(resolved.local_imported_index, offset, destination, size);
                }
                default:
                {
                    return false;
                }
            }
        }

        [[nodiscard]] inline constexpr bool
            preload_memory_write_impl(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept
        {
            // Write mirrors the read path and keeps range validation close to the copy that mutates wasm memory.
            if(size != 0uz && source == nullptr) [[unlikely]] { return false; }

            resolved_preload_memory_t resolved{};
            if(!try_build_preload_memory_descriptor(memory_index, nullptr, ::std::addressof(resolved), nullptr)) [[unlikely]] { return false; }
            if(size == 0uz) [[unlikely]] { return true; }

            switch(resolved.kind)
            {
                case resolved_preload_memory_t::target_kind::native_defined:
                {
                    auto const memory{resolved.native_memory};
                    if(memory == nullptr) [[unlikely]] { return false; }
                    return with_native_preload_copy_access(
                        *memory,
                        [&](::std::byte* memory_begin, ::std::size_t byte_length) constexpr noexcept
                        {
                            if(memory_begin == nullptr) [[unlikely]] { return false; }

                            ::std::size_t host_offset{};
                            if(!validate_preload_copy_range(static_cast<::std::uint_least64_t>(byte_length), offset, size, host_offset)) [[unlikely]]
                            {
                                return false;
                            }

                            ::std::memcpy(memory_begin + host_offset, source, size);
                            return true;
                        });
                }
                case resolved_preload_memory_t::target_kind::local_imported:
                {
                    auto const local_imported{resolved.local_imported};
                    if(local_imported == nullptr) [[unlikely]] { return false; }
                    return local_imported->memory_write_to_index(resolved.local_imported_index, offset, source, size);
                }
                default:
                {
                    return false;
                }
            }
        }

        // IMPORTANT:
        // Do NOT wrap `alloca` in a helper function that returns the pointer: `alloca` is stack-frame bound,
        // so allocating in a callee and returning the pointer is a dangling pointer unless the compiler inlines it.
        // Use this macro so the `alloca` happens in the caller's stack frame.
#if UWVM_HAS_BUILTIN(__builtin_alloca)
# define UWVM_ALLOCA_BYTES(uwvm_n) __builtin_alloca(uwvm_n)
#elif defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__) && !defined(__CYGWIN__)
# define UWVM_ALLOCA_BYTES(uwvm_n) _alloca(uwvm_n)
#else
# define UWVM_ALLOCA_BYTES(uwvm_n) alloca(uwvm_n)
#endif

#define UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(dst_ptr, bytes_expr, guard_obj)                                                                          \
    {                                                                                                                                                          \
        ::std::size_t uwvm_n{bytes_expr};                                                                                                                      \
        if(uwvm_n == 0uz) [[unlikely]] { uwvm_n = 1uz; }                                                                                                       \
        if(uwvm_n < 1024uz)                                                                                                                                    \
        {                                                                                                                                                      \
            void* const uwvm_p{UWVM_ALLOCA_BYTES(uwvm_n)};                                                                                                     \
            ::std::memset(uwvm_p, 0, uwvm_n);                                                                                                                  \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
        else                                                                                                                                                   \
        {                                                                                                                                                      \
            void* const uwvm_p{byte_allocator::allocate_zero(uwvm_n)};                                                                                         \
            guard_obj.ptr = uwvm_p;                                                                                                                            \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
    }

#define UWVM_STACK_OR_HEAP_ALLOC_BYTES_NONNULL(dst_ptr, bytes_expr, guard_obj)                                                                                 \
    {                                                                                                                                                          \
        ::std::size_t uwvm_n{bytes_expr};                                                                                                                      \
        if(uwvm_n == 0uz) [[unlikely]] { uwvm_n = 1uz; }                                                                                                       \
        if(uwvm_n < 1024uz)                                                                                                                                    \
        {                                                                                                                                                      \
            void* const uwvm_p{UWVM_ALLOCA_BYTES(uwvm_n)};                                                                                                     \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
        else                                                                                                                                                   \
        {                                                                                                                                                      \
            void* const uwvm_p{byte_allocator::allocate(uwvm_n)};                                                                                              \
            guard_obj.ptr = uwvm_p;                                                                                                                            \
            dst_ptr = static_cast<::std::byte*>(uwvm_p);                                                                                                       \
        }                                                                                                                                                      \
    }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // =========================================================================
        // Interpreter frame layout and opfunc dispatch
        // -------------------------------------------------------------------------
        // The interpreter executes translated opfunc streams over a byte-packed local
        // area and operand stack. This section defines how host buffers, local frames,
        // direct calls, lazy materialization, and tiered fallbacks enter that model.
        //
        // Coverage invariants:
        // - alloca-backed frame buffers must be allocated in the caller's stack frame.
        // - Local and operand regions are aligned separately because opfuncs may read typed values.
        // - Tail-call dispatch and by-reference dispatch must observe the same stack-top convention.
        // - Raw host/JIT buffers are staged through byte stacks before interpreter bodies run.
        // - Tiered T0 uses the same frame layout so it can fall back without ABI translation drift.
        // =========================================================================
        using opfunc_byref_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_byref_t<::std::byte const*, ::std::byte*, ::std::byte*>;

        template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        using uwvmint_interp_arg_t = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::interpreter_tuple_arg_t<I, CompileOption>;

        template <::std::size_t I, ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
        UWVM_ALWAYS_INLINE inline constexpr uwvmint_interp_arg_t<I, CompileOption>
            uwvmint_init_interp_arg(::std::byte const* ip, ::std::byte* stack_top, ::std::byte* local_base) noexcept
        {
            if constexpr(I == 0uz) { return ip; }
            else if constexpr(I == 1uz) { return stack_top; }
            else if constexpr(I == 2uz) { return local_base; }
            else
            {
                return uwvmint_interp_arg_t<I, CompileOption>{};
            }
        }

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption, ::std::size_t... Is>
        UWVM_ALWAYS_INLINE inline constexpr void execute_compiled_defined_tailcall_impl(::std::index_sequence<Is...>,
                                                                                        ::std::byte const* ip,
                                                                                        ::std::byte* stack_top,
                                                                                        ::std::byte* local_base) noexcept
        {
            // Tail-call dispatch passes the interpreter's fixed arguments plus ABI-selected stack-top cache slots directly to opfuncs.
            using opfunc_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_opfunc_t<uwvmint_interp_arg_t<Is, CompileOption>...>;
            opfunc_t fn;  // no init
            ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
            fn(uwvmint_init_interp_arg<Is, CompileOption>(ip, stack_top, local_base)...);
        }

        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tranopt() noexcept
        {
            // Select stack-top cache register windows for the current ABI. Register-poor or ambiguous ABIs intentionally leave this
            // disabled because spills can cost more than byte-stack traffic.
            ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t res{};

# if !(defined(__pdp11) || (defined(__wasm__) && !defined(__wasm_tail_call__)))
            res.is_tail_call = true;
# endif

# if defined(__ARM_PCS_AAPCS64) || defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
            // aarch64: AAPCS64 (x0-x7 integer args, v0-v7 fp/simd args)
            // 3 fixed args: (ip, operand_stack_top, local_base) => occupy x0-x2
            // Use remaining integer args (x3-x7) for i32/i64 stack-top caching, and fp/simd args (v0-v7) for f32/f64/v128.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
# elif defined(__arm__) || defined(_M_ARM)
            // ARM32: AAPCS/EABI (r0-r3 integer args; hard-float variants may also use VFP regs).
            // After the 3 fixed interpreter args, there is at most 1 remaining core argument register (r3).
            // A full scalar+fp stack-top cache would largely spill to memory on most ABIs, negating the benefit.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
# elif ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                                    \
     (!defined(_WIN32) || (defined(__GNUC__) || defined(__clang__)))
            // x86_64: sysv abi
            // x86_64: sysv abi in ms abi
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 14uz;
# elif defined(_WIN32) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                 \
     !(defined(__GNUC__) || defined(__clang__))
            // x86_64: Windows x64 (MS ABI) (rcx/rdx/r8/r9, xmm0-xmm3)
            // This ABI provides only 4 register argument slots total. After the 3 fixed interpreter args, only 1 slot remains (r9/xmm3).
            // Empirically, enabling a 1-slot scalar4-merged stack-top cache tends to regress overall performance
            // (register pressure + spills), so keep stack-top caching disabled by default here.
#  if 0
            /// @deprecated MS ABI "1-slot" stack-top cache experiment.
            ///             Often regresses performance due to spills/register shuffling. Kept for reference.
            ///             Keep v128 caching off by default.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 4uz;
#  endif
# elif defined(__i386__) || defined(_M_IX86)
            // i386: (usually) only 2 register argument slots under fastcall (ecx/edx), and we already need 3 fixed args.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
# elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)
            // powerpc64: SysV ELF (r3-r10 integer args, VSX for fp/simd)
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = res.v128_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = res.v128_stack_top_end_pos = 16uz;
# elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
            // powerpc32: AIX/SysV/EABI variants differ in i64/f64 passing (often reg-pairs) and hard-float rules.
            // Keep stack-top caching disabled by default for correctness across ABIs.
# elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
#  if defined(__riscv_float_abi_soft) || defined(__riscv_float_abi_single)
            // riscv64: soft-float / single-float (f64 may not be passed in fp regs). Use a scalar4-merged ring in the integer register file.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  else
            // riscv64: psABI (a0-a7 integer args, fa0-fa7 fp args). Keep v128 caching off by default:
            // `wasm_v128` argument passing is not consistently vector-reg based across toolchains/ABIs.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
#  endif
# elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
            // riscv32: i64/f64 are passed in register pairs and the effective register slots are tight.
            // Leave stack-top caching disabled here (SIZE_MAX/SIZE_MAX).
# elif defined(__loongarch__) && defined(__loongarch64)
#  if defined(__loongarch_soft_float) || defined(__loongarch_single_float)
            // loongarch64: soft-float / single-float (f64 may not be passed in fp regs). Use a scalar4-merged ring.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#  else
            // loongarch64: LP64D (a0-a7 integer args, fa0-fa7 fp args). Keep v128 caching off by default:
            // `wasm_v128` argument passing may be lowered to GPR pairs/stack depending on ABI + vector extension.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 8uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 8uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 16uz;
#  endif
# elif defined(__loongarch__)
            // loongarch32: i64/f64 passing uses pairs / stack depending on ABI; keep caching disabled by default.
# elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
            // MIPS ABIs are slot-based: fp args are only register-passed while they remain within the ABI's argument slots.
            // We conservatively target the 8-slot N32/N64 ABIs; O32 has only 4 slots and cannot satisfy Wasm1's minimum ring sizes without heavy spilling.
#  if defined(__mips_n32) || defined(__mips_n64)
#   if defined(__mips_soft_float)
            // N32/N64 soft-float: use a scalar4-merged ring in the integer slots (fits in the 8 arg slots).
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#   else
            // N32/N64 hard-float: keep total args within 8 slots so fp values still use FPRs.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
#   endif
#  endif
# elif defined(__s390x__)
            // s390x: Linux ABI (r2-r6 integer args, f0/f2/... fp args). Keep v128 caching off by default:
            // 16-byte vectors can be passed indirectly by pointer.
            res.i32_stack_top_begin_pos = res.i64_stack_top_begin_pos = 3uz;
            res.i32_stack_top_end_pos = res.i64_stack_top_end_pos = 6uz;
            res.f32_stack_top_begin_pos = res.f64_stack_top_begin_pos = 6uz;
            res.f32_stack_top_end_pos = res.f64_stack_top_end_pos = 8uz;
# elif defined(__s390__) || defined(__SYSC_ZARCH__)
            // s390 (31-bit) / z/Architecture (non-s390x toolchains): i64/f64 passing is ABI-sensitive (often reg pairs).
            // Leave stack-top caching disabled by default.
# elif defined(__sparc__)
            // SPARC: multiple ABIs (v8/v9, 32/64) with different fp arg rules. Leave caching disabled by default.
# elif defined(__IA64__) || defined(_M_IA64) || defined(__ia64__) || defined(__itanium__)
            // IA-64: Itanium ABI is rare today; keep caching disabled by default to avoid ABI mismatches.
# elif defined(__alpha__)
            // Alpha: uncommon; leave caching disabled by default.
# elif defined(__m68k__) || defined(__mc68000__)
            // m68k: uncommon; leave caching disabled by default.
# elif defined(__HPPA__)
            // HPPA: uncommon; leave caching disabled by default.
# elif defined(__e2k__)
            // E2K: uncommon; leave caching disabled by default.
# elif defined(__XTENSA__)
            // Xtensa: embedded; leave caching disabled by default.
# elif defined(__BFIN__)
            // Blackfin: embedded; leave caching disabled by default.
# elif defined(__convex__)
            // Convex: historical; leave caching disabled by default.
# elif defined(__370__) || defined(__THW_370__)
            // System/370: historical; leave caching disabled by default.
# elif defined(__pdp10) || defined(__pdp7) || defined(__pdp11)
            // PDP family: historical; leave caching disabled by default.
# elif defined(__THW_RS6000) || defined(_IBMR2) || defined(_POWER) || defined(_ARCH_PWR) || defined(_ARCH_PWR2)
            // RS/6000: historical; leave caching disabled by default.
# elif defined(__CUDA_ARCH__)
            // PTX (CUDA device code): stack-top caching is not applicable here.
# elif defined(__sh__)
            // SuperH: embedded; leave caching disabled by default.
# elif defined(__AVR__)
            // AVR: embedded; leave caching disabled by default.
# elif defined(__wasm__)
            // UWVM itself may be built as wasm32-wasi; stack-top caching via native ABI registers is not applicable here.
# endif

            return res;
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tiered_tranopt() noexcept
        {
            auto res{get_curr_target_tranopt()};
            res.enable_tiered_loop_osr_poll = true;
            return res;
        }

        static_assert(get_curr_target_tiered_tranopt().enable_tiered_loop_osr_poll);
# endif
#endif

        [[maybe_unused]] UWVM_ALWAYS_INLINE inline constexpr void copy_bytes_small(::std::byte* dst, ::std::byte const* src, ::std::size_t n) noexcept
        {
            if(n != 0uz) [[likely]] { ::std::memcpy(dst, src, n); }
        }

        [[maybe_unused]] UWVM_ALWAYS_INLINE inline constexpr void zero_bytes_small(::std::byte* dst, ::std::size_t n) noexcept
        {
            if(n != 0uz) [[likely]] { ::std::memset(dst, 0, n); }
        }

        [[maybe_unused]] [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::byte* align_ptr_up(::std::byte* p, ::std::size_t align) noexcept
        {
            auto const v{reinterpret_cast<::std::uintptr_t>(p)};
            auto const a{align};
            return reinterpret_cast<::std::byte*>((v + (a - 1uz)) & ~(a - 1uz));
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::std::size_t> build_type_canon_index(runtime_module_storage_t const& module) noexcept
        {
            // Deduplicate equivalent type-section signatures so call_indirect checks can compare small canonical ids.
            using ft_t = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t;

            auto const type_begin{module.type_section_storage.type_section_begin};
            auto const type_end{module.type_section_storage.type_section_end};
            if(type_begin == nullptr || type_end == nullptr) [[unlikely]] { return {}; }

            auto const total{static_cast<::std::size_t>(type_end - type_begin)};
            ::uwvm2::utils::container::vector<::std::size_t> canon{};
            canon.resize(total);

            auto const sig_equal{[](ft_t const& a, ft_t const& b) constexpr noexcept -> bool
                                 {
                                     auto const a_params{static_cast<::std::size_t>(a.parameter.end - a.parameter.begin)};
                                     auto const b_params{static_cast<::std::size_t>(b.parameter.end - b.parameter.begin)};
                                     if(a_params != b_params) { return false; }

                                     auto const a_res{static_cast<::std::size_t>(a.result.end - a.result.begin)};
                                     auto const b_res{static_cast<::std::size_t>(b.result.end - b.result.begin)};
                                     if(a_res != b_res) { return false; }

                                     for(::std::size_t i{}; i != a_params; ++i)
                                     {
                                         if(a.parameter.begin[i] != b.parameter.begin[i]) { return false; }
                                     }

                                     for(::std::size_t i{}; i != a_res; ++i)
                                     {
                                         if(a.result.begin[i] != b.result.begin[i]) { return false; }
                                     }

                                     return true;
                                 }};

            for(::std::size_t i{}; i != total; ++i)
            {
                canon.index_unchecked(i) = i;
                for(::std::size_t j{}; j != i; ++j)
                {
                    if(sig_equal(type_begin[i], type_begin[j]))
                    {
                        canon.index_unchecked(i) = canon.index_unchecked(j);
                        break;
                    }
                }
            }

            return canon;
        }

        [[nodiscard]] inline constexpr ::std::uint_least32_t invalid_llvm_jit_encoded_type_id() noexcept
        { return (::std::numeric_limits<::std::uint_least32_t>::max)(); }

        [[maybe_unused]] [[nodiscard]] inline constexpr ::std::uint_least32_t find_canonical_type_id_for_sig(compiled_module_record const& rec,
                                                                                                             func_sig_view sig) noexcept
        {
            // Encode a normalized signature for LLVM indirect-call target tables. Invalid or unsupported signatures use a sentinel.
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return invalid_llvm_jit_encoded_type_id(); }

            auto const type_begin{runtime_module->type_section_storage.type_section_begin};
            auto const type_end{runtime_module->type_section_storage.type_section_end};
            if(type_begin == nullptr || type_end == nullptr || type_begin > type_end) [[unlikely]] { return invalid_llvm_jit_encoded_type_id(); }

            auto const total{static_cast<::std::size_t>(type_end - type_begin)};
            auto const canon_ok{rec.type_canon_index.size() == total};

            for(::std::size_t i{}; i != total; ++i)
            {
                auto const& ft{type_begin[i]};
                auto const ft_sig{
                    func_sig_view{{valtype_kind::wasm_enum, ft.parameter.begin, static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)},
                                  {valtype_kind::wasm_enum, ft.result.begin, static_cast<::std::size_t>(ft.result.end - ft.result.begin)}}
                };
                if(!func_sig_equal(sig, ft_sig)) { continue; }

                auto const canonical_index{canon_ok ? rec.type_canon_index.index_unchecked(i) : i};
                if(canonical_index > static_cast<::std::size_t>((::std::numeric_limits<::std::uint_least32_t>::max)())) [[unlikely]]
                {
                    return invalid_llvm_jit_encoded_type_id();
                }
                return static_cast<::std::uint_least32_t>(canonical_index);
            }

            return invalid_llvm_jit_encoded_type_id();
        }

        [[maybe_unused]] [[nodiscard]] inline constexpr ::std::uintptr_t pointer_to_uintptr(void const* ptr) noexcept
        { return reinterpret_cast<::std::uintptr_t>(ptr); }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool
            try_execute_trivial_defined_call(::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const& info,
                                             ::std::byte** stack_top_ptr) noexcept
        {
            // Execute compiler-recognized tiny functions directly in the bridge, avoiding a full local/operand-stack frame setup.
            using trivial_kind_t = ::uwvm2::runtime::compiler::uwvm_int::optable::trivial_defined_call_kind;

            if(info.trivial_kind == trivial_kind_t::none) { return false; }

            auto const stack_top{*stack_top_ptr};
            auto const args_begin{stack_top - info.param_bytes};

            auto const load_u32{[](::std::byte const* p) constexpr noexcept -> ::std::uint_least32_t
                                {
                                    ::std::uint_least32_t v{};  // no init
                                    ::std::memcpy(::std::addressof(v), p, sizeof(v));
                                    return v;
                                }};
            auto const store_u32{[](::std::byte* p, ::std::uint_least32_t v) constexpr noexcept { ::std::memcpy(p, ::std::addressof(v), sizeof(v)); }};

            ::std::uint_least32_t const imm_u32{::std::bit_cast<::std::uint_least32_t>(info.trivial_imm)};
            ::std::uint_least32_t const imm2_u32{::std::bit_cast<::std::uint_least32_t>(info.trivial_imm2)};

            switch(info.trivial_kind)
            {
                case trivial_kind_t::nop_void:
                {
                    if(info.result_bytes != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    *stack_top_ptr = args_begin;
                    return true;
                }
                case trivial_kind_t::const_i32:
                {
                    if(info.result_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, imm_u32);
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::param0_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes < 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::add_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(load_u32(args_begin) + imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xor_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(load_u32(args_begin) ^ imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::mul_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(load_u32(args_begin) * imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::rotr_add_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t const shift{static_cast<::std::uint_least32_t>(imm2_u32) & 31u};
                    store_u32(args_begin,
                              static_cast<::std::uint_least32_t>(::std::rotr(static_cast<::std::uint_least32_t>(load_u32(args_begin)), shift) + imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xorshift32_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t x{load_u32(args_begin)};
                    x ^= static_cast<::std::uint_least32_t>(x << 13u);
                    x ^= static_cast<::std::uint_least32_t>(x >> 17u);
                    x ^= static_cast<::std::uint_least32_t>(x << 5u);
                    store_u32(args_begin, x);
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::mul_add_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 4uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>((load_u32(args_begin) * imm_u32) + imm2_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xor_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 8uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(load_u32(args_begin) ^ load_u32(args_begin + 4uz)));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::xor_add_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 8uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(load_u32(args_begin) + (load_u32(args_begin + 4uz) ^ imm_u32)));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::sub_or_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 8uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(load_u32(args_begin) - (load_u32(args_begin + 4uz) | imm_u32)));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                case trivial_kind_t::sum8_xor_const_i32:
                {
                    if(info.result_bytes != 4uz || info.param_bytes != 32uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uint_least32_t acc{};
                    for(::std::size_t i{}; i != 8uz; ++i) { acc += load_u32(args_begin + i * 4uz); }
                    store_u32(args_begin, static_cast<::std::uint_least32_t>(acc ^ imm_u32));
                    *stack_top_ptr = args_begin + 4uz;
                    return true;
                }
                [[unlikely]] default:
                {
                    break;
                }
            }

            return false;
        }

        [[maybe_unused]] [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool try_execute_trivial_defined_call(compiled_defined_func_info const& info,
                                                                                                                 ::std::byte** stack_top_ptr) noexcept
        {
            auto const compiled_call_info{info.compiled_call_info};
            if(compiled_call_info == nullptr) [[unlikely]] { return false; }
            return try_execute_trivial_defined_call(*compiled_call_info, stack_top_ptr);
        }

        inline constexpr void execute_compiled_defined(call_stack_tls_state& call_stack,
                                                       [[maybe_unused]] runtime_local_func_storage_t const* runtime_func,
                                                       compiled_local_func_t const* compiled_func,
                                                       ::std::size_t param_bytes,
                                                       ::std::size_t result_bytes,
                                                       ::std::byte** caller_stack_top_ptr) noexcept
        {
            // Build the interpreter frame from compiler metadata: params become locals, wasm-visible locals are zeroed, and operands
            // live on a separate byte-packed stack sized for the function's maximum depth.
            auto const local_bytes_raw{compiled_func->local_bytes_max};
            auto const stack_cap_raw{compiled_func->operand_stack_byte_max};
            constexpr ::std::size_t kInternalTempLocalBytes{8uz};
            ::std::size_t wasm_locals_bytes{local_bytes_raw};
            if(local_bytes_raw >= kInternalTempLocalBytes) [[likely]] { wasm_locals_bytes = local_bytes_raw - kInternalTempLocalBytes; }
            if(param_bytes > wasm_locals_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const zeroinit_end_raw{compiled_func->local_bytes_zeroinit_end};
            if(zeroinit_end_raw < param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(zeroinit_end_raw > wasm_locals_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const zero_n{zeroinit_end_raw - param_bytes};
            constexpr ::std::size_t kFrameAlign{16uz};
            constexpr ::std::size_t kFrameAlignPad{kFrameAlign - 1uz};
            bool const align_wasm_locals_start{zero_n >= 64uz};

            // Frame layout:
            // - locals region (with optional padding for aligning the Wasm-locals start after params)
            // - operand stack region (16-byte aligned)
            ::std::size_t local_alloc_n{local_bytes_raw};
            if(local_alloc_n == 0uz) [[unlikely]] { local_alloc_n = 1uz; }
            if(align_wasm_locals_start)
            {
                if(local_alloc_n > (::std::numeric_limits<::std::size_t>::max() - kFrameAlignPad)) [[unlikely]] { ::fast_io::fast_terminate(); }
                local_alloc_n += kFrameAlignPad;
            }

            ::std::size_t frame_alloc_n{local_alloc_n};
            if(stack_cap_raw != 0uz) [[likely]]
            {
                if(frame_alloc_n > (::std::numeric_limits<::std::size_t>::max() - (kFrameAlignPad + stack_cap_raw))) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
                frame_alloc_n += kFrameAlignPad + stack_cap_raw;
            }

            constexpr ::std::size_t kAllocaMaxBytesPerFrame{4096uz};
            constexpr ::std::size_t kAllocaMaxCallDepth{128uz};
# if defined(UWVM_USE_THREAD_LOCAL)
            bool const use_scratch{frame_alloc_n > kAllocaMaxBytesPerFrame || call_stack.frames.size() > kAllocaMaxCallDepth};
            thread_local_bump_allocator::mark_t scratch_mark{};
            if(use_scratch) { scratch_mark = g_call_scratch.mark(); }
# else
            bool const use_scratch{frame_alloc_n > kAllocaMaxBytesPerFrame || call_stack.frames.size() > kAllocaMaxCallDepth};
            thread_local_bump_allocator::mark_t scratch_mark{};
            if(use_scratch) { scratch_mark = get_call_scratch().mark(); }
# endif

            auto caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - param_bytes};
            // Pop params from the caller stack first (so nested calls can't see them).
            *caller_stack_top_ptr = caller_args_begin;

            ::std::byte* frame_alloc{};
# if defined(UWVM_USE_THREAD_LOCAL)
            if(use_scratch) { frame_alloc = g_call_scratch.allocate_bytes(frame_alloc_n, kFrameAlign); }
            else
# else
            if(use_scratch) { frame_alloc = get_call_scratch().allocate_bytes(frame_alloc_n, kFrameAlign); }
            else
# endif
            {
                frame_alloc = static_cast<::std::byte*>(UWVM_ALLOCA_BYTES(frame_alloc_n));
            }

            // Allocate locals as a packed byte buffer (i32/f32=4, i64/f64=8, plus the internal temp local).
            ::std::byte* const local_alloc{frame_alloc};
            ::std::byte* local_base{};
            if(align_wasm_locals_start)
            {
                // Align the start of the Wasm locals region (after params). This can improve bulk-zero performance on some libc `memset`s.
                local_base = align_ptr_up(local_alloc + param_bytes, kFrameAlign) - param_bytes;
            }
            else
            {
                local_base = local_alloc;
            }

            if(param_bytes > local_bytes_raw) [[unlikely]]
            {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# else
                ::fast_io::fast_terminate();
# endif
            }

            if(param_bytes != 0uz) { copy_bytes_small(local_base, caller_args_begin, param_bytes); }

            // Wasm-visible locals (excluding params) are zero-initialized. The internal temp local (last 8 bytes) is not Wasm-visible and does not
            // require initialization; avoiding a tiny `memset` per call saves a lot of overhead on call-heavy benchmarks.
            if(zero_n != 0uz) { zero_bytes_small(local_base + param_bytes, zero_n); }

            // Allocate operand stack with the exact max byte size computed by the compiler (byte-packed: i32/f32=4, i64/f64=8).
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(stack_cap_raw == 0uz && compiled_func->operand_stack_max != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
# endif
            if(stack_cap_raw < result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::std::byte* operand_base{};
            ::std::byte operand_dummy{};
            if(stack_cap_raw == 0uz) [[unlikely]] { operand_base = ::std::addressof(operand_dummy); }
            else
            {
                operand_base = align_ptr_up(frame_alloc + local_alloc_n, kFrameAlign);
            }

# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            // Operand stack is fully defined by writes along interpreter execution; it does not require zero-initialization.
            // Keep it zeroed only in detailed debug-check builds to catch any accidental reads of uninitialized operands.
            ::std::memset(operand_base, 0, (stack_cap_raw == 0uz ? 1uz : stack_cap_raw));
# endif

            ::std::byte const* ip{compiled_func->op.operands.data()};
            ::std::byte* stack_top{operand_base};

            constexpr auto curr_target_tranopt{get_curr_target_tranopt()};

            if constexpr(curr_target_tranopt.is_tail_call)
            {
                constexpr ::std::size_t tuple_size{
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details::interpreter_tuple_size<curr_target_tranopt>()};
                execute_compiled_defined_tailcall_impl<curr_target_tranopt>(::std::make_index_sequence<tuple_size>{}, ip, stack_top, local_base);
            }
            else
            {
                while(ip != nullptr) [[likely]]
                {
                    opfunc_byref_t fn;  // no init
                    ::std::memcpy(::std::addressof(fn), ip, sizeof(fn));
                    fn(ip, stack_top, local_base);
                }

                auto const actual_result_bytes{static_cast<::std::size_t>(stack_top - operand_base)};
                if(actual_result_bytes != result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            }

            // Append results back to caller stack.
            copy_bytes_small(*caller_stack_top_ptr, operand_base, result_bytes);
            *caller_stack_top_ptr += result_bytes;

# if defined(UWVM_USE_THREAD_LOCAL)
            if(use_scratch) { g_call_scratch.release(scratch_mark); }
# else
            if(use_scratch) { get_call_scratch().release(scratch_mark); }
# endif
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        [[nodiscard]] inline constexpr bool ensure_lazy_compile_request_direct_for_tiered_t0(::uwvm2::utils::thread::lazy_compile_request request) noexcept
        {
            // Tiered T0 can require immediate interpreter materialization on the execution thread. Claim the lazy unit directly rather
            // than waiting for a worker that may be intentionally deferred.
            if(request.unit == nullptr || request.compile == nullptr) [[unlikely]] { return false; }

            for(;;)
            {
                auto const st{request.unit->state.load(::std::memory_order_acquire)};
                if(::uwvm2::utils::thread::lazy_compile_state_is_terminal(st)) { return st == ::uwvm2::utils::thread::lazy_compile_state::compiled; }

                if(st == ::uwvm2::utils::thread::lazy_compile_state::uncompiled || st == ::uwvm2::utils::thread::lazy_compile_state::queued)
                {
                    auto expected{st};
                    if(request.unit->state.compare_exchange_strong(expected,
                                                                   ::uwvm2::utils::thread::lazy_compile_state::compiling,
                                                                   ::std::memory_order_acq_rel,
                                                                   ::std::memory_order_acquire))
                    {
                        request.compile(request.user_data);
                        g_runtime.lazy_scheduler.complete_request(*request.unit);
                        return request.unit->state.load(::std::memory_order_acquire) == ::uwvm2::utils::thread::lazy_compile_state::compiled;
                    }
                    continue;
                }

                g_runtime.lazy_scheduler.wait_for_unit_event(*request.unit, st);
            }
        }
# endif

        template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t TranslateOpt, bool DirectCompileForTieredT0 = false>
        inline constexpr void ensure_lazy_defined_function_compiled_impl(::std::size_t module_id, ::std::size_t function_index) noexcept
        {
            // Demand gate for interpreter lazy mode. It maps a wasm function index to the owning lazy compile unit and blocks until
            // the unit is compiled or a validation/compile failure is reported.
            if(!g_runtime.lazy_compile_active) { return; }

            if(module_id >= g_runtime.modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const local_index{function_index - import_n};
            if(local_index >= rec.lazy_compiled.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& fn{rec.lazy_compiled.functions.index_unchecked(local_index)};
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled)
            {
                g_runtime.lazy_runtime_compiled_hit_count.fetch_add(1uz, ::std::memory_order_relaxed);
                return;
            }
            g_runtime.lazy_runtime_miss_count.fetch_add(1uz, ::std::memory_order_relaxed);
            if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]]
            {
                ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"demand-failed module=\"",
                                                                             rec.module_name,
                                                                             u8"\" module_id=",
                                                                             module_id,
                                                                             u8" fn=",
                                                                             function_index,
                                                                             u8" local_fn=",
                                                                             local_index,
                                                                             u8" cu=",
                                                                             fn.primary_cu_index,
                                                                             u8" state=failed");
                ::fast_io::fast_terminate();
            }
            if(fn.primary_cu_index >= rec.lazy_compiled.compile_units.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::uwvm2::validation::error::code_validation_error_impl err{};
            ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_compile_request_context ctx{.curr_module = runtime_module,
                                                                                                                   .lazy_storage =
                                                                                                                       ::std::addressof(rec.lazy_compiled),
                                                                                                                   .options = rec.lazy_compile_options,
                                                                                                                   .compile_unit_index = fn.primary_cu_index,
                                                                                                                   .err = ::std::addressof(err),
                                                                                                                   .module_name = rec.module_name};

            auto const request_priority{1u};
            auto request{::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::make_lazy_compile_request<TranslateOpt>(ctx, request_priority)};
            ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::line(u8"demand-request module=\"",
                                                                         rec.module_name,
                                                                         u8"\" module_id=",
                                                                         module_id,
                                                                         u8" fn=",
                                                                         function_index,
                                                                         u8" local_fn=",
                                                                         local_index,
                                                                         u8" cu=",
                                                                         fn.primary_cu_index,
                                                                         u8" state=",
                                                                         ::uwvm2::runtime::compiler::uwvm_int::lazy_runtime_log::compile_state_name(st),
                                                                         u8" priority=",
                                                                         request_priority);
            if constexpr(DirectCompileForTieredT0)
            {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                if(ensure_lazy_compile_request_direct_for_tiered_t0(request)) [[likely]] { return; }
# endif
            }
            else
            {
                if(g_runtime.lazy_scheduler.ensure_ready(request)) [[likely]] { return; }
            }
# ifdef UWVM_CPP_EXCEPTIONS
            print_and_terminate_compile_validation_error(rec.module_name, err);
# else
            ::fast_io::fast_terminate();
# endif
        }

        inline constexpr void ensure_lazy_defined_function_compiled(::std::size_t module_id, ::std::size_t function_index) noexcept
        {
            // Normal lazy interpreter demand path uses the default translation option selected for the host ABI.
            ensure_lazy_defined_function_compiled_impl<get_curr_target_tranopt()>(module_id, function_index);
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        inline constexpr void ensure_tiered_lazy_defined_function_compiled(::std::size_t module_id, ::std::size_t function_index) noexcept
        {
            // Tiered T0 may compile directly on the execution thread when deferred workers have not started yet.
            ensure_lazy_defined_function_compiled_impl<get_curr_target_tiered_tranopt(), true>(module_id, function_index);
        }
# endif

#endif

        // =========================================================================
        // Import and native-call staging
        // -------------------------------------------------------------------------
        // Imported functions can resolve to wasm-defined functions, local imported
        // modules, dynamically loaded C API functions, or weak symbols. The helpers
        // below normalize all non-wasm targets into byte-buffer calls while keeping
        // WASI and preload-memory context scoped to the actual caller module.
        //
        // Coverage invariants:
        // - Interpreter operands are copied out before native calls can mutate result buffers.
        // - WASI Preview 1 environment selection follows the calling runtime module, not the provider cache owner.
        // - C API preload memory attributes are carried with the cached import target.
        // - Results are copied back only after the native call returns successfully.
        // =========================================================================
        [[maybe_unused]] inline constexpr void invoke_local_imported(runtime_imported_func_storage_t::local_imported_target_t const& tgt,
                                                                     ::std::size_t para_bytes,
                                                                     ::std::size_t res_bytes,
                                                                     ::std::byte** caller_stack_top_ptr) noexcept
        {
            // Move parameters from the interpreter byte stack into the local-imported module ABI buffers, invoke the native module,
            // then copy results back onto the caller's stack.
            local_imported_t const* m{tgt.module_ptr};
            if(m == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - para_bytes};
            *caller_stack_top_ptr = caller_args_begin;

            heap_buf_guard res_guard{};
            heap_buf_guard par_guard{};
            ::std::byte* resbuf{};
            ::std::byte* parbuf{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(resbuf, res_bytes, res_guard);
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(parbuf, para_bytes, par_guard);
            if(para_bytes != 0uz) { ::std::memcpy(parbuf, caller_args_begin, para_bytes); }

            auto& call_stack{get_call_stack()};
            auto const caller_module_id{call_stack.frames.empty() ? preload_call_context_t::invalid_module_id : call_stack.frames.back().module_id};
            call_local_imported_with_wasip1_env(*m, tgt.index, resbuf, parbuf, caller_module_id);

            if(res_bytes != 0uz) { ::std::memcpy(*caller_stack_top_ptr, resbuf, res_bytes); }
            *caller_stack_top_ptr += res_bytes;
        }

        [[maybe_unused]] inline constexpr void invoke_capi(capi_function_t const* f,
                                                           preload_module_memory_attribute_t const* preload_module_memory_attribute,
                                                           ::std::size_t para_bytes,
                                                           ::std::size_t res_bytes,
                                                           ::std::byte** caller_stack_top_ptr) noexcept
        {
            // C API imports share the same stack-to-buffer staging as local-imported modules, with preload memory/WASI context scoped
            // inside call_capi_with_wasip1_env.
            if(f == nullptr || f->func_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const caller_stack_top{*caller_stack_top_ptr};
            auto const caller_args_begin{caller_stack_top - para_bytes};
            *caller_stack_top_ptr = caller_args_begin;

            heap_buf_guard res_guard{};
            heap_buf_guard par_guard{};
            ::std::byte* resbuf{};
            ::std::byte* parbuf{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(resbuf, res_bytes, res_guard);
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(parbuf, para_bytes, par_guard);
            if(para_bytes != 0uz) { ::std::memcpy(parbuf, caller_args_begin, para_bytes); }

            auto& call_stack{get_call_stack()};
            auto const caller_module_id{call_stack.frames.empty() ? preload_call_context_t::invalid_module_id : call_stack.frames.back().module_id};
            call_capi_with_wasip1_env(*f, preload_module_memory_attribute, resbuf, parbuf, caller_module_id);

            if(res_bytes != 0uz) { ::std::memcpy(*caller_stack_top_ptr, resbuf, res_bytes); }
            *caller_stack_top_ptr += res_bytes;
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr bool
            ensure_lazy_llvm_jit_defined_function_compiled(::std::size_t module_id, ::std::size_t function_index, bool allow_tiered) noexcept
        {
            // Demand gate for LLVM lazy mode. It ensures the requested local function has been emitted, materialized, and published
            // before a raw entry address is used.
# if !defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            static_cast<void>(allow_tiered);
# endif
            auto const runtime_compiler{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler};
            auto const llvm_jit_lazy_backend{runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only};
            auto const tiered_lazy_backend{
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                allow_tiered && runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
# else
                false
# endif
            };
            if(!g_runtime.lazy_compile_active || (!llvm_jit_lazy_backend && !tiered_lazy_backend)) { return true; }

            if(module_id >= g_runtime.modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_lazy_compiled.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& fn{rec.llvm_jit_lazy_compiled.functions.index_unchecked(local_index)};
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled)
            {
                g_runtime.lazy_runtime_compiled_hit_count.fetch_add(1uz, ::std::memory_order_relaxed);
                return true;
            }

            g_runtime.lazy_runtime_miss_count.fetch_add(1uz, ::std::memory_order_relaxed);
            if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]]
            {
                ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(u8"demand-failed module=\"",
                                                                                                             rec.module_name,
                                                                                                             u8"\" module_id=",
                                                                                                             module_id,
                                                                                                             u8" fn=",
                                                                                                             function_index,
                                                                                                             u8" local_fn=",
                                                                                                             local_index,
                                                                                                             u8" cu=",
                                                                                                             fn.primary_cu_index,
                                                                                                             u8" state=failed");
                return false;
            }
            if(fn.primary_cu_index >= rec.llvm_jit_lazy_compiled.compile_units.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

            ::uwvm2::validation::error::code_validation_error_impl err{};
            ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_compile_request_context ctx{
                .curr_module = runtime_module,
                .lazy_storage = ::std::addressof(rec.llvm_jit_lazy_compiled),
                .options = rec.llvm_jit_lazy_compile_options,
                .compile_unit_index = fn.primary_cu_index,
                .err = ::std::addressof(err),
                .module_name = rec.module_name,
                .publish_materialized_function = publish_llvm_jit_lazy_materialized_function,
                .publish_user_data = ::std::addressof(rec)};

            auto const request_priority{1u};
            // Demand requests are higher priority than background prefetch work but still use the same compile-unit context shape.
            auto request{::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::make_lazy_compile_request(ctx, request_priority)};
            auto const use_urgent_scheduler{llvm_jit_lazy_demand_should_use_urgent_scheduler(rec, local_index, st)};
            ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_runtime_log::line(
                u8"demand-request module=\"",
                rec.module_name,
                u8"\" module_id=",
                module_id,
                u8" fn=",
                function_index,
                u8" local_fn=",
                local_index,
                u8" cu=",
                fn.primary_cu_index,
                u8" size=",
                llvm_jit_lazy_compile_unit_code_size(rec, local_index),
                u8" local_functions=",
                rec.llvm_jit_lazy_compiled.functions.size(),
                u8" state=",
                ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::compile_state_name(st),
                u8" priority=",
                request_priority,
                u8" lane=",
                use_urgent_scheduler ? u8"urgent" : u8"inline");
            if(use_urgent_scheduler)
            {
                // Large demanded functions get a chance to run on the urgent lane, then this caller waits passively for completion.
                auto urgent_request{request};
                if(local_index < rec.llvm_jit_lazy_background_request_contexts.size()) [[likely]]
                {
                    auto& urgent_ctx{rec.llvm_jit_lazy_background_request_contexts.index_unchecked(local_index)};
                    urgent_request =
                        ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::make_lazy_compile_request(urgent_ctx, request_priority);
                }

                if(g_runtime.llvm_jit_urgent_scheduler.try_request_or_shadow_queued(urgent_request))
                {
                    g_runtime.llvm_jit_urgent_request_count.fetch_add(1uz, ::std::memory_order_relaxed);
                    if(g_runtime.llvm_jit_urgent_scheduler.wait_until_ready_passive(*urgent_request.unit)) [[likely]] { return true; }
                }
            }

            if(!g_runtime.lazy_scheduler.ensure_ready(request)) [[unlikely]]
            {
                // Inline ensure_ready is the fallback when no urgent path was used or the urgent lane could not satisfy the request.
# ifdef UWVM_CPP_EXCEPTIONS
                print_and_terminate_compile_validation_error(rec.module_name, err);
# else
                return false;
# endif
            }

            return true;
        }

        [[nodiscard]] inline constexpr bool try_get_runtime_llvm_jit_raw_defined_entry_address(::std::size_t module_id,
                                                                                               ::std::size_t function_index,
                                                                                               ::std::uintptr_t& function_address) noexcept
        {
            // Prefer a full-module raw entry, then fall back to the lazy materialized entry table when lazy JIT is active.
            function_address = 0u;
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto const& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) { return false; }

            auto const local_index{function_index - import_n};
            bool full_ready{};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(tiered_runtime_active()) { full_ready = tiered_full_ready(rec); }
            else
# endif
            {
                full_ready = rec.llvm_jit_ready;
            }
            if(full_ready && local_index < rec.llvm_jit_local_raw_entry_addresses.size())
            {
                function_address = rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local_index);
                if(function_address != 0u) { return true; }
            }

            if(g_runtime.lazy_compile_active &&
               (::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                || ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                       ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
# endif
                ))
            {
                return ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_raw_entry_address(rec.llvm_jit_lazy_compiled,
                                                                                                                            local_index,
                                                                                                                            function_address);
            }

            return false;
        }

        [[nodiscard]] inline constexpr bool
            try_get_runtime_llvm_jit_defined_entry_address(::std::size_t module_id, ::std::size_t function_index, ::std::uintptr_t& function_address) noexcept
        {
            // Return the typed wasm entry address used by JIT-to-JIT direct calls and indirect-call table views.
            function_address = 0u;
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto const& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) { return false; }

            auto const local_index{function_index - import_n};
            bool full_ready{};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(tiered_runtime_active()) { full_ready = tiered_full_ready(rec); }
            else
# endif
            {
                full_ready = rec.llvm_jit_ready;
            }
            if(full_ready && local_index < rec.llvm_jit_local_entry_addresses.size())
            {
                function_address = rec.llvm_jit_local_entry_addresses.index_unchecked(local_index);
                if(function_address != 0u) { return true; }
            }

            if(g_runtime.lazy_compile_active &&
               (::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                || ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                       ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
# endif
                ))
            {
                return ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::try_get_lazy_entry_address(rec.llvm_jit_lazy_compiled,
                                                                                                                        local_index,
                                                                                                                        function_address);
            }

            return false;
        }

        [[nodiscard]] inline constexpr bool try_invoke_runtime_llvm_jit_raw_defined_entry(::std::size_t module_id,
                                                                                          ::std::size_t function_index,
                                                                                          void* result_buffer,
                                                                                          ::std::size_t result_bytes,
                                                                                          void const* param_buffer,
                                                                                          ::std::size_t param_bytes,
                                                                                          bool allow_tiered = false,
                                                                                          bool push_logical_entry_frame = false) noexcept
        {
            // Enter generated code through the raw wrapper ABI so host buffers do not need to match the typed wasm function signature.
            if(!ensure_lazy_llvm_jit_defined_function_compiled(module_id, function_index, allow_tiered)) [[unlikely]] { return false; }
            if(!prepare_lazy_llvm_jit_unwind_native_call_graph(module_id, function_index, allow_tiered)) [[unlikely]] { return false; }

            ::std::uintptr_t function_address{};
            if(!try_get_runtime_llvm_jit_raw_defined_entry_address(module_id, function_index, function_address)) { return false; }

            using entry_fn_t =
                void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t, ::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::size_t);
            auto const entry_fn{reinterpret_cast<entry_fn_t>(function_address)};
            if(push_logical_entry_frame)
            {
                // Tiered/no-T0 enters the raw JIT entry directly from the host. Some noreturn traps let LLVM erase or
                // fold the native entry frame, so keep the wasm entry visible as a logical frame for trap reporting.
                auto& call_stack{get_call_stack()};
                call_stack_guard g{call_stack, module_id, function_index};
                entry_fn(0u, pointer_to_uintptr(result_buffer), result_bytes, pointer_to_uintptr(param_buffer), param_bytes);
                return true;
            }

            entry_fn(0u, pointer_to_uintptr(result_buffer), result_bytes, pointer_to_uintptr(param_buffer), param_bytes);
            return true;
        }

        [[nodiscard]] inline constexpr bool
            prepare_lazy_llvm_jit_unwind_native_call_graph(::std::size_t entry_module_id, ::std::size_t entry_function_index, bool allow_tiered) noexcept
        {
            // Lazy materialization normally lets the first Wasm call to a cold function cross a C++ raw-entry bridge.
            // That bridge is not a Wasm frame, and native unwinders may stop or coalesce frames across it before the
            // caller's generated frame is visible.  In unwind call-stack mode, materialize the entry's direct-call graph
            // before execution so ordinary Wasm calls use typed native-entry fast paths while unrelated cold functions stay lazy.
            // Tiered/no-T0 enters LLVM lazy code directly from the host too, so it needs the same preparation even though normal
            // tiered execution should keep using T0/OSR and must not be forced into eager JIT materialization.
            if(!runtime_llvm_jit_unwind_call_stack_requested() || !g_runtime.lazy_compile_active) { return true; }
            auto const runtime_compiler{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler};
            auto const llvm_jit_lazy_backend{runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only};
            auto const tiered_no_t0_backend{
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                allow_tiered && runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered &&
                !tiered_t0_enabled()
# else
                false
# endif
            };
            if(!llvm_jit_lazy_backend && !tiered_no_t0_backend) { return true; }

            if(entry_module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto const& rec{g_runtime.modules.index_unchecked(entry_module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            auto const local_n{runtime_module->local_defined_function_vec_storage.size()};
            if(entry_function_index < import_n) [[unlikely]] { return false; }

            auto const entry_local_index{entry_function_index - import_n};
            if(entry_local_index >= local_n) [[unlikely]] { return false; }

            ::uwvm2::utils::container::vector<::std::size_t> graph_order{};
            if(!collect_llvm_jit_lazy_entry_direct_graph_order(*runtime_module, entry_local_index, local_n, graph_order) || graph_order.empty()) [[unlikely]]
            {
                return false;
            }
            if(local_n > (::std::numeric_limits<::std::size_t>::max() - import_n)) [[unlikely]] { return false; }

            for(auto const local_index: graph_order)
            {
                if(local_index >= local_n) [[unlikely]] { return false; }
                if(!ensure_lazy_llvm_jit_defined_function_compiled(entry_module_id, import_n + local_index, tiered_no_t0_backend)) [[unlikely]]
                {
                    return false;
                }
            }

            return true;
        }
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR [[maybe_unused]] inline constexpr void tiered_raw_call_defined_entry(::std::uintptr_t context_address,
                                                                                                                        ::std::uintptr_t result_buffer_address,
                                                                                                                        ::std::size_t result_bytes,
                                                                                                                        ::std::uintptr_t param_buffer_address,
                                                                                                                        ::std::size_t param_bytes) noexcept;

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool try_execute_tiered_llvm_jit_defined_from_stack_active(::std::size_t module_id,
                                                                                                                     ::std::size_t function_index,
                                                                                                                     ::std::size_t param_bytes,
                                                                                                                     ::std::size_t result_bytes,
                                                                                                                     ::std::byte** stack_top_ptr) noexcept
        {
            // Stack-active tiered call path is used from interpreter direct calls; parameters already live at the top of the byte stack.
            if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) [[unlikely]] { return false; }

            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_lazy_direct_call_targets.size()) [[unlikely]] { return false; }
            if(!tiered_local_function_direct_supported(rec, local_index)) { return false; }
            auto const pre_target_entry_gate{
                !::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_osr_poll_enabled_for_module_local_function_count(
                    rec.llvm_jit_lazy_compiled.functions.size())};
            if(pre_target_entry_gate && !tiered_entry_miss_should_enter_prepare(rec, local_index)) { return false; }

            auto const& target{rec.llvm_jit_lazy_direct_call_targets.index_unchecked(local_index)};
            auto& entry_address_ref{const_cast<::std::uintptr_t&>(target.entry_address)};
            auto function_address{::std::atomic_ref<::std::uintptr_t>{entry_address_ref}.load(::std::memory_order_acquire)};
            if(function_address == 0u || function_address == reinterpret_cast<::std::uintptr_t>(tiered_raw_call_defined_entry)) [[unlikely]]
            {
                if(!pre_target_entry_gate && !tiered_entry_miss_should_enter_prepare(rec, local_index)) { return false; }
                if(prepare_tiered_llvm_jit_defined_function(module_id, function_index, function_address) != tiered_llvm_jit_demand_state::ready)
                {
                    return false;
                }
            }

            auto const stack_top{*stack_top_ptr};
            auto const args_begin{stack_top - param_bytes};

            using entry_fn_t =
                void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t, ::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::size_t);
            auto const entry_fn{reinterpret_cast<entry_fn_t>(function_address)};
            auto& call_stack{get_call_stack()};
            // A direct tiered call replaces the interpreter callee with a raw JIT entry while the caller remains on the logical
            // interpreter stack. The guard makes that mixed boundary explicit, which is necessary when LLVM inlines the faulting
            // callee or folds the raw wrapper such that platform unwind data no longer exposes the original wasm caller chain.
            tiered_jit_entry_call_stack_snapshot_guard snapshot_guard{call_stack};
            entry_fn(0u, pointer_to_uintptr(args_begin), result_bytes, pointer_to_uintptr(args_begin), param_bytes);
            *stack_top_ptr = args_begin + result_bytes;
            record_tiered_llvm_jit_switch(rec);
            return true;
        }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool try_execute_tiered_llvm_jit_defined_raw_buffers_active(::std::size_t module_id,
                                                                                                                      ::std::size_t function_index,
                                                                                                                      ::std::size_t param_bytes,
                                                                                                                      ::std::size_t result_bytes,
                                                                                                                      ::std::byte* result_buffer,
                                                                                                                      ::std::byte const* param_buffer) noexcept
        {
            // Raw-buffer tiered path is used by host/JIT bridges where arguments and results already live in explicit ABI buffers.
            if((result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) [[unlikely]] { return false; }
            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_lazy_direct_call_targets.size()) [[unlikely]] { return false; }
            if(!tiered_local_function_direct_supported(rec, local_index)) { return false; }
            auto const pre_target_entry_gate{
                !::uwvm2::runtime::compiler::uwvm_int::optable::interpreter_tiered_osr_poll_enabled_for_module_local_function_count(
                    rec.llvm_jit_lazy_compiled.functions.size())};
            if(pre_target_entry_gate && !tiered_entry_miss_should_enter_prepare(rec, local_index)) { return false; }

            auto const& target{rec.llvm_jit_lazy_direct_call_targets.index_unchecked(local_index)};
            auto& entry_address_ref{const_cast<::std::uintptr_t&>(target.entry_address)};
            auto function_address{::std::atomic_ref<::std::uintptr_t>{entry_address_ref}.load(::std::memory_order_acquire)};
            if(function_address == 0u || function_address == reinterpret_cast<::std::uintptr_t>(tiered_raw_call_defined_entry)) [[unlikely]]
            {
                if(!pre_target_entry_gate && !tiered_entry_miss_should_enter_prepare(rec, local_index)) { return false; }
                if(prepare_tiered_llvm_jit_defined_function(module_id, function_index, function_address) != tiered_llvm_jit_demand_state::ready)
                {
                    return false;
                }
            }

            using entry_fn_t =
                void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t, ::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::size_t);
            auto const entry_fn{reinterpret_cast<entry_fn_t>(function_address)};
            auto& call_stack{get_call_stack()};
            // Raw-buffer tiered entry has the same unwind hazard as the stack-active path: the generated frame may be visible to
            // native unwind, but its interpreter-side caller frames live only in TLS. Keep a short boundary snapshot across the
            // call so traps raised before control returns can merge both views into one wasm call stack.
            tiered_jit_entry_call_stack_snapshot_guard snapshot_guard{call_stack};
            entry_fn(0u, pointer_to_uintptr(result_buffer), result_bytes, pointer_to_uintptr(param_buffer), param_bytes);
            record_tiered_llvm_jit_switch(rec);
            return true;
        }
# endif

        inline constexpr void execute_defined_with_optional_tiered_jit(call_stack_tls_state& call_stack,
                                                                       ::std::size_t module_id,
                                                                       ::std::size_t function_index,
                                                                       runtime_local_func_storage_t const* runtime_func,
                                                                       compiled_local_func_t const* compiled_func,
                                                                       ::std::size_t param_bytes,
                                                                       ::std::size_t result_bytes,
                                                                       ::std::byte** stack_top_ptr) noexcept
        {
            // Normal interpreter execution path: materialize the function lazily when needed, then run the compiled interpreter body.
            if(runtime_func == nullptr || compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            ensure_lazy_defined_function_compiled(module_id, function_index);
            execute_compiled_defined(call_stack, runtime_func, compiled_func, param_bytes, result_bytes, stack_top_ptr);
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        [[nodiscard]] inline constexpr bool invoke_tiered_llvm_jit_defined_from_stack_sync(::std::size_t module_id,
                                                                                           ::std::size_t function_index,
                                                                                           ::std::size_t param_bytes,
                                                                                           ::std::size_t result_bytes,
                                                                                           ::std::byte** stack_top_ptr) noexcept
        {
            // No-T0 tiered mode must synchronously enter LLVM; failure here means there is no interpreter fallback configured.
            if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const stack_top{*stack_top_ptr};
            auto const args_begin{stack_top - param_bytes};
            if(!try_invoke_runtime_llvm_jit_raw_defined_entry(module_id, function_index, args_begin, result_bytes, args_begin, param_bytes, true))
            {
                return false;
            }

            *stack_top_ptr = args_begin + result_bytes;
            if(tiered_t2_enabled() && module_id < g_runtime.modules.size()) { record_tiered_llvm_jit_switch(g_runtime.modules.index_unchecked(module_id)); }
            return true;
        }

        inline constexpr void execute_defined_with_tiered_jit(call_stack_tls_state& call_stack,
                                                              ::std::size_t module_id,
                                                              ::std::size_t function_index,
                                                              runtime_local_func_storage_t const* runtime_func,
                                                              compiled_local_func_t const* compiled_func,
                                                              ::std::size_t param_bytes,
                                                              ::std::size_t result_bytes,
                                                              ::std::byte** stack_top_ptr) noexcept
        {
            // Tiered execution tries a ready generated entry first; on miss it records interpreter fallback pressure and runs T0.
            if(runtime_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(!tiered_t0_enabled())
            {
                if(!invoke_tiered_llvm_jit_defined_from_stack_sync(module_id, function_index, param_bytes, result_bytes, stack_top_ptr)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
                return;
            }
            if(compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(try_execute_tiered_llvm_jit_defined_from_stack_active(module_id, function_index, param_bytes, result_bytes, stack_top_ptr)) { return; }
            record_tiered_interpreter_entry(module_id, function_index);
            ensure_tiered_lazy_defined_function_compiled(module_id, function_index);
            execute_compiled_defined(call_stack, runtime_func, compiled_func, param_bytes, result_bytes, stack_top_ptr);
        }
# endif

        inline constexpr void execute_defined_with_optional_tiered_jit(call_stack_tls_state& call_stack,
                                                                       compiled_defined_func_info const& info,
                                                                       ::std::byte** stack_top_ptr) noexcept
        {
            // Convenience overload: unwrap the cached defined-function record and call the full backend-selecting path.
            execute_defined_with_optional_tiered_jit(call_stack,
                                                     info.module_id,
                                                     info.function_index,
                                                     info.runtime_func,
                                                     info.compiled_func,
                                                     info.param_bytes,
                                                     info.result_bytes,
                                                     stack_top_ptr);
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        inline constexpr void
            execute_defined_with_tiered_jit(call_stack_tls_state& call_stack, compiled_defined_func_info const& info, ::std::byte** stack_top_ptr) noexcept
        {
            // Convenience overload for tiered interpreter call sites that already resolved the defined-function cache entry.
            execute_defined_with_tiered_jit(call_stack,
                                            info.module_id,
                                            info.function_index,
                                            info.runtime_func,
                                            info.compiled_func,
                                            info.param_bytes,
                                            info.result_bytes,
                                            stack_top_ptr);
        }

        inline constexpr void execute_defined_with_tiered_jit(call_stack_tls_state& call_stack,
                                                              ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const& info,
                                                              ::std::byte** stack_top_ptr) noexcept
        {
            // The compiler embeds compiled_defined_call_info pointers at some direct-call sites; normalize that form here.
            execute_defined_with_tiered_jit(call_stack,
                                            info.module_id,
                                            info.function_index,
                                            static_cast<runtime_local_func_storage_t const*>(info.runtime_func),
                                            info.compiled_func,
                                            info.param_bytes,
                                            info.result_bytes,
                                            stack_top_ptr);
        }
# endif

        inline constexpr void execute_defined_with_optional_tiered_jit(call_stack_tls_state& call_stack,
                                                                       ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const& info,
                                                                       ::std::byte** stack_top_ptr) noexcept
        {
            // Non-tiered bridge overload for embedded compiled_defined_call_info pointers.
            execute_defined_with_optional_tiered_jit(call_stack,
                                                     info.module_id,
                                                     info.function_index,
                                                     static_cast<runtime_local_func_storage_t const*>(info.runtime_func),
                                                     info.compiled_func,
                                                     info.param_bytes,
                                                     info.result_bytes,
                                                     stack_top_ptr);
        }

        template <bool UseTieredEnsure>
        inline constexpr void invoke_compiled_defined_raw_buffers_impl(call_stack_tls_state& call_stack,
                                                                       call_stack_frame frame,
                                                                       runtime_local_func_storage_t const* runtime_func,
                                                                       compiled_local_func_t const* compiled_func,
                                                                       ::std::size_t param_bytes,
                                                                       ::std::size_t result_bytes,
                                                                       ::std::byte* result_buffer,
                                                                       ::std::byte const* param_buffer) noexcept
        {
            // Raw host/JIT calls use explicit input/output buffers. Convert them into interpreter stack layout, execute, then copy
            // results back into the caller-provided result buffer.
            if((result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr) || runtime_func == nullptr ||
               compiled_func == nullptr) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            auto const stack_bytes{param_bytes < result_bytes ? result_bytes : param_bytes};
            heap_buf_guard host_stack_guard{};
            ::std::byte* host_stack_base{};
            UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);

            if(param_bytes != 0uz) { ::std::memcpy(host_stack_base, param_buffer, param_bytes); }

            ::std::byte* stack_top_ptr{host_stack_base + param_bytes};
            call_stack_guard g{call_stack, frame.module_id, frame.function_index};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if constexpr(UseTieredEnsure) { ensure_tiered_lazy_defined_function_compiled(frame.module_id, frame.function_index); }
            else
# endif
            {
                ensure_lazy_defined_function_compiled(frame.module_id, frame.function_index);
            }
            execute_compiled_defined(call_stack, runtime_func, compiled_func, param_bytes, result_bytes, ::std::addressof(stack_top_ptr));

            if(result_bytes != 0uz) { ::std::memcpy(result_buffer, host_stack_base, result_bytes); }
        }

        inline constexpr void invoke_compiled_defined_raw_buffers(call_stack_tls_state& call_stack,
                                                                  call_stack_frame frame,
                                                                  runtime_local_func_storage_t const* runtime_func,
                                                                  compiled_local_func_t const* compiled_func,
                                                                  ::std::size_t param_bytes,
                                                                  ::std::size_t result_bytes,
                                                                  ::std::byte* result_buffer,
                                                                  ::std::byte const* param_buffer) noexcept
        {
            // Non-tiered raw-buffer interpreter invocation uses the normal lazy compile guard.
            invoke_compiled_defined_raw_buffers_impl<false>(call_stack,
                                                            frame,
                                                            runtime_func,
                                                            compiled_func,
                                                            param_bytes,
                                                            result_bytes,
                                                            result_buffer,
                                                            param_buffer);
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        inline constexpr void invoke_tiered_compiled_defined_raw_buffers(call_stack_tls_state& call_stack,
                                                                         call_stack_frame frame,
                                                                         runtime_local_func_storage_t const* runtime_func,
                                                                         compiled_local_func_t const* compiled_func,
                                                                         ::std::size_t param_bytes,
                                                                         ::std::size_t result_bytes,
                                                                         ::std::byte* result_buffer,
                                                                         ::std::byte const* param_buffer) noexcept
        {
            // Tiered raw-buffer interpreter invocation uses the tiered T0 demand guard.
            invoke_compiled_defined_raw_buffers_impl<true>(call_stack,
                                                           frame,
                                                           runtime_func,
                                                           compiled_func,
                                                           param_bytes,
                                                           result_bytes,
                                                           result_buffer,
                                                           param_buffer);
        }
# endif

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        // -------------------------------------------------------------------------
        // Raw-entry placeholders used by generated LLVM code
        // -------------------------------------------------------------------------
        // Generated calls may arrive before a target has been materialized. These
        // wrappers keep the raw buffer ABI stable while resolving the best backend
        // available at that moment.
        //
        // Placeholder invariants:
        // - context_address always points to a stable cache record.
        // - parameter/result byte counts are checked before any buffer copy.
        // - tiered wrappers prefer ready LLVM entries, then no-T0 demand LLVM, then T0.
        // - interpreter fallback preserves the caller's logical wasm frame.
        UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR [[maybe_unused]] inline constexpr void tiered_raw_call_defined_entry(::std::uintptr_t context_address,
                                                                                                                        ::std::uintptr_t result_buffer_address,
                                                                                                                        ::std::size_t result_bytes,
                                                                                                                        ::std::uintptr_t param_buffer_address,
                                                                                                                        ::std::size_t param_bytes) noexcept
        {
            // Tiered raw entry wrapper used as an initial placeholder. It first tries an already-published generated entry; if no
            // JIT tier is available, it falls back to the T0 interpreter path and records that fallback for tiering heuristics.
            auto const info{reinterpret_cast<compiled_defined_func_info const*>(context_address)};
            if(info == nullptr || info->runtime_func == nullptr || param_bytes != info->param_bytes || result_bytes != info->result_bytes) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            auto& call_stack{get_call_stack()};
            auto const result_buffer{reinterpret_cast<::std::byte*>(result_buffer_address)};
            auto const param_buffer{reinterpret_cast<::std::byte const*>(param_buffer_address)};
            if(try_execute_tiered_llvm_jit_defined_raw_buffers_active(info->module_id,
                                                                      info->function_index,
                                                                      info->param_bytes,
                                                                      info->result_bytes,
                                                                      result_buffer,
                                                                      param_buffer))
            {
                return;
            }
            if(!tiered_t0_enabled())
            {
                if(!try_invoke_runtime_llvm_jit_raw_defined_entry(info->module_id,
                                                                  info->function_index,
                                                                  result_buffer,
                                                                  info->result_bytes,
                                                                  param_buffer,
                                                                  info->param_bytes,
                                                                  true)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
                if(tiered_t2_enabled() && info->module_id < g_runtime.modules.size())
                {
                    record_tiered_llvm_jit_switch(g_runtime.modules.index_unchecked(info->module_id));
                }
                return;
            }
            if(info->compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            record_tiered_interpreter_entry(info->module_id, info->function_index);
            invoke_tiered_compiled_defined_raw_buffers(call_stack,
                                                       call_stack_frame{info->module_id, info->function_index},
                                                       info->runtime_func,
                                                       info->compiled_func,
                                                       info->param_bytes,
                                                       info->result_bytes,
                                                       result_buffer,
                                                       param_buffer);
        }
# endif

        UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR
        [[maybe_unused]] inline constexpr void llvm_jit_raw_call_defined_entry(::std::uintptr_t context_address,
                                                                               ::std::uintptr_t result_buffer_address,
                                                                               ::std::size_t result_bytes,
                                                                               ::std::uintptr_t param_buffer_address,
                                                                               ::std::size_t param_bytes) noexcept
        {
            // Raw defined-entry bridge used by LLVM-generated wrappers when the target is still interpreter-backed. The context is
            // a compiled_defined_call_info pointer captured during import/direct-call table publication.
# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                auto const info{reinterpret_cast<::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const*>(context_address)};
                auto const result_buffer{reinterpret_cast<::std::byte*>(result_buffer_address)};
                auto const param_buffer{reinterpret_cast<::std::byte const*>(param_buffer_address)};

                if(info == nullptr || param_bytes != info->param_bytes || result_bytes != info->result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto& call_stack{get_call_stack()};
                invoke_compiled_defined_raw_buffers(call_stack,
                                                    call_stack_frame{info->module_id, info->function_index},
                                                    static_cast<runtime_local_func_storage_t const*>(info->runtime_func),
                                                    info->compiled_func,
                                                    info->param_bytes,
                                                    info->result_bytes,
                                                    result_buffer,
                                                    param_buffer);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                trap_fatal(trap_kind::uncatched_int_tag);
            }
# endif
        }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
        // Non-tiered LLVM raw placeholders share the same ABI contract but skip interpreter T0 policy. The lazy wrapper compiles the
        // requested function before dispatch; the raw interpreter wrapper is used only when generated code must call interpreter-backed
        // wasm definitions.
        UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR
        [[maybe_unused]] inline constexpr void llvm_jit_lazy_raw_call_defined_entry(::std::uintptr_t context_address,
                                                                                    ::std::uintptr_t result_buffer_address,
                                                                                    ::std::size_t result_bytes,
                                                                                    ::std::uintptr_t param_buffer_address,
                                                                                    ::std::size_t param_bytes) noexcept
        {
            // Lazy LLVM placeholder: synchronously materialize the requested function, publish the direct/indirect-call targets, then
            // tail into the real raw generated entry.
            auto const info{reinterpret_cast<compiled_defined_func_info const*>(context_address)};
            if(info == nullptr || param_bytes != info->param_bytes || result_bytes != info->result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

            bool allow_tiered{};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            allow_tiered = tiered_runtime_active() && !tiered_t0_enabled() && !tiered_t2_enabled();
# endif
            if(!ensure_lazy_llvm_jit_defined_function_compiled(info->module_id, info->function_index, allow_tiered)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            ::std::uintptr_t function_address{};
            if(!try_get_runtime_llvm_jit_raw_defined_entry_address(info->module_id, info->function_index, function_address)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            ::std::uintptr_t typed_function_address{};
            if(!try_get_runtime_llvm_jit_defined_entry_address(info->module_id, info->function_index, typed_function_address)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            if(info->module_id < g_runtime.modules.size()) [[likely]]
            {
                auto& rec{g_runtime.modules.index_unchecked(info->module_id)};
                auto const runtime_module{rec.runtime_module};
                if(runtime_module != nullptr)
                {
                    auto const import_n{runtime_module->imported_function_vec_storage.size()};
                    if(info->function_index >= import_n)
                    {
                        auto const local_index{info->function_index - import_n};
                        if(local_index < rec.llvm_jit_lazy_direct_call_targets.size())
                        {
                            auto& target{rec.llvm_jit_lazy_direct_call_targets.index_unchecked(local_index)};
                            target.entry_address = function_address;
                            target.context_address = 0u;
                        }
                        if(local_index < rec.llvm_jit_lazy_direct_typed_entry_targets.size())
                        {
                            rec.llvm_jit_lazy_direct_typed_entry_targets.index_unchecked(local_index) = typed_function_address;
                        }
                    }
                }
            }
            publish_llvm_jit_call_indirect_defined_entry_targets(info, function_address, typed_function_address);

            using entry_fn_t =
                void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t, ::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::size_t);
            auto const entry_fn{reinterpret_cast<entry_fn_t>(function_address)};
            entry_fn(0u, result_buffer_address, result_bytes, param_buffer_address, param_bytes);
        }
#endif

        UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR
        [[maybe_unused]] inline constexpr void llvm_jit_raw_call_cached_import_entry(::std::uintptr_t context_address,
                                                                                     ::std::uintptr_t result_buffer_address,
                                                                                     ::std::size_t result_bytes,
                                                                                     ::std::uintptr_t param_buffer_address,
                                                                                     ::std::size_t param_bytes) noexcept
        {
            // Generated LLVM code calls imports through a raw buffer ABI. The cached target carries the final resolved import kind,
            // signature byte sizes, and preload/WASI metadata, so the JIT does not need to repeat import-chain resolution at runtime.
#ifdef UWVM_CPP_EXCEPTIONS
            try
#endif
            {
                auto const tgt{reinterpret_cast<cached_import_target const*>(context_address)};
                auto const result_buffer{reinterpret_cast<::std::byte*>(result_buffer_address)};
                auto const param_buffer{reinterpret_cast<::std::byte const*>(param_buffer_address)};

                if(tgt == nullptr || param_bytes != tgt->param_bytes || result_bytes != tgt->result_bytes ||
                   (result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                auto& call_stack{get_call_stack()};

                switch(tgt->k)
                {
                    case cached_import_target::kind::defined:
                    {
#if defined(UWVM_RUNTIME_LLVM_JIT)
                        // Prefer a generated wasm entry when it is already available; falling back through the interpreter preserves
                        // lazy and tiered modes without exposing backend choice to the generated caller.
                        bool allow_tiered_llvm_lazy{};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                        allow_tiered_llvm_lazy = tiered_runtime_active() && !tiered_t0_enabled();
# endif
                        if(try_invoke_runtime_llvm_jit_raw_defined_entry(tgt->frame.module_id,
                                                                         tgt->frame.function_index,
                                                                         result_buffer,
                                                                         tgt->result_bytes,
                                                                         param_buffer,
                                                                         tgt->param_bytes,
                                                                         allow_tiered_llvm_lazy))
                        {
                            return;
                        }
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                        invoke_compiled_defined_raw_buffers(call_stack,
                                                            tgt->frame,
                                                            tgt->u.defined.runtime_func,
                                                            tgt->u.defined.compiled_func,
                                                            tgt->param_bytes,
                                                            tgt->result_bytes,
                                                            result_buffer,
                                                            param_buffer);
                        return;
#else
                        ::fast_io::fast_terminate();
#endif
                    }
                    case cached_import_target::kind::local_imported:
                    {
                        auto const local_imported_module{tgt->u.local_imported.module_ptr};
                        if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        call_stack_guard g{call_stack, tgt->frame.module_id, tgt->frame.function_index};
                        call_local_imported_with_wasip1_env(*local_imported_module,
                                                            tgt->u.local_imported.index,
                                                            result_buffer,
                                                            const_cast<::std::byte*>(param_buffer),
                                                            tgt->frame.module_id);
                        return;
                    }
                    case cached_import_target::kind::dl:
                    case cached_import_target::kind::weak_symbol:
                    {
                        auto const capi_ptr{tgt->u.capi_ptr};
                        if(capi_ptr == nullptr || capi_ptr->func_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        call_stack_guard g{call_stack, tgt->frame.module_id, tgt->frame.function_index};
                        call_capi_with_wasip1_env(*capi_ptr,
                                                  tgt->preload_module_memory_attribute,
                                                  result_buffer,
                                                  const_cast<::std::byte*>(param_buffer),
                                                  tgt->frame.module_id);
                        return;
                    }
                    [[unlikely]] default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }
#ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                trap_fatal(trap_kind::uncatched_int_tag);
            }
#endif
        }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        [[maybe_unused]] inline constexpr void invoke_resolved(resolved_func const& rf, ::std::byte** caller_stack_top_ptr) noexcept
        {
            // Interpreter import dispatch is normalized to the same resolved_func shape used during cache construction. This keeps
            // direct imports, alias chains, native modules, and weak symbols behind one stack-buffer calling convention.
            switch(rf.k)
            {
                case resolved_func::kind::defined:
                {
                    auto const info{find_defined_func_info(rf.u.defined_ptr)};
                    if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto& call_stack{get_call_stack()};
                    execute_defined_with_optional_tiered_jit(call_stack, *info, caller_stack_top_ptr);
                    return;
                }
                case resolved_func::kind::local_imported:
                {
                    local_imported_t const* m{rf.u.local_imported.module_ptr};
                    auto const sig{func_sig_from_local_imported(m, rf.u.local_imported.index)};
                    if(sig.params.data == nullptr && sig.params.size != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto const para_bytes{total_abi_bytes(sig.params)};
                    auto const res_bytes{total_abi_bytes(sig.results)};
                    if((para_bytes == 0uz && sig.params.size != 0uz) || (res_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    invoke_local_imported(rf.u.local_imported, para_bytes, res_bytes, caller_stack_top_ptr);
                    return;
                }
                case resolved_func::kind::dl:
                case resolved_func::kind::weak_symbol:
                {
                    capi_function_t const* f{rf.u.capi_ptr};
                    auto const sig{func_sig_from_capi(f)};

                    auto const para_bytes{total_abi_bytes(sig.params)};
                    auto const res_bytes{total_abi_bytes(sig.results)};
                    if((para_bytes == 0uz && sig.params.size != 0uz) || (res_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    invoke_capi(f, find_preload_module_memory_attribute(f), para_bytes, res_bytes, caller_stack_top_ptr);
                    return;
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }
#endif

        // -------------------------------------------------------------------------
        // Table and call_indirect metadata helpers
        // -------------------------------------------------------------------------
        // These helpers connect runtime table storage with interpreter/JIT dispatch.
        // Imported tables are resolved through aliases, signatures are canonicalized,
        // and LLVM table views are rebuilt from stable runtime cache records.
        //
        // Table invariants:
        // - Table indices are always interpreted in the caller module.
        // - Function references may point to provider modules, so cache lookup must follow ownership.
        // - Canonical type ids speed up equality checks but never replace exact signature fallback.
        // - LLVM views store raw-entry targets plus encoded type ids for runtime checks.
        [[nodiscard]] inline constexpr runtime_table_storage_t const* resolve_table(runtime_module_storage_t const& module, ::std::size_t table_index) noexcept
        {
            // Tables can be imported through aliases. Resolve to the defining table once so call_indirect sees the current element
            // storage and table growth semantics of the provider module.
            auto const import_n{module.imported_table_vec_storage.size()};
            if(table_index < import_n)
            {
                auto t{::std::addressof(module.imported_table_vec_storage.index_unchecked(table_index))};
                for(;;)
                {
                    if(t == nullptr) [[unlikely]] { return nullptr; }
                    using lk = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t::imported_table_link_kind;
                    switch(t->link_kind)
                    {
                        case lk::defined:
                        {
                            return t->target.defined_ptr;
                        }
                        case lk::imported:
                        {
                            t = t->target.imported_ptr;
                            continue;
                        }
                        case lk::unresolved: [[fallthrough]];
                        default:
                        {
                            return nullptr;
                        }
                    }
                }
            }

            auto const local_index{table_index - import_n};
            if(local_index >= module.local_defined_table_vec_storage.size()) [[unlikely]] { return nullptr; }
            return ::std::addressof(module.local_defined_table_vec_storage.index_unchecked(local_index));
        }

        [[maybe_unused]] [[nodiscard]] inline constexpr func_sig_view
            expected_sig_from_type_index(runtime_module_storage_t const& module, ::std::size_t type_index, bool& ok) noexcept
        {
            ok = false;
            auto const begin{module.type_section_storage.type_section_begin};
            auto const end{module.type_section_storage.type_section_end};
            if(begin == nullptr || end == nullptr) [[unlikely]] { return {}; }
            auto const total{static_cast<::std::size_t>(end - begin)};
            if(type_index >= total) [[unlikely]] { return {}; }

            auto const ft{begin + type_index};
            ok = true;
            return {
                {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
            };
        }

        [[maybe_unused]] [[nodiscard]] inline constexpr ::std::size_t
            find_runtime_module_id_from_storage_ptr(runtime_module_storage_t const* runtime_module_ptr) noexcept
        {
            if(runtime_module_ptr == nullptr) [[unlikely]] { return ::std::numeric_limits<::std::size_t>::max(); }

            for(::std::size_t module_id{}; module_id != g_runtime.modules.size(); ++module_id)
            {
                if(g_runtime.modules.index_unchecked(module_id).runtime_module == runtime_module_ptr) { return module_id; }
            }

            return ::std::numeric_limits<::std::size_t>::max();
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        inline constexpr void populate_llvm_jit_call_indirect_table_views() noexcept
        {
            // LLVM call_indirect lowers through compact table-view arrays. They are rebuilt after initialization/materialization so
            // every table element points either at a ready raw entry or a lazy bridge with the correct per-target context.
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

            for(::std::size_t caller_module_id{}; caller_module_id != g_runtime.modules.size(); ++caller_module_id)
            {
                auto& caller_rec{g_runtime.modules.index_unchecked(caller_module_id)};
                auto const caller_runtime_module{const_cast<runtime_module_storage_t*>(caller_rec.runtime_module)};
                if(caller_runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const all_table_count{caller_runtime_module->imported_table_vec_storage.size() +
                                           caller_runtime_module->local_defined_table_vec_storage.size()};
                if(caller_runtime_module->llvm_jit_call_indirect_table_views.size() != all_table_count) [[unlikely]] { ::fast_io::fast_terminate(); }

                caller_rec.llvm_jit_call_indirect_targets.clear();
                caller_rec.llvm_jit_call_indirect_targets.resize(all_table_count);

                for(::std::size_t table_index{}; table_index != all_table_count; ++table_index)
                {
                    auto& table_view{caller_runtime_module->llvm_jit_call_indirect_table_views.index_unchecked(table_index)};
                    table_view = {};

                    auto const resolved_table{resolve_table(*caller_runtime_module, table_index)};
                    if(resolved_table == nullptr) { continue; }

                    auto const provider_runtime_module{resolved_table->owner_module_rt_ptr};
                    if(provider_runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto const provider_module_id{find_runtime_module_id_from_storage_ptr(provider_runtime_module)};
                    if(provider_module_id == ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::fast_io::fast_terminate(); }

                    auto& target_vec{caller_rec.llvm_jit_call_indirect_targets.index_unchecked(table_index)};
                    target_vec.clear();
                    target_vec.resize(resolved_table->elems.size());

                    auto const provider_import_begin{provider_runtime_module->imported_function_vec_storage.data()};
                    auto const provider_import_count{provider_runtime_module->imported_function_vec_storage.size()};
                    // Imported function references in a provider table are indexed against the provider's import cache, not the caller's.
                    auto const provider_import_cache{
                        provider_module_id < g_import_call_cache.size() ? ::std::addressof(g_import_call_cache.index_unchecked(provider_module_id)) : nullptr};
                    if(provider_import_cache == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    for(::std::size_t elem_index{}; elem_index != resolved_table->elems.size(); ++elem_index)
                    {
                        auto& target{target_vec.index_unchecked(elem_index)};
                        target = {};

                        auto const& elem{resolved_table->elems.index_unchecked(elem_index)};
                        switch(elem.type)
                        {
                            case table_elem_type::func_ref_defined:
                            {
                                auto const defined_func_ptr{elem.storage.defined_ptr};
                                if(defined_func_ptr == nullptr) { break; }

                                auto const defined_info{find_defined_func_info(defined_func_ptr)};
                                if(defined_info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

# if defined(UWVM_RUNTIME_LLVM_JIT)
                                // Publish the most direct executable address available. Lazy/tiered placeholders keep indirect calls
                                // valid before the target function has been materialized.
                                ::std::uintptr_t raw_defined_entry_address{};
                                if(try_get_runtime_llvm_jit_raw_defined_entry_address(defined_info->module_id,
                                                                                      defined_info->function_index,
                                                                                      raw_defined_entry_address))
                                {
                                    target.entry_address = raw_defined_entry_address;
                                    target.context_address = 0u;
                                    ::std::uintptr_t typed_defined_entry_address{};
                                    if(try_get_runtime_llvm_jit_defined_entry_address(defined_info->module_id,
                                                                                      defined_info->function_index,
                                                                                      typed_defined_entry_address))
                                    {
                                        target.typed_entry_address = typed_defined_entry_address;
                                    }
                                }
                                else
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
                                    if(g_runtime.lazy_compile_active && ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                                                            ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only)
                                {
                                    target.entry_address = reinterpret_cast<::std::uintptr_t>(llvm_jit_lazy_raw_call_defined_entry);
                                    target.context_address = reinterpret_cast<::std::uintptr_t>(defined_info);
                                }
                                else
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                                    if(tiered_runtime_active() && tiered_uses_tiered_targets())
                                {
                                    target.entry_address = reinterpret_cast<::std::uintptr_t>(tiered_raw_call_defined_entry);
                                    target.context_address = reinterpret_cast<::std::uintptr_t>(defined_info);
                                }
                                else
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                                    if(tiered_runtime_active())
                                {
                                    target.entry_address = reinterpret_cast<::std::uintptr_t>(llvm_jit_lazy_raw_call_defined_entry);
                                    target.context_address = reinterpret_cast<::std::uintptr_t>(defined_info);
                                }
                                else
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                                {
                                    target.entry_address = reinterpret_cast<::std::uintptr_t>(llvm_jit_raw_call_defined_entry);
                                    target.context_address = reinterpret_cast<::std::uintptr_t>(defined_info->compiled_call_info);
                                }
# else
                                {
                                    ::fast_io::fast_terminate();
                                }
# endif
                                target.encoded_type_id = find_canonical_type_id_for_sig(caller_rec, func_sig_from_defined(defined_info->runtime_func));
                                break;
                            }
                            case table_elem_type::func_ref_imported:
                            {
                                auto const imported_func_ptr{elem.storage.imported_ptr};
                                if(imported_func_ptr == nullptr) { break; }
                                if(provider_import_begin == nullptr || imported_func_ptr < provider_import_begin ||
                                   imported_func_ptr >= provider_import_begin + provider_import_count) [[unlikely]]
                                {
                                    ::fast_io::fast_terminate();
                                }

                                auto const import_index{static_cast<::std::size_t>(imported_func_ptr - provider_import_begin)};
                                if(import_index >= provider_import_cache->size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                                auto const& cached_target{provider_import_cache->index_unchecked(import_index)};
                                target.entry_address = reinterpret_cast<::std::uintptr_t>(llvm_jit_raw_call_cached_import_entry);
                                target.context_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(cached_target));
                                target.encoded_type_id = find_canonical_type_id_for_sig(caller_rec, cached_target.sig);
                                break;
                            }
                            [[unlikely]] default:
                            {
                                ::fast_io::fast_terminate();
                            }
                        }
                    }

                    table_view.data_address = reinterpret_cast<::std::uintptr_t>(target_vec.data());
                    table_view.size = target_vec.size();
                }
            }
        }
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        inline constexpr void ensure_bridges_initialized() noexcept;
#endif
        inline constexpr void compile_all_modules_if_needed(bool initialize_interpreter_bridges = true) noexcept;

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            describe_runtime_mode(::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t mode) noexcept
        {
            // Human-readable labels for runtime diagnostics and verbose startup logs.
            switch(mode)
            {
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::auto_compile: return u8"auto_compile";
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile: return u8"lazy_compile";
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::lazy_compile_with_full_code_verification:
                    return u8"lazy_compile_with_full_code_verification";
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile:
                    return u8"full_compile";
                [[unlikely]] default:
                    return u8"<unknown-runtime-mode>";
            }
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            describe_runtime_compiler(::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t compiler) noexcept
        {
            // Keep logging independent of which compiler backends are compiled into this build.
            switch(compiler)
            {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only: return u8"uwvm_interpreter_only";
#endif
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::debug_interpreter: return u8"debug_interpreter";
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered: return u8"uwvm_interpreter_llvm_jit_tiered";
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only: return u8"llvm_jit_only";
#endif
                [[unlikely]] default:
                    return u8"<unknown-runtime-compiler>";
            }
        }

        enum class default_runtime_scheduling_profile_t : unsigned
        {
            // Default split policies differ by backend because interpreter translation and LLVM codegen scale differently.
            uwvm_int,
            llvm_jit,
            debug_int
            /// @todo debug_llvm_jit
            // Reserve an explicit future slot in the dispatch switch below when a debug-llvm-jit compiler mode is added.
        };

        [[nodiscard]] inline constexpr default_runtime_scheduling_profile_t
            resolve_default_runtime_scheduling_profile(::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t compiler) noexcept
        {
            // Map the active compiler to the scheduling profile used when the user leaves scheduling at its default.
            using runtime_compiler_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t;

            switch(compiler)
            {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                case runtime_compiler_t::uwvm_interpreter_only:
                {
                    return default_runtime_scheduling_profile_t::uwvm_int;
                }
#endif
#if defined(UWVM_RUNTIME_DEBUG_INTERPRETER)
                case runtime_compiler_t::debug_interpreter:
                {
                    return default_runtime_scheduling_profile_t::debug_int;
                }
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                case runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered:
                {
                    return default_runtime_scheduling_profile_t::llvm_jit;
                }
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case runtime_compiler_t::llvm_jit_only:
                {
                    return default_runtime_scheduling_profile_t::llvm_jit;
                }
#endif
                [[unlikely]] default:
                {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
            }
        }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_uwvm_int_translation() noexcept
        {
            // Tiered mode may still request interpreter T0 unless explicitly disabled.
            switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler)
            {
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only: return true;
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered:
                    return !::uwvm2::uwvm::runtime::runtime_mode::runtime_tiered_disable_uwvm_int_lazy_interpreter;
# endif
                [[unlikely]] default:
                    return false;
            }
        }
#else
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_uwvm_int_translation() noexcept { return false; }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_llvm_jit_translation() noexcept
        {
            // Full and tiered LLVM modes both need LLVM translation artifacts; interpreter-only modes do not.
            switch(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler)
            {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered: return true;
# endif
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only:
                    return true;
                [[unlikely]] default:
                    return false;
            }
        }

        [[nodiscard]] inline constexpr bool runtime_compiler_requires_llvm_jit_execution() noexcept
        {
            // In llvm_jit_only mode failure to materialize is fatal; mixed modes can still fall back to the interpreter.
            return ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            get_runtime_llvm_jit_wasm_function_name(runtime_module_storage_t const& runtime_module, ::std::size_t func_index) noexcept
        {
            // Mirror the translator's symbol naming so materialization can resolve typed wasm entries from MCJIT.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            return llvm_jit_translate_details::get_llvm_wasm_function_name(
                runtime_module,
                static_cast<llvm_jit_translate_details::validation_module_traits_t::wasm_u32>(func_index));
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            get_runtime_llvm_jit_wasm_raw_function_name(runtime_module_storage_t const& runtime_module, ::std::size_t func_index) noexcept
        {
            // Raw entry symbols use the buffer ABI exposed to hosts, import thunks, and lazy placeholders.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            return llvm_jit_translate_details::get_llvm_wasm_raw_function_name(
                runtime_module,
                static_cast<llvm_jit_translate_details::validation_module_traits_t::wasm_u32>(func_index));
        }

        template <typename ValueType>
        [[nodiscard]] inline constexpr bool runtime_llvm_jit_cache_range_bytes(ValueType const* begin,
                                                                               ValueType const* end,
                                                                               ::std::byte const*& first,
                                                                               ::std::size_t& size) noexcept
        {
            if(begin == end)
            {
                first = nullptr;
                size = 0uz;
                return true;
            }
            if(begin == nullptr || end == nullptr) [[unlikely]]
            {
                first = nullptr;
                size = 0uz;
                return false;
            }

            auto const begin_u{reinterpret_cast<::std::uintptr_t>(begin)};
            auto const end_u{reinterpret_cast<::std::uintptr_t>(end)};
            if(end_u < begin_u) [[unlikely]]
            {
                first = nullptr;
                size = 0uz;
                return false;
            }

            auto const diff{end_u - begin_u};
            if((diff % sizeof(ValueType)) != 0uz) [[unlikely]]
            {
                first = nullptr;
                size = 0uz;
                return false;
            }

            first = reinterpret_cast<::std::byte const*>(begin);
            size = static_cast<::std::size_t>(diff);
            return true;
        }

        inline constexpr void runtime_llvm_jit_cache_sha_update(::fast_io::sha256_context& sha,
                                                                ::std::byte const* first,
                                                                ::std::size_t size) noexcept
        {
            if(first != nullptr && size != 0uz) { sha.update(first, first + size); }
        }

        template <::std::size_t N>
        inline constexpr void runtime_llvm_jit_cache_sha_update_literal(::fast_io::sha256_context& sha, char8_t const (&literal)[N]) noexcept
        {
            static_assert(N != 0uz);
            runtime_llvm_jit_cache_sha_update(sha, reinterpret_cast<::std::byte const*>(literal), N - 1uz);
        }

        template <typename ValueType>
        inline constexpr void runtime_llvm_jit_cache_sha_update_le(::fast_io::sha256_context& sha, ValueType value) noexcept
        {
            auto const le{::fast_io::little_endian(value)};
            runtime_llvm_jit_cache_sha_update(sha, reinterpret_cast<::std::byte const*>(::std::addressof(le)), sizeof(le));
        }

        inline constexpr void runtime_llvm_jit_cache_sha_update_u8string_view(::fast_io::sha256_context& sha,
                                                                              ::uwvm2::utils::container::u8string_view view) noexcept
        {
            runtime_llvm_jit_cache_sha_update_literal(sha, u8"str");
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(view.size()));
            runtime_llvm_jit_cache_sha_update(sha, reinterpret_cast<::std::byte const*>(view.data()), view.size());
        }

        template <::std::size_t ValidN, ::std::size_t InvalidN>
        inline constexpr void runtime_llvm_jit_cache_sha_update_range_or_tag(::fast_io::sha256_context& sha,
                                                                             char8_t const (&valid_tag)[ValidN],
                                                                             char8_t const (&invalid_tag)[InvalidN],
                                                                             ::std::byte const* first,
                                                                             ::std::size_t size,
                                                                             bool valid) noexcept
        {
            if(valid) { runtime_llvm_jit_cache_sha_update_literal(sha, valid_tag); }
            else
            {
                runtime_llvm_jit_cache_sha_update_literal(sha, invalid_tag);
            }
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(size));
            if(valid) { runtime_llvm_jit_cache_sha_update(sha, first, size); }
        }

        template <typename FunctionType>
        inline constexpr void runtime_llvm_jit_cache_sha_update_function_type(::fast_io::sha256_context& sha,
                                                                              FunctionType const* function_type) noexcept
        {
            if(function_type == nullptr) [[unlikely]]
            {
                runtime_llvm_jit_cache_sha_update_literal(sha, u8"function-type-missing");
                return;
            }

            ::std::byte const* first{};
            ::std::size_t size{};
            auto const parameter_valid{runtime_llvm_jit_cache_range_bytes(function_type->parameter.begin, function_type->parameter.end, first, size)};
            runtime_llvm_jit_cache_sha_update_range_or_tag(sha, u8"param", u8"param-invalid", first, size, parameter_valid);

            auto const result_valid{runtime_llvm_jit_cache_range_bytes(function_type->result.begin, function_type->result.end, first, size)};
            runtime_llvm_jit_cache_sha_update_range_or_tag(sha, u8"result", u8"result-invalid", first, size, result_valid);
        }

        template <typename ImportType>
        inline constexpr void runtime_llvm_jit_cache_sha_update_import_type(::fast_io::sha256_context& sha, ImportType const* import_type) noexcept
        {
            if(import_type == nullptr) [[unlikely]]
            {
                runtime_llvm_jit_cache_sha_update_literal(sha, u8"import-type-missing");
                return;
            }

            runtime_llvm_jit_cache_sha_update_u8string_view(sha, import_type->module_name);
            runtime_llvm_jit_cache_sha_update_u8string_view(sha, import_type->extern_name);
            auto const import_tag{static_cast<::std::uint_least64_t>(import_type->imports.type)};
            runtime_llvm_jit_cache_sha_update_le(sha, import_tag);
            if(import_tag == 0u) { runtime_llvm_jit_cache_sha_update_function_type(sha, import_type->imports.storage.function); }
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            runtime_llvm_jit_cache_sha256_hex(::fast_io::sha256_context& sha) noexcept
        {
            sha.do_final();
            ::uwvm2::utils::container::array<::std::byte, ::uwvm2::runtime::llvm_jit_cache::cache_sha256_digest_size> digest{};
            sha.digest_to_byte_ptr(digest.data());

            ::uwvm2::utils::container::u8string out{};
            out.reserve(digest.size() * 2uz);
            ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
            for(auto b: digest) { ::fast_io::io::print(ref, ::fast_io::mnp::hex<false, true>(::std::to_integer<::std::uint_least8_t>(b))); }
            return out;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
            runtime_llvm_jit_full_module_cache_fingerprint(runtime_module_storage_t const& runtime_module) noexcept
        {
            ::fast_io::sha256_context sha{};
            runtime_llvm_jit_cache_sha_update_literal(sha, u8"uwvm2-llvm-jit-full-module-cache-fingerprint-v2");
            runtime_llvm_jit_cache_sha_update_u8string_view(sha, runtime_module.module_name);

            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.imported_function_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.local_defined_function_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.imported_table_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.local_defined_table_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.imported_memory_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.local_defined_memory_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.imported_global_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.local_defined_global_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.local_defined_element_vec_storage.size()));
            runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(runtime_module.local_defined_data_vec_storage.size()));

            for(auto const& imported_func: runtime_module.imported_function_vec_storage)
            {
                runtime_llvm_jit_cache_sha_update_import_type(sha, imported_func.import_type_ptr);
                runtime_llvm_jit_cache_sha_update_le(sha, static_cast<::std::uint_least64_t>(imported_func.link_kind));
            }

            for(auto const& local_func: runtime_module.local_defined_function_vec_storage)
            {
                runtime_llvm_jit_cache_sha_update_function_type(sha, local_func.function_type_ptr);
                auto const wasm_code_ptr{local_func.wasm_code_ptr};
                if(wasm_code_ptr == nullptr) [[unlikely]]
                {
                    runtime_llvm_jit_cache_sha_update_literal(sha, u8"code-missing");
                    continue;
                }

                ::std::byte const* first{};
                ::std::size_t size{};
                auto const code_valid{runtime_llvm_jit_cache_range_bytes(wasm_code_ptr->body.code_begin, wasm_code_ptr->body.code_end, first, size)};
                runtime_llvm_jit_cache_sha_update_range_or_tag(sha, u8"code", u8"code-invalid", first, size, code_valid);
            }

            return runtime_llvm_jit_cache_sha256_hex(sha);
        }

        [[nodiscard]] inline constexpr ::uwvm2::runtime::llvm_jit_cache::cache_context
            runtime_llvm_jit_parallel_object_cache_context(::uwvm2::runtime::llvm_jit_cache::cache_context const& base_context,
                                                           ::std::size_t object_count,
                                                           ::std::size_t object_index) noexcept
        {
            auto ctx{base_context};
            auto key{::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"parallel-object")};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(key, u8"base-key", base_context.cache_key);
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                key, u8"parallel-object-count", static_cast<::std::uint_least64_t>(object_count));
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                key, u8"parallel-object-index", static_cast<::std::uint_least64_t>(object_index));
            ctx.cache_key = ::std::move(key);
            ctx.cache_key_is_complete = true;
            return ctx;
        }

        [[nodiscard]] inline constexpr bool load_runtime_llvm_jit_parallel_object_cache(
            ::uwvm2::runtime::llvm_jit_cache::cache_context const& base_context,
            ::uwvm2::runtime::llvm_jit_cache::cache_policy const& cache_policy,
            ::std::size_t object_count,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>& object_outputs) noexcept
        {
            if(!cache_policy.enable || object_count == 0uz) { return false; }

            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> loaded_objects{};
            loaded_objects.reserve(object_count);
            for(::std::size_t object_index{}; object_index != object_count; ++object_index)
            {
                auto object_context{runtime_llvm_jit_parallel_object_cache_context(base_context, object_count, object_index)};
                auto load{::uwvm2::runtime::llvm_jit_cache::load_object(object_context, cache_policy)};
                if(load.status != ::uwvm2::runtime::llvm_jit_cache::cache_status::ok) { return false; }

                loaded_objects.emplace_back(::uwvm2::utils::container::u8string_view{
                    reinterpret_cast<char8_t const*>(load.object.cbegin()), load.object.size()});
            }

            object_outputs = ::std::move(loaded_objects);
            return true;
        }

        inline constexpr void store_runtime_llvm_jit_parallel_object_cache(
            ::uwvm2::runtime::llvm_jit_cache::cache_context const& base_context,
            ::uwvm2::runtime::llvm_jit_cache::cache_policy const& cache_policy,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& object_outputs) noexcept
        {
            if(!cache_policy.enable) { return; }

            auto const object_count{object_outputs.size()};
            for(::std::size_t object_index{}; object_index != object_count; ++object_index)
            {
                auto object_context{runtime_llvm_jit_parallel_object_cache_context(base_context, object_count, object_index)};
                auto const& object_output{object_outputs.index_unchecked(object_index)};
                auto const first{reinterpret_cast<::std::byte const*>(object_output.cbegin())};
                (void)::uwvm2::runtime::llvm_jit_cache::store_object_async(object_context, first, object_output.size(), cache_policy);
            }
        }

        inline constexpr bool ensure_llvm_jit_native_target_initialized() noexcept
        {
            // LLVM native target setup is process-global and not idempotent in all versions, so guard it behind local statics.
            static bool initialized{};
            static bool success{};

            if(initialized) { return success; }

            initialized = true;
            auto& pass_registry{*::llvm::PassRegistry::getPassRegistry()};
            ::llvm::initializeCore(pass_registry);
            ::llvm::initializeTransformUtils(pass_registry);
            ::llvm::initializeScalarOpts(pass_registry);
            ::llvm::initializeInstCombine(pass_registry);
            ::llvm::initializeAnalysis(pass_registry);
            ::llvm::initializeTarget(pass_registry);
            success = !::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter();
            return success;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>
            get_llvm_jit_host_target_attribute_storage() noexcept
        {
            // Snapshot host CPU features as owned strings; StringRefs used by LLVM later must outlive the EngineBuilder call.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> mattrs{};
            auto host_features{::llvm::sys::getHostCPUFeatures()};
            if(!host_features.empty())
            {
                mattrs.reserve(host_features.size());
                for(auto const& [feature_name, feature_enabled]: host_features)
                {
                    mattrs.push_back(::uwvm2::utils::container::u8concat_uwvm(feature_enabled ? u8"+" : u8"-",
                                                                              llvm_jit_translate_details::get_uwvm_u8string_view(feature_name)));
                }
            }
            return mattrs;
        }

        inline constexpr void
            append_llvm_jit_host_target_attribute_refs(::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& attr_storage,
                                                       ::llvm::SmallVector<::llvm::StringRef, 16>& attr_refs) noexcept
        {
            // Convert owned feature strings into LLVM StringRefs without reallocating the backing storage.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            attr_refs.clear();
            attr_refs.reserve(attr_storage.size());
            for(auto const& attr: attr_storage) { attr_refs.push_back(llvm_jit_translate_details::get_llvm_string_ref(attr)); }
        }

        enum class runtime_llvm_jit_full_pipeline_kind : unsigned
        {
            // Controls the optimization pass pipeline run before full-module MCJIT materialization.
            none,
            legacy_light,
            passbuilder_tuned
        };

        struct runtime_llvm_jit_full_materialize_strategy
        {
            // Resolved policy: optimization pipeline, codegen level, and the log label explaining where it came from.
            runtime_llvm_jit_full_pipeline_kind pipeline{};
            ::llvm::CodeGenOptLevel codegen_opt_level{};
            ::uwvm2::utils::container::u8string_view policy_name{};
        };

        [[nodiscard]] inline constexpr ::llvm::CodeGenOptLevel resolve_runtime_llvm_jit_lazy_codegen_opt_level(::llvm::CodeGenOptLevel default_level) noexcept
        {
            // Lazy units bias toward faster codegen; explicit lazy policy overrides the broader LLVM JIT policy.
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            using runtime_llvm_jit_lazy_policy_t = runtime_mode::runtime_llvm_jit_lazy_policy_t;
            if(runtime_mode::runtime_llvm_jit_lazy_policy_existed)
            {
                switch(runtime_mode::global_runtime_llvm_jit_lazy_policy)
                {
                    case runtime_llvm_jit_lazy_policy_t::auto_policy: break;
                    case runtime_llvm_jit_lazy_policy_t::debug: return ::llvm::CodeGenOptLevel::None;
                    case runtime_llvm_jit_lazy_policy_t::light: return ::llvm::CodeGenOptLevel::Less;
                    case runtime_llvm_jit_lazy_policy_t::balanced: return ::llvm::CodeGenOptLevel::Default;
                }
            }

            using runtime_llvm_jit_policy_t = runtime_mode::runtime_llvm_jit_policy_t;
            if(runtime_mode::runtime_llvm_jit_policy_existed)
            {
                switch(runtime_mode::global_runtime_llvm_jit_policy)
                {
                    case runtime_llvm_jit_policy_t::debug: return ::llvm::CodeGenOptLevel::None;
                    case runtime_llvm_jit_policy_t::default_policy: break;
                    case runtime_llvm_jit_policy_t::fast_compile: return ::llvm::CodeGenOptLevel::Less;
                    case runtime_llvm_jit_policy_t::balanced: return ::llvm::CodeGenOptLevel::Less;
                    case runtime_llvm_jit_policy_t::max: return ::llvm::CodeGenOptLevel::Less;
                }
            }

            return default_level;
        }

        [[nodiscard]] inline constexpr runtime_llvm_jit_full_materialize_strategy
            make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind pipeline,
                                                            ::llvm::CodeGenOptLevel codegen_opt_level,
                                                            ::uwvm2::utils::container::u8string_view policy_name) noexcept
        {
            // CodeGenOptLevel::None disables LLVM optimization passes entirely even if a pipeline was requested.
            if(codegen_opt_level == ::llvm::CodeGenOptLevel::None) { pipeline = runtime_llvm_jit_full_pipeline_kind::none; }
            return {.pipeline = pipeline, .codegen_opt_level = codegen_opt_level, .policy_name = policy_name};
        }

        [[nodiscard]] inline constexpr runtime_llvm_jit_full_materialize_strategy
            resolve_runtime_llvm_jit_full_materialize_strategy(::llvm::CodeGenOptLevel default_level) noexcept
        {
            // Resolve the final full-JIT materialization policy. Full-specific options win, then general JIT policy, then default.
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            using runtime_llvm_jit_full_policy_t = runtime_mode::runtime_llvm_jit_full_policy_t;
            if(runtime_mode::runtime_llvm_jit_full_policy_existed)
            {
                switch(runtime_mode::global_runtime_llvm_jit_full_policy)
                {
                    case runtime_llvm_jit_full_policy_t::auto_policy: break;
                    case runtime_llvm_jit_full_policy_t::debug:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::none,
                                                                               ::llvm::CodeGenOptLevel::None,
                                                                               ::uwvm2::utils::container::u8string_view{u8"full:debug", 10uz});
                    case runtime_llvm_jit_full_policy_t::legacy_light:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::legacy_light,
                                                                               default_level,
                                                                               ::uwvm2::utils::container::u8string_view{u8"full:legacy-light", 17uz});
                    case runtime_llvm_jit_full_policy_t::passbuilder_o1:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::passbuilder_tuned,
                                                                               ::llvm::CodeGenOptLevel::Less,
                                                                               ::uwvm2::utils::container::u8string_view{u8"full:pb-o1", 10uz});
                    case runtime_llvm_jit_full_policy_t::passbuilder_o2:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::passbuilder_tuned,
                                                                               ::llvm::CodeGenOptLevel::Default,
                                                                               ::uwvm2::utils::container::u8string_view{u8"full:pb-o2", 10uz});
                    case runtime_llvm_jit_full_policy_t::passbuilder_o3:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::passbuilder_tuned,
                                                                               ::llvm::CodeGenOptLevel::Aggressive,
                                                                               ::uwvm2::utils::container::u8string_view{u8"full:pb-o3", 10uz});
                }
            }

            using runtime_llvm_jit_policy_t = runtime_mode::runtime_llvm_jit_policy_t;
            if(runtime_mode::runtime_llvm_jit_policy_existed)
            {
                switch(runtime_mode::global_runtime_llvm_jit_policy)
                {
                    case runtime_llvm_jit_policy_t::debug:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::none,
                                                                               ::llvm::CodeGenOptLevel::None,
                                                                               ::uwvm2::utils::container::u8string_view{u8"policy:debug", 12uz});
                    case runtime_llvm_jit_policy_t::default_policy: break;
                    case runtime_llvm_jit_policy_t::fast_compile:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::legacy_light,
                                                                               ::llvm::CodeGenOptLevel::Less,
                                                                               ::uwvm2::utils::container::u8string_view{u8"policy:fast-compile", 19uz});
                    case runtime_llvm_jit_policy_t::balanced:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::passbuilder_tuned,
                                                                               ::llvm::CodeGenOptLevel::Less,
                                                                               ::uwvm2::utils::container::u8string_view{u8"policy:balanced", 15uz});
                    case runtime_llvm_jit_policy_t::max:
                        return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::passbuilder_tuned,
                                                                               ::llvm::CodeGenOptLevel::Aggressive,
                                                                               ::uwvm2::utils::container::u8string_view{u8"policy:max", 10uz});
                }
            }

            if(runtime_mode::global_runtime_mode == runtime_mode::runtime_mode_t::full_compile &&
               runtime_mode::global_runtime_compiler == runtime_mode::runtime_compiler_t::llvm_jit_only)
            {
                return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::passbuilder_tuned,
                                                                       ::llvm::CodeGenOptLevel::Aggressive,
                                                                       ::uwvm2::utils::container::u8string_view{u8"aot:max", 7uz});
            }

            return make_runtime_llvm_jit_full_materialize_strategy(runtime_llvm_jit_full_pipeline_kind::legacy_light,
                                                                   default_level,
                                                                   ::uwvm2::utils::container::u8string_view{u8"default", 7uz});
        }

        [[nodiscard]] inline constexpr ::llvm::OptimizationLevel get_runtime_llvm_jit_pipeline_opt_level(::llvm::CodeGenOptLevel codegen_opt_level) noexcept
        {
            // Translate MCJIT codegen levels into the new pass-manager optimization levels.
            switch(codegen_opt_level)
            {
                case ::llvm::CodeGenOptLevel::None: return ::llvm::OptimizationLevel::O0;
                case ::llvm::CodeGenOptLevel::Less: return ::llvm::OptimizationLevel::O1;
                case ::llvm::CodeGenOptLevel::Default: return ::llvm::OptimizationLevel::O2;
                case ::llvm::CodeGenOptLevel::Aggressive: return ::llvm::OptimizationLevel::O3;
            }
            ::fast_io::fast_terminate();
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            get_runtime_llvm_jit_full_pipeline_name(runtime_llvm_jit_full_pipeline_kind pipeline) noexcept
        {
            // Stable text used in runtime logs.
            switch(pipeline)
            {
                case runtime_llvm_jit_full_pipeline_kind::none: return ::uwvm2::utils::container::u8string_view{u8"none", 4uz};
                case runtime_llvm_jit_full_pipeline_kind::legacy_light: return ::uwvm2::utils::container::u8string_view{u8"legacy-light", 12uz};
                case runtime_llvm_jit_full_pipeline_kind::passbuilder_tuned: return ::uwvm2::utils::container::u8string_view{u8"passbuilder-tuned", 17uz};
            }
            ::fast_io::fast_terminate();
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            get_runtime_llvm_jit_codegen_opt_level_name(::llvm::CodeGenOptLevel codegen_opt_level) noexcept
        {
            // Stable text used in runtime logs.
            switch(codegen_opt_level)
            {
                case ::llvm::CodeGenOptLevel::None: return ::uwvm2::utils::container::u8string_view{u8"none", 4uz};
                case ::llvm::CodeGenOptLevel::Less: return ::uwvm2::utils::container::u8string_view{u8"less", 4uz};
                case ::llvm::CodeGenOptLevel::Default: return ::uwvm2::utils::container::u8string_view{u8"default", 7uz};
                case ::llvm::CodeGenOptLevel::Aggressive: return ::uwvm2::utils::container::u8string_view{u8"aggressive", 10uz};
            }
            ::fast_io::fast_terminate();
        }

        inline constexpr void run_runtime_llvm_jit_legacy_light_function_pipeline(::llvm::Module& module, ::llvm::TargetMachine& target_machine) noexcept
        {
            // Lightweight per-function optimization pipeline used when compile time matters more than whole-module optimization.
            ::llvm::legacy::FunctionPassManager function_pass_manager(::std::addressof(module));
            function_pass_manager.add(::llvm::createTargetTransformInfoWrapperPass(target_machine.getTargetIRAnalysis()));
            function_pass_manager.add(::llvm::createPromoteMemoryToRegisterPass());
            function_pass_manager.add(::llvm::createInstructionCombiningPass());
            function_pass_manager.add(::llvm::createReassociatePass());
            function_pass_manager.add(::llvm::createGVNPass());
            function_pass_manager.add(::llvm::createCFGSimplificationPass());
            function_pass_manager.add(::llvm::createLICMPass());
            function_pass_manager.add(::llvm::createInstSimplifyLegacyPass());
            function_pass_manager.add(::llvm::createDeadCodeEliminationPass());

            function_pass_manager.doInitialization();
            for(auto& function: module)
            {
                if(function.isDeclaration()) { continue; }
                function_pass_manager.run(function);
            }
            function_pass_manager.doFinalization();
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> make_runtime_llvm_jit_materialize_target_machine(
            ::uwvm2::utils::container::u8string const& host_cpu_name,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& host_target_attribute_storage,
            ::llvm::CodeGenOptLevel codegen_opt_level) noexcept
        {
            // Build a target machine with the same CPU/features that the final MCJIT engine will use.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

            ::llvm::SmallVector<::llvm::StringRef, 16> host_target_attributes{};
            append_llvm_jit_host_target_attribute_refs(host_target_attribute_storage, host_target_attributes);

            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT)
                .setOptLevel(codegen_opt_level)
                .setMCPU(llvm_jit_translate_details::get_llvm_string_ref(host_cpu_name))
                .setMAttrs(host_target_attributes);

            return ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine>{target_builder.selectTarget()};
        }

        struct runtime_llvm_jit_legacy_light_task_preopt_context
        {
            // Shared context for pre-link optimization of per-task LLVM modules before they are linked into the full module.
            ::uwvm2::utils::container::u8string host_cpu_name{};
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> host_target_attribute_storage{};
            ::llvm::CodeGenOptLevel codegen_opt_level{};
            ::std::atomic_size_t optimized_task_modules{};
        };

        [[nodiscard]] inline constexpr bool optimize_runtime_llvm_jit_legacy_light_task_module_pre_link(
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_module_storage_t& module_storage,
            void* opaque) noexcept
        {
            // Optional task-module preoptimization keeps the final legacy-light materialization cheap after parallel translation.
            auto const context{static_cast<runtime_llvm_jit_legacy_light_task_preopt_context*>(opaque)};
            if(context == nullptr || module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

            auto target_machine{
                make_runtime_llvm_jit_materialize_target_machine(context->host_cpu_name, context->host_target_attribute_storage, context->codegen_opt_level)};
            if(target_machine == nullptr) [[unlikely]] { return false; }

            module_storage.llvm_module->setTargetTriple(target_machine->getTargetTriple());
            module_storage.llvm_module->setDataLayout(target_machine->createDataLayout());
            run_runtime_llvm_jit_legacy_light_function_pipeline(*module_storage.llvm_module, *target_machine);
            context->optimized_task_modules.fetch_add(1uz, ::std::memory_order_relaxed);
            return true;
        }

        enum class runtime_llvm_jit_parallel_object_emit_result : unsigned
        {
            // Result of the optional partitioned object-emission path.
            not_applicable,
            success,
            failed
        };

        [[nodiscard]] inline constexpr bool serialize_runtime_llvm_jit_module_bitcode(::llvm::Module& module,
                                                                                      ::uwvm2::utils::container::u8string& output) noexcept
        {
            // Serialize the optimized module once so worker tasks can parse independent LLVMContexts without sharing IR objects.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

            output.clear();
            llvm_jit_translate_details::raw_uwvm_string_ostream bitcode_stream(output);
            ::llvm::WriteBitcodeToFile(module, bitcode_stream);
            return true;
        }

        [[nodiscard]] inline constexpr bool
            runtime_llvm_jit_function_name_in_range(::llvm::StringRef name,
                                                    ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& function_names,
                                                    ::std::size_t begin,
                                                    ::std::size_t end) noexcept
        {
            // Linear lookup is fine here because this helper runs during object partitioning, not on execution hot paths.
            for(::std::size_t i{begin}; i != end; ++i)
            {
                auto const& curr{function_names.index_unchecked(i)};
                if(name.size() != curr.size()) { continue; }
                if(name.size() == 0u) { return true; }
                if(::std::memcmp(name.data(), curr.data(), name.size()) == 0) { return true; }
            }
            return false;
        }

        [[nodiscard]] inline constexpr bool
            runtime_llvm_jit_name_in_list(::llvm::StringRef name, ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& names) noexcept
        {
            // Check whether a symbol name was collected as a cross-partition dependency.
            for(auto const& curr: names)
            {
                if(name.size() != curr.size()) { continue; }
                if(name.size() == 0u) { return true; }
                if(::std::memcmp(name.data(), curr.data(), name.size()) == 0) { return true; }
            }
            return false;
        }

        inline constexpr void collect_runtime_llvm_jit_parallel_object_function_names(
            ::llvm::Module& module,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>& function_names) noexcept
        {
            function_names.clear();
            for(auto const& function: module)
            {
                if(function.isDeclaration()) { continue; }
                auto const name{function.getName()};
                if(name.empty()) { continue; }
                function_names.emplace_back(::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_uwvm_u8string_view(name));
            }
        }

        [[nodiscard]] inline constexpr ::std::size_t
            runtime_llvm_jit_parallel_object_task_count(::std::size_t function_count, ::std::size_t extra_compile_threads) noexcept
        {
            if(extra_compile_threads == 0uz || function_count <= 1uz) { return 0uz; }

            auto const effective_extra_threads{::uwvm2::utils::thread::clamp_extra_worker_count(function_count, extra_compile_threads)};
            if(effective_extra_threads == 0uz) { return 0uz; }
            return effective_extra_threads + 1uz;
        }

        inline constexpr void expose_runtime_llvm_jit_partition_symbol(::llvm::GlobalValue& global_value) noexcept
        {
            // Partitioned objects must be able to resolve cross-object references while still hiding symbols from the host process.
            if(global_value.hasLocalLinkage()) { global_value.setLinkage(::llvm::GlobalValue::ExternalLinkage); }
            global_value.setVisibility(::llvm::GlobalValue::HiddenVisibility);
            global_value.setDSOLocal(true);
        }

        inline constexpr void drop_runtime_llvm_jit_partition_global_definition(::llvm::GlobalVariable& global_variable) noexcept
        {
            // Non-owner object partitions keep only declarations for globals whose storage is emitted by partition zero.
            if(global_variable.isDeclaration()) { return; }
            expose_runtime_llvm_jit_partition_symbol(global_variable);
            global_variable.setInitializer(nullptr);
        }

        inline constexpr void collect_runtime_llvm_jit_partition_exposed_symbol_from_value(::llvm::Value const* value,
                                                                                           ::std::size_t source_task_index,
                                                                                           ::llvm::StringMap<::std::size_t> const& function_task_indices,
                                                                                           ::llvm::StringMap<char>& exposed_symbol_names) noexcept
        {
            // Walk constants and instruction operands to find non-local symbols a partition must keep externally visible.
            if(value == nullptr) { return; }

            auto const stripped_value{value->stripPointerCasts()};
            if(auto const global_value{::llvm::dyn_cast<::llvm::GlobalValue>(stripped_value)})
            {
                if(global_value->isDeclaration()) { return; }

                auto const name{global_value->getName()};
                if(name.empty()) { return; }

                if(::llvm::isa<::llvm::Function>(global_value))
                {
                    auto const target_task_it{function_task_indices.find(name)};
                    if(target_task_it != function_task_indices.end() && target_task_it->getValue() != source_task_index) { exposed_symbol_names[name] = 0; }
                }
                else if(source_task_index != 0uz) { exposed_symbol_names[name] = 0; }
                return;
            }

            auto const constant_value{::llvm::dyn_cast<::llvm::Constant>(stripped_value)};
            if(constant_value == nullptr) { return; }

            for(auto const& operand: constant_value->operands())
            {
                collect_runtime_llvm_jit_partition_exposed_symbol_from_value(operand.get(), source_task_index, function_task_indices, exposed_symbol_names);
            }
        }

        inline constexpr void collect_runtime_llvm_jit_partition_exposed_symbols(
            ::llvm::Module& module,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& function_names,
            ::std::size_t functions_per_task,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>& exposed_symbol_names) noexcept
        {
            // Determine which function/global names are referenced across object partitions before each worker deletes bodies it
            // does not own.
            ::llvm::StringMap<::std::size_t> function_task_indices{};
            for(::std::size_t function_index{}; function_index != function_names.size(); ++function_index)
            {
                auto const& function_name{function_names.index_unchecked(function_index)};
                auto task_index{function_index / functions_per_task};
                function_task_indices[::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_llvm_string_ref(function_name)] = task_index;
            }

            ::llvm::StringMap<char> exposed_symbol_name_map{};
            for(auto const& function: module)
            {
                if(function.isDeclaration()) { continue; }

                auto const source_task_it{function_task_indices.find(function.getName())};
                if(source_task_it == function_task_indices.end()) { continue; }
                auto const source_task_index{source_task_it->getValue()};

                for(auto const& basic_block: function)
                {
                    for(auto const& instruction: basic_block)
                    {
                        for(auto const& operand: instruction.operands())
                        {
                            collect_runtime_llvm_jit_partition_exposed_symbol_from_value(operand.get(),
                                                                                         source_task_index,
                                                                                         function_task_indices,
                                                                                         exposed_symbol_name_map);
                        }
                    }
                }
            }

            for(auto const& global_variable: module.globals())
            {
                if(global_variable.isDeclaration() || !global_variable.hasInitializer()) { continue; }
                collect_runtime_llvm_jit_partition_exposed_symbol_from_value(global_variable.getInitializer(),
                                                                             0uz,
                                                                             function_task_indices,
                                                                             exposed_symbol_name_map);
            }

            exposed_symbol_names.clear();
            exposed_symbol_names.reserve(exposed_symbol_name_map.size());
            for(auto const& exposed_symbol_name: exposed_symbol_name_map)
            {
                auto const key{exposed_symbol_name.getKey()};
                exposed_symbol_names.emplace_back(::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_uwvm_u8string_view(key));
            }
        }

        struct runtime_llvm_jit_parallel_object_emit_failure_state
        {
            // Shared cancellation flag: any worker failure makes the main materializer fall back to single-object MCJIT.
            ::std::atomic_bool failed{};
        };

        inline ::uwvm2::utils::thread::scheduled_task make_runtime_llvm_jit_parallel_object_emit_task(
            ::uwvm2::utils::container::u8string const& source_bitcode,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& function_names,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& exposed_symbol_names,
            ::std::size_t begin,
            ::std::size_t end,
            bool owns_global_definitions,
            ::uwvm2::utils::container::u8string const& host_cpu_name,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& host_target_attribute_storage,
            ::llvm::CodeGenOptLevel codegen_opt_level,
            bool verify_llvm_jit_ir,
            ::uwvm2::utils::container::u8string& object_output,
            runtime_llvm_jit_parallel_object_emit_failure_state& failure_state) noexcept
        {
            // Worker coroutine: parse shared bitcode, keep only this partition's function bodies, emit one native object buffer.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

            if(failure_state.failed.load(::std::memory_order_acquire)) { co_return; }

# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                ::llvm::LLVMContext context{};
                auto parsed_module_expected{
                    ::llvm::parseBitcodeFile(::llvm::MemoryBufferRef(llvm_jit_translate_details::get_llvm_string_ref(source_bitcode),
                                                                     llvm_jit_translate_details::get_llvm_string_ref(u8"uwvm2-llvm-jit-object-emit-source")),
                                             context)};
                if(!parsed_module_expected) [[unlikely]]
                {
                    ::llvm::consumeError(parsed_module_expected.takeError());
                    failure_state.failed.store(true, ::std::memory_order_release);
                    co_return;
                }

                auto module{::std::move(*parsed_module_expected)};
                for(auto& function: *module)
                {
                    if(function.isDeclaration()) { continue; }

                    auto const function_name{function.getName()};
                    if(runtime_llvm_jit_function_name_in_range(function_name, function_names, begin, end))
                    {
                        if(runtime_llvm_jit_name_in_list(function_name, exposed_symbol_names)) { expose_runtime_llvm_jit_partition_symbol(function); }
                    }
                    else
                    {
                        expose_runtime_llvm_jit_partition_symbol(function);
                        function.deleteBody();
                    }
                }

                for(auto& global_variable: module->globals())
                {
                    if(owns_global_definitions)
                    {
                        if(!global_variable.isDeclaration() && runtime_llvm_jit_name_in_list(global_variable.getName(), exposed_symbol_names))
                        {
                            expose_runtime_llvm_jit_partition_symbol(global_variable);
                        }
                    }
                    else
                    {
                        drop_runtime_llvm_jit_partition_global_definition(global_variable);
                    }
                }
                if(!owns_global_definitions)
                {
                    module->setModuleInlineAsm(::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_llvm_string_ref(u8""));
                }

                auto target_machine{make_runtime_llvm_jit_materialize_target_machine(host_cpu_name, host_target_attribute_storage, codegen_opt_level)};
                if(target_machine == nullptr) [[unlikely]]
                {
                    failure_state.failed.store(true, ::std::memory_order_release);
                    co_return;
                }
                if(codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target_machine->setFastISel(true); }

                module->setTargetTriple(target_machine->getTargetTriple());
                module->setDataLayout(target_machine->createDataLayout());
                if(verify_llvm_jit_ir && ::llvm::verifyModule(*module)) [[unlikely]]
                {
                    failure_state.failed.store(true, ::std::memory_order_release);
                    co_return;
                }

                ::llvm::legacy::PassManager pass_manager{};
                ::llvm::SmallVector<char, 0> object_buffer{};
                ::llvm::raw_svector_ostream object_stream(object_buffer);
                if(target_machine->addPassesToEmitFile(pass_manager, object_stream, nullptr, ::llvm::CodeGenFileType::ObjectFile)) [[unlikely]]
                {
                    failure_state.failed.store(true, ::std::memory_order_release);
                    co_return;
                }

                pass_manager.run(*module);
                object_output = ::uwvm2::utils::container::u8string{
                    llvm_jit_translate_details::get_uwvm_u8string_view(::llvm::StringRef{object_buffer.data(), object_buffer.size()})};
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
                failure_state.failed.store(true, ::std::memory_order_release);
            }
# endif

            co_return;
        }

        [[nodiscard]] inline constexpr runtime_llvm_jit_parallel_object_emit_result
            emit_runtime_llvm_jit_objects_parallel(::llvm::Module& module,
                                                   ::uwvm2::utils::container::u8string const& host_cpu_name,
                                                   ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& host_target_attribute_storage,
                                                   ::llvm::CodeGenOptLevel codegen_opt_level,
                                                   bool verify_llvm_jit_ir,
                                                   ::std::size_t extra_compile_threads,
                                                   ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>& object_outputs,
                                                   ::std::size_t& defined_function_count) noexcept
        {
            // Optional compile-time optimization: emit multiple native object files in parallel and load all of them into one MCJIT engine.
            defined_function_count = 0uz;
            object_outputs.clear();
            if(extra_compile_threads == 0uz) { return runtime_llvm_jit_parallel_object_emit_result::not_applicable; }

            // Partitioning is function-name based: each task owns a slice of definitions and imports any cross-slice references.
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> function_names{};
            collect_runtime_llvm_jit_parallel_object_function_names(module, function_names);
            defined_function_count = function_names.size();
            auto const task_count{runtime_llvm_jit_parallel_object_task_count(function_names.size(), extra_compile_threads)};
            if(task_count == 0uz) { return runtime_llvm_jit_parallel_object_emit_result::not_applicable; }

            auto const effective_extra_threads{task_count - 1uz};

            ::uwvm2::utils::container::u8string source_bitcode{};
            if(!serialize_runtime_llvm_jit_module_bitcode(module, source_bitcode)) [[unlikely]] { return runtime_llvm_jit_parallel_object_emit_result::failed; }

            auto const functions_per_task{function_names.size() / task_count + static_cast<::std::size_t>(function_names.size() % task_count != 0uz)};
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> exposed_symbol_names{};
            collect_runtime_llvm_jit_partition_exposed_symbols(module, function_names, functions_per_task, exposed_symbol_names);
            object_outputs.resize(task_count);

            ::uwvm2::utils::thread::scheduled_task_batch task_batch{task_count};
            runtime_llvm_jit_parallel_object_emit_failure_state failure_state{};
            for(::std::size_t task_index{}; task_index != task_count; ++task_index)
            {
                // The first partition keeps global definitions; later partitions expose references and drop duplicate definitions.
                auto const begin{task_index * functions_per_task};
                auto end{begin + functions_per_task};
                if(end > function_names.size()) { end = function_names.size(); }

                auto task{make_runtime_llvm_jit_parallel_object_emit_task(source_bitcode,
                                                                          function_names,
                                                                          exposed_symbol_names,
                                                                          begin,
                                                                          end,
                                                                          task_index == 0uz,
                                                                          host_cpu_name,
                                                                          host_target_attribute_storage,
                                                                          codegen_opt_level,
                                                                          verify_llvm_jit_ir,
                                                                          object_outputs.index_unchecked(task_index),
                                                                          failure_state)};
                ::std::construct_at(task_batch.handles.buffer + task_batch.handle_count, task.release());
                ++task_batch.handle_count;
            }

            ::uwvm2::utils::thread::native_thread_pool thread_pool{};
            thread_pool.run(task_batch, effective_extra_threads);
            if(failure_state.failed.load(::std::memory_order_acquire)) { return runtime_llvm_jit_parallel_object_emit_result::failed; }

            return runtime_llvm_jit_parallel_object_emit_result::success;
        }

        [[nodiscard]] inline constexpr bool optimize_runtime_llvm_jit_module(::llvm::Module& module,
                                                                             ::llvm::TargetMachine& target_machine,
                                                                             runtime_llvm_jit_full_pipeline_kind pipeline,
                                                                             ::llvm::CodeGenOptLevel codegen_opt_level,
                                                                             bool verify_llvm_jit_ir,
                                                                             bool legacy_light_preoptimized) noexcept
        {
            // Verify before/after optimization and run the selected full-module pipeline.
            if(verify_llvm_jit_ir)
            {
                if(::llvm::verifyModule(module)) [[unlikely]]
                {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                    ::fast_io::fast_terminate();
                }
            }

            if(pipeline == runtime_llvm_jit_full_pipeline_kind::none) { return true; }

            if(pipeline == runtime_llvm_jit_full_pipeline_kind::legacy_light)
            {
                // Legacy-light preoptimization may already have run per task; avoid applying the same function pipeline twice.
                if(!legacy_light_preoptimized) { run_runtime_llvm_jit_legacy_light_function_pipeline(module, target_machine); }
            }
            else
            {
                // PassBuilder pipelines require all analysis managers and cross-proxies even for a single module pass manager.
                ::llvm::LoopAnalysisManager loop_analysis_manager{};
                ::llvm::FunctionAnalysisManager function_analysis_manager{};
                ::llvm::CGSCCAnalysisManager cgscc_analysis_manager{};
                ::llvm::ModuleAnalysisManager module_analysis_manager{};
                auto const pipeline_opt_level{get_runtime_llvm_jit_pipeline_opt_level(codegen_opt_level)};
                ::llvm::PipelineTuningOptions pipeline_tuning_options{};
                auto const pipeline_speed_level{pipeline_opt_level.getSpeedupLevel()};
                pipeline_tuning_options.LoopUnrolling = pipeline_speed_level > 1u;
                pipeline_tuning_options.LoopInterleaving = pipeline_tuning_options.LoopUnrolling;
                pipeline_tuning_options.LoopVectorization = pipeline_speed_level > 1u;
                pipeline_tuning_options.SLPVectorization = pipeline_speed_level > 1u;
                ::llvm::PassBuilder pass_builder{::std::addressof(target_machine), pipeline_tuning_options};

                pass_builder.registerModuleAnalyses(module_analysis_manager);
                pass_builder.registerCGSCCAnalyses(cgscc_analysis_manager);
                pass_builder.registerFunctionAnalyses(function_analysis_manager);
                pass_builder.registerLoopAnalyses(loop_analysis_manager);
                pass_builder.crossRegisterProxies(loop_analysis_manager, function_analysis_manager, cgscc_analysis_manager, module_analysis_manager);

                auto module_pass_manager{pass_builder.buildPerModuleDefaultPipeline(pipeline_opt_level)};
                module_pass_manager.run(module, module_analysis_manager);
            }

            if(verify_llvm_jit_ir)
            {
                if(::llvm::verifyModule(module)) [[unlikely]]
                {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                    ::fast_io::fast_terminate();
                }
            }
            return true;
        }

        // -------------------------------------------------------------------------
        // Full LLVM materialization pipeline
        // -------------------------------------------------------------------------
        // The full JIT path consumes translated LLVM IR and produces executable
        // typed/raw entry addresses. It can optionally partition object emission,
        // but final symbol lookup and runtime publication remain single-record
        // operations on compiled_module_record.
        //
        // Materialization invariants:
        // - The LLVM context/module owner moves exactly once into the engine path.
        // - Target triple and data layout are assigned before verification/optimization.
        // - Parallel object emission is an optimization, never a correctness dependency.
        // - Entry-address vectors are repopulated only after finalizeObject succeeds.
        [[nodiscard]] inline constexpr bool try_materialize_runtime_module_llvm_jit(compiled_module_record& rec,
                                                                                    bool publish_full_ready,
                                                                                    ::llvm::CodeGenOptLevel default_codegen_opt_level,
                                                                                    ::std::size_t extra_materialize_threads) noexcept
        {
            // Full-module LLVM materialization consumes emitted IR, optionally optimizes/splits it, creates an MCJIT engine, then
            // publishes typed/raw entry addresses into the runtime record.
# if !defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            static_cast<void>(publish_full_ready);
# endif
            if(extra_materialize_threads == ::std::numeric_limits<::std::size_t>::max())
            {
                extra_materialize_threads = ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved;
            }
            auto const llvm_jit_materialize_runtime_log_now{[]() constexpr noexcept
                                                            {
                                                                // Logging timestamps are optional; the materializer must remain usable without a monotonic clock.
                                                                ::fast_io::unix_timestamp ts{};
                                                                if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
                                                                {
# ifdef UWVM_CPP_EXCEPTIONS
                                                                    try
# endif
                                                                    {
                                                                        ts = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                                                                    }
# ifdef UWVM_CPP_EXCEPTIONS
                                                                    catch(::fast_io::error)
                                                                    {
                                                                        // do nothing
                                                                    }
# endif
                                                                }
                                                                return ts;
                                                            }};
            auto const llvm_jit_materialize_runtime_log_line{
                []<typename... Args>(Args&&... args) constexpr noexcept
                {
                    // Avoid formatting costs entirely when runtime logging is disabled.
                    if(!::uwvm2::uwvm::io::enable_runtime_log) [[likely]] { return; }

                    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8runtime_log_output, u8"[llvm-jit-full] ", ::std::forward<Args>(args)...);
                }};
            constexpr auto llvm_jit_materialize_error{
                []<typename... Args>(Args&&... args) constexpr noexcept
                {
                    // Verbose materialization errors go to the normal log channel because they explain a fallback decision.
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                        u8"\n");
                }};

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(!publish_full_ready) { store_tiered_full_ready(rec, false); }
            else
# endif
            {
                rec.llvm_jit_ready = false;
            }
            // Clear previous publication state before stealing the new LLVM module; callers must not observe stale function pointers
            // while this materialization attempt is still in progress.
            rec.llvm_jit_local_entry_addresses.clear();
            rec.llvm_jit_local_raw_entry_addresses.clear();
            rec.llvm_jit_engine.reset();
            rec.llvm_jit_context_holder.reset();

            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr) [[unlikely]] { return false; }

            auto const local_func_count{runtime_module->local_defined_function_vec_storage.size()};
            if(local_func_count == 0uz)
            {
                // Empty modules have no native entry points to publish, but the full-JIT state is still considered ready.
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                if(publish_full_ready)
# endif
                {
                    rec.llvm_jit_ready = true;
                }
                return true;
            }

            auto& llvm_jit_module_storage{rec.llvm_jit_compiled.llvm_jit_module};
            if(!llvm_jit_module_storage.emitted || llvm_jit_module_storage.llvm_context_holder == nullptr || llvm_jit_module_storage.llvm_module == nullptr)
                [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT materialization missing emitted module state for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT native target initialization failed for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }

            // Move LLVM ownership out of the compiler result. From here onward the materializer owns the context/module until the
            // ExecutionEngine has taken or retained them.
            auto llvm_context_holder{::std::move(llvm_jit_module_storage.llvm_context_holder)};
            auto merged_module{::std::move(llvm_jit_module_storage.llvm_module)};
            if(llvm_context_holder == nullptr || merged_module == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT materialization lost module/context ownership for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }

            auto const host_cpu_name{::llvm::sys::getHostCPUName()};
            auto const host_target_attribute_storage{get_llvm_jit_host_target_attribute_storage()};
            ::llvm::SmallVector<::llvm::StringRef, 16> host_target_attributes{};
            append_llvm_jit_host_target_attribute_refs(host_target_attribute_storage, host_target_attributes);

            auto const full_materialize_strategy{resolve_runtime_llvm_jit_full_materialize_strategy(default_codegen_opt_level)};
            auto const codegen_opt_level{full_materialize_strategy.codegen_opt_level};
            // Full materialization keeps optimization policy and codegen level together because some policies intentionally select a
            // lighter codegen level to reduce compile latency.

            // The target machine drives both optimization cost modeling and final object emission, so set module triple/layout before
            // running any optimization pipeline.
            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(codegen_opt_level).setMCPU(host_cpu_name).setMAttrs(host_target_attributes);

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{target_builder.selectTarget()};
            if(target_machine == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT target selection failed for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }
            if(codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target_machine->setFastISel(true); }

            merged_module->setTargetTriple(target_machine->getTargetTriple());
            merged_module->setDataLayout(target_machine->createDataLayout());
            auto llvm_jit_cache_key{::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"full-module")};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module", rec.module_name);
            auto full_module_wasm_hash{runtime_llvm_jit_full_module_cache_fingerprint(*runtime_module)};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                llvm_jit_cache_key, u8"module-wasm-hash", full_module_wasm_hash);
            auto llvm_jit_cache_codegen_policy{::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"codegen-policy")};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy, u8"cache-unit", u8"full");
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                llvm_jit_cache_codegen_policy, u8"pipeline", get_runtime_llvm_jit_full_pipeline_name(full_materialize_strategy.pipeline));
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                llvm_jit_cache_codegen_policy, u8"codegen-opt-level", get_runtime_llvm_jit_codegen_opt_level_name(codegen_opt_level));
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                llvm_jit_cache_codegen_policy, u8"policy", full_materialize_strategy.policy_name);
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                llvm_jit_cache_codegen_policy, u8"call-stack", get_runtime_llvm_jit_call_stack_mode_name());
            auto llvm_jit_cache_context{::uwvm2::runtime::llvm_jit_cache::default_cache_context(
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_key.data(), llvm_jit_cache_key.size()},
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_codegen_policy.data(), llvm_jit_cache_codegen_policy.size()},
                *target_machine)};
            llvm_jit_cache_context.cache_key_is_complete = true;
            auto const llvm_jit_cache_policy{::uwvm2::runtime::llvm_jit_cache::default_cache_policy()};

            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> parallel_object_outputs{};
            ::std::size_t parallel_object_defined_function_count{};
            bool use_parallel_objects{};
            if(extra_materialize_threads != 0uz)
            {
                ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> parallel_function_names{};
                collect_runtime_llvm_jit_parallel_object_function_names(*merged_module, parallel_function_names);
                parallel_object_defined_function_count = parallel_function_names.size();
                auto const parallel_object_task_count{
                    runtime_llvm_jit_parallel_object_task_count(parallel_object_defined_function_count, extra_materialize_threads)};

                if(load_runtime_llvm_jit_parallel_object_cache(llvm_jit_cache_context,
                                                               llvm_jit_cache_policy,
                                                               parallel_object_task_count,
                                                               parallel_object_outputs))
                {
                    use_parallel_objects = true;
                    ::std::size_t object_bytes{};
                    for(auto const& object_output: parallel_object_outputs) { object_bytes += object_output.size(); }
                    llvm_jit_materialize_runtime_log_line(u8"object-cache-hit module=\"",
                                                          rec.module_name,
                                                          u8"\" functions=",
                                                          parallel_object_defined_function_count,
                                                          u8" objects=",
                                                          parallel_object_outputs.size(),
                                                          u8" bytes=",
                                                          object_bytes);
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                            u8"[info]  ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"LLVM JIT full cache hit for module \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                            rec.module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\".",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                            u8"\n");
                    }
                }
            }

            if(use_parallel_objects)
            {
                llvm_jit_materialize_runtime_log_line(u8"optimize-skip module=\"", rec.module_name, u8"\" reason=object-cache-hit");
            }
            else
            {
                auto const optimize_start_time{llvm_jit_materialize_runtime_log_now()};
                llvm_jit_materialize_runtime_log_line(u8"optimize-start module=\"",
                                                      rec.module_name,
                                                      u8"\" policy=",
                                                      full_materialize_strategy.policy_name,
                                                      u8" pipeline=",
                                                      get_runtime_llvm_jit_full_pipeline_name(full_materialize_strategy.pipeline),
                                                      u8" codegen_opt=",
                                                      get_runtime_llvm_jit_codegen_opt_level_name(codegen_opt_level),
                                                      u8" call_stack=",
                                                      get_runtime_llvm_jit_call_stack_mode_name(),
                                                      u8" unwind_backend=",
                                                      runtime_llvm_jit_unwind_backend_name(),
                                                      u8" unwind_check=",
                                                      runtime_llvm_jit_unwind_check_requested()
                                                          ? ::uwvm2::utils::container::u8string_view{u8"live"}
                                                          : (runtime_llvm_jit_unwind_call_stack_requested() ? ::uwvm2::utils::container::u8string_view{u8"static"}
                                                                                                            : ::uwvm2::utils::container::u8string_view{u8"off"}),
                                                      u8" unwind_replace_frames=",
                                                      runtime_llvm_jit_unwind_can_replace_instruction_frames() ? ::uwvm2::utils::container::u8string_view{u8"yes"}
                                                                                                               : ::uwvm2::utils::container::u8string_view{u8"no"},
                                                      u8" call_stack_frames=",
                                                      runtime_llvm_jit_uses_instruction_call_stack_frames() ? u8"emit" : u8"omit");
                if(!optimize_runtime_llvm_jit_module(*merged_module,
                                                     *target_machine,
                                                     full_materialize_strategy.pipeline,
                                                     codegen_opt_level,
                                                     !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_disable_ir_verifaction,
                                                     rec.llvm_jit_compiled.llvm_jit_task_modules_pre_link_optimized &&
                                                         full_materialize_strategy.pipeline == runtime_llvm_jit_full_pipeline_kind::legacy_light)) [[unlikely]]
                {
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        llvm_jit_materialize_error(u8"LLVM JIT module optimization/verification failed for module=\"", rec.module_name, u8"\".");
                    }
                    return false;
                }
                llvm_jit_materialize_runtime_log_line(u8"optimize-end module=\"",
                                                      rec.module_name,
                                                      u8"\" time=",
                                                      llvm_jit_materialize_runtime_log_now() - optimize_start_time);
            }

            if(extra_materialize_threads != 0uz && !use_parallel_objects)
            {
                // Try a partitioned object-emission path for large modules. On failure, fall back to MCJIT's normal single-module
                // emission rather than failing the entire materialization.
                auto const object_emit_start_time{llvm_jit_materialize_runtime_log_now()};
                llvm_jit_materialize_runtime_log_line(u8"object-emit-start module=\"", rec.module_name, u8"\" extra_threads=", extra_materialize_threads);
                auto const object_emit_result{emit_runtime_llvm_jit_objects_parallel(
                    *merged_module,
                    ::uwvm2::utils::container::u8string{
                        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_uwvm_u8string_view(host_cpu_name)},
                    host_target_attribute_storage,
                    codegen_opt_level,
                    !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_disable_ir_verifaction,
                    extra_materialize_threads,
                    parallel_object_outputs,
                    parallel_object_defined_function_count)};
                if(object_emit_result == runtime_llvm_jit_parallel_object_emit_result::success)
                {
                    use_parallel_objects = true;
                    ::std::size_t object_bytes{};
                    for(auto const& object_output: parallel_object_outputs) { object_bytes += object_output.size(); }
                    store_runtime_llvm_jit_parallel_object_cache(llvm_jit_cache_context, llvm_jit_cache_policy, parallel_object_outputs);
                    llvm_jit_materialize_runtime_log_line(u8"object-emit-end module=\"",
                                                          rec.module_name,
                                                          u8"\" functions=",
                                                          parallel_object_defined_function_count,
                                                          u8" objects=",
                                                          parallel_object_outputs.size(),
                                                          u8" bytes=",
                                                          object_bytes,
                                                          u8" time=",
                                                          llvm_jit_materialize_runtime_log_now() - object_emit_start_time);
                }
                else if(object_emit_result == runtime_llvm_jit_parallel_object_emit_result::failed)
                {
                    llvm_jit_materialize_runtime_log_line(u8"object-emit-fallback module=\"",
                                                          rec.module_name,
                                                          u8"\" reason=emit-failed time=",
                                                          llvm_jit_materialize_runtime_log_now() - object_emit_start_time);
                    parallel_object_outputs.clear();
                }
                else
                {
                    llvm_jit_materialize_runtime_log_line(u8"object-emit-skip module=\"",
                                                          rec.module_name,
                                                          u8"\" reason=not-applicable time=",
                                                          llvm_jit_materialize_runtime_log_now() - object_emit_start_time);
                }
            }

            ::llvm::ExecutionEngine* raw_engine{};
            if(use_parallel_objects)
            {
                // When objects were emitted externally, create an empty engine module that only hosts the MCJIT engine and object loader.
                auto engine_module{::uwvm2::utils::container::make_delete_owned<::llvm::Module>(merged_module->getName(), *llvm_context_holder)};
                if(engine_module == nullptr) [[unlikely]]
                {
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        llvm_jit_materialize_error(u8"LLVM JIT engine module creation failed for module=\"", rec.module_name, u8"\".");
                    }
                    return false;
                }
                engine_module->setTargetTriple(target_machine->getTargetTriple());
                engine_module->setDataLayout(target_machine->createDataLayout());
                merged_module.reset();
                raw_engine = ::llvm::EngineBuilder(llvm_module_owner_t{engine_module.release()})
                                 .setEngineKind(::llvm::EngineKind::JIT)
                                 .setOptLevel(codegen_opt_level)
                                 .setMCPU(host_cpu_name)
                                 .setMAttrs(host_target_attributes)
                                 .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{
                                     ::uwvm2::utils::container::make_delete_owned<
                                         ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()
                                         .release()})
                                 .create(target_machine.get());
            }
            else
            {
                // Normal path: hand the optimized LLVM module directly to MCJIT.
                raw_engine = ::llvm::EngineBuilder(llvm_module_owner_t{merged_module.release()})
                                 .setEngineKind(::llvm::EngineKind::JIT)
                                 .setOptLevel(codegen_opt_level)
                                 .setMCPU(host_cpu_name)
                                 .setMAttrs(host_target_attributes)
                                 .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{
                                     ::uwvm2::utils::container::make_delete_owned<
                                         ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()
                                         .release()})
                                 .create(target_machine.get());
            }
            if(raw_engine == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT engine creation failed for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }
            static_cast<void>(target_machine.release());
            // From this point the ExecutionEngine owns the target machine and all executable allocations are tracked by rec.

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> llvm_jit_engine{raw_engine};
            ::uwvm2::runtime::llvm_jit_cache::llvm_jit_object_cache llvm_jit_object_cache{
                ::std::move(llvm_jit_cache_context), llvm_jit_cache_policy};
            if(!use_parallel_objects) { llvm_jit_engine->setObjectCache(::std::addressof(llvm_jit_object_cache)); }
            if(runtime_llvm_jit_unwind_call_stack_requested())
            {
                // Preserve non-executable metadata sections for the debug listener/fallback object copy.  Optimized inline
                // frames live in DWARF, not in the native unwind table itself.
                llvm_jit_engine->setProcessAllSections(true);
                llvm_jit_engine->RegisterJITEventListener(::std::addressof(get_uwvm_llvm_jit_debug_listener()));
            }
            if(use_parallel_objects)
            {
                // Feed each externally emitted object back into the MCJIT engine so symbol lookup and debug listeners work normally.
                auto const object_load_start_time{llvm_jit_materialize_runtime_log_now()};
                llvm_jit_materialize_runtime_log_line(u8"object-load-start module=\"", rec.module_name, u8"\" objects=", parallel_object_outputs.size());
                for(::std::size_t object_index{}; object_index != parallel_object_outputs.size(); ++object_index)
                {
                    auto const& object_output{parallel_object_outputs.index_unchecked(object_index)};
                    auto object_buffer{::llvm::MemoryBuffer::getMemBufferCopy(
                        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_llvm_string_ref(object_output),
                        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_llvm_string_ref(u8"uwvm2-llvm-jit-object"))};
                    if(object_buffer == nullptr) [[unlikely]]
                    {
                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            llvm_jit_materialize_error(u8"LLVM JIT object buffer creation failed for module=\"", rec.module_name, u8"\".");
                        }
                        return false;
                    }

                    auto object_file_expected{::llvm::object::ObjectFile::createObjectFile(object_buffer->getMemBufferRef())};
                    if(!object_file_expected) [[unlikely]]
                    {
                        ::llvm::consumeError(object_file_expected.takeError());
                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            llvm_jit_materialize_error(u8"LLVM JIT object parse failed for module=\"", rec.module_name, u8"\".");
                        }
                        return false;
                    }

                    llvm_jit_engine->addObjectFile(
                        ::llvm::object::OwningBinary<::llvm::object::ObjectFile>{::std::move(*object_file_expected), ::std::move(object_buffer)});
                    if(llvm_jit_engine->hasError()) [[unlikely]]
                    {
                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            llvm_jit_materialize_error(u8"LLVM JIT object load failed for module=\"",
                                                       rec.module_name,
                                                       u8"\": ",
                                                       ::fast_io::mnp::code_cvt(llvm_jit_engine->getErrorMessage()));
                        }
                        return false;
                    }
                }
                llvm_jit_materialize_runtime_log_line(u8"object-load-end module=\"",
                                                      rec.module_name,
                                                      u8"\" time=",
                                                      llvm_jit_materialize_runtime_log_now() - object_load_start_time);
            }
            auto const finalize_start_time{llvm_jit_materialize_runtime_log_now()};
            llvm_jit_materialize_runtime_log_line(u8"finalize-object-start module=\"", rec.module_name, u8"\"");
            // finalizeObject performs relocation, memory permission changes, and JIT event notifications.
            llvm_jit_engine->finalizeObject();
            if(!use_parallel_objects) { llvm_jit_engine->setObjectCache(nullptr); }
            llvm_jit_materialize_runtime_log_line(u8"finalize-object-end module=\"",
                                                  rec.module_name,
                                                  u8"\" time=",
                                                  llvm_jit_materialize_runtime_log_now() - finalize_start_time);

            auto const materialized_module_id{find_runtime_module_id_from_storage_ptr(runtime_module)};
            auto const import_func_count{runtime_module->imported_function_vec_storage.size()};
            rec.llvm_jit_local_entry_addresses.resize(local_func_count);
            rec.llvm_jit_local_raw_entry_addresses.resize(local_func_count);
            for(::std::size_t local_index{}; local_index != local_func_count; ++local_index)
            {
                // Resolve and publish both typed wasm entries and raw buffer entries for every local function.
                auto const function_index{import_func_count + local_index};
                auto const resolve_function_address{
                    [&](::uwvm2::utils::container::u8string const& function_name) constexpr noexcept -> ::std::uintptr_t
                    {
                        namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
                        llvm_jit_function_address_name_t function_address_name{function_name.data(), function_name.data() + function_name.size()};
                        auto const direct_function_address{llvm_jit_engine->getFunctionAddress(function_address_name)};
                        if(direct_function_address != 0u) [[likely]] { return static_cast<::std::uintptr_t>(direct_function_address); }

                        auto found_function{llvm_jit_engine->FindFunctionNamed(llvm_jit_translate_details::get_llvm_string_ref(function_name))};
                        if(found_function != nullptr && !found_function->isDeclaration()) [[likely]]
                        {
                            auto const function_address{llvm_jit_engine->getPointerToFunction(found_function)};
                            if(function_address != nullptr) [[likely]] { return reinterpret_cast<::std::uintptr_t>(function_address); }
                        }

                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            ::uwvm2::utils::container::u8string function_type_text{};
                            if(found_function != nullptr)
                            {
                                llvm_jit_translate_details::raw_uwvm_string_ostream function_type_stream(function_type_text);
                                found_function->getFunctionType()->print(function_type_stream);
                            }
                            llvm_jit_materialize_error(u8"LLVM JIT could not resolve function address for module=\"",
                                                       rec.module_name,
                                                       u8"\", function=\"",
                                                       ::fast_io::mnp::code_cvt(function_name),
                                                       u8"\", found=",
                                                       found_function != nullptr,
                                                       u8", declaration=",
                                                       found_function != nullptr && found_function->isDeclaration(),
                                                       u8", linkage=",
                                                       found_function != nullptr ? static_cast<unsigned>(found_function->getLinkage())
                                                                                 : static_cast<unsigned>(::llvm::GlobalValue::ExternalLinkage),
                                                       u8", type=",
                                                       ::fast_io::mnp::code_cvt(function_type_text),
                                                       u8".");
                        }
                        return 0u;
                    }};

                auto const function_name{get_runtime_llvm_jit_wasm_function_name(*runtime_module, function_index)};
                auto const function_address{resolve_function_address(function_name)};
                if(function_address == 0u) [[unlikely]] { return false; }
                rec.llvm_jit_local_entry_addresses.index_unchecked(local_index) = function_address;

                auto const raw_function_name{get_runtime_llvm_jit_wasm_raw_function_name(*runtime_module, function_index)};
                auto const raw_function_address{resolve_function_address(raw_function_name)};
                if(raw_function_address == 0u) [[unlikely]] { return false; }
                rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local_index) = raw_function_address;

                if(materialized_module_id != ::std::numeric_limits<::std::size_t>::max())
                {
                    record_llvm_jit_unwind_entry(materialized_module_id, function_index, function_address, false);
                    record_llvm_jit_unwind_entry(materialized_module_id, function_index, raw_function_address, true);
                }
            }

            rec.llvm_jit_context_holder = ::std::move(llvm_context_holder);
            rec.llvm_jit_engine = ::std::move(llvm_jit_engine);
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(publish_full_ready)
# endif
            {
                rec.llvm_jit_ready = true;
            }
            bool publish_materialized_indirect_targets{true};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            publish_materialized_indirect_targets = publish_full_ready;
# endif
            if(publish_materialized_indirect_targets && materialized_module_id != ::std::numeric_limits<::std::size_t>::max() &&
               materialized_module_id < g_runtime.defined_func_cache.size())
            {
                auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(materialized_module_id)};
                for(::std::size_t local_index{}; local_index != local_func_count && local_index < mod_cache.size(); ++local_index)
                {
                    auto const raw_entry_address{rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local_index)};
                    auto const typed_entry_address{rec.llvm_jit_local_entry_addresses.index_unchecked(local_index)};
                    publish_llvm_jit_call_indirect_defined_entry_targets(::std::addressof(mod_cache.index_unchecked(local_index)),
                                                                         raw_entry_address,
                                                                         typed_entry_address);
                }
            }
            return true;
        }

        [[nodiscard, maybe_unused]] inline constexpr bool try_invoke_runtime_llvm_jit_defined_entry(::std::size_t module_id,
                                                                                                    ::std::size_t function_index) noexcept
        {
            // Enter generated code through the raw wrapper so the C++ boundary keeps the host ABI even when typed Wasm bodies use a private ABI.
            return try_invoke_runtime_llvm_jit_raw_defined_entry(module_id, function_index, nullptr, 0uz, nullptr, 0uz);
        }

#else
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_llvm_jit_translation() noexcept { return false; }

        [[nodiscard, maybe_unused]] inline constexpr bool runtime_compiler_requires_llvm_jit_execution() noexcept { return false; }
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // =========================================================================
        // Interpreter callback bridges
        // -------------------------------------------------------------------------
        // Translated opfunc streams call these callbacks for traps, direct calls, and
        // call_indirect. The callbacks are process-global optable entries, so they
        // must be initialized before any interpreter body executes and refreshed when
        // tiered mode changes which call boundary should be used.
        //
        // Coverage invariants:
        // - Trap callbacks terminate through the shared fatal-reporting path.
        // - Direct-call bridges accept either a compact call-info pointer or a module/function pair.
        // - call_indirect bridges perform table, null, and signature checks before dispatch.
        // - Inline caches are thread-local because table state and trap context are thread-sensitive.
        // - Tiered bridges keep validation behavior identical while adding optional generated-entry dispatch.
        // =========================================================================

        // These callbacks are entered from uwvm-int opfuncs. Keep their ABI exactly synchronized with
        // UWVM_INTERPRETER_OPFUNC_TYPE_MACRO, otherwise Windows x86_64 SysV opfuncs will corrupt runtime callback calls.
        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void unreachable_trap() noexcept { trap_fatal(trap_kind::unreachable); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void trap_invalid_conversion_to_integer() noexcept
        { trap_fatal(trap_kind::invalid_conversion_to_integer); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void trap_integer_divide_by_zero() noexcept
        { trap_fatal(trap_kind::integer_divide_by_zero); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void trap_integer_overflow() noexcept { trap_fatal(trap_kind::integer_overflow); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void
            trap_memory_out_of_bounds(::uwvm2::object::memory::error::memory_error_t const& memerr) noexcept
        { print_memory_out_of_bounds_trap(memerr); }

        template <bool TryTieredJit, typename... Args>
        UWVM_ALWAYS_INLINE inline constexpr void execute_defined_for_bridge(Args&&... args) noexcept
        {
            // The bridge template keeps the normal and tiered interpreter callbacks ABI-identical while allowing the tiered build to
            // try a generated entry at each call boundary.
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if constexpr(TryTieredJit) { execute_defined_with_tiered_jit(static_cast<Args&&>(args)...); }
            else
# endif
            {
                execute_defined_with_optional_tiered_jit(static_cast<Args&&>(args)...);
            }
        }

        template <bool TryTieredJit>
        inline constexpr void call_bridge_impl(::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
            // Compiled interpreter opfuncs call this for direct wasm calls. The bridge handles both compact pre-resolved call-info
            // pointers and module/function indices so generated opfuncs can choose the cheapest encoding for each call site.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(!g_runtime.compiled_all.load(::std::memory_order_acquire)) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
            if(wasm_module_id == SIZE_MAX) [[likely]]
            {
                // SIZE_MAX marks the translator's fast path: func_index is already a compiled_defined_call_info pointer.
                using call_info_t = ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info;
                auto const info{reinterpret_cast<call_info_t const*>(func_index)};
                if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const rf{static_cast<runtime_local_func_storage_t const*>(info->runtime_func)};
                auto const cf{info->compiled_func};
                if(rf == nullptr || cf == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                if(try_execute_trivial_defined_call(*info, stack_top_ptr)) { return; }

                auto& call_stack{get_call_stack()};
                call_stack_guard g{call_stack, info->module_id, info->function_index};
                execute_defined_for_bridge<TryTieredJit>(call_stack, *info, stack_top_ptr);
                return;
            }

            auto& call_stack{get_call_stack()};

            if(wasm_module_id >= g_runtime.modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_runtime.modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};

            auto const import_n{module.imported_function_vec_storage.size()};
            auto const local_n{module.local_defined_function_vec_storage.size()};
            if(func_index >= import_n + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }

            if(func_index < import_n)
            {
                // Import call sites are resolved through a cache built at runtime initialization, avoiding repeated alias chasing and
                // giving native/preload imports the caller module context they need.
                if(wasm_module_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const& cache{g_import_call_cache.index_unchecked(wasm_module_id)};
                if(func_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const& tgt{cache.index_unchecked(func_index)};
                call_stack_guard g{call_stack, tgt.frame.module_id, tgt.frame.function_index};

                switch(tgt.k)
                {
                    case cached_import_target::kind::defined:
                    {
                        execute_defined_for_bridge<TryTieredJit>(call_stack,
                                                                 tgt.frame.module_id,
                                                                 tgt.frame.function_index,
                                                                 tgt.u.defined.runtime_func,
                                                                 tgt.u.defined.compiled_func,
                                                                 tgt.param_bytes,
                                                                 tgt.result_bytes,
                                                                 stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::local_imported:
                    {
                        invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::dl:
                    {
                        invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    case cached_import_target::kind::weak_symbol:
                    {
                        invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                        return;
                    }
                    [[unlikely]] default:
                    {
                        ::fast_io::fast_terminate();
                    }
                }
            }

            auto const local_index{func_index - import_n};
            auto const lf{::std::addressof(module.local_defined_function_vec_storage.index_unchecked(local_index))};
            call_stack_guard g{call_stack, wasm_module_id, func_index};
            if(wasm_module_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(wasm_module_id)};
            if(local_index >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& info{mod_cache.index_unchecked(local_index)};
            if(info.runtime_func != lf) [[unlikely]] { ::fast_io::fast_terminate(); }
            execute_defined_for_bridge<TryTieredJit>(call_stack, info, stack_top_ptr);
        }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void
            call_bridge(::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
            // Standard interpreter direct-call callback; it can still use ready LLVM entries when the generic optional path allows it.
            call_bridge_impl<false>(wasm_module_id, func_index, stack_top_ptr);
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void
            tiered_call_bridge(::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
            // Tier-aware direct-call callback gives every interpreter call boundary a chance to promote into LLVM.
            call_bridge_impl<true>(wasm_module_id, func_index, stack_top_ptr);
        }
# endif

        template <bool TryTieredJit>
        inline constexpr void call_indirect_bridge_impl(::std::size_t wasm_module_id,
                                                        ::std::size_t type_index,
                                                        ::std::size_t table_index,
                                                        ::std::byte** stack_top_ptr) UWVM_THROWS
        {
            // call_indirect must enforce wasm table bounds, null-element, and signature rules even when the eventual target is native
            // or tiered JIT code. This bridge centralizes those checks for all interpreter-generated indirect call sites.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(!g_runtime.compiled_all.load(::std::memory_order_acquire)) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
            if(wasm_module_id >= g_runtime.modules.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& module_rec{g_runtime.modules.index_unchecked(wasm_module_id)};
            auto const& module{*module_rec.runtime_module};
            auto& call_stack{get_call_stack()};

            // Pop selector index (i32).
            wasm_i32 selector_i32;  // no init
            *stack_top_ptr -= sizeof(selector_i32);
            ::std::memcpy(::std::addressof(selector_i32), *stack_top_ptr, sizeof(selector_i32));
            auto const selector_u32{::std::bit_cast<::std::uint_least32_t>(selector_i32)};

            auto const table{resolve_table(module, table_index)};
            if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(selector_u32 >= table->elems.size()) [[unlikely]] { trap_fatal(trap_kind::call_indirect_table_out_of_bounds); }

            auto const& elem{table->elems.index_unchecked(static_cast<::std::size_t>(selector_u32))};

            // The expected signature is taken from the caller module's type section, as required by the wasm call_indirect operand.
            auto const type_begin{module.type_section_storage.type_section_begin};
            auto const type_end{module.type_section_storage.type_section_end};
            if(type_begin == nullptr || type_end == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const type_total{static_cast<::std::size_t>(type_end - type_begin)};
            if(type_index >= type_total) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const expected_ft_ptr{type_begin + type_index};
            if(expected_ft_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Inline cache: the common hot case is a stable (table, selector, expected-type) triple inside a loop.
            // Cache the resolved call target to avoid repeating signature checks, pointer arithmetic and cache lookups.
            void const* const elems_data{table->elems.data()};
            auto const cache_index{static_cast<::std::size_t>(selector_u32) & (call_stack_tls_state::kCallIndirectCacheEntries - 1uz)};
            {
                auto& ic{call_stack.call_indirect_cache[cache_index]};
                if(ic.table == table && ic.elems_data == elems_data && ic.selector == selector_u32 && ic.expected_ft_ptr == expected_ft_ptr &&
                   ic.elem_type == elem.type && ic.target_ptr != nullptr)
                {
                    if(elem.type == ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined)
                    {
                        auto const def_ptr{elem.storage.defined_ptr};
                        if(def_ptr != nullptr && ic.target_ptr == static_cast<void const*>(def_ptr) && ic.defined_info != nullptr)
                        {
                            auto const& info{*ic.defined_info};
                            if(try_execute_trivial_defined_call(info, stack_top_ptr)) { return; }
                            call_stack_guard g{call_stack, info.module_id, info.function_index};
                            auto const rf{static_cast<runtime_local_func_storage_t const*>(info.runtime_func)};
                            if(rf == nullptr || info.compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                            execute_defined_for_bridge<TryTieredJit>(call_stack, info, stack_top_ptr);
                            return;
                        }
                    }
                    else if(elem.type == ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported)
                    {
                        auto const imp_ptr{elem.storage.imported_ptr};
                        if(imp_ptr != nullptr && ic.target_ptr == static_cast<void const*>(imp_ptr) && ic.imported_tgt != nullptr)
                        {
                            auto const& tgt{*ic.imported_tgt};
                            call_stack_guard g{call_stack, tgt.frame.module_id, tgt.frame.function_index};
                            switch(tgt.k)
                            {
                                case cached_import_target::kind::defined:
                                {
                                    execute_defined_for_bridge<TryTieredJit>(call_stack,
                                                                             tgt.frame.module_id,
                                                                             tgt.frame.function_index,
                                                                             tgt.u.defined.runtime_func,
                                                                             tgt.u.defined.compiled_func,
                                                                             tgt.param_bytes,
                                                                             tgt.result_bytes,
                                                                             stack_top_ptr);
                                    return;
                                }
                                case cached_import_target::kind::local_imported:
                                {
                                    invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                                    return;
                                }
                                case cached_import_target::kind::dl:
                                case cached_import_target::kind::weak_symbol:
                                {
                                    invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                                    return;
                                }
                                [[unlikely]] default:
                                {
                                    ::fast_io::fast_terminate();
                                }
                            }
                        }
                    }
                }
            }

            // Fast signature check (miss path): compare canonical type indices (deduplicated by signature) to avoid scanning long param lists.
            auto const& canon{module_rec.type_canon_index};
            bool const canon_ok{canon.size() == type_total};
            auto const expected_canon{canon_ok ? canon.index_unchecked(type_index) : type_index};
            auto const type_begin_addr{reinterpret_cast<::std::uintptr_t>(type_begin)};
            auto const type_end_addr{reinterpret_cast<::std::uintptr_t>(type_end)};
            constexpr ::std::size_t kFTSize{sizeof(*type_begin)};

            auto const sig_from_ft{[](::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* ft) constexpr noexcept -> func_sig_view
                                   {
                                       return {
                                           {valtype_kind::wasm_enum, ft->parameter.begin, static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin)},
                                           {valtype_kind::wasm_enum, ft->result.begin,    static_cast<::std::size_t>(ft->result.end - ft->result.begin)      }
                                       };
                                   }};

            switch(elem.type)
            {
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined:
                {
                    auto const def_ptr{elem.storage.defined_ptr};
                    if(def_ptr == nullptr) [[unlikely]] { trap_fatal(trap_kind::call_indirect_null_element); }

                    auto const actual_ft_ptr{def_ptr->function_type_ptr};
                    if(actual_ft_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    if(actual_ft_ptr != expected_ft_ptr)
                    {
                        bool match{};
                        if(canon_ok)
                        {
                            auto const actual_addr{reinterpret_cast<::std::uintptr_t>(actual_ft_ptr)};
                            if(actual_addr >= type_begin_addr && actual_addr < type_end_addr)
                            {
                                auto const diff{static_cast<::std::size_t>(actual_addr - type_begin_addr)};
                                if(diff % kFTSize == 0uz)
                                {
                                    auto const actual_index{diff / kFTSize};
                                    match = (canon.index_unchecked(actual_index) == expected_canon);
                                }
                            }
                        }

                        if(!match)
                        {
                            auto const expected_sig{sig_from_ft(expected_ft_ptr)};
                            if(!func_sig_equal(expected_sig, sig_from_ft(actual_ft_ptr))) [[unlikely]]
                            {
                                auto const base{module.local_defined_function_vec_storage.data()};
                                auto const local_n{module.local_defined_function_vec_storage.size()};
                                if(base != nullptr && def_ptr >= base && def_ptr < base + local_n)
                                {
                                    auto& suppressed_frame{get_suppressed_call_stack_frame()};
                                    suppressed_frame.module_id = wasm_module_id;
                                    suppressed_frame.function_index = module.imported_function_vec_storage.size() + static_cast<::std::size_t>(def_ptr - base);
                                }
                                trap_fatal(trap_kind::call_indirect_type_mismatch);
                            }
                        }
                    }

                    auto const base{module.local_defined_function_vec_storage.data()};
                    auto const local_n{module.local_defined_function_vec_storage.size()};
                    if(base == nullptr || def_ptr < base || def_ptr >= base + local_n) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const local_idx{static_cast<::std::size_t>(def_ptr - base)};

                    // Resolve back through the defined-function cache so the interpreter, lazy compiler, and tiered JIT share one
                    // metadata authority for ABI sizes and backend entry points.
                    if(wasm_module_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(wasm_module_id)};
                    if(local_idx >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& info{mod_cache.index_unchecked(local_idx)};
                    if(info.runtime_func != def_ptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    // Update inline cache.
                    {
                        auto& ic{call_stack.call_indirect_cache[cache_index]};
                        ic.table = table;
                        ic.elems_data = table->elems.data();
                        ic.selector = selector_u32;
                        ic.expected_ft_ptr = expected_ft_ptr;
                        ic.elem_type = elem.type;
                        ic.target_ptr = static_cast<void const*>(def_ptr);
                        ic.defined_info = info.compiled_call_info;
                        ic.imported_tgt = nullptr;
                    }

                    if(try_execute_trivial_defined_call(info, stack_top_ptr)) { return; }

                    call_stack_guard g{call_stack, info.module_id, info.function_index};
                    auto const rf{static_cast<runtime_local_func_storage_t const*>(info.runtime_func)};
                    if(rf == nullptr || info.compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    execute_defined_for_bridge<TryTieredJit>(call_stack, info, stack_top_ptr);
                    return;
                }
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported:
                {
                    auto const imp_ptr{elem.storage.imported_ptr};
                    if(imp_ptr == nullptr) [[unlikely]] { trap_fatal(trap_kind::call_indirect_null_element); }

                    auto const import_type_ptr{imp_ptr->import_type_ptr};
                    if(import_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const actual_ft_ptr{import_type_ptr->imports.storage.function};
                    if(actual_ft_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    if(actual_ft_ptr != expected_ft_ptr)
                    {
                        bool match{};
                        if(canon_ok)
                        {
                            auto const actual_addr{reinterpret_cast<::std::uintptr_t>(actual_ft_ptr)};
                            if(actual_addr >= type_begin_addr && actual_addr < type_end_addr)
                            {
                                auto const diff{static_cast<::std::size_t>(actual_addr - type_begin_addr)};
                                if(diff % kFTSize == 0uz)
                                {
                                    auto const actual_index{diff / kFTSize};
                                    match = (canon.index_unchecked(actual_index) == expected_canon);
                                }
                            }
                        }

                        if(!match)
                        {
                            auto const expected_sig{sig_from_ft(expected_ft_ptr)};
                            if(!func_sig_equal(expected_sig, sig_from_ft(actual_ft_ptr))) [[unlikely]]
                            {
                                auto const base{module.imported_function_vec_storage.data()};
                                auto const imp_n{module.imported_function_vec_storage.size()};
                                if(base != nullptr && imp_ptr >= base && imp_ptr < base + imp_n)
                                {
                                    auto& suppressed_frame{get_suppressed_call_stack_frame()};
                                    suppressed_frame.module_id = wasm_module_id;
                                    suppressed_frame.function_index = static_cast<::std::size_t>(imp_ptr - base);
                                }
                                trap_fatal(trap_kind::call_indirect_type_mismatch);
                            }
                        }
                    }

                    auto const base{module.imported_function_vec_storage.data()};
                    auto const imp_n{module.imported_function_vec_storage.size()};
                    if(base == nullptr || imp_ptr < base || imp_ptr >= base + imp_n) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const idx{static_cast<::std::size_t>(imp_ptr - base)};

                    // Imported table elements intentionally reuse the import cache; it already stores the flattened target and the
                    // correct diagnostic frame identity.
                    if(wasm_module_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& cache{g_import_call_cache.index_unchecked(wasm_module_id)};
                    if(idx >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& tgt{cache.index_unchecked(idx)};

                    // Update inline cache.
                    {
                        auto& ic{call_stack.call_indirect_cache[cache_index]};
                        ic.table = table;
                        ic.elems_data = table->elems.data();
                        ic.selector = selector_u32;
                        ic.expected_ft_ptr = expected_ft_ptr;
                        ic.elem_type = elem.type;
                        ic.target_ptr = static_cast<void const*>(imp_ptr);
                        ic.defined_info = nullptr;
                        ic.imported_tgt = ::std::addressof(tgt);
                    }

                    call_stack_guard g{call_stack, tgt.frame.module_id, tgt.frame.function_index};
                    switch(tgt.k)
                    {
                        case cached_import_target::kind::defined:
                        {
                            execute_defined_for_bridge<TryTieredJit>(call_stack,
                                                                     tgt.frame.module_id,
                                                                     tgt.frame.function_index,
                                                                     tgt.u.defined.runtime_func,
                                                                     tgt.u.defined.compiled_func,
                                                                     tgt.param_bytes,
                                                                     tgt.result_bytes,
                                                                     stack_top_ptr);
                            return;
                        }
                        case cached_import_target::kind::local_imported:
                        {
                            invoke_local_imported(tgt.u.local_imported, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                            return;
                        }
                        case cached_import_target::kind::dl:
                        case cached_import_target::kind::weak_symbol:
                        {
                            invoke_capi(tgt.u.capi_ptr, tgt.preload_module_memory_attribute, tgt.param_bytes, tgt.result_bytes, stack_top_ptr);
                            return;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void
            call_indirect_bridge(::std::size_t wasm_module_id, ::std::size_t type_index, ::std::size_t table_index, ::std::byte** stack_top_ptr) UWVM_THROWS
        {
            // Standard interpreter call_indirect callback with wasm table/type checks and optional backend dispatch.
            call_indirect_bridge_impl<false>(wasm_module_id, type_index, table_index, stack_top_ptr);
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void tiered_call_indirect_bridge(::std::size_t wasm_module_id,
                                                                                                       ::std::size_t type_index,
                                                                                                       ::std::size_t table_index,
                                                                                                       ::std::byte** stack_top_ptr) UWVM_THROWS
        {
            // Tier-aware indirect-call callback keeps wasm validation checks identical while allowing ready generated targets.
            call_indirect_bridge_impl<true>(wasm_module_id, type_index, table_index, stack_top_ptr);
        }
# endif

        inline constexpr void configure_interpreter_call_bridges_for_current_runtime() noexcept
        {
            // Bridge function pointers live in the interpreter optable and may be observed by already-compiled opfuncs. Reconfigure
            // them whenever the runtime mode changes so tiered T0 can switch call boundaries to the tier-aware callbacks.
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                   ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered &&
               tiered_t0_enabled())
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::call_func = tiered_call_bridge;
                ::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func = tiered_call_indirect_bridge;
                return;
            }
# endif
            ::uwvm2::runtime::compiler::uwvm_int::optable::call_func = call_bridge;
            ::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func = call_indirect_bridge;
        }

        inline constexpr void ensure_bridges_initialized() noexcept
        {
            // Trap and call callbacks are process-global optable entries. Initialize them once with release/acquire publication, then
            // still refresh the mode-dependent call bridges for tiered runtime transitions.
            if(g_runtime.bridges_initialized.load(::std::memory_order_acquire))
            {
                configure_interpreter_call_bridges_for_current_runtime();
                return;
            }

            static ::std::atomic_flag init_lock = ATOMIC_FLAG_INIT;
            while(init_lock.test_and_set(::std::memory_order_acquire))
            {
                if(g_runtime.bridges_initialized.load(::std::memory_order_acquire))
                {
                    configure_interpreter_call_bridges_for_current_runtime();
                    return;
                }
                ::fast_io::this_thread::yield();
            }

            if(!g_runtime.bridges_initialized.load(::std::memory_order_relaxed))
            {
                ::uwvm2::runtime::compiler::uwvm_int::optable::unreachable_func = unreachable_trap;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_invalid_conversion_to_integer_func = trap_invalid_conversion_to_integer;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_divide_by_zero_func = trap_integer_divide_by_zero;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_integer_overflow_func = trap_integer_overflow;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_memory_out_of_bounds_func = trap_memory_out_of_bounds;

# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                ::uwvm2::runtime::compiler::uwvm_int::optable::tiered_loop_osr_func = tiered_try_enter_loop_osr;
# endif

                g_runtime.bridges_initialized.store(true, ::std::memory_order_release);
            }

            configure_interpreter_call_bridges_for_current_runtime();
            init_lock.clear(::std::memory_order_release);
        }
#endif

        inline constexpr void compile_all_modules_if_needed(bool initialize_interpreter_bridges) noexcept
        {
            // =========================================================================
            // Eager compilation and runtime cache publication
            // -----------------------------------------------------------------------
            // This function is the eager-mode registry builder. It assigns module ids,
            // translates the selected backends, materializes optional LLVM engines, and
            // publishes every cache used later by direct calls, import calls, indirect
            // calls, host APIs, and trap reporting.
            //
            // Coverage invariants:
            // - The compile lock protects all global registries as one publication unit.
            // - Lazy schedulers are stopped before records are rebuilt.
            // - Defined-function caches must exist before import caches are flattened.
            // - LLVM call_indirect table views are populated only after entry addresses are available.
            // =========================================================================
            // Full compilation is a one-shot publication barrier for eager runtime execution. After this function returns, module
            // ids, defined-function caches, import caches, and optional JIT table views are all consistent with each other.
            ensure_memory_signal_trap_bridge_initialized();

#if !defined(UWVM_RUNTIME_HAS_BACKEND)
            static_cast<void>(initialize_interpreter_bridges);
            ::fast_io::fast_terminate();
#else
# if !defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            static_cast<void>(initialize_interpreter_bridges);
# endif
            auto const runtime_compiler{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler};
            auto const compile_uwvm_int_translation{runtime_compiler_requests_uwvm_int_translation()};
            auto const compile_llvm_jit_translation{runtime_compiler_requests_llvm_jit_translation()};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            // Interpreter-generated code stores callbacks in global optable slots, so those slots must be ready before any compiled
            // interpreter body can execute.
            if(initialize_interpreter_bridges && compile_uwvm_int_translation) { ensure_bridges_initialized(); }
# endif
            if(g_runtime.compiled_all.load(::std::memory_order_acquire)) { return; }

            // A lightweight spin lock is enough here because compilation is process-wide and callers only contend during startup or
            // explicit host-side reset/reentry.
            static ::std::atomic_flag compile_lock = ATOMIC_FLAG_INIT;
            while(compile_lock.test_and_set(::std::memory_order_acquire))
            {
                if(g_runtime.compiled_all.load(::std::memory_order_acquire)) { return; }
                ::fast_io::this_thread::yield();
            }

            if(g_runtime.compiled_all.load(::std::memory_order_relaxed))
            {
                compile_lock.clear(::std::memory_order_release);
                return;
            }

            ::fast_io::unix_timestamp start_time{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
# endif
            }

            [[maybe_unused]] constexpr auto runtime_compile_threads_warn{
                []<typename... Args>(Args&&... args) constexpr noexcept
                {
                    // Warn when user-specified compile-thread settings cannot be honored exactly on this build/runtime.
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        u8"[warn]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8" (runtime-compile-threads)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }};

            [[maybe_unused]] constexpr auto runtime_compile_threads_warn_to_fatal{
                []() constexpr noexcept
                {
                    // Escalate scheduling warnings into a hard failure when the user requested fatal runtime-thread diagnostics.
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(runtime-compile-threads)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }};

            constexpr auto runtime_compile_threads_verbose_info{
                []<typename... Args>(Args&&... args) constexpr noexcept
                {
                    // Verbose scheduling lines explain how global runtime flags become per-module compilation choices.
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }};

            auto const runtime_compile_threads_verbose_now{[]() constexpr noexcept
                                                           {
                                                               // Keep timing best-effort so verbose logging never changes compilation semantics.
                                                               ::fast_io::unix_timestamp ts{};
                                                               if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                                                               {
# ifdef UWVM_CPP_EXCEPTIONS
                                                                   try
# endif
                                                                   {
                                                                       ts = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                                                                   }
# ifdef UWVM_CPP_EXCEPTIONS
                                                                   catch(::fast_io::error)
                                                                   {
                                                                       // do nothing
                                                                   }
# endif
                                                               }
                                                               return ts;
                                                           }};

            auto const runtime_compile_threads_verbose_done{
                []<typename... Args>(::fast_io::unix_timestamp start_time, Args&&... args) constexpr noexcept
                {
                    // Finish logs reuse the same formatting path as setup logs so startup output stays consistent.
                    if(!::uwvm2::uwvm::io::show_verbose) [[likely]] { return; }

                    ::fast_io::unix_timestamp end_time{};
# ifdef UWVM_CPP_EXCEPTIONS
                    try
# endif
                    {
                        end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                    }
# ifdef UWVM_CPP_EXCEPTIONS
                    catch(::fast_io::error)
                    {
                        // do nothing
                    }
# endif

                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                        u8"[info]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        ::std::forward<Args>(args)...,
                                        u8" done. (time=",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        end_time - start_time,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"s). ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                                        u8"[",
                                        ::uwvm2::uwvm::io::get_local_realtime(),
                                        u8"] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(verbose)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                }};

            using runtime_compile_threads_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_policy_t;

            // Resolve compile-thread policy once before walking modules. Individual modules may still downshift to avoid spending more
            // scheduling overhead than their function/body count can amortize.
            auto effective_extra_compile_threads{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved};
            auto const runtime_compile_threads_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_policy};
            auto const default_runtime_compile_threads_policy_active{!::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed ||
                                                                     runtime_compile_threads_policy == runtime_compile_threads_policy_t::default_policy};
            auto const aggressive_runtime_compile_threads_policy_active{::uwvm2::uwvm::runtime::runtime_mode::runtime_compile_threads_existed &&
                                                                        runtime_compile_threads_policy == runtime_compile_threads_policy_t::aggressive};
            auto const adaptive_runtime_compile_threads_policy_active{default_runtime_compile_threads_policy_active ||
                                                                      aggressive_runtime_compile_threads_policy_active};
            ::uwvm2::utils::container::u8string_view const adaptive_runtime_compile_threads_policy_name{
                aggressive_runtime_compile_threads_policy_active ? ::uwvm2::utils::container::u8string_view{u8"aggressive"}
                                                                 : ::uwvm2::utils::container::u8string_view{u8"default"}};
            auto const adaptive_target_task_groups_per_adjusted_compile_thread{
                aggressive_runtime_compile_threads_policy_active
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    ? ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::aggressive_target_task_groups_per_adjusted_compile_thread
                    : ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::default_target_task_groups_per_adjusted_compile_thread};
# else
                    ? ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::aggressive_target_task_groups_per_adjusted_compile_thread
                    : ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::default_target_task_groups_per_adjusted_compile_thread};
# endif
            // Adaptive policies aim to keep several task groups per worker so small modules avoid scheduler overhead.
# ifndef UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
            if(effective_extra_compile_threads != 0uz)
            {
                if(::uwvm2::uwvm::io::show_runtime_compile_threads_warning)
                {
                    runtime_compile_threads_warn(
                        u8"Runtime compile threads resolved to ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        effective_extra_compile_threads,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8", but this platform does not provide fast_io::native_thread. Falling back to main-thread-only runtime full translation.");

                    if(::uwvm2::uwvm::io::runtime_compile_threads_warning_fatal) [[unlikely]] { runtime_compile_threads_warn_to_fatal(); }
                }

                effective_extra_compile_threads = 0uz;
            }
# endif

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::uwvm2::utils::container::u8string_view const runtime_full_translation_main_thread_message{
                    compile_llvm_jit_translation ? ::uwvm2::utils::container::u8string_view{u8"LLVM JIT full IR translation will run on the main thread only. "}
                                                 : ::uwvm2::utils::container::u8string_view{u8"Runtime full translation will run on the main thread only. "}};
                ::uwvm2::utils::container::u8string_view const runtime_full_translation_parallel_prefix{
                    compile_llvm_jit_translation ? ::uwvm2::utils::container::u8string_view{u8"LLVM JIT full IR translation will use up to "}
                                                 : ::uwvm2::utils::container::u8string_view{u8"Runtime full translation will use up to "}};
                ::uwvm2::utils::container::u8string_view const runtime_full_translation_fixed_prefix{
                    compile_llvm_jit_translation ? ::uwvm2::utils::container::u8string_view{u8"LLVM JIT full IR translation will use "}
                                                 : ::uwvm2::utils::container::u8string_view{u8"Runtime full translation will use "}};
                if(effective_extra_compile_threads == 0uz) { runtime_compile_threads_verbose_info(runtime_full_translation_main_thread_message); }
                else if(adaptive_runtime_compile_threads_policy_active)
                {
                    runtime_compile_threads_verbose_info(runtime_full_translation_parallel_prefix,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads + 1uz,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" compile threads (main=1, extra-up-to=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8"). ");
                }
                else
                {
                    runtime_compile_threads_verbose_info(runtime_full_translation_fixed_prefix,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads + 1uz,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8" compile threads (main=1, extra=",
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                         effective_extra_compile_threads,
                                                         ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                         u8"). ");
                }
            }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t kTranslateOpt{get_curr_target_tranopt()};
# endif
            auto const default_runtime_scheduling_profile{resolve_default_runtime_scheduling_profile(runtime_compiler)};
            // Resolve the backend's default scheduling profile before translating runtime flags into split configs.

            using runtime_scheduling_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_t;
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            using compile_task_split_policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t;
# else
            using compile_task_split_policy_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t;
# endif

            auto const runtime_scheduling_policy{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_policy};
            auto const runtime_scheduling_size{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_size};
            auto const runtime_scheduling_policy_existed{::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_existed};
            // Default scheduling can be adjusted only for backends where the runtime owns the policy. Explicit user policy is treated
            // as authoritative, even if it is not optimal for a small module.
            auto const allow_default_runtime_scheduling_adjustment{[&]() constexpr noexcept
                                                                   {
                                                                       if(runtime_scheduling_policy_existed) { return false; }

                                                                       switch(default_runtime_scheduling_profile)
                                                                       {
                                                                           case default_runtime_scheduling_profile_t::uwvm_int:
                                                                           {
                                                                               return true;
                                                                           }
                                                                           case default_runtime_scheduling_profile_t::llvm_jit:
                                                                           {
                                                                               return false;
                                                                           }
                                                                           case default_runtime_scheduling_profile_t::debug_int:
                                                                           {
                                                                               return true;
                                                                           }
                                                                           [[unlikely]] default:
                                                                           {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                                               ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                                                               ::std::unreachable();
                                                                           }
                                                                       }
                                                                   }()};

            auto const compile_task_split_conf{
                // Convert runtime scheduling flags into the backend-specific split-config type used by the active compiler.
                runtime_scheduling_policy == runtime_scheduling_policy_t::function_count
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    ? ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config{.policy =
                                                                                                                 compile_task_split_policy_t::function_count,
                                                                                                             .split_size = runtime_scheduling_size,
                                                                                                             .adjust_for_default_policy =
                                                                                                                 allow_default_runtime_scheduling_adjustment}
                    : ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config{.policy = compile_task_split_policy_t::code_size,
                                                                                                             .split_size = runtime_scheduling_size,
                                                                                                             .adjust_for_default_policy =
                                                                                                                 allow_default_runtime_scheduling_adjustment}
# else
                    ? ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config{.policy =
                                                                                                                 compile_task_split_policy_t::function_count,
                                                                                                             .split_size = runtime_scheduling_size,
                                                                                                             .adjust_for_default_policy =
                                                                                                                 allow_default_runtime_scheduling_adjustment}
                    : ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config{.policy = compile_task_split_policy_t::code_size,
                                                                                                             .split_size = runtime_scheduling_size,
                                                                                                             .adjust_for_default_policy =
                                                                                                                 allow_default_runtime_scheduling_adjustment}
# endif
            };

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                if(effective_extra_compile_threads == 0uz)
                {
                    if(::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_existed)
                    {
                        runtime_compile_threads_verbose_info(
                            u8"Runtime scheduling policy is configured but inactive because no extra runtime compile threads are enabled. ");
                    }
                }
            }

            // Assign module ids.
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
            // Switching from lazy to eager mode must tear down schedulers first, because old background contexts contain pointers into
            // module records that will be rebuilt below.
            g_runtime.lazy_scheduler.stop();
#  if defined(UWVM_RUNTIME_LLVM_JIT)
            g_runtime.llvm_jit_urgent_scheduler.stop();
#  endif
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            g_runtime.tiered_urgent_scheduler.stop();
#  endif
            g_runtime.lazy_initialized.store(false, ::std::memory_order_release);
            g_runtime.lazy_compile_active = false;
            g_runtime.lazy_prefetch_module_id = SIZE_MAX;
            g_runtime.lazy_prefetch_local_function_index = SIZE_MAX;
            g_runtime.lazy_runtime_miss_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.lazy_runtime_compiled_hit_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);
# endif
            g_runtime.modules.clear();
            g_runtime.module_name_to_id.clear();
            g_runtime.defined_func_cache.clear();
            g_runtime.defined_func_ptr_ranges.clear();
            g_import_call_cache.clear();
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            g_wasip1_runtime_module_context_cache.clear();
# endif

            auto const& rt_map{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage};
            g_runtime.modules.reserve(rt_map.size());
            g_runtime.module_name_to_id.reserve(rt_map.size());

            // Module ids are dense indices into g_runtime.modules. They are also embedded into generated code and diagnostics, so the
            // name-to-id map is built before any backend emits per-module artifacts.
            ::std::size_t id{};
            for(auto const& kv: rt_map)
            {
                g_runtime.module_name_to_id.emplace(kv.first, id);
                compiled_module_record rec{};
                rec.module_name = kv.first;
                rec.runtime_module = ::std::addressof(kv.second);
                g_runtime.modules.push_back(::std::move(rec));
                ++id;
            }

            g_runtime.defined_func_cache.resize(g_runtime.modules.size());
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            rebuild_wasip1_runtime_module_context_cache();
# endif

            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
                ::uwvm2::utils::container::u8string_view const uwvm_int_translation_state{compile_uwvm_int_translation
                                                                                              ? ::uwvm2::utils::container::u8string_view{u8"enabled"}
                                                                                              : ::uwvm2::utils::container::u8string_view{u8"disabled"}};
                ::uwvm2::utils::container::u8string_view const llvm_jit_translation_state{compile_llvm_jit_translation
                                                                                              ? ::uwvm2::utils::container::u8string_view{u8"enabled"}
                                                                                              : ::uwvm2::utils::container::u8string_view{u8"disabled"}};
                runtime_compile_threads_verbose_info(
                    u8"Resolved runtime configuration: mode=",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                    describe_runtime_mode(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode),
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8", compiler=",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                    describe_runtime_compiler(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler),
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8", uwvm-int-translation=",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, compile_uwvm_int_translation ? UWVM_COLOR_U8_LT_GREEN : UWVM_COLOR_U8_YELLOW),
                    uwvm_int_translation_state,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8", llvm-jit-ir-translation=",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, compile_llvm_jit_translation ? UWVM_COLOR_U8_LT_GREEN : UWVM_COLOR_U8_YELLOW),
                    llvm_jit_translation_state,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8". ");

                if(compile_llvm_jit_translation)
                {
                    runtime_compile_threads_verbose_info(
                        compile_uwvm_int_translation
                            ? ::uwvm2::utils::container::u8string_view{u8"LLVM JIT IR translation artifacts will also be generated during full translation. "}
                            : ::uwvm2::utils::container::u8string_view{u8"LLVM JIT full IR translation will be generated without uwvm-int full artifacts. "});
                    runtime_compile_threads_verbose_info(
                        u8"Current LLVM JIT IR translation parallelizes Wasm-to-LLVM IR emission across task modules and links them back into one module before optimization/materialization, so runtime compile-thread scheduling can also reduce LLVM JIT IR translation time while keeping the final aggressive whole-module optimization path unchanged. ");
                }
            }

            // Compile modules and build function map.
            for(auto& rec: g_runtime.modules)
            {
                // Each module can have a different effective parallelism after adaptive scheduling, but all modules publish metadata
                // into the same global caches.
                auto const it{g_runtime.module_name_to_id.find(rec.module_name)};
                if(it == g_runtime.module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const module_id{it->second};

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option opt{};
                opt.curr_wasm_id = module_id;
                // First resolve the split size against the actual module before deciding how many worker threads are worthwhile.
                auto const thread_resolution_compile_task_split_conf{
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_compile_task_split_config(*rec.runtime_module,
                                                                                                                             compile_task_split_conf,
                                                                                                                             effective_extra_compile_threads)};
# else
                // LLVM-only builds use the LLVM split resolver but publish into the same runtime module/cache records.
                auto const thread_resolution_compile_task_split_conf{
                    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::resolve_effective_compile_task_split_config(*rec.runtime_module,
                                                                                                                             compile_task_split_conf,
                                                                                                                             effective_extra_compile_threads)};
# endif
                auto const runtime_scheduling_policy_adjusted_for_thread_resolution{
                    !runtime_scheduling_policy_existed && compile_task_split_conf.policy == compile_task_split_policy_t::code_size &&
                    thread_resolution_compile_task_split_conf.split_size != runtime_scheduling_size};
                // Thread-resolution can shrink default code-size batches before adaptive per-module worker reduction is calculated.
                auto effective_module_extra_compile_threads{effective_extra_compile_threads};
                if(adaptive_runtime_compile_threads_policy_active)
                {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    effective_module_extra_compile_threads =
                        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_adaptive_extra_compile_threads(
                            *rec.runtime_module,
                            thread_resolution_compile_task_split_conf,
                            effective_extra_compile_threads,
                            adaptive_target_task_groups_per_adjusted_compile_thread,
                            runtime_scheduling_policy_adjusted_for_thread_resolution);
# else
                    effective_module_extra_compile_threads =
                        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::resolve_effective_adaptive_extra_compile_threads(
                            *rec.runtime_module,
                            thread_resolution_compile_task_split_conf,
                            effective_extra_compile_threads,
                            adaptive_target_task_groups_per_adjusted_compile_thread,
                            runtime_scheduling_policy_adjusted_for_thread_resolution);
# endif
                }
                auto const effective_compile_task_split_conf{
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_compile_task_split_config(
                        *rec.runtime_module,
                        compile_task_split_conf,
                        effective_module_extra_compile_threads)};
# else
                    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::resolve_effective_compile_task_split_config(
                        *rec.runtime_module,
                        compile_task_split_conf,
                        effective_module_extra_compile_threads)};
# endif
                auto const default_runtime_scheduling_policy_adjusted{!runtime_scheduling_policy_existed &&
                                                                      compile_task_split_conf.policy == compile_task_split_policy_t::code_size &&
                                                                      effective_compile_task_split_conf.split_size != runtime_scheduling_size};
                // This flag is only for diagnostics; explicit scheduling values are never rewritten silently.

                if(::uwvm2::uwvm::io::show_verbose && effective_extra_compile_threads != 0uz) [[unlikely]]
                {
                    // Per-module verbose lines show the final worker count and split policy after all adaptive adjustments.
                    if(effective_module_extra_compile_threads == 0uz)
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation will run on the main thread only. ");
                    }
                    else
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation will use ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_module_extra_compile_threads + 1uz,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" compile threads (main=1, extra=",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_module_extra_compile_threads,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"). ");
                    }

                    if(effective_compile_task_split_conf.policy == compile_task_split_policy_t::function_count)
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation scheduling policy uses ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             u8"func_count",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" with ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_compile_task_split_conf.split_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" functions per task. ");
                    }
                    else
                    {
                        runtime_compile_threads_verbose_info(u8"Module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" full translation scheduling policy uses ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             u8"code_size",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" with ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_compile_task_split_conf.split_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" wasm code-body bytes per task. ");
                    }

                    if(adaptive_runtime_compile_threads_policy_active && effective_module_extra_compile_threads != effective_extra_compile_threads)
                    {
                        runtime_compile_threads_verbose_info(::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             adaptive_runtime_compile_threads_policy_name,
                                                             u8" runtime compile thread policy adjusted module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" extra compile threads from ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_extra_compile_threads,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_module_extra_compile_threads,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to avoid overscheduling a small module. ");
                    }

                    if(default_runtime_scheduling_policy_adjusted)
                    {
                        runtime_compile_threads_verbose_info(u8"Default runtime scheduling policy adjusted module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\" code_size from ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             runtime_scheduling_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to ",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             effective_compile_task_split_conf.split_size,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8" to reduce scheduling overhead for a small module. ");
                    }
                }

                ::uwvm2::validation::error::code_validation_error_impl err{};

                // Translation phase:
                // - interpreter builds opfunc streams and compiled call-info records;
                // - LLVM builds IR/task modules but does not publish executable entry addresses yet.
                // Materialization and cache publication happen after this try block.
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    if(compile_uwvm_int_translation)
                    {
                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            runtime_compile_threads_verbose_info(u8"Begin runtime full translation for module \"",
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                                 rec.module_name,
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                                 u8"\". ");
                        }
                        auto const uwvm_int_translation_start_time{runtime_compile_threads_verbose_now()};
                        rec.compiled = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_all_from_uwvm<kTranslateOpt>(
                            *rec.runtime_module,
                            opt,
                            err,
                            effective_module_extra_compile_threads,
                            effective_compile_task_split_conf);

                        runtime_compile_threads_verbose_done(uwvm_int_translation_start_time,
                                                             u8"Runtime full translation for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\"");
                    }
# endif

# if defined(UWVM_RUNTIME_LLVM_JIT)
                    if(compile_llvm_jit_translation)
                    {
                        // Full LLVM mode emits IR for all local functions first, then materializes as a module. Keeping the two phases
                        // explicit lets interpreter+LLVM builds fall back cleanly if native materialization is unavailable.
                        if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                        {
                            runtime_compile_threads_verbose_info(u8"Begin LLVM JIT IR translation for module \"",
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                                 rec.module_name,
                                                                 ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                                 u8"\". ");
                        }
                        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option llvm_jit_opt{};
                        llvm_jit_opt.curr_wasm_id = module_id;
                        llvm_jit_opt.verify_llvm_jit_ir = !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_disable_ir_verifaction;
                        configure_runtime_llvm_jit_call_stack_policy(llvm_jit_opt);
                        runtime_llvm_jit_legacy_light_task_preopt_context legacy_light_task_preopt_context{};
                        bool legacy_light_task_preopt_enabled{};
                        if(effective_module_extra_compile_threads != 0uz)
                        {
                            auto const full_translation_strategy{resolve_runtime_llvm_jit_full_materialize_strategy(::llvm::CodeGenOptLevel::Aggressive)};
                            if(full_translation_strategy.pipeline == runtime_llvm_jit_full_pipeline_kind::legacy_light &&
                               ensure_llvm_jit_native_target_initialized())
                            {
                                auto const host_cpu_name{::llvm::sys::getHostCPUName()};
                                legacy_light_task_preopt_context.host_cpu_name = ::uwvm2::utils::container::u8string{
                                    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::get_uwvm_u8string_view(host_cpu_name)};
                                legacy_light_task_preopt_context.host_target_attribute_storage = get_llvm_jit_host_target_attribute_storage();
                                legacy_light_task_preopt_context.codegen_opt_level = full_translation_strategy.codegen_opt_level;
                                llvm_jit_opt.llvm_jit_task_module_pre_link_callback =
                                    ::std::addressof(optimize_runtime_llvm_jit_legacy_light_task_module_pre_link);
                                llvm_jit_opt.llvm_jit_task_module_pre_link_callback_context = ::std::addressof(legacy_light_task_preopt_context);
                                legacy_light_task_preopt_enabled = true;

                                if(::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
                                {
                                    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8runtime_log_output,
                                                          u8"[llvm-jit-full] legacy-light-task-preopt-start module=\"",
                                                          rec.module_name,
                                                          u8"\" extra_threads=",
                                                          effective_module_extra_compile_threads,
                                                          u8" codegen_opt=",
                                                          get_runtime_llvm_jit_codegen_opt_level_name(full_translation_strategy.codegen_opt_level));
                                }
                            }
                        }
                        auto const llvm_jit_translation_start_time{runtime_compile_threads_verbose_now()};
                        rec.llvm_jit_compiled = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_all_from_uwvm(
                            *rec.runtime_module,
                            llvm_jit_opt,
                            err,
                            effective_module_extra_compile_threads,
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                            {.policy = static_cast<::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t>(
                                 static_cast<unsigned>(effective_compile_task_split_conf.policy)),
                             .split_size = effective_compile_task_split_conf.split_size,
                             .adjust_for_default_policy = effective_compile_task_split_conf.adjust_for_default_policy}
#  else
                            effective_compile_task_split_conf
#  endif
                        );
                        if(legacy_light_task_preopt_enabled && ::uwvm2::uwvm::io::enable_runtime_log) [[unlikely]]
                        {
                            ::fast_io::io::perrln(::uwvm2::uwvm::io::u8runtime_log_output,
                                                  u8"[llvm-jit-full] legacy-light-task-preopt-end module=\"",
                                                  rec.module_name,
                                                  u8"\" tasks=",
                                                  legacy_light_task_preopt_context.optimized_task_modules.load(::std::memory_order_relaxed),
                                                  u8" applied=",
                                                  ::fast_io::mnp::cond(rec.llvm_jit_compiled.llvm_jit_task_modules_pre_link_optimized, u8"yes", u8"no"));
                        }
                        runtime_compile_threads_verbose_done(llvm_jit_translation_start_time,
                                                             u8"LLVM JIT IR translation for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\"");
                    }
# endif
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    print_and_terminate_compile_validation_error(rec.module_name, err);
                }
# endif

                rec.type_canon_index = build_type_canon_index(*rec.runtime_module);

                auto const local_n{rec.runtime_module->local_defined_function_vec_storage.size()};
                // Backend artifacts must match the local function count exactly; mismatches indicate translator/runtime cache drift.
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                if(compile_uwvm_int_translation && (local_n != rec.compiled.local_funcs.size() || local_n != rec.compiled.local_defined_call_info.size()))
                    [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
                if(compile_llvm_jit_translation && local_n != rec.llvm_jit_compiled.local_funcs.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                if(compile_llvm_jit_translation)
                {
                    // Materialization turns LLVM IR into executable code and fills raw/typed entry-address arrays for this module.
                    if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                    {
                        runtime_compile_threads_verbose_info(u8"Begin LLVM JIT materialization for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\". ");
                    }
                    auto const llvm_jit_materialize_start_time{runtime_compile_threads_verbose_now()};
                    if(!try_materialize_runtime_module_llvm_jit(rec, true, ::llvm::CodeGenOptLevel::Aggressive, effective_module_extra_compile_threads))
                        [[unlikely]]
                    {
                        if(runtime_compiler_requires_llvm_jit_execution())
                        {
                            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                                u8"uwvm: ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                                u8"[fatal] ",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"LLVM JIT materialization failed for module=\"",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                rec.module_name,
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                u8"\".\n\n",
                                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                            ::fast_io::fast_terminate();
                        }

                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            u8"[warn]  ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"LLVM JIT materialization failed for module=\"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            rec.module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\"; falling back to interpreter execution for this module.\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    }
                    else
                    {
                        runtime_compile_threads_verbose_done(llvm_jit_materialize_start_time,
                                                             u8"LLVM JIT materialization for module \"",
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                                             rec.module_name,
                                                             ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                                             u8"\"");
                    }
                }
# endif

                // Canonical type ids and defined-function metadata are backend-neutral runtime indexes. Direct calls, indirect calls,
                // lazy placeholders, host APIs, and trap printers all consult these caches.
                auto& mod_cache{g_runtime.defined_func_cache.index_unchecked(module_id)};
                mod_cache.clear();
                mod_cache.resize(local_n);
                // From here to the end of the module loop, the code builds backend-neutral lookup records from backend-specific
                // translation/materialization outputs.

                if(local_n != 0uz)
                {
                    // Record the address interval for fast reverse lookup from runtime function pointers to module/local ids.
                    auto const base_ptr{rec.runtime_module->local_defined_function_vec_storage.data()};
                    if(base_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    ::std::uintptr_t const begin{reinterpret_cast<::std::uintptr_t>(base_ptr)};
                    constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
                    static_assert(elem_size != 0uz);
                    if(local_n > (::std::numeric_limits<::std::uintptr_t>::max() / elem_size)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uintptr_t const bytes{static_cast<::std::uintptr_t>(local_n * elem_size)};
                    if(begin > ::std::numeric_limits<::std::uintptr_t>::max() - bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

                    g_runtime.defined_func_ptr_ranges.push_back(defined_func_ptr_range{begin, begin + bytes, module_id});
                }

                for(::std::size_t i{}; i != local_n; ++i)
                {
                    // Defined-function cache entries normalize runtime storage, compiled artifacts, signatures, and byte ABI sizes.
                    auto const runtime_func{::std::addressof(rec.runtime_module->local_defined_function_vec_storage.index_unchecked(i))};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const* compiled_call_info{};
                    compiled_local_func_t const* compiled_func{};
                    if(compile_uwvm_int_translation)
                    {
                        compiled_call_info = ::std::addressof(rec.compiled.local_defined_call_info.index_unchecked(i));
                        compiled_func = ::std::addressof(rec.compiled.local_funcs.index_unchecked(i));
                    }
# endif

                    auto const sig{func_sig_from_defined(runtime_func)};
                    auto const param_bytes{total_abi_bytes(sig.params)};
                    auto const result_bytes{total_abi_bytes(sig.results)};
                    if((param_bytes == 0uz && sig.params.size != 0uz) || (result_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    mod_cache.index_unchecked(i) = compiled_defined_func_info{module_id,
                                                                              rec.runtime_module->imported_function_vec_storage.size() + i,
                                                                              runtime_func,
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                                                                              compiled_call_info,
                                                                              compiled_func,
# endif
                                                                              param_bytes,
                                                                              result_bytes};
                }
            }

            if(!g_runtime.defined_func_ptr_ranges.empty())
            {
                ::std::sort(g_runtime.defined_func_ptr_ranges.begin(),
                            g_runtime.defined_func_ptr_ranges.end(),
                            [](defined_func_ptr_range const& a, defined_func_ptr_range const& b) constexpr noexcept { return a.begin < b.begin; });
            }

            // Build an O(1) dispatch table for imported calls, flattening any import-alias chains ahead of time.
            //
            // Import-cache publication is separated from module translation because imported functions can target modules that appear
            // later in the runtime storage map. Waiting until every defined-function cache is built lets all import aliases resolve to
            // final backend-neutral call records.
            g_import_call_cache.resize(g_runtime.modules.size());
            for(::std::size_t mid{}; mid != g_runtime.modules.size(); ++mid)
            {
                auto const& rec{g_runtime.modules.index_unchecked(mid)};
                auto const rt{rec.runtime_module};
                if(rt == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_n{rt->imported_function_vec_storage.size()};
                auto& cache{g_import_call_cache.index_unchecked(mid)};
                cache.clear();
                cache.resize(import_n);

                for(::std::size_t i{}; i != import_n; ++i)
                {
                    // Resolve every import once, validate module-specific WASI visibility, then store the final callable target.
                    auto const imp{::std::addressof(rt->imported_function_vec_storage.index_unchecked(i))};
                    if(imp->import_type_ptr != nullptr && imp->import_type_ptr->module_name == u8"wasi_snapshot_preview1" &&
                       !is_wasip1_import_visible_for_runtime_module_id(mid)) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"Runtime initializer: module-specific WASI Preview 1 setting disables import \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                            imp->import_type_ptr->module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\" for module \"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                            rec.module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\".\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                    auto const rf{resolve_func_from_import_assuming_initialized(imp)};

                    cached_import_target tgt{};
                    // Default to the import slot frame; for resolved wasm functions we overwrite with the final module/function index.
                    tgt.frame.module_id = mid;
                    tgt.frame.function_index = i;

                    switch(rf.k)
                    {
                        case resolved_func::kind::defined:
                        {
                            auto const info{find_defined_func_info(rf.u.defined_ptr)};
                            if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                            tgt.k = cached_import_target::kind::defined;
                            tgt.frame.module_id = info->module_id;
                            tgt.frame.function_index = info->function_index;
                            tgt.sig = func_sig_from_defined(info->runtime_func);
                            tgt.param_bytes = info->param_bytes;
                            tgt.result_bytes = info->result_bytes;
                            tgt.u.defined.runtime_func = info->runtime_func;
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                            tgt.u.defined.compiled_func = info->compiled_func;
# endif
                            break;
                        }
                        case resolved_func::kind::local_imported:
                        {
                            tgt.k = cached_import_target::kind::local_imported;
                            tgt.u.local_imported = rf.u.local_imported;
                            tgt.sig = func_sig_from_local_imported(tgt.u.local_imported.module_ptr, tgt.u.local_imported.index);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::dl:
                        {
                            tgt.k = cached_import_target::kind::dl;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::weak_symbol:
                        {
                            tgt.k = cached_import_target::kind::weak_symbol;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }

                    cache.index_unchecked(i) = tgt;
                }
            }

# if defined(UWVM_RUNTIME_LLVM_JIT)
            populate_llvm_jit_call_indirect_table_views();
# endif

            // Report the one-shot eager compilation duration after every cache and optional table view has been published.
            ::fast_io::unix_timestamp end_time{};
            if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
            {
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
# endif

                // Keep the verbose message enum-safe for int-only builds where
                // `llvm_jit_only` is not part of `runtime_compiler_t`.
# if defined(UWVM_RUNTIME_LLVM_JIT)
                auto const llvm_jit_full_compile{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                                 ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only};
# else
                constexpr bool llvm_jit_full_compile{};
# endif

                // Keep the success line short and colorized; detailed module-level scheduling logs were emitted earlier.
                ::fast_io::io::perr(
                    ::uwvm2::uwvm::io::u8log_output,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                    u8"uwvm: ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                    u8"[info]  ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    ::fast_io::mnp::cond(llvm_jit_full_compile, u8"llvm-jit full compilation done. (time=", u8"uwvm-int full translation done. (time="),
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                    end_time - start_time,
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                    u8"s). ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_GREEN),
                    u8"[",
                    ::uwvm2::uwvm::io::get_local_realtime(),
                    u8"] ",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                    u8"(verbose)\n",
                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            }

            g_runtime.compiled_all.store(true, ::std::memory_order_release);
            compile_lock.clear(::std::memory_order_release);
#endif
        }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        inline constexpr void initialize_lazy_modules_if_needed(::uwvm2::utils::container::u8string_view main_module_name, lazy_compile_run_config cfg) noexcept
        {

            // =========================================================================
            // Lazy interpreter runtime initialization
            // -----------------------------------------------------------------------
            // Build only metadata, placeholder interpreter artifacts, and dispatch
            // caches. Function bodies are translated later by demand calls or optional
            // background workers.
            //
            // Coverage invariants:
            // - Module ids and cache shapes match eager mode.
            // - Validator module storage is stored in compile options for future per-unit validation.
            // - Entry-biased prefetch improves startup but cannot be required for correctness.
            // - Zero-worker mode remains fully synchronous and demand-driven.
            // =========================================================================
            // Lazy interpreter initialization builds only the metadata and per-function compile units needed to compile on demand. The
            // same public caches are populated as full mode so call bridges do not need a separate lazy dispatch path.
            ensure_memory_signal_trap_bridge_initialized();
            ensure_bridges_initialized();

            if(g_runtime.lazy_initialized.load(::std::memory_order_acquire)) { return; }

            static ::std::atomic_flag lazy_init_lock = ATOMIC_FLAG_INIT;
            while(lazy_init_lock.test_and_set(::std::memory_order_acquire))
            {
                if(g_runtime.lazy_initialized.load(::std::memory_order_acquire)) { return; }
                ::uwvm2::utils::thread::lazy_compile_thread_yield();
            }

            if(g_runtime.lazy_initialized.load(::std::memory_order_relaxed))
            {
                lazy_init_lock.clear(::std::memory_order_release);
                return;
            }

            g_runtime.lazy_scheduler.stop();
# if defined(UWVM_RUNTIME_LLVM_JIT)
            g_runtime.llvm_jit_urgent_scheduler.stop();
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            g_runtime.tiered_urgent_scheduler.stop();
# endif
            // Clear all runtime registries before rebuilding lazy metadata; stale caches may contain old compiled-function pointers.
            g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);
            g_runtime.modules.clear();
            g_runtime.module_name_to_id.clear();
            g_runtime.defined_func_cache.clear();
            g_runtime.defined_func_ptr_ranges.clear();
            g_import_call_cache.clear();
            g_runtime.lazy_runtime_miss_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.lazy_runtime_compiled_hit_count.store(0uz, ::std::memory_order_relaxed);
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            g_wasip1_runtime_module_context_cache.clear();
# endif

            auto const& rt_map{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage};
            g_runtime.modules.reserve(rt_map.size());
            g_runtime.module_name_to_id.reserve(rt_map.size());

            ::std::size_t id{};
            for(auto const& kv: rt_map)
            {
                // Preserve the same dense module-id assignment used by eager mode so host APIs and diagnostics are mode-independent.
                g_runtime.module_name_to_id.emplace(kv.first, id);
                compiled_module_record rec{};
                rec.module_name = kv.first;
                rec.runtime_module = ::std::addressof(kv.second);
                g_runtime.modules.push_back(::std::move(rec));
                ++id;
            }

            g_runtime.defined_func_cache.resize(g_runtime.modules.size());
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            rebuild_wasip1_runtime_module_context_cache();
# endif

            using lazy_split_config = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_split_config;
            using lazy_eu_policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_execution_unit_split_policy_t;
            using lazy_cu_policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_compile_unit_split_policy_t;
            using runtime_scheduling_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_t;

            lazy_split_config split_config{};
            split_config.cu_code_size = ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_size;
            // Function-count scheduling keeps lazy compile units small and predictable when the user requests function-based runtime
            // scheduling instead of code-size batching.
            if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_policy == runtime_scheduling_policy_t::function_count)
            {
                split_config.eu_policy = lazy_eu_policy_t::function_only;
                split_config.cu_policy = lazy_cu_policy_t::function;
            }

            auto const lazy_validation_mode{cfg.assume_full_code_verified ? lazy_validation_mode_t::assume_full_code_verified
                                                                          : lazy_validation_mode_t::validate_on_lazy_compile};

            for(auto& rec: g_runtime.modules)
            {
                // Store validator-side module storage in the lazy options so each future function compile can validate only the code
                // it is about to materialize.
                auto const it{g_runtime.module_name_to_id.find(rec.module_name)};
                if(it == g_runtime.module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const module_id{it->second};

                ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option opt{};
                opt.curr_wasm_id = module_id;

                rec.lazy_compile_options.compile_options = opt;
                rec.lazy_compile_options.validation_mode = lazy_validation_mode;
                rec.lazy_compile_options.validator_module_storage = find_lazy_validator_module_storage(rec.module_name);
                if(lazy_validation_mode == lazy_validation_mode_t::validate_on_lazy_compile && rec.lazy_compile_options.validator_module_storage == nullptr)
                    [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                ::uwvm2::validation::error::code_validation_error_impl err{};
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    rec.lazy_compiled =
                        ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::initialize_lazy_module_storage(*rec.runtime_module,
                                                                                                                             opt,
                                                                                                                             err,
                                                                                                                             split_config);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    print_and_terminate_compile_validation_error(rec.module_name, err);
                }
# endif

                rec.type_canon_index = build_type_canon_index(*rec.runtime_module);

                auto const local_n{rec.runtime_module->local_defined_function_vec_storage.size()};
                if(local_n != rec.lazy_compiled.compiled.local_funcs.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto& mod_cache{g_runtime.defined_func_cache.index_unchecked(module_id)};
                mod_cache.clear();
                mod_cache.resize(local_n);

                if(local_n != 0uz)
                {
                    // Lazy mode still needs pointer-range reverse lookup because imports and host APIs resolve by runtime storage.
                    auto const base_ptr{rec.runtime_module->local_defined_function_vec_storage.data()};
                    if(base_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    ::std::uintptr_t const begin{reinterpret_cast<::std::uintptr_t>(base_ptr)};
                    constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
                    static_assert(elem_size != 0uz);
                    if(local_n > (::std::numeric_limits<::std::uintptr_t>::max() / elem_size)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uintptr_t const bytes{static_cast<::std::uintptr_t>(local_n * elem_size)};
                    if(begin > ::std::numeric_limits<::std::uintptr_t>::max() - bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

                    g_runtime.defined_func_ptr_ranges.push_back(defined_func_ptr_range{begin, begin + bytes, module_id});
                }

                for(::std::size_t i{}; i != local_n; ++i)
                {
                    // Cache the interpreter placeholder function and call-info entry; the lazy compiler will fill them on demand.
                    auto const runtime_func{::std::addressof(rec.runtime_module->local_defined_function_vec_storage.index_unchecked(i))};
                    auto const compiled_call_info{::std::addressof(rec.lazy_compiled.compiled.local_defined_call_info.index_unchecked(i))};
                    auto const compiled_func{::std::addressof(rec.lazy_compiled.compiled.local_funcs.index_unchecked(i))};

                    auto const sig{func_sig_from_defined(runtime_func)};
                    auto const param_bytes{total_abi_bytes(sig.params)};
                    auto const result_bytes{total_abi_bytes(sig.results)};
                    if((param_bytes == 0uz && sig.params.size != 0uz) || (result_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    mod_cache.index_unchecked(i) = compiled_defined_func_info{module_id,
                                                                              rec.runtime_module->imported_function_vec_storage.size() + i,
                                                                              runtime_func,
                                                                              compiled_call_info,
                                                                              compiled_func,
                                                                              param_bytes,
                                                                              result_bytes};
                }

                prepare_lazy_background_request_contexts(rec);
            }

            if(!g_runtime.defined_func_ptr_ranges.empty())
            {
                ::std::sort(g_runtime.defined_func_ptr_ranges.begin(),
                            g_runtime.defined_func_ptr_ranges.end(),
                            [](defined_func_ptr_range const& a, defined_func_ptr_range const& b) constexpr noexcept { return a.begin < b.begin; });
            }

            g_import_call_cache.resize(g_runtime.modules.size());
            for(::std::size_t mid{}; mid != g_runtime.modules.size(); ++mid)
            {
                // Rebuild import caches after defined-function caches because imported wasm definitions point into those records.
                auto const& rec{g_runtime.modules.index_unchecked(mid)};
                auto const rt{rec.runtime_module};
                if(rt == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_n{rt->imported_function_vec_storage.size()};
                auto& cache{g_import_call_cache.index_unchecked(mid)};
                cache.clear();
                cache.resize(import_n);

                for(::std::size_t i{}; i != import_n; ++i)
                {
                    // Lazy interpreter imports use the same flattened target representation as eager mode.
                    auto const imp{::std::addressof(rt->imported_function_vec_storage.index_unchecked(i))};
                    if(imp->import_type_ptr != nullptr && imp->import_type_ptr->module_name == u8"wasi_snapshot_preview1" &&
                       !is_wasip1_import_visible_for_runtime_module_id(mid)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    auto const rf{resolve_func_from_import_assuming_initialized(imp)};

                    cached_import_target tgt{};
                    tgt.frame.module_id = mid;
                    tgt.frame.function_index = i;

                    switch(rf.k)
                    {
                        case resolved_func::kind::defined:
                        {
                            auto const info{find_defined_func_info(rf.u.defined_ptr)};
                            if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                            tgt.k = cached_import_target::kind::defined;
                            tgt.frame.module_id = info->module_id;
                            tgt.frame.function_index = info->function_index;
                            tgt.sig = func_sig_from_defined(info->runtime_func);
                            tgt.param_bytes = info->param_bytes;
                            tgt.result_bytes = info->result_bytes;
                            tgt.u.defined.runtime_func = info->runtime_func;
                            tgt.u.defined.compiled_func = info->compiled_func;
                            break;
                        }
                        case resolved_func::kind::local_imported:
                        {
                            tgt.k = cached_import_target::kind::local_imported;
                            tgt.u.local_imported = rf.u.local_imported;
                            tgt.sig = func_sig_from_local_imported(tgt.u.local_imported.module_ptr, tgt.u.local_imported.index);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::dl:
                        {
                            tgt.k = cached_import_target::kind::dl;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::weak_symbol:
                        {
                            tgt.k = cached_import_target::kind::weak_symbol;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }

                    cache.index_unchecked(i) = tgt;
                }
            }

            g_runtime.lazy_runtime_miss_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.lazy_runtime_compiled_hit_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.lazy_prefetch_module_id = SIZE_MAX;
            g_runtime.lazy_prefetch_local_function_index = SIZE_MAX;
            g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);

            // Bias the first background requests toward the configured entry path. This improves startup latency without permanently
            // starving the rest of the module because refill callbacks continue from the shared prefetch order.
            auto const main_it{g_runtime.module_name_to_id.find(main_module_name)};
            if(main_it != g_runtime.module_name_to_id.end())
            {
                auto const main_id{main_it->second};
                auto const& main_rec{g_runtime.modules.index_unchecked(main_id)};
                auto const main_rt{main_rec.runtime_module};
                if(main_rt != nullptr)
                {
                    auto const import_n{main_rt->imported_function_vec_storage.size()};
                    auto const total_n{import_n + main_rt->local_defined_function_vec_storage.size()};
                    if(cfg.entry_function_index < total_n)
                    {
                        if(cfg.entry_function_index < import_n)
                        {
                            auto const& tgt{g_import_call_cache.index_unchecked(main_id).index_unchecked(cfg.entry_function_index)};
                            if(tgt.k == cached_import_target::kind::defined)
                            {
                                auto const target_module_id{tgt.frame.module_id};
                                if(target_module_id < g_runtime.modules.size())
                                {
                                    auto const& target_rec{g_runtime.modules.index_unchecked(target_module_id)};
                                    auto const target_rt{target_rec.runtime_module};
                                    if(target_rt != nullptr)
                                    {
                                        auto const target_import_n{target_rt->imported_function_vec_storage.size()};
                                        if(tgt.frame.function_index >= target_import_n)
                                        {
                                            g_runtime.lazy_prefetch_module_id = target_module_id;
                                            g_runtime.lazy_prefetch_local_function_index = tgt.frame.function_index - target_import_n;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            g_runtime.lazy_prefetch_module_id = main_id;
                            g_runtime.lazy_prefetch_local_function_index = cfg.entry_function_index - import_n;
                        }
                    }
                }
            }

            if(g_runtime.lazy_prefetch_module_id < g_runtime.modules.size())
            {
                auto& preferred_rec{g_runtime.modules.index_unchecked(g_runtime.lazy_prefetch_module_id)};
                prioritize_lazy_background_entry(preferred_rec, g_runtime.lazy_prefetch_local_function_index);
            }

            auto const worker_count{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved};
            // With zero workers, lazy compilation remains entirely demand-driven on the executing thread.
            g_runtime.lazy_scheduler.start({.worker_count = worker_count,
                                            .queue_capacity = 0uz,
                                            .refill_callback = worker_count == 0uz ? nullptr : &lazy_background_refill_callback,
                                            .refill_user_data = nullptr});
            g_runtime.lazy_compile_active = true;
            if(worker_count != 0uz) { (void)lazy_background_refill_callback(nullptr, g_runtime.lazy_scheduler); }
            g_runtime.compiled_all.store(true, ::std::memory_order_release);
            g_runtime.lazy_initialized.store(true, ::std::memory_order_release);
            lazy_init_lock.clear(::std::memory_order_release);
        }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
        inline constexpr void initialize_llvm_jit_lazy_modules_if_needed(::uwvm2::utils::container::u8string_view main_module_name,
                                                                         lazy_compile_run_config cfg) noexcept
        {
            // =========================================================================
            // Lazy LLVM and tiered runtime initialization
            // -----------------------------------------------------------------------
            // Build raw-entry placeholders, typed-entry publication arrays, optional
            // interpreter T0 state, import caches, and scheduler queues before any host
            // entry point can execute generated code.
            //
            // Coverage invariants:
            // - Placeholder raw entries are valid call targets even before materialization.
            // - Tiered T0 metadata is built under the same module ids as LLVM lazy metadata.
            // - Direct-call and call_indirect targets use atomic publication cells.
            // - Scheduler startup policy must leave demand compilation correct with zero workers.
            // =========================================================================
            // Lazy LLVM initialization publishes bridgeable raw-entry targets up front, then lets materialization replace those
            // placeholders atomically as individual functions become native code.
            ensure_memory_signal_trap_bridge_initialized();

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            auto const tiered_backend{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                                      ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered};
            auto const tiered_t0_backend{tiered_backend && tiered_t0_enabled()};
            auto const tiered_t2_backend{tiered_backend && tiered_t2_enabled()};
            auto const tiered_targets_backend{tiered_t0_backend || tiered_t2_backend};
            if(tiered_t0_backend) { ensure_bridges_initialized(); }
# else
            constexpr bool tiered_t0_backend{};
# endif

            if(g_runtime.lazy_initialized.load(::std::memory_order_acquire)) { return; }

            // Lazy LLVM state includes native engines, debug listeners, interpreter T0 metadata in tiered mode, and background request
            // contexts. Rebuild it under one lock so published target arrays never reference half-reset records.
            static ::std::atomic_flag lazy_init_lock = ATOMIC_FLAG_INIT;
            while(lazy_init_lock.test_and_set(::std::memory_order_acquire))
            {
                if(g_runtime.lazy_initialized.load(::std::memory_order_acquire)) { return; }
                ::uwvm2::utils::thread::lazy_compile_thread_yield();
            }

            if(g_runtime.lazy_initialized.load(::std::memory_order_relaxed))
            {
                lazy_init_lock.clear(::std::memory_order_release);
                return;
            }

            g_runtime.lazy_scheduler.stop();
# if defined(UWVM_RUNTIME_LLVM_JIT)
            g_runtime.llvm_jit_urgent_scheduler.stop();
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            g_runtime.tiered_urgent_scheduler.stop();
# endif
            // Drop every previous publication array before rebuilding lazy LLVM placeholders and tiered counters.
            g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);
            g_runtime.modules.clear();
            g_runtime.module_name_to_id.clear();
            g_runtime.defined_func_cache.clear();
            g_runtime.defined_func_ptr_ranges.clear();
            g_import_call_cache.clear();
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            g_wasip1_runtime_module_context_cache.clear();
# endif

            auto const& rt_map{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage};
            g_runtime.modules.reserve(rt_map.size());
            g_runtime.module_name_to_id.reserve(rt_map.size());

            ::std::size_t id{};
            for(auto const& kv: rt_map)
            {
                // Lazy LLVM uses the same module-id namespace as full LLVM so generated call targets and host APIs stay compatible.
                g_runtime.module_name_to_id.emplace(kv.first, id);
                compiled_module_record rec{};
                rec.module_name = kv.first;
                rec.runtime_module = ::std::addressof(kv.second);
                g_runtime.modules.push_back(::std::move(rec));
                ++id;
            }

            g_runtime.defined_func_cache.resize(g_runtime.modules.size());
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            rebuild_wasip1_runtime_module_context_cache();
# endif

            using lazy_split_config = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::lazy_split_config;

            lazy_split_config split_config{};
            split_config.cu_code_size = ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_size;

            auto const lazy_validation_mode{cfg.assume_full_code_verified ? llvm_jit_lazy_validation_mode_t::assume_full_code_verified
                                                                          : llvm_jit_lazy_validation_mode_t::validate_on_lazy_compile};

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            using int_lazy_split_config = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_split_config;
            using int_lazy_eu_policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_execution_unit_split_policy_t;
            using int_lazy_cu_policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::lazy_compile_unit_split_policy_t;
            using runtime_scheduling_policy_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_scheduling_policy_t;

            int_lazy_split_config interpreter_split_config{};
            interpreter_split_config.cu_code_size = ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_size;
            if(::uwvm2::uwvm::runtime::runtime_mode::global_runtime_scheduling_policy == runtime_scheduling_policy_t::function_count)
            {
                interpreter_split_config.eu_policy = int_lazy_eu_policy_t::function_only;
                interpreter_split_config.cu_policy = int_lazy_cu_policy_t::function;
            }

            auto const interpreter_lazy_validation_mode{cfg.assume_full_code_verified ? lazy_validation_mode_t::assume_full_code_verified
                                                                                      : lazy_validation_mode_t::validate_on_lazy_compile};
# endif

            for(auto& rec: g_runtime.modules)
            {
                // LLVM lazy and interpreter lazy share the same wasm module id. In tiered T0 mode, the interpreter metadata provides
                // immediate execution while LLVM lazy units compile the replacement entries.
                auto const it{g_runtime.module_name_to_id.find(rec.module_name)};
                if(it == g_runtime.module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const module_id{it->second};

                ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option opt{};
                opt.curr_wasm_id = module_id;
                opt.verify_llvm_jit_ir = !::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_disable_ir_verifaction;
                configure_runtime_llvm_jit_call_stack_policy(opt);
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                opt.emit_tiered_loop_reentry_entries = tiered_t0_backend;
# endif

                rec.llvm_jit_lazy_compile_options.compile_options = opt;
                rec.llvm_jit_lazy_compile_options.validation_mode = lazy_validation_mode;
                rec.llvm_jit_lazy_compile_options.codegen_opt_level = resolve_runtime_llvm_jit_lazy_codegen_opt_level(
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                    tiered_backend ? ::llvm::CodeGenOptLevel::Less :
# endif
                                   ::llvm::CodeGenOptLevel::Less);
                rec.llvm_jit_lazy_compile_options.jit_event_listener =
                    opt.emit_unwind_call_stack_frames ? ::std::addressof(get_uwvm_llvm_jit_debug_listener()) : nullptr;
                rec.llvm_jit_lazy_compile_options.validator_module_storage = find_lazy_validator_module_storage(rec.module_name);
                if(lazy_validation_mode == llvm_jit_lazy_validation_mode_t::validate_on_lazy_compile &&
                   rec.llvm_jit_lazy_compile_options.validator_module_storage == nullptr) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                ::uwvm2::validation::error::code_validation_error_impl err{};
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    rec.llvm_jit_lazy_compiled =
                        ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator::initialize_lazy_module_storage(*rec.runtime_module,
                                                                                                                             opt,
                                                                                                                             err,
                                                                                                                             split_config);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    print_and_terminate_compile_validation_error(rec.module_name, err);
                }
# endif

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                if(tiered_t0_backend)
                {
                    // Build interpreter T0 storage beside LLVM lazy storage so tiered execution can start immediately.
                    ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option interpreter_opt{};
                    interpreter_opt.curr_wasm_id = module_id;

                    rec.lazy_compile_options.compile_options = interpreter_opt;
                    rec.lazy_compile_options.validation_mode = interpreter_lazy_validation_mode;
                    rec.lazy_compile_options.validator_module_storage = rec.llvm_jit_lazy_compile_options.validator_module_storage;
                    if(interpreter_lazy_validation_mode == lazy_validation_mode_t::validate_on_lazy_compile &&
                       rec.lazy_compile_options.validator_module_storage == nullptr) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }

                    err = {};
#  ifdef UWVM_CPP_EXCEPTIONS
                    try
#  endif
                    {
                        rec.lazy_compiled =
                            ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator::initialize_lazy_module_storage(*rec.runtime_module,
                                                                                                                                 interpreter_opt,
                                                                                                                                 err,
                                                                                                                                 interpreter_split_config);
                    }
#  ifdef UWVM_CPP_EXCEPTIONS
                    catch(::fast_io::error)
                    {
                        print_and_terminate_compile_validation_error(rec.module_name, err);
                    }
#  endif
                }
# endif

                rec.type_canon_index = build_type_canon_index(*rec.runtime_module);

                auto const local_n{rec.runtime_module->local_defined_function_vec_storage.size()};
                if(local_n != rec.llvm_jit_lazy_compiled.compiled.local_funcs.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                if(tiered_t0_backend && local_n != rec.lazy_compiled.compiled.local_funcs.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
# endif

                auto& mod_cache{g_runtime.defined_func_cache.index_unchecked(module_id)};
                mod_cache.clear();
                mod_cache.resize(local_n);

                if(local_n != 0uz)
                {
                    // Pointer ranges allow table/import resolution to map runtime function pointers back to cache entries.
                    auto const base_ptr{rec.runtime_module->local_defined_function_vec_storage.data()};
                    if(base_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    ::std::uintptr_t const begin{reinterpret_cast<::std::uintptr_t>(base_ptr)};
                    constexpr ::std::size_t elem_size{sizeof(runtime_local_func_storage_t)};
                    static_assert(elem_size != 0uz);
                    if(local_n > (::std::numeric_limits<::std::uintptr_t>::max() / elem_size)) [[unlikely]] { ::fast_io::fast_terminate(); }
                    ::std::uintptr_t const bytes{static_cast<::std::uintptr_t>(local_n * elem_size)};
                    if(begin > ::std::numeric_limits<::std::uintptr_t>::max() - bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

                    g_runtime.defined_func_ptr_ranges.push_back(defined_func_ptr_range{begin, begin + bytes, module_id});
                }

                for(::std::size_t i{}; i != local_n; ++i)
                {
                    // Each cache entry starts with runtime metadata and, in tiered T0, interpreter artifacts for the fallback tier.
                    auto const runtime_func{::std::addressof(rec.runtime_module->local_defined_function_vec_storage.index_unchecked(i))};
                    auto const sig{func_sig_from_defined(runtime_func)};
                    auto const param_bytes{total_abi_bytes(sig.params)};
                    auto const result_bytes{total_abi_bytes(sig.results)};
                    if((param_bytes == 0uz && sig.params.size != 0uz) || (result_bytes == 0uz && sig.results.size != 0uz)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }

                    compiled_defined_func_info info{};
                    info.module_id = module_id;
                    info.function_index = rec.runtime_module->imported_function_vec_storage.size() + i;
                    info.runtime_func = runtime_func;
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                    if(tiered_t0_backend)
                    {
                        info.compiled_call_info = ::std::addressof(rec.lazy_compiled.compiled.local_defined_call_info.index_unchecked(i));
                        info.compiled_func = ::std::addressof(rec.lazy_compiled.compiled.local_funcs.index_unchecked(i));
                    }
                    else
#  endif
                    {
                        info.compiled_call_info = nullptr;
                        info.compiled_func = nullptr;
                    }
# endif
                    info.param_bytes = param_bytes;
                    info.result_bytes = result_bytes;
                    mod_cache.index_unchecked(i) = info;
                }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                if(tiered_backend)
                {
                    // Reset tiered counters before any function executes so hotness sampling describes this run, not a previous host
                    // invocation that reused the runtime process.
                    rec.llvm_jit_lazy_compile_options.compile_options.route_wasm_calls_through_runtime_bridge = tiered_targets_backend;
                    rec.llvm_jit_lazy_compile_options.compile_options.lazy_defined_targets_are_atomic = tiered_targets_backend;
                    rec.tiered_full_compile_state.state.store(::uwvm2::utils::thread::lazy_compile_state::uncompiled, ::std::memory_order_relaxed);
                    store_tiered_full_ready(rec, false);
                    rec.tiered_switch_count = 0uz;
                    rec.tiered_interpreter_fallback_count = 0uz;
                    rec.tiered_entry_miss_count = 0uz;
                    rec.tiered_large_loop_sample_count = 0uz;
                    rec.tiered_large_long_run_ready = 0u;
                    rec.tiered_entry_hot_counters.clear();
                    rec.tiered_entry_hot_counters.resize(local_n);
                    rec.tiered_osr_request_counters.clear();
                    rec.tiered_osr_request_counters.resize(local_n);
                }
                else
                {
                    rec.tiered_full_compile_state.state.store(::uwvm2::utils::thread::lazy_compile_state::uncompiled, ::std::memory_order_relaxed);
                    store_tiered_full_ready(rec, false);
                    rec.tiered_interpreter_fallback_count = 0uz;
                    rec.tiered_entry_miss_count = 0uz;
                    rec.tiered_large_loop_sample_count = 0uz;
                    rec.tiered_large_long_run_ready = 0u;
                    rec.tiered_entry_hot_counters.clear();
                    rec.tiered_osr_request_counters.clear();
                }
# endif

                if(::uwvm2::runtime::llvm_jit_cache::default_cache_policy().enable)
                {
                    // Cacheable lazy LLVM IR must not depend on whether another lazy function happened to be materialized
                    // earlier in this process. Force dynamic target-table loads so identical local functions hash the same
                    // across cold and warm cache runs.
                    rec.llvm_jit_lazy_compile_options.compile_options.lazy_defined_targets_are_atomic = true;
                }

                rec.llvm_jit_lazy_direct_call_targets.clear();
                rec.llvm_jit_lazy_direct_call_targets.resize(local_n);
                // Direct-call targets are mutable publication cells. Generated code loads these cells and can therefore jump from a
                // lazy bridge to a newly materialized entry without recompiling the caller.
                for(::std::size_t i{}; i != local_n; ++i)
                {
                    auto& target{rec.llvm_jit_lazy_direct_call_targets.index_unchecked(i)};
                    auto const initial_entry_address{
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                        tiered_targets_backend ? reinterpret_cast<::std::uintptr_t>(tiered_raw_call_defined_entry) :
# endif
                                               reinterpret_cast<::std::uintptr_t>(llvm_jit_lazy_raw_call_defined_entry)};
                    ::std::atomic_ref<::std::uintptr_t>{target.entry_address}.store(initial_entry_address, ::std::memory_order_relaxed);
                    ::std::atomic_ref<::std::uintptr_t>{target.context_address}.store(
                        reinterpret_cast<::std::uintptr_t>(::std::addressof(mod_cache.index_unchecked(i))),
                        ::std::memory_order_relaxed);
                    target.encoded_type_id = find_canonical_type_id_for_sig(rec, func_sig_from_defined(mod_cache.index_unchecked(i).runtime_func));
                }

                rec.llvm_jit_lazy_compile_options.compile_options.lazy_defined_raw_call_target_base_address =
                    reinterpret_cast<::std::uintptr_t>(rec.llvm_jit_lazy_direct_call_targets.data());
                rec.llvm_jit_lazy_compile_options.compile_options.lazy_defined_raw_call_target_count = rec.llvm_jit_lazy_direct_call_targets.size();
                rec.llvm_jit_lazy_direct_typed_entry_targets.clear();
                rec.llvm_jit_lazy_direct_typed_entry_targets.resize(local_n);
                // Typed-entry publication slots are separate from raw-entry slots because call_indirect/type-aware calls use the
                // normal wasm function ABI while host/import bridges use the raw buffer ABI.
                rec.llvm_jit_lazy_compile_options.compile_options.lazy_defined_typed_entry_target_base_address =
                    reinterpret_cast<::std::uintptr_t>(rec.llvm_jit_lazy_direct_typed_entry_targets.data());
                rec.llvm_jit_lazy_compile_options.compile_options.lazy_defined_typed_entry_target_count = rec.llvm_jit_lazy_direct_typed_entry_targets.size();

                prepare_llvm_jit_lazy_background_request_contexts(rec);
            }

            if(!g_runtime.defined_func_ptr_ranges.empty())
            {
                ::std::sort(g_runtime.defined_func_ptr_ranges.begin(),
                            g_runtime.defined_func_ptr_ranges.end(),
                            [](defined_func_ptr_range const& a, defined_func_ptr_range const& b) constexpr noexcept { return a.begin < b.begin; });
            }

            g_import_call_cache.resize(g_runtime.modules.size());
            for(::std::size_t mid{}; mid != g_runtime.modules.size(); ++mid)
            {
                // Import caches must be rebuilt after lazy direct-call targets, because wasm-defined imports may point at placeholders.
                auto const& rec{g_runtime.modules.index_unchecked(mid)};
                auto const rt{rec.runtime_module};
                if(rt == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_n{rt->imported_function_vec_storage.size()};
                auto& cache{g_import_call_cache.index_unchecked(mid)};
                cache.clear();
                cache.resize(import_n);

                for(::std::size_t i{}; i != import_n; ++i)
                {
                    // Keep the import slot frame until resolution proves the final target is a wasm-defined function.
                    auto const imp{::std::addressof(rt->imported_function_vec_storage.index_unchecked(i))};
                    if(imp->import_type_ptr != nullptr && imp->import_type_ptr->module_name == u8"wasi_snapshot_preview1" &&
                       !is_wasip1_import_visible_for_runtime_module_id(mid)) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                    auto const rf{resolve_func_from_import_assuming_initialized(imp)};

                    cached_import_target tgt{};
                    tgt.frame.module_id = mid;
                    tgt.frame.function_index = i;

                    switch(rf.k)
                    {
                        case resolved_func::kind::defined:
                        {
                            auto const info{find_defined_func_info(rf.u.defined_ptr)};
                            if(info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                            tgt.k = cached_import_target::kind::defined;
                            tgt.frame.module_id = info->module_id;
                            tgt.frame.function_index = info->function_index;
                            tgt.sig = func_sig_from_defined(info->runtime_func);
                            tgt.param_bytes = info->param_bytes;
                            tgt.result_bytes = info->result_bytes;
                            tgt.u.defined.runtime_func = info->runtime_func;
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                            tgt.u.defined.compiled_func = info->compiled_func;
# endif
                            break;
                        }
                        case resolved_func::kind::local_imported:
                        {
                            tgt.k = cached_import_target::kind::local_imported;
                            tgt.u.local_imported = rf.u.local_imported;
                            tgt.sig = func_sig_from_local_imported(tgt.u.local_imported.module_ptr, tgt.u.local_imported.index);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::dl:
                        {
                            tgt.k = cached_import_target::kind::dl;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        case resolved_func::kind::weak_symbol:
                        {
                            tgt.k = cached_import_target::kind::weak_symbol;
                            tgt.u.capi_ptr = rf.u.capi_ptr;
                            tgt.preload_module_memory_attribute = find_preload_module_memory_attribute(tgt.u.capi_ptr);
                            tgt.sig = func_sig_from_capi(tgt.u.capi_ptr);
                            tgt.param_bytes = total_abi_bytes(tgt.sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt.sig.results);
                            if((tgt.param_bytes == 0uz && tgt.sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt.sig.results.size != 0uz)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                            break;
                        }
                        [[unlikely]] default:
                        {
                            ::fast_io::fast_terminate();
                        }
                    }

                    cache.index_unchecked(i) = tgt;
                }
            }

            g_runtime.lazy_compile_active = true;
            populate_llvm_jit_call_indirect_table_views();

            // Runtime log counters are reset after table views are live so the first execution sample reflects steady lazy operation.
            g_runtime.lazy_runtime_miss_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.lazy_runtime_compiled_hit_count.store(0uz, ::std::memory_order_relaxed);
            g_runtime.llvm_jit_urgent_request_count.store(0uz, ::std::memory_order_relaxed);
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            if(tiered_backend) { reset_tiered_runtime_log_metrics(); }
# endif
            g_runtime.lazy_prefetch_module_id = SIZE_MAX;
            g_runtime.lazy_prefetch_local_function_index = SIZE_MAX;
            g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);

            auto const main_it{g_runtime.module_name_to_id.find(main_module_name)};
            if(main_it != g_runtime.module_name_to_id.end())
            {
                // Resolve the configured entry through imports if necessary so background work follows the real wasm entry target.
                auto const main_id{main_it->second};
                auto const& main_rec{g_runtime.modules.index_unchecked(main_id)};
                auto const main_rt{main_rec.runtime_module};
                if(main_rt != nullptr)
                {
                    auto const import_n{main_rt->imported_function_vec_storage.size()};
                    auto const total_n{import_n + main_rt->local_defined_function_vec_storage.size()};
                    if(cfg.entry_function_index < total_n)
                    {
                        if(cfg.entry_function_index < import_n)
                        {
                            auto const& tgt{g_import_call_cache.index_unchecked(main_id).index_unchecked(cfg.entry_function_index)};
                            if(tgt.k == cached_import_target::kind::defined)
                            {
                                auto const target_module_id{tgt.frame.module_id};
                                if(target_module_id < g_runtime.modules.size())
                                {
                                    auto const& target_rec{g_runtime.modules.index_unchecked(target_module_id)};
                                    auto const target_rt{target_rec.runtime_module};
                                    if(target_rt != nullptr)
                                    {
                                        auto const target_import_n{target_rt->imported_function_vec_storage.size()};
                                        if(tgt.frame.function_index >= target_import_n)
                                        {
                                            g_runtime.lazy_prefetch_module_id = target_module_id;
                                            g_runtime.lazy_prefetch_local_function_index = tgt.frame.function_index - target_import_n;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            g_runtime.lazy_prefetch_module_id = main_id;
                            g_runtime.lazy_prefetch_local_function_index = cfg.entry_function_index - import_n;
                        }
                    }
                }
            }

            auto const llvm_lazy_background_enabled{!tiered_t0_backend};
            if(llvm_lazy_background_enabled && g_runtime.lazy_prefetch_module_id < g_runtime.modules.size())
            {
                auto& preferred_rec{g_runtime.modules.index_unchecked(g_runtime.lazy_prefetch_module_id)};
                // For LLVM lazy mode, seeding the direct-call graph around the entry function usually compiles a better first batch
                // than a simple linear prefetch.
                if(!seed_llvm_jit_lazy_background_entry_direct_graph(preferred_rec, g_runtime.lazy_prefetch_local_function_index))
                {
                    prioritize_llvm_jit_lazy_background_entry(preferred_rec, g_runtime.lazy_prefetch_local_function_index);
                }
            }

            auto const worker_count{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved};
            auto const has_lazy_background_work{llvm_lazy_background_enabled && worker_count != 0uz && has_llvm_jit_lazy_background_work()};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            bool has_tiered_urgent_scheduler_candidate{};
            bool tiered_defer_jit_scheduler_start{};
            if(tiered_t0_backend && worker_count != 0uz)
            {
                // The current urgent scheduler is a conservative background lane for
                // counter-hot entries and loop OSR.  Future policy should route these samples
                // through the execution thread and a main-thread coordinator, then optionally
                // fan out when WASI threads are implemented.
                bool has_medium_or_large_module{};
                for(auto const& rec: g_runtime.modules)
                {
                    auto const local_n{rec.llvm_jit_lazy_compiled.functions.size()};
                    if(local_n != 0uz)
                    {
                        has_tiered_urgent_scheduler_candidate = true;
                        if(local_n >= 128uz) { has_medium_or_large_module = true; }
                    }
                }
                tiered_defer_jit_scheduler_start = has_tiered_urgent_scheduler_candidate && !has_medium_or_large_module;
            }
            g_runtime.tiered_schedulers_deferred.store(tiered_defer_jit_scheduler_start, ::std::memory_order_release);
            g_runtime.tiered_deferred_worker_count = tiered_defer_jit_scheduler_start ? worker_count : 0uz;
# endif
            auto const lazy_scheduler_worker_count{
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                tiered_t0_backend ? (worker_count == 0uz || tiered_defer_jit_scheduler_start ? 0uz : 1uz) :
# endif
                                  (has_lazy_background_work ? worker_count : 0uz)};
            // Scheduler workers are intentionally split by urgency: normal lazy prefetch fills background code, while urgent/tiered
            // lanes are reserved for direct demand and loop OSR so they are not hidden behind broad module prefetching.
            g_runtime.lazy_scheduler.start({.worker_count = lazy_scheduler_worker_count,
                                            .queue_capacity = 0uz,
                                            .refill_callback = has_lazy_background_work ? &llvm_jit_lazy_background_refill_callback : nullptr,
                                            .refill_user_data = nullptr});
            g_runtime.llvm_jit_urgent_scheduler.start({.worker_count = 0uz, .queue_capacity = 0uz, .refill_callback = nullptr, .refill_user_data = nullptr});
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            auto const urgent_scheduler_worker_count{has_tiered_urgent_scheduler_candidate && !tiered_defer_jit_scheduler_start ? 1uz : 0uz};
            if(tiered_t0_backend && !tiered_defer_jit_scheduler_start)
            {
                g_runtime.tiered_urgent_scheduler.start({.worker_count = urgent_scheduler_worker_count,
                                                         .queue_capacity = urgent_scheduler_worker_count == 0uz ? 0uz : tiered_urgent_scheduler_queue_capacity,
                                                         .refill_callback = nullptr,
                                                         .refill_user_data = nullptr});
                verbose_log_registered_tiered_urgent_scheduler(urgent_scheduler_worker_count);
            }
            else
            {
                g_runtime.tiered_urgent_scheduler.stop();
            }
# endif
            if(has_lazy_background_work) { (void)llvm_jit_lazy_background_refill_callback(nullptr, g_runtime.lazy_scheduler); }
            g_runtime.compiled_all.store(true, ::std::memory_order_release);
            g_runtime.lazy_initialized.store(true, ::std::memory_order_release);
            lazy_init_lock.clear(::std::memory_order_release);
        }
#endif

    }  // namespace

#if defined(UWVM_RUNTIME_LLVM_JIT)
    // =========================================================================
    // LLVM-generated trap and logical stack callbacks
    // -------------------------------------------------------------------------
    // Generated code calls these extern "C++" helpers when a wasm trap must enter
    // the shared diagnostic path or when instruction-level logical call-stack
    // frames are enabled.
    //
    // Coverage invariants:
    // - Trap helpers capture the generated caller context before fatal reporting.
    // - Memory traps preserve detailed offset/length/type information.
    // - Push/pop callbacks are used only when codegen emitted logical stack frames.
    // - Tail-call disabling keeps the C++ helper visible to platform unwinders.
    // =========================================================================
    extern "C++"
# if UWVM_HAS_CPP_ATTRIBUTE(clang::disable_tail_calls)
        [[clang::disable_tail_calls]]
# endif
        UWVM_NOINLINE void llvm_jit_runtime_trap(llvm_jit_trap_kind k,
                                                 [[maybe_unused]] ::std::uintptr_t explicit_frame_address,
                                                 [[maybe_unused]] ::std::uintptr_t explicit_stack_pointer) noexcept
    {
        // JIT code reaches traps through a C++ runtime entry so we can capture a native return/frame address before the reporting path
        // unwinds or terminates. The wasm trap kind is then translated to the shared interpreter/JIT diagnostic machinery.
# if UWVM_HAS_BUILTIN(__builtin_return_address)
        auto const return_address{reinterpret_cast<::std::uintptr_t>(__builtin_return_address(0))};
# else
        constexpr ::std::uintptr_t return_address{};
# endif
# if !UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
#  if UWVM_HAS_BUILTIN(__builtin_frame_address)
        // Prefer the generated caller's frame pointer when the IR trap bridge supplied it.  Starting the seeded
        // unwind from the C++ helper frame can lose Wasm callers across host-call ABI boundaries, especially for
        // detailed memory traps.
        auto const helper_frame_address{reinterpret_cast<::std::uintptr_t>(__builtin_frame_address(0))};
        auto const frame_address{explicit_frame_address == 0u ? helper_frame_address : explicit_frame_address};
#  else
        auto const frame_address{explicit_frame_address};
#  endif
# else
        auto const frame_address{explicit_frame_address};
# endif
        store_llvm_jit_trap_context(k, return_address, frame_address, explicit_stack_pointer);
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        store_llvm_jit_win64_trap_caller_context(return_address, frame_address, explicit_stack_pointer);
# endif
        switch(k)
        {
            case llvm_jit_trap_kind::unreachable:
            {
                trap_fatal(trap_kind::unreachable);
                return;
            }
            case llvm_jit_trap_kind::invalid_conversion_to_integer:
            {
                trap_fatal(trap_kind::invalid_conversion_to_integer);
                return;
            }
            case llvm_jit_trap_kind::integer_divide_by_zero:
            {
                trap_fatal(trap_kind::integer_divide_by_zero);
                return;
            }
            case llvm_jit_trap_kind::integer_overflow:
            {
                trap_fatal(trap_kind::integer_overflow);
                return;
            }
            case llvm_jit_trap_kind::call_indirect_table_out_of_bounds:
            {
                trap_fatal(trap_kind::call_indirect_table_out_of_bounds);
                return;
            }
            case llvm_jit_trap_kind::call_indirect_null_element:
            {
                trap_fatal(trap_kind::call_indirect_null_element);
                return;
            }
            case llvm_jit_trap_kind::call_indirect_type_mismatch:
            {
                trap_fatal(trap_kind::call_indirect_type_mismatch);
                return;
            }
            case llvm_jit_trap_kind::memory_out_of_bounds:
            {
                trap_fatal(trap_kind::memory_out_of_bounds);
                return;
            }
            case llvm_jit_trap_kind::runtime_invariant_failure:
            {
                trap_fatal(trap_kind::runtime_invariant_failure);
                return;
            }
            [[unlikely]] default:
            {
                trap_fatal(trap_kind::runtime_invariant_failure);
                return;
            }
        }
    }

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
                                                              [[maybe_unused]] ::std::uintptr_t explicit_frame_address,
                                                              [[maybe_unused]] ::std::uintptr_t explicit_stack_pointer) noexcept
    {
        // Memory traps carry the computed dynamic offset and static offset separately. Keeping both values lets diagnostics explain
        // the failing wasm memory access instead of only reporting a generic bounds violation.
# if UWVM_HAS_BUILTIN(__builtin_return_address)
        auto const return_address{reinterpret_cast<::std::uintptr_t>(__builtin_return_address(0))};
# else
        constexpr ::std::uintptr_t return_address{};
# endif
# if !UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
#  if UWVM_HAS_BUILTIN(__builtin_frame_address)
        // Detailed memory traps already receive the generated frame pointer from IR.  Use it on DWARF/libunwind
        // targets too so the seeded unwind walks the Wasm caller chain instead of the helper's host frame.
        auto const helper_frame_address{reinterpret_cast<::std::uintptr_t>(__builtin_frame_address(0))};
        auto const frame_address{explicit_frame_address == 0u ? helper_frame_address : explicit_frame_address};
#  else
        auto const frame_address{explicit_frame_address};
#  endif
# else
        auto const frame_address{explicit_frame_address};
# endif
        store_llvm_jit_trap_context(llvm_jit_trap_kind::memory_out_of_bounds, return_address, frame_address, explicit_stack_pointer);
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        store_llvm_jit_win64_trap_caller_context(return_address, frame_address, explicit_stack_pointer);
# endif

        ::uwvm2::object::memory::error::memory_error_t const memerr{
            .memory_idx = memory_idx,
            .memory_offset = {.offset = memory_offset, .offset_65_bit = offset_65_bit != 0u},
            .memory_static_offset = memory_static_offset,
            .memory_length = memory_length,
            .memory_type_size = memory_type_size
        };
        print_memory_out_of_bounds_trap(memerr);

        using terminate_func_t = void (*)() noexcept;
        terminate_func_t volatile terminate_func{::fast_io::fast_terminate};
        terminate_func();
        ::std::atomic_signal_fence(::std::memory_order_seq_cst);
    }

    extern "C++" void llvm_jit_push_call_stack_frame(::std::size_t module_id, ::std::size_t function_index) noexcept
    {
        // Generated LLVM code calls this only when instruction-level logical call-stack frames are enabled.
        get_call_stack().push(call_stack_frame{module_id, function_index});
    }

    extern "C++" void llvm_jit_pop_call_stack_frame() noexcept
    {
        // Balance llvm_jit_push_call_stack_frame from generated epilogues and trap-safe cleanup paths.
        get_call_stack().pop();
    }
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
    // =========================================================================
    // Lazy-mode host entry point
    // -------------------------------------------------------------------------
    // The host calls this for bounded lazy execution. Initialization builds shared
    // runtime metadata, the selected backend executes one entry function, then
    // lazy schedulers are stopped before returning to the embedding host.
    //
    // Coverage invariants:
    // - Entry ABI buffers are validated before any copy or raw JIT call.
    // - Imported entries must resolve to wasm-defined code.
    // - Tiered mode can choose direct LLVM, no-T0 demand LLVM, or interpreter T0.
    // - Current-thread runtime state is erased after the run.
    // =========================================================================
    extern "C++" void lazy_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, lazy_compile_run_config cfg) noexcept
    {
        // Lazy execution initializes backend metadata, validates the host-provided entry ABI buffers, then invokes exactly one entry
        // function. Background schedulers are stopped before return because the current host API models a bounded run.
        auto const lazy_log_enabled{::uwvm2::uwvm::io::enable_runtime_log};
        auto const lazy_run_start{lazy_log_enabled ? lazy_clock_now() : ::fast_io::unix_timestamp{}};

# if defined(UWVM_RUNTIME_LLVM_JIT)
        auto const runtime_compiler{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler};
        auto const llvm_jit_lazy_backend{runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only};
        auto const tiered_lazy_backend{
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            runtime_compiler == ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_llvm_jit_tiered
#  else
            false
#  endif
        };
        // Select the lazy initializer matching the runtime compiler. Tiered LLVM may also build interpreter T0 metadata inside the
        // LLVM lazy initializer.
        if(llvm_jit_lazy_backend || tiered_lazy_backend) { initialize_llvm_jit_lazy_modules_if_needed(main_module_name, cfg); }
        else
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            initialize_lazy_modules_if_needed(main_module_name, cfg);
# else
        {
            ::fast_io::fast_terminate();
        }
# endif

        auto const it{g_runtime.module_name_to_id.find(main_module_name)};
        if(it == g_runtime.module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const main_id{it->second};

        auto const main_module{g_runtime.modules.index_unchecked(main_id).runtime_module};
        if(main_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        auto& entry_wasip1_env{resolve_wasip1_env_for_runtime_module_id(main_id)};
        bind_wasip1_memory_for_selected_env(entry_wasip1_env, main_id);
        ::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_env_t entry_wasip1_env_guard{entry_wasip1_env};
# endif

        auto const import_n{main_module->imported_function_vec_storage.size()};
        auto const total_n{import_n + main_module->local_defined_function_vec_storage.size()};
        if(cfg.entry_function_index >= total_n) [[unlikely]] { ::fast_io::fast_terminate(); }

# if defined(UWVM_RUNTIME_LLVM_JIT)
        // Imported wasm entry functions are redirected to their provider module/function for direct LLVM entry dispatch.
        ::std::size_t llvm_jit_entry_module_id{main_id};
        ::std::size_t llvm_jit_entry_function_index{cfg.entry_function_index};
# endif

        [[maybe_unused]] ::std::size_t param_bytes{};
        [[maybe_unused]] ::std::size_t result_bytes{};
        if(cfg.entry_function_index < import_n)
        {
            // An imported entry is allowed only when it ultimately resolves to wasm-defined code; native imports do not have a wasm
            // frame to serve as the program entry point.
            if(main_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& cache{g_import_call_cache.index_unchecked(main_id)};
            if(cfg.entry_function_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& tgt{cache.index_unchecked(cfg.entry_function_index)};

            if(tgt.k != cached_import_target::kind::defined) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function is imported but resolves to a non-wasm implementation (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            param_bytes = tgt.param_bytes;
            result_bytes = tgt.result_bytes;
            validate_entry_run_buffers(main_module_name,
                                       tgt.sig.params.size,
                                       param_bytes,
                                       tgt.sig.results.size,
                                       result_bytes,
                                       cfg.entry_abi_buffers.param_buffer,
                                       cfg.entry_abi_buffers.param_bytes,
                                       cfg.entry_abi_buffers.result_buffer,
                                       cfg.entry_abi_buffers.result_bytes);
# if defined(UWVM_RUNTIME_LLVM_JIT)
            llvm_jit_entry_module_id = tgt.frame.module_id;
            llvm_jit_entry_function_index = tgt.frame.function_index;
# endif
        }
        else
        {
            // Local entries validate against the defining function type and use the defined-function cache for ABI byte sizes.
            auto const local_index{cfg.entry_function_index - import_n};
            if(main_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(main_id)};
            if(local_index >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& entry_info{mod_cache.index_unchecked(local_index)};
            auto const expected_rt{::std::addressof(main_module->local_defined_function_vec_storage.index_unchecked(local_index))};
            if(entry_info.runtime_func != expected_rt) [[unlikely]] { ::fast_io::fast_terminate(); }

            param_bytes = entry_info.param_bytes;
            result_bytes = entry_info.result_bytes;
            auto const ft{expected_rt->function_type_ptr};
            if(ft == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            validate_entry_run_buffers(main_module_name,
                                       static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin),
                                       param_bytes,
                                       static_cast<::std::size_t>(ft->result.end - ft->result.begin),
                                       result_bytes,
                                       cfg.entry_abi_buffers.param_buffer,
                                       cfg.entry_abi_buffers.param_bytes,
                                       cfg.entry_abi_buffers.result_buffer,
                                       cfg.entry_abi_buffers.result_bytes);
        }

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // Interpreter entry uses the same byte-packed operand stack convention as internal calls, which keeps host entry ABI handling
        // identical to import and raw-call bridges.
        if(param_bytes > (::std::numeric_limits<::std::size_t>::max() - result_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const stack_bytes{param_bytes + result_bytes};

        heap_buf_guard host_stack_guard{};
        ::std::byte* host_stack_base{};
        UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);
        if(param_bytes != 0uz) { ::std::memcpy(host_stack_base, cfg.entry_abi_buffers.param_buffer, param_bytes); }
        ::std::byte* stack_top_ptr{host_stack_base + param_bytes};
        bool entry_result_on_stack{};
# endif

        auto const lazy_exec_start{lazy_log_enabled ? lazy_clock_now() : ::fast_io::unix_timestamp{}};
        ::uwvm2::uwvm::global::record_total_wasm_time_start();
# if defined(UWVM_RUNTIME_LLVM_JIT)
        if(llvm_jit_lazy_backend)
        {
            if(!prepare_lazy_llvm_jit_unwind_native_call_graph(llvm_jit_entry_module_id, llvm_jit_entry_function_index)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            // LLVM lazy-only mode can enter the raw generated wrapper directly; tiered mode may still choose an interpreter T0 bridge.
            if(!try_invoke_runtime_llvm_jit_raw_defined_entry(llvm_jit_entry_module_id,
                                                              llvm_jit_entry_function_index,
                                                              cfg.entry_abi_buffers.result_buffer,
                                                              result_bytes,
                                                              cfg.entry_abi_buffers.param_buffer,
                                                              param_bytes)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
        }
        else
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        {
#  ifdef UWVM_CPP_EXCEPTIONS
            try
#  endif
            {
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
                if(tiered_lazy_backend && llvm_jit_entry_module_id < g_runtime.modules.size())
                {
                    // Tiered entry dispatch first checks whether initialization or earlier background work already published a raw
                    // LLVM entry for the selected function.
                    bool invoked_tiered_entry{};
                    auto const& entry_rec{g_runtime.modules.index_unchecked(llvm_jit_entry_module_id)};
                    auto const entry_runtime_module{entry_rec.runtime_module};
                    if(entry_runtime_module != nullptr)
                    {
                        auto const entry_import_n{entry_runtime_module->imported_function_vec_storage.size()};
                        if(llvm_jit_entry_function_index >= entry_import_n)
                        {
                            auto const entry_local_index{llvm_jit_entry_function_index - entry_import_n};
                            if(entry_local_index < entry_rec.llvm_jit_lazy_direct_call_targets.size())
                            {
                                auto const& target{entry_rec.llvm_jit_lazy_direct_call_targets.index_unchecked(entry_local_index)};
                                auto& entry_address_ref{const_cast<::std::uintptr_t&>(target.entry_address)};
                                auto& context_address_ref{const_cast<::std::uintptr_t&>(target.context_address)};
                                auto const entry_address{::std::atomic_ref<::std::uintptr_t>{entry_address_ref}.load(::std::memory_order_acquire)};
                                auto const context_address{::std::atomic_ref<::std::uintptr_t>{context_address_ref}.load(::std::memory_order_acquire)};
                                if(entry_address != 0u)
                                {
                                    if(!tiered_t0_enabled() &&
                                       !prepare_lazy_llvm_jit_unwind_native_call_graph(llvm_jit_entry_module_id, llvm_jit_entry_function_index, true))
                                    {
                                        ::fast_io::fast_terminate();
                                    }

                                    using entry_fn_t = void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t,
                                                                                                       ::std::uintptr_t,
                                                                                                       ::std::size_t,
                                                                                                       ::std::uintptr_t,
                                                                                                       ::std::size_t);
                                    auto const entry_fn{reinterpret_cast<entry_fn_t>(entry_address)};
                                    if(!tiered_t0_enabled())
                                    {
                                        auto& call_stack{get_call_stack()};
                                        call_stack_guard g{call_stack, llvm_jit_entry_module_id, llvm_jit_entry_function_index};
                                        entry_fn(context_address,
                                                 pointer_to_uintptr(cfg.entry_abi_buffers.result_buffer),
                                                 result_bytes,
                                                 pointer_to_uintptr(cfg.entry_abi_buffers.param_buffer),
                                                 param_bytes);
                                    }
                                    else
                                    {
                                        // T0-enabled tiered mode can still enter a published raw JIT entry directly from the host
                                        // when unwind mode or prior preparation made the entry target ready before dispatch. Keep the
                                        // wasm entry frame on the logical stack so OSR/full-ready traps below it can merge the host
                                        // entry edge with native unwind output instead of losing func_idx=entry.
                                        auto& call_stack{get_call_stack()};
                                        call_stack_guard g{call_stack, llvm_jit_entry_module_id, llvm_jit_entry_function_index};
                                        entry_fn(context_address,
                                                 pointer_to_uintptr(cfg.entry_abi_buffers.result_buffer),
                                                 result_bytes,
                                                 pointer_to_uintptr(cfg.entry_abi_buffers.param_buffer),
                                                 param_bytes);
                                    }
                                    invoked_tiered_entry = true;
                                }
                            }
                        }
                    }

                    if(!invoked_tiered_entry)
                    {
                        // If no generated entry is ready, no-T0 tiered mode forces LLVM demand compilation while T0-enabled mode
                        // falls back to the tier-aware interpreter bridge.
                        if(!tiered_t0_enabled())
                        {
                            if(!try_invoke_runtime_llvm_jit_raw_defined_entry(llvm_jit_entry_module_id,
                                                                              llvm_jit_entry_function_index,
                                                                              cfg.entry_abi_buffers.result_buffer,
                                                                              result_bytes,
                                                                              cfg.entry_abi_buffers.param_buffer,
                                                                              param_bytes,
                                                                              true,
                                                                              true)) [[unlikely]]
                            {
                                ::fast_io::fast_terminate();
                            }
                        }
                        else
                        {
                            tiered_call_bridge(main_id, cfg.entry_function_index, ::std::addressof(stack_top_ptr));
                            entry_result_on_stack = true;
                        }
                    }
                }
                else
#  endif
                {
                    call_bridge(main_id, cfg.entry_function_index, ::std::addressof(stack_top_ptr));
                    entry_result_on_stack = true;
                }
            }
#  ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                trap_fatal(trap_kind::uncatched_int_tag);
            }
#  endif
        }
# else
        {
            ::fast_io::fast_terminate();
        }
# endif

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        if(entry_result_on_stack && result_bytes != 0uz) { ::std::memcpy(cfg.entry_abi_buffers.result_buffer, host_stack_base, result_bytes); }
# endif

        ::uwvm2::uwvm::global::record_total_wasm_time_end();
        auto const lazy_exec_end{lazy_log_enabled ? lazy_clock_now() : ::fast_io::unix_timestamp{}};
        erase_current_thread_state();
        // Stop lazy workers after the bounded run so embedding hosts can observe a quiescent runtime before process exit or reset.
        g_runtime.lazy_scheduler.stop();
# if defined(UWVM_RUNTIME_LLVM_JIT)
        g_runtime.llvm_jit_urgent_scheduler.stop();
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        g_runtime.tiered_urgent_scheduler.stop();
# endif

        if(lazy_log_enabled)
        {
            auto const lazy_stop_end{lazy_clock_now()};
            print_lazy_runtime_compiler_log(lazy_run_start, lazy_exec_start, lazy_exec_end, lazy_stop_end);
        }
    }
#endif

    // =========================================================================
    // Full-compile host entry point
    // -------------------------------------------------------------------------
    // The host calls this when all runtime artifacts should be ready before entry
    // execution. LLVM-capable modes try the raw native entry first, then apply the
    // configured fatal/fallback policy.
    //
    // Coverage invariants:
    // - Eager caches are built before entry lookup.
    // - Imported wasm entries are redirected to their provider module/function.
    // - LLVM-only mode treats native entry unavailability as fatal.
    // - Interpreter fallback uses the same ABI stack staging as lazy mode.
    // =========================================================================
    extern "C++" void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config cfg) noexcept
    {
        // Full execution forces all requested backend artifacts to be ready before selecting the entry. This keeps the run path simple
        // and makes JIT fallback policy an explicit choice below.
        compile_all_modules_if_needed();

        auto const it{g_runtime.module_name_to_id.find(main_module_name)};
        if(it == g_runtime.module_name_to_id.end()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const main_id{it->second};

        auto const main_module{g_runtime.modules.index_unchecked(main_id).runtime_module};
        if(main_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        // Keep the entry module's WASI environment selected across the hot run loop.
        auto& entry_wasip1_env{resolve_wasip1_env_for_runtime_module_id(main_id)};
        bind_wasip1_memory_for_selected_env(entry_wasip1_env, main_id);
        ::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_env_t entry_wasip1_env_guard{entry_wasip1_env};
#endif

        auto const import_n{main_module->imported_function_vec_storage.size()};
        auto const total_n{import_n + main_module->local_defined_function_vec_storage.size()};
        if(cfg.entry_function_index >= total_n) [[unlikely]] { ::fast_io::fast_terminate(); }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        // Track provider module/function separately because the configured entry can be an import alias to wasm-defined code.
        ::std::size_t llvm_jit_entry_module_id{main_id};
        ::std::size_t llvm_jit_entry_function_index{cfg.entry_function_index};
#endif

        // Allocate the exact host-call stack space required by the entry function signature.
        // Layout: [params...] then call; the callee pops params and pushes results.
        [[maybe_unused]] ::std::size_t param_bytes{};
        [[maybe_unused]] ::std::size_t result_bytes{};
        if(cfg.entry_function_index < import_n)
        {
            if(main_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& cache{g_import_call_cache.index_unchecked(main_id)};
            if(cfg.entry_function_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& tgt{cache.index_unchecked(cfg.entry_function_index)};

            // For VM entry, only allow imported functions that ultimately resolve to a wasm-defined function.
            if(tgt.k != cached_import_target::kind::defined) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Entry function is imported but resolves to a non-wasm implementation (module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\").\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            param_bytes = tgt.param_bytes;
            result_bytes = tgt.result_bytes;
            validate_entry_run_buffers(main_module_name,
                                       tgt.sig.params.size,
                                       param_bytes,
                                       tgt.sig.results.size,
                                       result_bytes,
                                       cfg.entry_abi_buffers.param_buffer,
                                       cfg.entry_abi_buffers.param_bytes,
                                       cfg.entry_abi_buffers.result_buffer,
                                       cfg.entry_abi_buffers.result_bytes);
#if defined(UWVM_RUNTIME_LLVM_JIT)
            llvm_jit_entry_module_id = tgt.frame.module_id;
            llvm_jit_entry_function_index = tgt.frame.function_index;
#endif
        }
        else
        {
            // Direct local entries read ABI sizes from the cache built during full compilation.
            auto const local_index{cfg.entry_function_index - import_n};
            if(main_id >= g_runtime.defined_func_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& mod_cache{g_runtime.defined_func_cache.index_unchecked(main_id)};
            if(local_index >= mod_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& entry_info{mod_cache.index_unchecked(local_index)};
            auto const expected_rt{::std::addressof(main_module->local_defined_function_vec_storage.index_unchecked(local_index))};
            if(entry_info.runtime_func != expected_rt) [[unlikely]] { ::fast_io::fast_terminate(); }

            param_bytes = entry_info.param_bytes;
            result_bytes = entry_info.result_bytes;
            auto const ft{expected_rt->function_type_ptr};
            if(ft == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            validate_entry_run_buffers(main_module_name,
                                       static_cast<::std::size_t>(ft->parameter.end - ft->parameter.begin),
                                       param_bytes,
                                       static_cast<::std::size_t>(ft->result.end - ft->result.begin),
                                       result_bytes,
                                       cfg.entry_abi_buffers.param_buffer,
                                       cfg.entry_abi_buffers.param_bytes,
                                       cfg.entry_abi_buffers.result_buffer,
                                       cfg.entry_abi_buffers.result_bytes);
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        if(runtime_compiler_requests_llvm_jit_translation())
        {
            // Prefer the fully materialized native entry in LLVM-capable modes. Interpreter fallback is allowed only when the selected
            // runtime compiler does not require JIT execution.
            ::uwvm2::uwvm::global::record_total_wasm_time_start();
            if(try_invoke_runtime_llvm_jit_raw_defined_entry(llvm_jit_entry_module_id,
                                                             llvm_jit_entry_function_index,
                                                             cfg.entry_abi_buffers.result_buffer,
                                                             result_bytes,
                                                             cfg.entry_abi_buffers.param_buffer,
                                                             param_bytes))
            {
                ::uwvm2::uwvm::global::record_total_wasm_time_end();
                erase_current_thread_state();
                return;
            }
            ::uwvm2::uwvm::global::discard_total_wasm_time_record();

            if(runtime_compiler_requires_llvm_jit_execution()) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"LLVM JIT entry execution is unavailable for module=\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    main_module_name,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\", func_idx=",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    cfg.entry_function_index,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"LLVM JIT entry execution is unavailable for module=\"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                main_module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\", func_idx=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                cfg.entry_function_index,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"; falling back to interpreter execution.\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
        }
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // Interpreter fallback uses the same stack-buffer staging as lazy execution and internal call bridges.
        if(param_bytes > (::std::numeric_limits<::std::size_t>::max() - result_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const stack_bytes{param_bytes + result_bytes};

        heap_buf_guard host_stack_guard{};
        ::std::byte* host_stack_base{};
        UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);
        if(param_bytes != 0uz) { ::std::memcpy(host_stack_base, cfg.entry_abi_buffers.param_buffer, param_bytes); }
        ::std::byte* stack_top_ptr{host_stack_base + param_bytes};

# ifdef UWVM_CPP_EXCEPTIONS
        try
# endif
        {
            ::uwvm2::uwvm::global::record_total_wasm_time_start();
            call_bridge(main_id, cfg.entry_function_index, ::std::addressof(stack_top_ptr));
            ::uwvm2::uwvm::global::record_total_wasm_time_end();
        }
# ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            trap_fatal(trap_kind::uncatched_int_tag);
        }
# endif
        if(result_bytes != 0uz) { ::std::memcpy(cfg.entry_abi_buffers.result_buffer, host_stack_base, result_bytes); }

        // Currently only main-thread execution exists. Clean up current thread state on exit to avoid state growth and
        // possible thread-id reuse issues. Do NOT `clear()` here: main-thread exit does not imply other threads exit.
        erase_current_thread_state();
#endif
    }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
    template <typename TrailerWriter, typename InvokeBridge>
    inline constexpr void llvm_jit_invoke_raw_host_bridge_common(void const* runtime_module_ptr,
                                                                 void* result_buffer,
                                                                 ::std::size_t result_bytes,
                                                                 void const* param_buffer,
                                                                 ::std::size_t param_bytes,
                                                                 ::std::size_t trailer_bytes,
                                                                 TrailerWriter&& trailer_writer,
                                                                 InvokeBridge&& invoke_bridge) noexcept
    {
        // Raw host bridge helpers are used by generated JIT wrappers that need to re-enter interpreter call bridges. The optional
        // trailer writes operands such as function/table indices after the normal parameter payload.
        compile_all_modules_if_needed();
        ensure_bridges_initialized();

        if((result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const runtime_module_storage_ptr{static_cast<runtime_module_storage_t const*>(runtime_module_ptr)};
        auto const wasm_module_id{find_runtime_module_id_from_storage_ptr(runtime_module_storage_ptr)};
        if(wasm_module_id == ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::fast_io::fast_terminate(); }

        if(param_bytes > (::std::numeric_limits<::std::size_t>::max() - trailer_bytes)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const input_bytes{param_bytes + trailer_bytes};
        auto const stack_bytes{input_bytes < result_bytes ? result_bytes : input_bytes};

        heap_buf_guard host_stack_guard{};
        ::std::byte* host_stack_base{};
        UWVM_STACK_OR_HEAP_ALLOC_ZEROED_BYTES_NONNULL(host_stack_base, stack_bytes, host_stack_guard);

        if(param_bytes != 0uz) { ::std::memcpy(host_stack_base, param_buffer, param_bytes); }
        if(trailer_bytes != 0uz) { trailer_writer(host_stack_base + param_bytes); }

        ::std::byte* stack_top_ptr{host_stack_base + input_bytes};

# ifdef UWVM_CPP_EXCEPTIONS
        try
# endif
        {
            invoke_bridge(wasm_module_id, ::std::addressof(stack_top_ptr));
        }
# ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            trap_fatal(trap_kind::uncatched_int_tag);
        }
# endif

        if(result_bytes != 0uz) { ::std::memcpy(result_buffer, host_stack_base, result_bytes); }
    }
#endif

    // =========================================================================
    // Host-facing runtime maintenance APIs
    // -------------------------------------------------------------------------
    // These functions are called by embedding code rather than by wasm execution.
    // They stop lazy workers, reset runtime registries, bridge raw host calls into
    // the active backend, and expose preload memory descriptors/copy operations.
    //
    // Coverage invariants:
    // - Shutdown/reset must stop schedulers before clearing records they reference.
    // - Raw host calls validate the runtime module pointer against current registries.
    // - Import calls preserve WASI/preload context through cached target metadata.
    // - Preload memory APIs are thin wrappers over the active call context.
    // =========================================================================
    extern "C++" void lazy_compile_stop_before_proc_exit_host_api() noexcept
    {
        // Host/process shutdown stops background compilers before global objects begin destruction. This avoids worker threads
        // touching module records or LLVM state whose lifetime is about to end.
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
        g_runtime.lazy_scheduler.stop();
# if defined(UWVM_RUNTIME_LLVM_JIT)
        g_runtime.llvm_jit_urgent_scheduler.stop();
# endif
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        g_runtime.tiered_urgent_scheduler.stop();
# endif
#endif
    }

#if defined(UWVM_RUNTIME_LLVM_JIT)
    extern "C++" void llvm_jit_reset_runtime_state_host_api() noexcept
    {

        // Reset is intended for embedding hosts that reload runtime storage in the same process. Stop schedulers first, then drop
        // caches and publication flags so the next entry path rebuilds a coherent runtime registry.
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT)
        g_runtime.lazy_scheduler.stop();
#  if defined(UWVM_RUNTIME_LLVM_JIT)
        g_runtime.llvm_jit_urgent_scheduler.stop();
#  endif
#  if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        g_runtime.tiered_urgent_scheduler.stop();
#  endif
        g_runtime.lazy_initialized.store(false, ::std::memory_order_release);
        g_runtime.lazy_compile_active = false;
        g_runtime.lazy_prefetch_module_id = SIZE_MAX;
        g_runtime.lazy_prefetch_local_function_index = SIZE_MAX;
        g_runtime.lazy_runtime_miss_count.store(0uz, ::std::memory_order_relaxed);
        g_runtime.lazy_runtime_compiled_hit_count.store(0uz, ::std::memory_order_relaxed);
        g_runtime.lazy_prefetch_lock.clear(::std::memory_order_release);
# endif

        g_runtime.modules.clear();
        g_runtime.module_name_to_id.clear();
        g_runtime.defined_func_cache.clear();
        g_runtime.defined_func_ptr_ranges.clear();
        g_import_call_cache.clear();
# if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        g_wasip1_runtime_module_context_cache.clear();
# endif

        g_runtime.compiled_all.store(false, ::std::memory_order_release);
        g_runtime.bridges_initialized.store(false, ::std::memory_order_release);

        erase_current_thread_state();
    }

    extern "C++" void llvm_jit_call_raw_host_api(void const* runtime_module_ptr,
                                                 ::std::uint_least32_t func_index,
                                                 void* result_buffer,
                                                 ::std::size_t result_bytes,
                                                 void const* param_buffer,
                                                 ::std::size_t param_bytes) noexcept
    {
        // External raw calls use explicit ABI byte buffers and a runtime module pointer supplied by the host. The function validates
        // the pointer against the runtime registry before dispatching to either a cached import or a local defined entry.
        compile_all_modules_if_needed(false);

        if((result_bytes != 0uz && result_buffer == nullptr) || (param_bytes != 0uz && param_buffer == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const runtime_module_storage_ptr{static_cast<runtime_module_storage_t const*>(runtime_module_ptr)};
        auto const wasm_module_id{find_runtime_module_id_from_storage_ptr(runtime_module_storage_ptr)};
        if(wasm_module_id == ::std::numeric_limits<::std::size_t>::max()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& rec{g_runtime.modules.index_unchecked(wasm_module_id)};
        auto const curr_runtime_module{rec.runtime_module};
        if(curr_runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const import_n{curr_runtime_module->imported_function_vec_storage.size()};
        if(func_index < import_n)
        {
            if(wasm_module_id >= g_import_call_cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto const& cache{g_import_call_cache.index_unchecked(wasm_module_id)};
            if(func_index >= cache.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& tgt{cache.index_unchecked(func_index)};
            if(param_bytes != tgt.param_bytes || result_bytes != tgt.result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
            bool allow_tiered_llvm_lazy{};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
            allow_tiered_llvm_lazy = tiered_runtime_active() && !tiered_t0_enabled();
# endif

            switch(tgt.k)
            {
                case cached_import_target::kind::defined:
                {
                    if(try_invoke_runtime_llvm_jit_raw_defined_entry(tgt.frame.module_id,
                                                                     tgt.frame.function_index,
                                                                     result_buffer,
                                                                     result_bytes,
                                                                     param_buffer,
                                                                     param_bytes,
                                                                     allow_tiered_llvm_lazy))
                    {
                        return;
                    }
                    ::fast_io::fast_terminate();
                }
                case cached_import_target::kind::local_imported:
                {
                    auto const local_imported_module{tgt.u.local_imported.module_ptr};
                    if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    call_local_imported_with_wasip1_env(*local_imported_module,
                                                        tgt.u.local_imported.index,
                                                        static_cast<::std::byte*>(result_buffer),
                                                        const_cast<::std::byte*>(static_cast<::std::byte const*>(param_buffer)),
                                                        tgt.frame.module_id);
                    return;
                }
                case cached_import_target::kind::dl:
                case cached_import_target::kind::weak_symbol:
                {
                    auto const capi_ptr{tgt.u.capi_ptr};
                    if(capi_ptr == nullptr || capi_ptr->func_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    call_capi_with_wasip1_env(*capi_ptr,
                                              tgt.preload_module_memory_attribute,
                                              static_cast<::std::byte*>(result_buffer),
                                              const_cast<::std::byte*>(static_cast<::std::byte const*>(param_buffer)),
                                              tgt.frame.module_id);
                    return;
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
        }

        bool allow_tiered_llvm_lazy{};
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
        allow_tiered_llvm_lazy = tiered_runtime_active() && !tiered_t0_enabled();
# endif
        if(try_invoke_runtime_llvm_jit_raw_defined_entry(wasm_module_id,
                                                         func_index,
                                                         result_buffer,
                                                         result_bytes,
                                                         param_buffer,
                                                         param_bytes,
                                                         allow_tiered_llvm_lazy))
        {
            return;
        }

        ::fast_io::fast_terminate();
    }
#endif

    // Preload memory host APIs expose only the memories selected by the active preload call context. They are thin wrappers so the C
    // preload surface stays stable while descriptor construction remains centralized in the runtime helpers above.
    //
    // Descriptor coverage:
    // - count/at operate on policy-visible memories, not raw runtime memory indices.
    // - read/write perform backend-specific range validation before copying bytes.
    // - zero-length operations are accepted when the active descriptor resolves.
    // - mmap descriptors expose direct views only when rights and backend state allow it.
    extern "C++" [[nodiscard]] ::std::size_t preload_memory_descriptor_count_host_api() noexcept
    {
        // Count the policy-visible preload memories for the currently active runtime call context.
        return preload_memory_descriptor_count_impl();
    }

    extern "C++" [[nodiscard]] bool preload_memory_descriptor_at_host_api(::std::size_t descriptor_index, preload_memory_descriptor_t* out) noexcept
    {
        // Return the Nth visible descriptor using the stable C-facing preload_memory_descriptor_t layout.
        return preload_memory_descriptor_at_impl(descriptor_index, out);
    }

    extern "C++" [[nodiscard]] bool
        preload_memory_read_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept
    {
        // Copy from a selected preload memory after backend-specific bounds and rights checks.
        return preload_memory_read_impl(memory_index, offset, destination, size);
    }

    extern "C++" [[nodiscard]] bool
        preload_memory_write_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept
    {
        // Copy into a selected preload memory after backend-specific bounds and rights checks.
        return preload_memory_write_impl(memory_index, offset, source, size);
    }

}  // namespace uwvm2::runtime::lib

#pragma pop_macro("UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR")
#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI")

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
#endif
