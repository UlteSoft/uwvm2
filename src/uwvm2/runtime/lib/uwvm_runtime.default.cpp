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
// storage, import resolution, interpreter callbacks, optional LLVM materialization, WASI context selection,
// and host-facing entry points so every backend observes one coherent module-id space.
//
// Reading map:
// - The anonymous namespace owns all process-local runtime state, caches, trap helpers, and backend bridges.
// - `runtime_global_state` is the canonical registry; every function index, type cache, import cache, and JIT address table is
//   keyed from the module ids stored there.
// - Interpreter paths build byte-packed local/operand frames and call generated opfunc streams.
// - LLVM paths materialize full-module engines, then publish raw and typed entry addresses.
// - WASI/preload helpers bind the correct caller memory and environment around imported C API calls.
// - Host API entry points at the end of the file are thin, validated wrappers around those internal mechanisms.
//
// Full coverage guide:
// - Lines near the include block select platform, LLVM, unwind, and ABI integration points.
// - The first anonymous-namespace section declares shared aliases, module records, and process/thread global state.
// - The call-stack section records logical wasm frames shared by interpreter and generated-code diagnostics.
// - Trap reporting normalizes interpreter traps, mmap memory traps, and LLVM-generated-code traps into one fatal diagnostic path.
// - The LLVM unwind section maps physical Wasm activations from registered native unwind metadata, without generated logical frames.
// - Signature helpers normalize wasm enum signatures and raw C API signature bytes into a common ABI byte model.
// - Import helpers flatten already-initialized import chains and cache final targets for direct/import/table dispatch.
// - Scratch allocation helpers stage host buffers and interpreter frames without returning alloca-backed pointers from helper calls.
// - WASI helpers bind memory[0] and per-module/per-preload environments around local-imported and C API calls.
// - Preload memory helpers expose descriptors, copy read/write APIs, and mmap delivery metadata only for the active preload caller.
// - Interpreter helpers build local/operand frames, dispatch opfunc streams, and bridge direct/call_indirect/import calls.
// - Full compilation walks every runtime module, emits selected backend artifacts, builds caches, and publishes a one-shot ready flag.
// - Host entry points validate raw ABI buffers, pick LLVM-full or interpreter execution, then clean up current-thread runtime state.
// std
# include <algorithm>
# include <atomic>
# include <bit>
# include <coroutine>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <string>
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>

# include "uwvm_runtime_generation.h"
# include "uwvm_runtime_checked_size.h"
# include "uwvm_runtime_execution_entry.h"
# include "uwvm_runtime_native_stack_guard.h"
# include "uwvm_runtime_generated_wasm_bridge.h"
# include "uwvm_runtime_imported_function_lookup.h"
# include "uwvm_runtime_local_imported_provider_callbacks.h"
# include "uwvm_runtime_state_signature.h"
# include "uwvm_runtime_wasip1_memory_bindings.h"
# include "uwvm_runtime_wasm_fp_environment.h"
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include "uwvm_runtime_call_indirect_table_views.h"
#  include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
#  include "uwvm_runtime_llvm_expanded_lane_unroll_policy.h"
#  include "uwvm_runtime_native_unwind_execution_gate.h"
# endif

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
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/JITEventListener.h>
#  include <llvm/ExecutionEngine/MCJIT.h>
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#  include <llvm/Config/llvm-config.h>
#  include <llvm/InitializePasses.h>
#  include <llvm/IR/Constants.h>
#  include <llvm/IR/LegacyPassManager.h>
#  include <llvm/IR/Module.h>
#  include <llvm/IR/PassManager.h>
#  include <llvm/IR/Verifier.h>
#  include <llvm/MC/TargetRegistry.h>
#  include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#  include <llvm/Object/ObjectFile.h>
#  include <llvm/PassRegistry.h>
#  include <llvm/Passes/OptimizationLevel.h>
#  include <llvm/Passes/PassBuilder.h>
#  include <llvm/Support/CodeGen.h>
#  include <llvm/Support/MemoryBuffer.h>
#  include <llvm/Support/TargetSelect.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/TargetParser/Host.h>
#  include <llvm/Transforms/InstCombine/InstCombine.h>
#  include <llvm/Transforms/Scalar.h>
#  include <llvm/Transforms/Scalar/GVN.h>
#  include <llvm/Transforms/Utils.h>
# endif

# include "uwvm_runtime_native_unwind.h"

// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include <uwvm2/object/memory/impl.h>
# include <uwvm2/object/global/impl.h>
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
#  include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/impl.h>
#  include <uwvm2/runtime/compiler/uwvm_int/optable/impl.h>
# endif
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <fast_io_crypto.h>
#  include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#  include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#  include <uwvm2/runtime/llvm_jit_cache/impl.h>
# endif
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/uwvm/crtmain/global/process_time.h>
# include <uwvm2/validation/error/impl.h>
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
        [[nodiscard]] inline constexpr bool& get_llvm_wasm_fp_environment_active_marker() noexcept;
#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr ::std::size_t& get_llvm_jit_generated_bridge_scope_depth() noexcept;

        // A generated bridge may reuse the outer FP/unwind execution scopes only while this capability is live. Host
        // callbacks temporarily suspend it, so accidental calls to the internal bridge cannot inherit a possibly
        // poisoned callback environment. The scope is created only after runtime publication is ready.
        class llvm_jit_generated_bridge_scope_guard
        {
            ::std::size_t* depth{};

        public:
            inline explicit llvm_jit_generated_bridge_scope_guard(::std::size_t& value, bool enable = true) noexcept
            {
                if(!enable) { return; }
                // The bridge token is meaningful only inside the canonical generated-Wasm FP environment. Enforce the
                // invariant here as well as at bridge entry so future construction sites cannot mint a partial capability.
                if(!::uwvm2::runtime::lib::details::is_llvm_wasm_fp_environment_active(
                       get_llvm_wasm_fp_environment_active_marker())) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
                if(value == (::std::numeric_limits<::std::size_t>::max)()) [[unlikely]] { ::fast_io::fast_terminate(); }
                depth = ::std::addressof(value);
                ++value;
            }

            llvm_jit_generated_bridge_scope_guard(llvm_jit_generated_bridge_scope_guard const&) = delete;
            llvm_jit_generated_bridge_scope_guard& operator= (llvm_jit_generated_bridge_scope_guard const&) = delete;

            inline constexpr ~llvm_jit_generated_bridge_scope_guard() noexcept
            {
                if(depth == nullptr) { return; }
                if(*depth == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                --*depth;
            }
        };

        class llvm_jit_generated_bridge_scope_suspend_guard
        {
            ::std::size_t* depth{};
            ::std::size_t saved_depth{};

        public:
            inline explicit constexpr llvm_jit_generated_bridge_scope_suspend_guard(::std::size_t& value) noexcept
                : depth{::std::addressof(value)}, saved_depth{value}
            { value = 0uz; }

            llvm_jit_generated_bridge_scope_suspend_guard(llvm_jit_generated_bridge_scope_suspend_guard const&) = delete;
            llvm_jit_generated_bridge_scope_suspend_guard& operator= (llvm_jit_generated_bridge_scope_suspend_guard const&) = delete;

            inline constexpr ~llvm_jit_generated_bridge_scope_suspend_guard() noexcept
            {
                if(*depth != 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
                *depth = saved_depth;
            }
        };
#endif

        template <typename Callable>
        // Only an explicit provider contract can skip callback control restoration.
        // A Wasm module cannot certify arbitrary native callbacks. Keep this guard
        // in an ordinary call frame that returns before interpreter tail dispatch,
        // and keep public-entry save/restore even when a provider is trusted.
        // Trust in FP controls does not grant the generated-only bridge capability.
        inline void invoke_host_with_llvm_wasm_fp_control_policy(
            ::uwvm2::uwvm::wasm::type::local_imported_wasm_fp_control_policy_t policy,
            Callable&& callable) noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT)
            // Never let a callback inherit the generated-only bridge capability. A preserving callback may skip the
            // comparatively expensive fenv save/restore, but re-entry still has to use the public raw host API.
            llvm_jit_generated_bridge_scope_suspend_guard generated_bridge_scope_suspend{
                get_llvm_jit_generated_bridge_scope_depth()};
#endif
            if(policy == ::uwvm2::uwvm::wasm::type::local_imported_wasm_fp_control_policy_t::preserves_wasm_control)
            {
                ::std::forward<Callable>(callable)();
                return;
            }

            // Unknown host code may change rounding, exception masks, or architecture controls such as FTZ/DAZ. Save
            // and restore the active Wasm environment around every normal callback return. Non-local jumps across this
            // frame are outside the embedding contract; fatal Wasm traps terminate rather than attempting recovery.
            ::uwvm2::runtime::lib::details::scoped_llvm_wasm_host_fp_environment_restore fp_environment_guard{
                get_llvm_wasm_fp_environment_active_marker()};
            if(!fp_environment_guard.ready()) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::std::forward<Callable>(callable)();
        }

        template <typename Callable>
        inline void invoke_host_preserving_llvm_wasm_fp_environment(Callable&& callable) noexcept
        {
            invoke_host_with_llvm_wasm_fp_control_policy(
                ::uwvm2::uwvm::wasm::type::local_imported_wasm_fp_control_policy_t::may_modify,
                ::std::forward<Callable>(callable));
        }

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
        using win64_context_t = ::fast_io::win32::win_current_context;
# endif

        inline void set_llvm_module_target_triple_from_machine(::llvm::Module& module, ::llvm::TargetMachine const& target_machine)
        {
# if LLVM_VERSION_MAJOR >= 21
            module.setTargetTriple(target_machine.getTargetTriple());
# else
            module.setTargetTriple(target_machine.getTargetTriple().str());
# endif
        }
#endif
        using native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;

        using capi_function_t = ::uwvm2::uwvm::wasm::type::capi_function_t;
        using local_imported_t = ::uwvm2::uwvm::wasm::type::local_imported_t;
        using local_imported_wasm_fp_control_policy_t =
            ::uwvm2::uwvm::wasm::type::local_imported_wasm_fp_control_policy_t;
        using preload_module_memory_attribute_t = ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t;
        using preload_memory_descriptor_t = ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t;

        [[nodiscard]] inline local_imported_wasm_fp_control_policy_t
            query_local_imported_function_wasm_fp_control_policy(local_imported_t const* module, ::std::size_t function_index) noexcept
        {
            if(module == nullptr) [[unlikely]] { return local_imported_wasm_fp_control_policy_t::may_modify; }
#if defined(UWVM_RUNTIME_LLVM_JIT)
            return static_cast<local_imported_wasm_fp_control_policy_t>(
                ::uwvm2::runtime::lib::details::invoke_local_imported_provider_function_wasm_fp_control_policy(module, function_index));
#else
            // The pure-interpreter adapter obtains this policy from its constexpr function tuple.
            return module->function_wasm_fp_control_policy_from_index(function_index);
#endif
        }

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        using wasip1_env_type = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_env_type;
        using wasip1_module_override_t = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_override_t;
        using wasip1_module_target_kind_t = ::uwvm2::uwvm::imported::wasi::wasip1::storage::wasip1_module_target_kind_t;
#endif

        using parser_feature_parameter_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;

        [[nodiscard]] inline constexpr parser_feature_parameter_t const*
            find_validator_feature_parameter_storage(::uwvm2::utils::container::u8string_view module_name) noexcept
        {
            auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
            if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) [[unlikely]] { return nullptr; }

            using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
            auto const& am{it->second};
            if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) [[unlikely]] { return nullptr; }

            auto const wf{am.module_storage_ptr.wf};
            if(wf == nullptr || wf->binfmt_ver != 1u) [[unlikely]] { return nullptr; }
            return ::std::addressof(wf->wasm_parameter.binfmt1_para);
        }

        struct compiled_module_record;

#if (defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__) || defined(_ARCH_PPC64)) ||                                                            \
    (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC))
# define UWVM_TARGET_POWERPC_FAMILY 1
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        using compiled_module_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;
        using compiled_local_func_t = ::uwvm2::runtime::compiler::uwvm_int::optable::local_func_storage_t;

        inline consteval ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t get_curr_target_tranopt() noexcept;
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
        [[nodiscard]] inline constexpr bool
            try_materialize_runtime_module_llvm_jit(compiled_module_record& rec,
                                                    ::llvm::CodeGenOptLevel default_codegen_opt_level = ::llvm::CodeGenOptLevel::Aggressive,
                                                    ::std::size_t extra_materialize_threads = ::std::numeric_limits<::std::size_t>::max()) noexcept;
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

        // Per-module compilation record for eager interpreter or full LLVM AOT materialization.
        struct compiled_module_record
        {
            // module_name is a view into the global module map key; the map owns the storage lifetime.
            ::uwvm2::utils::container::u8string_view module_name{};
            runtime_module_storage_t const* runtime_module{};
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            compiled_module_t compiled{};
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
            llvm_jit_compiled_module_t llvm_jit_compiled{};
            // These owners must outlive every function address published into direct-call and call_indirect targets.
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::LLVMContext> llvm_jit_context_holder{};
            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> llvm_jit_engine{};
            ::uwvm2::utils::container::vector<::std::uintptr_t> llvm_jit_local_entry_addresses{};
            ::uwvm2::utils::container::vector<::std::uintptr_t> llvm_jit_local_raw_entry_addresses{};
            bool llvm_jit_ready{};
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
            ::std::uintptr_t end{};
            ::std::size_t module_id{};
            ::std::size_t function_index{};
            bool raw_entry{};
            // ABI adapters are physical frames, not additional Wasm activations. Raw-only bodies remain visible.
            bool wrapper_entry{};
        };

        // Sorted merged native code ranges cheaply filter unrelated host frames before looking up JIT unwind entries.
        struct llvm_jit_code_range
        {
            ::std::uintptr_t begin{};
            ::std::uintptr_t end{};
        };

#endif

        // Global runtime registry shared by eager compilation, host APIs, bridges, trap reporting, import calls, and WASI binding.
        struct runtime_global_state
        {
            // Every eager publication and reset owns the same pointer graph. Serialize those mutations and retain the exact
            // configuration that produced the live registries so a later host call cannot silently reuse incompatible code.
            ::std::atomic_flag runtime_state_publication_lock = ATOMIC_FLAG_INIT;
            details::runtime_state_signature published_runtime_state{};

            // Monotonically identifies the currently loaded runtime registry. Per-thread pointer caches record this value so a
            // quiescent reset invalidates caches owned by every thread, not only the thread that performs the reset.
            details::runtime_generation_epoch runtime_generation{};

            ::uwvm2::utils::container::vector<compiled_module_record> modules{};
            ::uwvm2::utils::container::unordered_flat_map<::uwvm2::utils::container::u8string_view, ::std::size_t> module_name_to_id{};

            // Full-compile: keep the hot local-call path O(1) by indexing local funcs with vectors (not hash maps).
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<compiled_defined_func_info>> defined_func_cache{};

            // For indirect calls / import-alias resolution that only has `local_defined_function_storage_t*`,
            // map pointer-address to {module_id, local_index} via a sorted range table.
            ::uwvm2::utils::container::vector<defined_func_ptr_range> defined_func_ptr_ranges{};

#if defined(UWVM_RUNTIME_LLVM_JIT)
            // Unwind-backed execution holds this gate while the compact native address maps may be read. Reset takes it before
            // the publication lock, matching execution's gate -> publication-lock order and preventing reader/writer overlap.
            details::native_unwind_execution_gate llvm_jit_native_unwind_execution_gate{};
            ::uwvm2::utils::container::vector<llvm_jit_unwind_entry> llvm_jit_unwind_entries{};
            ::uwvm2::utils::container::vector<llvm_jit_code_range> llvm_jit_code_ranges{};
#endif

            ::std::atomic_bool bridges_initialized{false};
            // compiled_all means eager-mode module records, import caches, function maps, and optional interpreter bridges are ready.
            ::std::atomic_bool compiled_all{false};
        };

        inline runtime_global_state g_runtime{};  // [global]

        [[maybe_unused]] [[nodiscard]] inline constexpr ::std::size_t
            find_runtime_module_id_from_storage_ptr(runtime_module_storage_t const* runtime_module_ptr) noexcept
        {
            // Runtime-storage pointers are stable identities owned by the currently published registry. Do not use pointer-range
            // arithmetic here: the records may refer to independently allocated module objects.
            if(runtime_module_ptr == nullptr) [[unlikely]] { return (::std::numeric_limits<::std::size_t>::max)(); }

            for(::std::size_t module_id{}; module_id != g_runtime.modules.size(); ++module_id)
            {
                if(g_runtime.modules.index_unchecked(module_id).runtime_module == runtime_module_ptr) { return module_id; }
            }

            return (::std::numeric_limits<::std::size_t>::max)();
        }

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        [[nodiscard]] inline ::std::uint_least64_t current_runtime_generation() noexcept
        {
            return g_runtime.runtime_generation.current();
        }
#endif

        inline void advance_runtime_generation() noexcept
        {
            // Never wrap: wrapping would eventually make a surviving TLS cache generation valid again (ABA). Reaching this
            // limit requires more than 2^64 process-local resets, so terminating is safer than publishing an ambiguous epoch.
            if(!g_runtime.runtime_generation.advance()) [[unlikely]] { ::fast_io::fast_terminate(); }
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
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
#endif

        inline constexpr compiled_module_record::~compiled_module_record() noexcept
        {
#if defined(UWVM_RUNTIME_LLVM_JIT)
            if(g_runtime_process_exiting)
            {
                // Avoid late LLVM MCJIT teardown during static destruction. The process is exiting, so releasing ownership is safer
                // than running destructors that may touch already-destroyed LLVM globals.
                static_cast<void>(llvm_jit_compiled.llvm_jit_module.llvm_module.release());
                static_cast<void>(llvm_jit_compiled.llvm_jit_module.llvm_context_holder.release());
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

        // Per-thread execution state: logical wasm stack frames plus a tiny call_indirect inline cache. Keeping this state thread-local
        // prevents re-entrant host calls from corrupting another thread's trap report or indirect-call cache.
        struct call_stack_tls_state
        {
            inline static constexpr ::std::size_t kCallStackMaxDepth{4096uz};
            inline static constexpr ::std::size_t kCallIndirectCacheEntries{8uz};

            using thread_local_allocator = ::fast_io::native_thread_local_allocator;
            ::uwvm2::utils::container::vector<call_stack_frame, thread_local_allocator> frames{};
#if defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_USE_THREAD_LOCAL)
            // Native-TLS interpreter builds use a separate constant-initialized marker below, avoiding early
            // initialization of the call-stack vector and changes to compilation allocation/layout.
            bool llvm_wasm_fp_environment_active{};
#endif
            // Capability proving that a generated-Wasm bridge is executing beneath a fully published outer JIT entry.
            // Host callbacks suspend it before leaving trusted runtime code, even when they promise to preserve FP state.
            ::std::size_t llvm_jit_generated_bridge_scope_depth{};

            struct call_indirect_cache_entry
            {
                // The cache key includes table identity, backing element storage, selector, and expected type. The backing pointer is
                // important because table growth/reallocation must invalidate stale entries.
                ::std::uint_least64_t runtime_generation{};
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
                // The container's checked emplacement compares its write/end pointers directly and grows when full.
                // Rechecking size() < capacity() here adds pointer-distance arithmetic on every generated Wasm call;
                // constructing from the two identities also avoids copying a temporary frame. Preserve slow-path growth.
                frames.emplace_back(fr.module_id, fr.function_index);
            }

            inline constexpr void pop() noexcept
            {
                if(!frames.empty()) [[likely]] { frames.pop_back_unchecked(); }
            }
        };

        struct preload_call_context_t
        {
            inline static constexpr ::std::size_t invalid_module_id{::std::numeric_limits<::std::size_t>::max()};

            // Preload C APIs need the current wasm caller to select the correct memory descriptor and WASI environment.
            ::std::size_t module_id{invalid_module_id};
            preload_module_memory_attribute_t const* preload_module_memory_attribute{};
            capi_function_t const* capi_function{};
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

# if !defined(UWVM_RUNTIME_LLVM_JIT)
#  if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#   ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#   else
        [[__gnu__::__tls_model__("local-dynamic")]]
#   endif
#  endif
        inline constinit thread_local bool g_interpreter_wasm_fp_environment_active{};  // [global] [thread_local]
# endif

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        // Kept separate from FP/bridge markers: interpreter execution and reset need the same re-entry invariant.
        inline thread_local ::std::size_t g_runtime_execution_entry_depth{};  // [global] [thread_local]

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        // Current-thread ownership of the non-recursive registry publication lock.
        inline thread_local ::std::size_t g_runtime_state_publication_depth{};  // [global] [thread_local]

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        // Active only around provider calls that supply LLVM compilation metadata.
        inline thread_local ::std::size_t g_runtime_compilation_metadata_callback_depth{};  // [global] [thread_local]

# if UWVM_HAS_CPP_ATTRIBUTE(__gnu__::__tls_model__)
#  ifdef UWVM
        [[__gnu__::__tls_model__("local-exec")]]
#  else
        [[__gnu__::__tls_model__("local-dynamic")]]
#  endif
# endif
        inline thread_local preload_call_context_t g_preload_call_context{};  // [global] [thread_local]

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr call_stack_tls_state& get_call_stack() noexcept { return g_call_stack; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr preload_call_context_t& get_preload_call_context() noexcept { return g_preload_call_context; }

        inline constexpr void erase_current_thread_state() noexcept
        {
            // Runtime reload must invalidate cached table/function pointers even when native TLS owns the storage for the
            // lifetime of the thread. Keeping the vector capacity is safe; every retained value is backend-neutral storage.
            g_call_stack.frames.clear();
            g_call_stack.call_indirect_cache = {};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            g_call_stack.llvm_wasm_fp_environment_active = false;
# else
            g_interpreter_wasm_fp_environment_active = false;
# endif
            g_call_stack.llvm_jit_generated_bridge_scope_depth = 0uz;
            g_runtime_execution_entry_depth = 0uz;
            g_runtime_state_publication_depth = 0uz;
            g_runtime_compilation_metadata_callback_depth = 0uz;
            g_preload_call_context = {};
        }
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
            // Runtime entry nesting is backend-neutral. It must survive a permitted public-raw re-entry until the outer entry leaves.
            ::std::size_t runtime_execution_entry_depth{};
            // Stored in the same fallback node, but publication guards re-acquire it on leave and never retain a node pointer.
            ::std::size_t runtime_state_publication_depth{};
            // Lazy single-CU metadata callbacks can run without publication-lock ownership, so retain a separate phase depth.
            ::std::size_t runtime_compilation_metadata_callback_depth{};
            preload_call_context_t preload_call_context{};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            ::std::uintptr_t llvm_jit_trap_return_address{};
            llvm_jit_trap_kind llvm_jit_last_trap_kind{};
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            win64_context_t llvm_jit_win64_trap_caller_context{};
            bool llvm_jit_win64_trap_caller_context_valid{};
#  endif
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

        /// @warning Every `g_thread_states` insertion needs an explicit ownership boundary. Outermost runtime execution
        ///          erases its current-thread state; a metadata-only compiler worker is erased by
        ///          `runtime_compilation_metadata_callback_scope` only when that scope inserted the node. Any future
        ///          standalone worker or `wasi-thread` entry must likewise erase its own node before thread exit to avoid
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

        inline constexpr void erase_current_thread_state() noexcept { g_thread_states.erase(current_thread_id()); }
#endif

#if defined(UWVM_USE_THREAD_LOCAL)
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t& get_runtime_execution_entry_depth() noexcept
        { return g_runtime_execution_entry_depth; }
#else
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t& get_runtime_execution_entry_depth() noexcept
        { return get_thread_state().runtime_execution_entry_depth; }
#endif

#if defined(UWVM_USE_THREAD_LOCAL)
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t& get_runtime_compilation_metadata_callback_depth() noexcept
        { return g_runtime_compilation_metadata_callback_depth; }
#else
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t& get_runtime_compilation_metadata_callback_depth() noexcept
        { return get_thread_state().runtime_compilation_metadata_callback_depth; }
#endif

#if defined(UWVM_USE_THREAD_LOCAL)
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t& get_runtime_state_publication_depth() noexcept
        { return g_runtime_state_publication_depth; }
#else
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::size_t& get_runtime_state_publication_depth() noexcept
        { return get_thread_state().runtime_state_publication_depth; }
#endif

        class runtime_compilation_metadata_callback_scope
        {
            bool entered{};
#if !defined(UWVM_USE_THREAD_LOCAL)
            os_thread_id_t thread_id{current_thread_id()};
            bool owns_inserted_node{};
#endif

        public:
            inline runtime_compilation_metadata_callback_scope() noexcept
            {
#if defined(UWVM_USE_THREAD_LOCAL)
                // Native TLS has no fallback-map node ownership; update its depth directly.
                if(!::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_enter(
                       get_runtime_compilation_metadata_callback_depth())) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
#else
                // `try_emplace_and_visit` reports whether this scope inserted the fallback node. Both visitors mutate
                // the node only while the container owns its lock; no reference survives the provider callback.
                bool enter_succeeded{};
                owns_inserted_node = g_thread_states.try_emplace_and_visit(
                    thread_id,
                    [&](auto& kv) constexpr noexcept
                    {
                        enter_succeeded = ::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_enter(
                            kv.second.runtime_compilation_metadata_callback_depth);
                    },
                    [&](auto& kv) constexpr noexcept
                    {
                        enter_succeeded = ::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_enter(
                            kv.second.runtime_compilation_metadata_callback_depth);
                    });
                if(!enter_succeeded) [[unlikely]] { ::fast_io::fast_terminate(); }
#endif
                entered = true;
            }

            runtime_compilation_metadata_callback_scope(runtime_compilation_metadata_callback_scope const&) = delete;
            runtime_compilation_metadata_callback_scope& operator= (runtime_compilation_metadata_callback_scope const&) = delete;
            runtime_compilation_metadata_callback_scope(runtime_compilation_metadata_callback_scope&&) = delete;
            runtime_compilation_metadata_callback_scope& operator= (runtime_compilation_metadata_callback_scope&&) = delete;

            inline ~runtime_compilation_metadata_callback_scope()
            {
                if(!entered) { return; }
#if defined(UWVM_USE_THREAD_LOCAL)
                if(!::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_leave(
                       get_runtime_compilation_metadata_callback_depth())) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
#else
                bool leave_succeeded{};
                ::std::size_t remaining_depth{(::std::numeric_limits<::std::size_t>::max)()};
                auto const visited_count{g_thread_states.visit(
                    thread_id,
                    [&](auto& kv) constexpr noexcept
                    {
                        leave_succeeded = ::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_leave(
                            kv.second.runtime_compilation_metadata_callback_depth);
                        remaining_depth = kv.second.runtime_compilation_metadata_callback_depth;
                    })};
                if(visited_count != 1uz || !leave_succeeded) [[unlikely]] { ::fast_io::fast_terminate(); }

                if(owns_inserted_node)
                {
                    // Erase only after `visit` released the node lock. Erasing inside the visitor would recursively
                    // acquire the same container shard and can deadlock.
                    if(!::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_owned_node_should_erase(
                           owns_inserted_node, remaining_depth) ||
                       g_thread_states.erase(thread_id) != 1uz) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }
                }
#endif
            }
        };

        class runtime_state_publication_guard
        {
            bool entered{};

        public:
            inline runtime_state_publication_guard() noexcept
            {
                // A same-thread recursive attempt cannot acquire this non-recursive lock. Fail before spinning.
                if(!::uwvm2::runtime::lib::details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth())) [[unlikely]]
                { ::fast_io::fast_terminate(); }

                while(g_runtime.runtime_state_publication_lock.test_and_set(::std::memory_order_acquire)) { ::fast_io::this_thread::yield(); }

                // Do not retain a TLS/map-backed node reference across callbacks. Every transition re-acquires the
                // current thread's storage; reset releases this guard before erasing that storage.
                if(!::uwvm2::runtime::lib::details::runtime_state_publication_enter(get_runtime_state_publication_depth())) [[unlikely]]
                {
                    g_runtime.runtime_state_publication_lock.clear(::std::memory_order_release);
                    ::fast_io::fast_terminate();
                }
                entered = true;
            }

            runtime_state_publication_guard(runtime_state_publication_guard const&) = delete;
            runtime_state_publication_guard& operator= (runtime_state_publication_guard const&) = delete;
            runtime_state_publication_guard(runtime_state_publication_guard&&) = delete;
            runtime_state_publication_guard& operator= (runtime_state_publication_guard&&) = delete;

            inline ~runtime_state_publication_guard()
            {
                if(!entered) { return; }
                if(!::uwvm2::runtime::lib::details::runtime_state_publication_leave(get_runtime_state_publication_depth())) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                g_runtime.runtime_state_publication_lock.clear(::std::memory_order_release);
            }
        };

        [[nodiscard]] inline constexpr bool& get_llvm_wasm_fp_environment_active_marker() noexcept
        {
#if defined(UWVM_USE_THREAD_LOCAL) && !defined(UWVM_RUNTIME_LLVM_JIT)
            return g_interpreter_wasm_fp_environment_active;
#else
            return get_call_stack().llvm_wasm_fp_environment_active;
#endif
        }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr ::std::size_t& get_llvm_jit_generated_bridge_scope_depth() noexcept
        { return get_call_stack().llvm_jit_generated_bridge_scope_depth; }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT) && !defined(UWVM_USE_THREAD_LOCAL)
        // Non-TLS fallback accessors. Do not route the thread_local build through these;
        // the direct TLS variables are the fast path and should stay visible in preprocessed code.
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ::std::uintptr_t& get_llvm_jit_trap_return_address() noexcept
        { return get_thread_state().llvm_jit_trap_return_address; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr llvm_jit_trap_kind& get_llvm_jit_last_trap_kind() noexcept
        { return get_thread_state().llvm_jit_last_trap_kind; }

# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr win64_context_t& get_llvm_jit_win64_trap_caller_context() noexcept
        { return get_thread_state().llvm_jit_win64_trap_caller_context; }

        [[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr bool& get_llvm_jit_win64_trap_caller_context_valid() noexcept
        { return get_thread_state().llvm_jit_win64_trap_caller_context_valid; }
# endif
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
            table_out_of_bounds,
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
                case trap_kind::table_out_of_bounds:
                {
                    return ::uwvm2::utils::container::u8string_view{u8"table access out of bounds"};
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
#  if UWVM_HAS_CPP_ATTRIBUTE(maybe_unused)
        [[maybe_unused]]
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
#  if UWVM_HAS_CPP_ATTRIBUTE(maybe_unused)
        [[maybe_unused]]
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
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] inline constexpr bool llvm_jit_win64_virtual_unwind_once(win64_context_t& context) noexcept;
        // Keep this helper as a real call boundary.  Win64 SEH backtracing uses it as the first stable host frame
        // after a generated-code trap; inlining it into a trap entry can make RtlCaptureContext/return-address
        // matching observe the trap entry's own layout and truncate the recovered Wasm stack.  The C++ `inline`
        // keyword is still intentional here for ODR-friendly header-style linkage; UWVM_NOINLINE preserves the
        // required unwind anchor.
        UWVM_NOINLINE inline constexpr void store_llvm_jit_win64_trap_caller_context(::std::uintptr_t expected_return_address,
                                                                                     ::std::uintptr_t trap_frame_address,
                                                                                     ::std::uintptr_t trap_stack_pointer,
                                                                                     void const* platform_context = nullptr) noexcept;
        [[nodiscard]] inline constexpr bool load_llvm_jit_win64_trap_caller_context(win64_context_t& context) noexcept;
# endif

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_call_stack_applies_to_current_compiler() noexcept
        {
            // Only the LLVM AOT backend should consult native unwind policy. Interpreter-only execution reports instruction frames.
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            auto const compiler{runtime_mode::global_runtime_compiler};
# if defined(UWVM_RUNTIME_LLVM_JIT)
            if(compiler == runtime_mode::runtime_compiler_t::llvm_jit_only) { return true; }
# endif
            return false;
        }

        inline constexpr void refresh_llvm_jit_unwind_entry_bounds() noexcept;

        inline constexpr void
            record_llvm_jit_unwind_entry(::std::size_t module_id, ::std::size_t function_index, ::std::uintptr_t address, bool raw_entry,
                                        bool wrapper_entry = false) noexcept
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
                entry.wrapper_entry = wrapper_entry;
                refresh_llvm_jit_unwind_entry_bounds();
                return;
            }

            entries.push_back(llvm_jit_unwind_entry{address, 0u, module_id, function_index, raw_entry, wrapper_entry});
            ::std::sort(entries.begin(), entries.end(), [](auto const& lhs, auto const& rhs) constexpr noexcept { return lhs.address < rhs.address; });
            refresh_llvm_jit_unwind_entry_bounds();
        }

        inline constexpr void record_llvm_jit_code_range(::std::uintptr_t begin, ::std::uintptr_t size) noexcept
        {
            // Keep ranges merged. Trap reporting first tests whether an IP belongs to generated code before resolving its
            // concrete native Wasm function.
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
            refresh_llvm_jit_unwind_entry_bounds();
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

        [[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_code_range_end_for_ip(::std::uintptr_t ip) noexcept
        {
            auto const& ranges{g_runtime.llvm_jit_code_ranges};
            if(ranges.empty()) [[unlikely]] { return 0u; }

            auto const it{::std::upper_bound(ranges.begin(),
                                             ranges.end(),
                                             ip,
                                             [](auto const address, llvm_jit_code_range const& range) constexpr noexcept { return address < range.begin; })};
            if(it == ranges.begin()) [[unlikely]] { return 0u; }

            auto const& range{*(it - 1u)};
            return ip >= range.begin && ip < range.end ? range.end : 0u;
        }

        inline constexpr void refresh_llvm_jit_unwind_entry_bounds() noexcept
        {
            // Native unwind reports instruction pointers while materialization initially publishes function entry addresses.
            // Bound each entry by the next entry and its executable section so unrelated native PCs cannot be attributed to Wasm.
            auto& entries{g_runtime.llvm_jit_unwind_entries};
            for(::std::size_t i{}; i != entries.size(); ++i)
            {
                auto& entry{entries.index_unchecked(i)};
                ::std::uintptr_t end{};
                if(i + 1uz != entries.size())
                {
                    auto const next_address{entries.index_unchecked(i + 1uz).address};
                    if(next_address > entry.address) { end = next_address; }
                }

                auto const code_range_end{llvm_jit_code_range_end_for_ip(entry.address)};
                if(code_range_end != 0u && code_range_end > entry.address && (end == 0u || code_range_end < end)) { end = code_range_end; }
                entry.end = end;
            }
        }

        class uwvm_llvm_jit_code_range_listener final : public ::llvm::JITEventListener
        {
        public:
            constexpr void
                notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& obj, ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
            {
                // MCJIT reports loaded sections through this listener. Capture executable ranges so native unwind PCs can be
                // rejected before the entry-address map is consulted for unrelated host frames.
                for(auto const& section: obj.sections())
                {
                    if(!section.isText()) { continue; }

                    auto const load_address{static_cast<::std::uintptr_t>(loaded.getSectionLoadAddress(section))};
                    auto const size{static_cast<::std::uintptr_t>(section.getSize())};
                    record_llvm_jit_code_range(load_address, size);
                }
            }
        };

        [[nodiscard]] inline constexpr uwvm_llvm_jit_code_range_listener& get_uwvm_llvm_jit_code_range_listener() noexcept
        {
            static uwvm_llvm_jit_code_range_listener listener{};
            return listener;
        }

        struct resolved_llvm_jit_unwind_entry
        {
            llvm_jit_unwind_entry const* entry{};
            ::std::uintptr_t offset{};
        };

        [[nodiscard, maybe_unused]] inline constexpr resolved_llvm_jit_unwind_entry resolve_llvm_jit_unwind_entry(::std::uintptr_t ip) noexcept
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
            if(entry_it->end != 0u && ip >= entry_it->end) [[unlikely]] { return {}; }
            auto const offset{ip - entry_it->address};
            return resolved_llvm_jit_unwind_entry{::std::addressof(*entry_it), offset};
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view runtime_llvm_jit_unwind_backend_name() noexcept
        {
            // Diagnostic label for the selected native unwind implementation.
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
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
            // Canonical frame addresses distinguish physical activations, including recursive calls to the same function.
            // They also anchor explicitly registered interpreter/native transitions; they are not pointers to scan.
            ::std::uintptr_t cfas[max_frames]{};
            ::std::size_t size{};
            ::std::size_t omit{};
        };

#  if UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN
        [[nodiscard]] inline constexpr bool llvm_jit_frame_record_address_aligned(::std::uintptr_t address) noexcept
        { return (address % alignof(::std::uintptr_t)) == 0u; }

        [[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_load_frame_record_word(::std::uintptr_t address) noexcept
        {
            ::std::uintptr_t value{};
            ::std::memcpy(::std::addressof(value), reinterpret_cast<void const*>(address), sizeof(value));
            return value;
        }

#  endif

#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_win64_context_instruction_pointer(win64_context_t const& context) noexcept
        {
#   if defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
            return static_cast<::std::uintptr_t>(context.Rip);
#   elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            return static_cast<::std::uintptr_t>(context.Pc);
#   else
            static_cast<void>(context);
            return 0u;
#   endif
        }

        inline constexpr void llvm_jit_win64_context_set_instruction_pointer(win64_context_t& context, ::std::uintptr_t value) noexcept
        {
#   if defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
            context.Rip = static_cast<::std::uint64_t>(value);
#   elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            context.Pc = static_cast<::std::uint64_t>(value);
#   else
            static_cast<void>(context);
            static_cast<void>(value);
#   endif
        }

        [[nodiscard, maybe_unused]] inline constexpr ::std::uintptr_t llvm_jit_win64_context_stack_pointer(win64_context_t const& context) noexcept
        {
#   if defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
            return static_cast<::std::uintptr_t>(context.Rsp);
#   elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            return static_cast<::std::uintptr_t>(context.Sp);
#   else
            static_cast<void>(context);
            return 0u;
#   endif
        }

        inline constexpr void llvm_jit_win64_context_set_stack_pointer(win64_context_t& context, ::std::uintptr_t value) noexcept
        {
#   if defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
            context.Rsp = static_cast<::std::uint64_t>(value);
#   elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            context.Sp = static_cast<::std::uint64_t>(value);
#   else
            static_cast<void>(context);
            static_cast<void>(value);
#   endif
        }

        [[nodiscard, maybe_unused]] inline constexpr ::std::uintptr_t llvm_jit_win64_context_frame_pointer(win64_context_t const& context) noexcept
        {
#   if defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
            return static_cast<::std::uintptr_t>(context.Rbp);
#   elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            return static_cast<::std::uintptr_t>(context.X[29u]);
#   else
            static_cast<void>(context);
            return 0u;
#   endif
        }

        inline constexpr void llvm_jit_win64_context_set_frame_pointer(win64_context_t& context, ::std::uintptr_t value) noexcept
        {
#   if defined(_WIN64) && (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64))
            context.Rbp = static_cast<::std::uint64_t>(value);
#   elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            context.X[29u] = static_cast<::std::uint64_t>(value);
#   else
            static_cast<void>(context);
            static_cast<void>(value);
#   endif
        }

        [[nodiscard, maybe_unused]] inline constexpr ::std::uintptr_t llvm_jit_win64_context_link_register(win64_context_t const& context) noexcept
        {
#   if defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            return static_cast<::std::uintptr_t>(context.X[30u]);
#   else
            static_cast<void>(context);
            return 0u;
#   endif
        }

        [[maybe_unused]] inline constexpr void llvm_jit_win64_context_set_link_register(win64_context_t& context, ::std::uintptr_t value) noexcept
        {
#   if defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            context.X[30u] = static_cast<::std::uint64_t>(value);
#   else
            static_cast<void>(context);
            static_cast<void>(value);
#   endif
        }

        [[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_win64_return_address_to_control_pc(::std::uintptr_t return_address) noexcept
        {
#   if defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            constexpr ::std::uintptr_t call_instruction_size{4u};
            return return_address >= call_instruction_size ? return_address - call_instruction_size : 0u;
#   else
            if(return_address != 0u) { --return_address; }
            return return_address;
#   endif
        }

        [[nodiscard]] inline constexpr ::std::uintptr_t llvm_jit_win64_context_control_pc(win64_context_t const& context) noexcept
        { return llvm_jit_win64_return_address_to_control_pc(llvm_jit_win64_context_instruction_pointer(context)); }

        inline constexpr void llvm_jit_win64_context_initialize_link_register(win64_context_t& context, ::std::uintptr_t frame_pointer) noexcept
        {
#   if defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
            if(frame_pointer == 0u || !llvm_jit_frame_record_address_aligned(frame_pointer)) [[unlikely]] { return; }
            auto const saved_link_register{llvm_jit_load_frame_record_word(frame_pointer + sizeof(::std::uintptr_t))};
            llvm_jit_win64_context_set_link_register(context, saved_link_register);
#   else
            static_cast<void>(context);
            static_cast<void>(frame_pointer);
#   endif
        }
#  endif

#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] inline constexpr llvm_jit_unwind_backtrace_storage capture_llvm_jit_win64_context_unwind_backtrace(::std::size_t omit) noexcept
        {
            llvm_jit_unwind_backtrace_storage storage{};
            win64_context_t context{};
            if(!load_llvm_jit_win64_trap_caller_context(context)) [[unlikely]] { return storage; }

            for(::std::size_t frame_index{}; storage.size != llvm_jit_unwind_backtrace_storage::max_frames; ++frame_index)
            {
                if(!llvm_jit_win64_virtual_unwind_once(context)) { break; }
                if(frame_index < omit) { continue; }

                auto const frame_ip{llvm_jit_win64_context_control_pc(context)};
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

            int ip_before_instruction{};
            auto ip{static_cast<::std::uintptr_t>(_Unwind_GetIPInfo(context, ::std::addressof(ip_before_instruction)))};
            // Return PCs denote the instruction after a call; a signal-frame PC denotes the faulting instruction itself.
            // Biasing a signal PC can attribute a fault at the first instruction to the preceding function.
            if(ip != 0u && !ip_before_instruction) { --ip; }
            storage->cfas[storage->size] = static_cast<::std::uintptr_t>(_Unwind_GetCFA(context));
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

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_can_replace_instruction_frames() noexcept
        {
            // This capability requires the supported native ABI plus registered JIT/host CFI. Checked mode additionally
            // verifies a real generated call chain before it allows omission of instruction-emitted TLS frames.
# if UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES
            return true;
# else
            return false;
# endif
        }

        inline constexpr bool ensure_llvm_jit_native_target_initialized() noexcept;
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string get_llvm_jit_host_cpu_name_storage() noexcept;
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>
            get_llvm_jit_host_target_attribute_storage() noexcept;
        [[nodiscard]] inline ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> select_runtime_llvm_jit_target(
            ::llvm::EngineBuilder&,
            ::uwvm2::utils::container::u8string const&,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const&);

# include "uwvm_runtime_posix_unwind_probe.h"
# include "uwvm_runtime_win64_unwind_probe.h"

        enum class runtime_llvm_jit_unwind_capability_status : unsigned
        {
            ok,
            no_backend,
            no_frame_replacement,
            live_probe_failed
        };

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
            runtime_llvm_jit_unwind_capability_status_reason(runtime_llvm_jit_unwind_capability_status st) noexcept
        {
            switch(st)
            {
                case runtime_llvm_jit_unwind_capability_status::ok: return u8"ok";
                case runtime_llvm_jit_unwind_capability_status::no_backend: return u8"no unwind backend was compiled into this build";
                case runtime_llvm_jit_unwind_capability_status::no_frame_replacement: return u8"generated-code unwind-frame replacement is unavailable";
                case runtime_llvm_jit_unwind_capability_status::live_probe_failed: return u8"generated-code recursive live unwind probe failed";
            }

            return u8"unknown unwind capability failure";
        }

        [[maybe_unused]] [[nodiscard]] inline constexpr runtime_llvm_jit_unwind_capability_status runtime_llvm_jit_unwind_capability() noexcept
        {
# if !UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
            return runtime_llvm_jit_unwind_capability_status::no_backend;
# else
            if(!runtime_llvm_jit_unwind_can_replace_instruction_frames()) { return runtime_llvm_jit_unwind_capability_status::no_frame_replacement; }
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            // Win64 checked mode must prove that each generated activation resolves through its registered dynamic
            // function table. A leaf fallback is insufficient because it cannot validate .pdata/.xdata registration.
            static bool const live_probe_ok{runtime_llvm_jit_win64_live_unwind_probe()};
            if(!live_probe_ok) [[unlikely]] { return runtime_llvm_jit_unwind_capability_status::live_probe_failed; }
#  elif UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE
            // Run once, before codegen chooses whether to omit logical frames. Keep the ROS runtime eager-only;
            // this generated probe tests the native ABI and does not add a lazy/tiered execution mechanism.
            static bool const live_probe_ok{runtime_llvm_jit_posix_live_unwind_probe()};
            if(!live_probe_ok) [[unlikely]] { return runtime_llvm_jit_unwind_capability_status::live_probe_failed; }
#  endif
            return runtime_llvm_jit_unwind_capability_status::ok;
# endif
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
            // Choose before emitting code: a failed native self-test retains instruction frames instead of claiming native success.
            auto const st{runtime_llvm_jit_unwind_capability()};
            if(st == runtime_llvm_jit_unwind_capability_status::ok) { return runtime_llvm_jit_call_stack_t::unwind; }
            return runtime_llvm_jit_call_stack_t::instruction;
# endif
        }

        [[nodiscard]] inline constexpr bool runtime_llvm_jit_unwind_call_stack_requested() noexcept
        {
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;
            if(!runtime_llvm_jit_call_stack_applies_to_current_compiler()) { return false; }
            auto const mode{get_runtime_llvm_jit_effective_call_stack_mode()};
            if(mode == runtime_mode::runtime_llvm_jit_call_stack_t::unwind_uncheck)
            {
# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE && UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES
                return true;
# else
                return false;
# endif
            }
            if(mode != runtime_mode::runtime_llvm_jit_call_stack_t::unwind) { return false; }
            return runtime_llvm_jit_unwind_capability() == runtime_llvm_jit_unwind_capability_status::ok;
        }

        [[noreturn]] inline constexpr void runtime_llvm_jit_explicit_unwind_call_stack_fatal(runtime_llvm_jit_unwind_capability_status st) noexcept
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                u8"[fatal] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"LLVM JIT native unwind call-stack mode is not supported on this target (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                runtime_llvm_jit_unwind_capability_status_reason(st),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"; backend=",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                runtime_llvm_jit_unwind_backend_name(),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"). Use ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"auto",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" or ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"instruction",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"; use ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                u8"unwind-uncheck",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8" only to skip the live self-test; it still omits logical JIT frames and requires native unwind support. ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                u8"(runtime)\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
            ::std::unreachable();
        }

        inline constexpr void validate_runtime_llvm_jit_unwind_call_stack_or_fatal() noexcept
        {
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;
            auto const requested{runtime_mode::global_runtime_llvm_jit_call_stack};
            if(requested != runtime_mode::runtime_llvm_jit_call_stack_t::unwind &&
               requested != runtime_mode::runtime_llvm_jit_call_stack_t::unwind_uncheck)
            {
                return;
            }

# if !UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
            runtime_llvm_jit_explicit_unwind_call_stack_fatal(runtime_llvm_jit_unwind_capability_status::no_backend);
# elif !UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES
            // `unwind-uncheck` skips only the live generated-chain probe. It may never bypass the compile-time
            // requirement that native unwind authoritatively replaces omitted instruction-emitted frames.
            runtime_llvm_jit_explicit_unwind_call_stack_fatal(runtime_llvm_jit_unwind_capability_status::no_frame_replacement);
# else
            if(requested == runtime_mode::runtime_llvm_jit_call_stack_t::unwind_uncheck) { return; }

            auto const st{runtime_llvm_jit_unwind_capability()};
            if(st != runtime_llvm_jit_unwind_capability_status::ok) [[unlikely]] { runtime_llvm_jit_explicit_unwind_call_stack_fatal(st); }
# endif
        }

        template <typename Output>
        inline constexpr void
            dump_llvm_jit_unwind_call_stack_frames_for_trap([[maybe_unused]] Output& u8log_output_ul,
                                                            [[maybe_unused]] ::std::size_t& printed_frame_count,
                                                            [[maybe_unused]] trap_kind current_trap_kind) noexcept
        {
# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            auto backtrace{capture_llvm_jit_win64_context_unwind_backtrace(0uz)};
            bool const seeded_context{backtrace.size != 0uz};
            if(backtrace.size == 0uz) { backtrace = capture_llvm_jit_unwind_backtrace(0uz); }
            // The Win64 context walker steps out of the saved generated caller before collecting; that caller is
            // therefore a separate first activation. The ordinary POSIX walk already contains its leaf and needs no seed.
            if(seeded_context)
            {
#   if defined(UWVM_USE_THREAD_LOCAL)
                auto const leaf_pc{llvm_jit_trap_return_address};
#   else
                auto const leaf_pc{get_llvm_jit_trap_return_address()};
#   endif
                auto const leaf{resolve_llvm_jit_unwind_entry(leaf_pc == 0u ? 0u : leaf_pc - 1u)};
                if(leaf.entry != nullptr && !leaf.entry->wrapper_entry &&
                   dump_call_stack_frame_for_trap(u8log_output_ul, printed_frame_count, leaf.entry->module_id, leaf.entry->function_index))
                {
                    ++printed_frame_count;
                }
            }
#  else
            // Start at the live helper and cross its CFI and, for memory faults, the OS signal trampoline.
            // Do not prepend a separately saved return PC: it duplicates the leaf activation and loses recursion identity.
            auto const backtrace{capture_llvm_jit_unwind_backtrace(0uz)};
#  endif
            for(::std::size_t i{}; i != backtrace.size; ++i)
            {
                auto const resolved{resolve_llvm_jit_unwind_entry(backtrace.frames[i])};
                if(resolved.entry == nullptr || resolved.entry->wrapper_entry) { continue; }
                // Distinct physical body frames are distinct Wasm activations, even when their function identities match.
                if(dump_call_stack_frame_for_trap(u8log_output_ul, printed_frame_count,
                                                  resolved.entry->module_id, resolved.entry->function_index))
                {
                    ++printed_frame_count;
                }
            }
# endif
        }
#endif

        inline constexpr void dump_call_stack_for_trap([[maybe_unused]] trap_kind current_trap_kind) noexcept
        {
            // Trap output is serialized on the log stream so concurrent traps do not interleave frame lines.
            // No copies will be made here.
            auto u8log_output_ref{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8log_output)};
            // Add raii locks while unlocking operations
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8log_output_ref)};
            // No copies will be made here.
            auto u8log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8log_output_ref)};

            ::fast_io::io::perr(u8log_output_ul,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Call stack:\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            ::std::size_t printed_frame_count{};
#if defined(UWVM_RUNTIME_LLVM_JIT)
            if(runtime_llvm_jit_unwind_call_stack_requested())
            {
                // Native and instruction are separate reporting modes, not two unordered lists of function identities.
                // ROS has no Tiered interpreter islands; every generated Wasm activation comes from the native walk.
                dump_llvm_jit_unwind_call_stack_frames_for_trap(u8log_output_ul, printed_frame_count, current_trap_kind);
                if(printed_frame_count == 0uz)
                {
                    ::fast_io::io::perrln(u8log_output_ul, u8"uwvm: [warn] native Wasm backtrace unavailable; no logical-stack fallback was recorded.");
                }
                ::fast_io::io::perrln(u8log_output_ul);
                return;
            }
#endif
            auto const& frames{get_call_stack().frames};
            for(::std::size_t i{frames.size()}; i != 0uz; --i)
            {
                auto const& fr{frames.index_unchecked(i - 1uz)};
                // A failed call_indirect has not entered its candidate callee. Suppressing all frames with that function
                // identity would incorrectly remove an already active recursive caller. Print each recorded activation once.
                if(dump_call_stack_frame_for_trap(u8log_output_ul, printed_frame_count, fr.module_id, fr.function_index))
                {
                    ++printed_frame_count;
                }
            }
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
        inline constexpr void store_llvm_jit_trap_context(llvm_jit_trap_kind k, ::std::uintptr_t return_address) noexcept
        {
            // Keep the immediate generated return address for leaf attribution. POSIX native unwind starts normally in the runtime
            // helper; only the separate Win64 SEH path consumes explicit frame/stack state.
# if defined(UWVM_USE_THREAD_LOCAL)
            llvm_jit_last_trap_kind = k;
            llvm_jit_trap_return_address = return_address;
# else
            get_llvm_jit_last_trap_kind() = k;
            get_llvm_jit_trap_return_address() = return_address;
# endif
        }

# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        [[nodiscard]] inline constexpr bool llvm_jit_win64_virtual_unwind_once(win64_context_t& context) noexcept
        {
            if(llvm_jit_win64_context_instruction_pointer(context) == 0u) [[unlikely]] { return false; }

            // RtlLookupFunctionEntry expects a control PC inside the call instruction's containing function. Trap
            // helpers usually give us a return address, so bias it back to the generated call site.
            auto const control_pc{llvm_jit_win64_context_control_pc(context)};
            if(control_pc == 0u) [[unlikely]] { return false; }

            ::fast_io::win32::win_current_unwind_address image_base{};
            auto const function_entry{::fast_io::win32::nt::RtlLookupFunctionEntry(static_cast<::fast_io::win32::win_current_unwind_address>(control_pc),
                                                                                   ::std::addressof(image_base),
                                                                                   nullptr)};
            if(function_entry == nullptr)
            {
#  if defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
                // A Win64 ARM64 function without a runtime-function entry is a leaf. The fault-time CONTEXT already
                // owns its architectural LR, while X29 may still describe the caller. Reading an X29 frame record here
                // would dereference stale caller state and can skip the immediate caller, so perform only the ABI leaf unwind.
                auto const return_address{llvm_jit_win64_context_link_register(context)};
                if(return_address == 0u) [[unlikely]] { return false; }
                llvm_jit_win64_context_set_instruction_pointer(context, return_address);
                return true;
#  else
                // Leaf functions may have no .pdata entry on Win64.  In that case the architectural convention is a
                // simple return-address pop from RSP, which is the same fallback used by the platform unwinder.
                auto const stack_pointer{llvm_jit_win64_context_stack_pointer(context)};
                if(stack_pointer == 0u || !llvm_jit_frame_record_address_aligned(stack_pointer)) [[unlikely]] { return false; }

                auto const return_address{llvm_jit_load_frame_record_word(stack_pointer)};
                if(return_address == 0u) [[unlikely]] { return false; }
                llvm_jit_win64_context_set_instruction_pointer(context, return_address);
                llvm_jit_win64_context_set_stack_pointer(context, stack_pointer + sizeof(::std::uintptr_t));
                return true;
#  endif
            }

            void* handler_data{};
            ::fast_io::win32::win_current_unwind_address establisher_frame{};
            // RtlVirtualUnwind applies the registered .pdata/.xdata metadata to mutate CONTEXT in place from the current
            // frame to its caller.  The image base must match the base used when the JIT registered the function table.
            static_cast<void>(::fast_io::win32::nt::RtlVirtualUnwind(::fast_io::win32::win64_unwind_flag_nhandler,
                                                                     image_base,
                                                                     static_cast<::fast_io::win32::win_current_unwind_address>(control_pc),
                                                                     function_entry,
                                                                     ::std::addressof(context),
                                                                     ::std::addressof(handler_data),
                                                                     ::std::addressof(establisher_frame),
                                                                     nullptr));
            return llvm_jit_win64_context_instruction_pointer(context) != 0u;
        }

        UWVM_NOINLINE inline constexpr void store_llvm_jit_win64_trap_caller_context(::std::uintptr_t expected_return_address,
                                                                                     ::std::uintptr_t trap_frame_address,
                                                                                     ::std::uintptr_t trap_stack_pointer,
                                                                                     void const* platform_context) noexcept
        {
            win64_context_t context{};
            bool valid{};

            // A VEH guard-page trap already supplies the complete processor state at the fault. Copy it before any
            // diagnostic output or host call, then retain the established return-address representation used by the
            // common unwinder. Keeping the complete CONTEXT preserves ARM64 LR and every nonvolatile register needed
            // by RtlVirtualUnwind; the scalar FP/SP path below remains only for generated helper calls.
            if(platform_context != nullptr && expected_return_address != 0u)
            {
                ::std::memcpy(::std::addressof(context), platform_context, sizeof(context));
                llvm_jit_win64_context_set_instruction_pointer(context, expected_return_address);
                valid = llvm_jit_win64_context_stack_pointer(context) != 0u;
            }
            // Start from the generated caller's architectural state when the trap helper was entered.  Win64 uses
            // frame-pointer offsets in SEH unwind metadata, so RSP must come from generated code instead of being
            // reconstructed from RBP as a simple frame-record address. When no complete fault CONTEXT is usable, this
            // scalar path is still preferred over capturing the reporter's mixed-ABI frame.
            if(!valid && expected_return_address != 0u && trap_frame_address != 0u && trap_stack_pointer != 0u)
            {
                context = {};
                llvm_jit_win64_context_set_instruction_pointer(context, expected_return_address);
                llvm_jit_win64_context_set_stack_pointer(context, trap_stack_pointer);
                llvm_jit_win64_context_set_frame_pointer(context, trap_frame_address);
                llvm_jit_win64_context_initialize_link_register(context, trap_frame_address);
                valid = true;
            }
            else if(!valid && expected_return_address != 0u)
            {
                // Best-effort fallback for older generated code or partially available trap context: capture the helper
                // context and unwind a few frames until the expected JIT return address is recovered.
                ::fast_io::win32::nt::RtlCaptureContext(::std::addressof(context));
                for(unsigned i{}; i != 4u; ++i)
                {
                    if(llvm_jit_win64_context_instruction_pointer(context) == expected_return_address)
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
            return llvm_jit_win64_context_instruction_pointer(context) != 0u;
        }
# endif

        [[nodiscard, maybe_unused]] inline constexpr ::std::uintptr_t llvm_jit_signal_trap_return_address(::std::uintptr_t instruction_address) noexcept
        {
            if(instruction_address == 0u) { return 0u; }
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE && defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) &&                                       \
     !(defined(__arm64ec__) || defined(_M_ARM64EC))
            constexpr ::std::uintptr_t return_address_bias{4u};
# else
            constexpr ::std::uintptr_t return_address_bias{1u};
# endif
            if(instruction_address > (::std::numeric_limits<::std::uintptr_t>::max)() - return_address_bias) [[unlikely]] { return instruction_address; }
            return instruction_address + return_address_bias;
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
        inline constexpr void print_mmap_memory_out_of_bounds_trap(::uwvm2::object::memory::error::mmap_memory_error_t const& memerr,
                                                                    void const* platform_context [[maybe_unused]]) noexcept
        {
# if defined(UWVM_RUNTIME_LLVM_JIT)
            store_llvm_jit_trap_context(::uwvm2::runtime::lib::llvm_jit_trap_kind::memory_out_of_bounds,
                                        llvm_jit_signal_trap_return_address(memerr.instruction_address));
#  if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
            store_llvm_jit_win64_trap_caller_context(llvm_jit_signal_trap_return_address(memerr.instruction_address),
                                                     memerr.frame_address,
                                                     memerr.stack_pointer,
                                                     platform_context);
#  endif
# endif
            ::uwvm2::object::memory::error::output_mmap_memory_error_line(memerr);
            print_trap_fatal_message(trap_kind::memory_out_of_bounds);
            dump_call_stack_for_trap(trap_kind::memory_out_of_bounds);
        }

        inline constexpr void ensure_memory_signal_trap_bridge_initialized() noexcept
        {
            ::uwvm2::object::memory::signal::detail::set_mmap_memory_out_of_bounds_with_context_handler(
                print_mmap_memory_out_of_bounds_trap);
        }
#else
        // Non-mmap builds cannot receive guard-page memory traps, so the bridge initializer is intentionally a no-op.
        inline constexpr void ensure_memory_signal_trap_bridge_initialized() noexcept {}
#endif

#ifdef UWVM_CPP_EXCEPTIONS
        [[noreturn]] inline constexpr void
            print_and_terminate_compile_validation_error(::uwvm2::utils::container::u8string_view module_name,
                                                         ::uwvm2::validation::error::code_validation_error_impl const& v_err) noexcept
        {
            // Reconstruct validator diagnostics from parser module storage so users see the same source indication as parsing.
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

        struct local_imported_func_sig_snapshot
        {
#if defined(UWVM_RUNTIME_LLVM_JIT)
            // LLVM compilation may invoke an extensible provider while locks/tokens are suspended. Own the normalized
            // type bytes so no provider pointer survives that callback boundary.
            ::uwvm2::runtime::lib::details::local_imported_provider_function_signature_t owned{};

            [[nodiscard]] inline constexpr func_sig_view view() const noexcept
            {
                return {{valtype_kind::raw_u8, owned.parameter_types.data(), owned.parameter_types.size()},
                        {valtype_kind::raw_u8, owned.result_types.data(),    owned.result_types.size()   }};
            }
#else
            // In a pure interpreter build the header-only type-erasure adapter returns its own constexpr signature
            // cache directly; there is no extensible LLVM metadata callback boundary to cross.
            func_sig_view borrowed{};

            [[nodiscard]] inline constexpr func_sig_view view() const noexcept { return borrowed; }
#endif
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
            local_imported_func_sig_snapshot local_imported_signature{};
            ::std::size_t param_bytes{};
            ::std::size_t result_bytes{};
            preload_module_memory_attribute_t const* preload_module_memory_attribute{};
            // Only an explicitly declared function contract may bypass the host fenv guard. All non-local-import
            // target kinds retain this fail-closed value.
            local_imported_wasm_fp_control_policy_t llvm_wasm_fp_control_policy{
                local_imported_wasm_fp_control_policy_t::may_modify};

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

            [[nodiscard]] inline constexpr func_sig_view signature() const noexcept
            { return k == kind::local_imported ? local_imported_signature.view() : sig; }
        };

        inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::vector<cached_import_target>> g_import_call_cache{};  // [global]

        [[nodiscard]] inline constexpr cached_import_target const*
            find_cached_import_target(runtime_imported_func_storage_t const* function) noexcept
        {
            // Table elements retain the storage pointer of the module that created the funcref. That module need not own the table:
            // imports, table.set, and table.copy can all move references across module boundaries.
            auto const location{details::find_runtime_imported_function_location(g_runtime.modules, function)};
            if(!location.found || location.module_id >= g_import_call_cache.size()) [[unlikely]] { return nullptr; }

            auto const& module_cache{g_import_call_cache.index_unchecked(location.module_id)};
            if(location.function_index >= module_cache.size()) [[unlikely]] { return nullptr; }
            return ::std::addressof(module_cache.index_unchecked(location.function_index));
        }

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


#if defined(UWVM_RUNTIME_LLVM_JIT)
        // =========================================================================
        // LLVM call-stack policy and generated target publication
        // -------------------------------------------------------------------------
        // Full-module LLVM materialization publishes raw and typed entry addresses
        // into the indirect-call tables after code generation completes.
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
            // Native mode is an alternative to instruction instrumentation: no JIT logical push/pop is generated.
            // Capability validation happens before this code-generation decision, not after execution starts.
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            using runtime_llvm_jit_call_stack_t = runtime_mode::runtime_llvm_jit_call_stack_t;
            switch(get_runtime_llvm_jit_effective_call_stack_mode())
            {
                case runtime_llvm_jit_call_stack_t::auto_policy: [[fallthrough]];
                case runtime_llvm_jit_call_stack_t::instruction: return true;
                case runtime_llvm_jit_call_stack_t::unwind: [[fallthrough]];
                case runtime_llvm_jit_call_stack_t::unwind_uncheck: return false;
                case runtime_llvm_jit_call_stack_t::none: return false;
            }

            return true;
        }

        inline constexpr void
            configure_runtime_llvm_jit_call_stack_policy(::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option& opt) noexcept
        {
            // Convert the runtime call-stack policy into codegen switches before IR is emitted.
            validate_runtime_llvm_jit_unwind_call_stack_or_fatal();
            opt.emit_call_stack_frames = runtime_llvm_jit_uses_instruction_call_stack_frames();
            opt.emit_unwind_call_stack_frames = runtime_llvm_jit_unwind_call_stack_requested();
        }

        inline constexpr void publish_llvm_jit_call_indirect_defined_entry_targets(compiled_defined_func_info const* info,
                                                                                   ::std::uintptr_t raw_entry_address,
                                                                                   ::std::uintptr_t typed_entry_address) noexcept
        {
            // Publish raw/typed addresses with release ordering so readers never observe a partially installed generated entry.
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
                    if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::funcref))
                    {
                        return sizeof(::uwvm2::object::global::wasm_funcref_t);
                    }
                    if(code == static_cast<::std::uint_least8_t>(::uwvm2::parser::wasm::standard::wasm1p1::type::value_type::externref))
                    {
                        return sizeof(::uwvm2::object::global::wasm_externref_t);
                    }
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

        [[nodiscard]] inline constexpr local_imported_func_sig_snapshot
            func_sig_from_local_imported(local_imported_t const* m, ::std::size_t idx) noexcept
        {
            // Local-imported modules expose signature metadata through their import adapter rather than wasm runtime storage.
            local_imported_func_sig_snapshot result{};
#if defined(UWVM_RUNTIME_LLVM_JIT)
            if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_function_signature(m, idx, result.owned)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
#else
            // The pure-interpreter adapter returns its own immutable signature cache; no generated token is involved.
            auto const info{m->get_function_information_from_index(idx)};
            if(!info.successed) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const& ft{info.function_type};
            result.borrowed = {
                {valtype_kind::wasm_enum, ft.parameter.begin, static_cast<::std::size_t>(ft.parameter.end - ft.parameter.begin)},
                {valtype_kind::wasm_enum, ft.result.begin,    static_cast<::std::size_t>(ft.result.end - ft.result.begin)      }
            };
#endif
            return result;
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

        inline constexpr void erase_current_thread_scratch_state() noexcept
        {
            // Scratch blocks contain only backend-neutral bytes. Keep their capacity for the lifetime of native TLS.
        }
#else
        inline ::uwvm2::utils::container::concurrent_node_map<os_thread_id_t, thread_local_bump_allocator> g_call_scratch_states{};  // [global]

# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
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
# endif

        inline constexpr void erase_current_thread_scratch_state() noexcept { g_call_scratch_states.erase(current_thread_id()); }
#endif

        inline constexpr void erase_current_thread_runtime_state() noexcept
        {
            erase_current_thread_state();
            erase_current_thread_scratch_state();
        }

        using ::uwvm2::runtime::lib::details::runtime_execution_entry_reentry;

        class runtime_execution_entry_scope
        {
            ::uwvm2::runtime::lib::native_stack::scope native_stack_scope{};
            bool entered{};

        public:
            inline explicit runtime_execution_entry_scope(
                runtime_execution_entry_reentry reentry = runtime_execution_entry_reentry::reject) noexcept
            {
#if (defined(__linux__) || defined(__APPLE__)) && !defined(_WIN32)
                if(!native_stack_scope.ready()) [[unlikely]] { ::uwvm2::runtime::lib::native_stack::setup_failed(); }
#endif
                auto& depth{get_runtime_execution_entry_depth()};
                // Full/interpreter host entries are not recursively executable. The public LLVM raw API is the one
                // callback re-entry surface whose nested lifetime is explicitly supported.
                if(!::uwvm2::runtime::lib::details::runtime_execution_entry_enter(depth, reentry)) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                entered = true;
            }

            runtime_execution_entry_scope(runtime_execution_entry_scope const&) = delete;
            runtime_execution_entry_scope& operator= (runtime_execution_entry_scope const&) = delete;

            inline ~runtime_execution_entry_scope() noexcept
            {
                if(!entered) { return; }

                // Never retain a pointer/reference to a map-backed node across erasure. A permitted nested raw call
                // merely returns depth N to N-1; only the outermost entry owns complete per-thread cleanup.
                auto& depth{get_runtime_execution_entry_depth()};
                auto const leave_result{::uwvm2::runtime::lib::details::runtime_execution_entry_leave(depth)};
                if(leave_result == ::uwvm2::runtime::lib::details::runtime_execution_entry_leave_result::invalid) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                if(leave_result == ::uwvm2::runtime::lib::details::runtime_execution_entry_leave_result::outermost)
                { erase_current_thread_runtime_state(); }
            }
        };

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
                                                                  local_imported_wasm_fp_control_policy_t fp_control_policy,
                                                                  ::std::byte* result_buffer,
                                                                  ::std::byte* param_buffer,
                                                                  ::std::size_t caller_module_id) noexcept
        {
            // Select the caller-specific WASI environment around local-imported calls only when overrides make the default fast path
            // insufficient.
            if(try_prepare_default_global_wasip1_env_fast_path(caller_module_id)) [[likely]]
            {
                invoke_host_with_llvm_wasm_fp_control_policy(fp_control_policy,
                    [&]() noexcept { module.call_func_index(function_index, result_buffer, param_buffer); });
                return;
            }

            auto& wasip1_env{resolve_wasip1_env_for_runtime_module_id(caller_module_id)};
            bind_wasip1_memory_for_selected_env(wasip1_env, caller_module_id);

            if(is_current_wasip1_env_selected(wasip1_env)) [[likely]]
            {
                invoke_host_with_llvm_wasm_fp_control_policy(fp_control_policy,
                    [&]() noexcept { module.call_func_index(function_index, result_buffer, param_buffer); });
                return;
            }

            ::uwvm2::uwvm::imported::wasi::wasip1::storage::scoped_current_wasip1_env_t wasip1_env_guard{wasip1_env};
            invoke_host_with_llvm_wasm_fp_control_policy(fp_control_policy,
                [&]() noexcept { module.call_func_index(function_index, result_buffer, param_buffer); });
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
                invoke_host_preserving_llvm_wasm_fp_environment([&]() noexcept { function.func_ptr(result_buffer, param_buffer); });
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
                        invoke_host_preserving_llvm_wasm_fp_environment([&]() noexcept { function.func_ptr(result_buffer, param_buffer); });
                    }
                    else
                    {
                        invoke_host_preserving_llvm_wasm_fp_environment([&]() noexcept { function.func_ptr(result_buffer, param_buffer); });
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
#else
        [[nodiscard]] inline constexpr bool is_wasip1_import_visible_for_runtime_module_id(::std::size_t) noexcept { return true; }

        inline constexpr void call_local_imported_with_wasip1_env(local_imported_t const& module,
                                                                  ::std::size_t function_index,
                                                                  local_imported_wasm_fp_control_policy_t fp_control_policy,
                                                                  ::std::byte* result_buffer,
                                                                  ::std::byte* param_buffer,
                                                                  ::std::size_t) noexcept
        {
            invoke_host_with_llvm_wasm_fp_control_policy(fp_control_policy,
                [&]() noexcept { module.call_func_index(function_index, result_buffer, param_buffer); });
        }

        inline constexpr void call_capi_with_wasip1_env(capi_function_t const& function,
                                                        preload_module_memory_attribute_t const* preload_module_memory_attribute,
                                                        ::std::byte* result_buffer,
                                                        ::std::byte* param_buffer,
                                                        ::std::size_t caller_module_id) noexcept
        {
            preload_call_context_guard preload_guard{preload_module_memory_attribute, ::std::addressof(function), caller_module_id};
            invoke_host_preserving_llvm_wasm_fp_environment([&]() noexcept { function.func_ptr(result_buffer, param_buffer); });
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

#if defined(UWVM_RUNTIME_LLVM_JIT)
            ::uwvm2::runtime::lib::details::local_imported_provider_memory_snapshot_t snapshot{};
            if(!::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_access_snapshot(
                   local_imported, local_imported_index, snapshot)) [[unlikely]]
            {
                return false;
            }
#else
            ::uwvm2::uwvm::wasm::type::memory_access_snapshot_result_t snapshot{};
            if(!local_imported->memory_access_snapshot_from_index(local_imported_index, snapshot)) [[unlikely]] { return false; }
#endif

            resolved.kind = resolved_preload_memory_t::target_kind::local_imported;
            resolved.native_memory = nullptr;
            resolved.local_imported = local_imported;
            resolved.local_imported_index = local_imported_index;
            resolved.memory_begin = snapshot.memory_begin;
            resolved.page_count = snapshot.page_count;
#if defined(UWVM_RUNTIME_LLVM_JIT)
            resolved.page_size_bytes =
                ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_page_size(local_imported, local_imported_index);
#else
            resolved.page_size_bytes = local_imported->memory_page_size_from_index(local_imported_index);
#endif
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
#if defined(UWVM_RUNTIME_LLVM_JIT)
                    return ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_read(
                        local_imported, resolved.local_imported_index, offset, destination, size);
#else
                    return local_imported->memory_read_from_index(resolved.local_imported_index, offset, destination, size);
#endif
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
#if defined(UWVM_RUNTIME_LLVM_JIT)
                    return ::uwvm2::runtime::lib::details::invoke_local_imported_provider_memory_write(
                        local_imported, resolved.local_imported_index, offset, source, size);
#else
                    return local_imported->memory_write_to_index(resolved.local_imported_index, offset, source, size);
#endif
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

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // =========================================================================
        // Interpreter frame layout and opfunc dispatch
        // -------------------------------------------------------------------------
        // The interpreter executes translated opfunc streams over a byte-packed local
        // area and operand stack. This section defines how host buffers, local frames,
        // direct calls, imports, and raw host buffers enter that model.
        //
        // Coverage invariants:
        // - alloca-backed frame buffers must be allocated in the caller's stack frame.
        // - Local and operand regions are aligned separately because opfuncs may read typed values.
        // - Tail-call dispatch and by-reference dispatch must observe the same stack-top convention.
        // - Raw host/JIT buffers are staged through byte stacks before interpreter bodies run.
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

# if !(defined(__pdp11) || defined(UWVM_TARGET_POWERPC_FAMILY) || defined(__s390x__) || (defined(__wasm__) && !defined(__wasm_tail_call__)))
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
            // powerpc64: UWVM opfunc dispatch is indirect; PPC musttail is conditional and this dispatch form is rejected.
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
            // s390x: GCC 15 can lower the indirect opfunc edge to a stack-stable sibling `br`, but Clang 22 rejects the
            // equivalent `musttail` edge when the function pointer is loaded from the bytecode stream. Keep the portable
            // byref dispatcher selected above so every supported compiler has a bounded interpreter-loop stack.
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

#endif

#undef UWVM_TARGET_POWERPC_FAMILY

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

#if defined(UWVM_RUNTIME_LLVM_JIT)
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
#endif

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
                if(!::uwvm2::runtime::lib::details::try_add_runtime_byte_extents(frame_alloc_n, kFrameAlignPad, stack_cap_raw, frame_alloc_n)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
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

            // Result capacity is a postcondition of normal return, not a condition for entering the function.
            // WebAssembly's `unreachable` is stack-polymorphic (already in Core 1.0): a valid result-typed function
            // may trap without ever producing those values, so its actual operand-stack peak can be zero.
            // https://webassembly.github.io/spec/core/valid/instructions.html#valid-unreachable
            // Keep the exact allocation and fail closed before copying any normal-return results.
            if(stack_cap_raw < result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

            // Append results back to caller stack.
            copy_bytes_small(*caller_stack_top_ptr, operand_base, result_bytes);
            *caller_stack_top_ptr += result_bytes;

# if defined(UWVM_USE_THREAD_LOCAL)
            if(use_scratch) { g_call_scratch.release(scratch_mark); }
# else
            if(use_scratch) { get_call_scratch().release(scratch_mark); }
# endif
        }

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
                                                                     local_imported_wasm_fp_control_policy_t fp_control_policy,
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
            call_local_imported_with_wasip1_env(*m, tgt.index, fp_control_policy, resbuf, parbuf, caller_module_id);

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
        [[nodiscard]] inline constexpr bool try_get_runtime_llvm_jit_raw_defined_entry_address(::std::size_t module_id,
                                                                                               ::std::size_t function_index,
                                                                                               ::std::uintptr_t& function_address) noexcept
        {
            function_address = 0u;
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto const& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr || !rec.llvm_jit_ready) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) { return false; }

            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_local_raw_entry_addresses.size()) [[unlikely]] { return false; }

            function_address = rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local_index);
            return function_address != 0u;
        }

        [[nodiscard]] inline constexpr bool
            try_get_runtime_llvm_jit_defined_entry_address(::std::size_t module_id, ::std::size_t function_index, ::std::uintptr_t& function_address) noexcept
        {
            function_address = 0u;
            if(module_id >= g_runtime.modules.size()) [[unlikely]] { return false; }

            auto const& rec{g_runtime.modules.index_unchecked(module_id)};
            auto const runtime_module{rec.runtime_module};
            if(runtime_module == nullptr || !rec.llvm_jit_ready) [[unlikely]] { return false; }

            auto const import_n{runtime_module->imported_function_vec_storage.size()};
            if(function_index < import_n) { return false; }

            auto const local_index{function_index - import_n};
            if(local_index >= rec.llvm_jit_local_entry_addresses.size()) [[unlikely]] { return false; }

            function_address = rec.llvm_jit_local_entry_addresses.index_unchecked(local_index);
            return function_address != 0u;
        }

        [[nodiscard]] inline constexpr bool try_invoke_runtime_llvm_jit_raw_defined_entry(::std::size_t module_id,
                                                                                          ::std::size_t function_index,
                                                                                          void* result_buffer,
                                                                                          ::std::size_t result_bytes,
                                                                                          void const* param_buffer,
                                                                                          ::std::size_t param_bytes) noexcept
        {
            // Enter eagerly materialized code through the raw wrapper ABI so host buffers do not need to match the typed wasm signature.
            ::std::uintptr_t function_address{};
            if(!try_get_runtime_llvm_jit_raw_defined_entry_address(module_id, function_index, function_address)) { return false; }

            using entry_fn_t =
                void(UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI*)(::std::uintptr_t, ::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::size_t);
            auto const entry_fn{reinterpret_cast<entry_fn_t>(function_address)};
            entry_fn(0u, pointer_to_uintptr(result_buffer), result_bytes, pointer_to_uintptr(param_buffer), param_bytes);
            return true;
        }
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        inline constexpr void execute_interpreter_defined(call_stack_tls_state& call_stack,
                                                          ::std::size_t module_id,
                                                          ::std::size_t function_index,
                                                          runtime_local_func_storage_t const* runtime_func,
                                                          compiled_local_func_t const* compiled_func,
                                                          ::std::size_t param_bytes,
                                                          ::std::size_t result_bytes,
                                                          ::std::byte** stack_top_ptr) noexcept
        {
            static_cast<void>(module_id);
            static_cast<void>(function_index);
            if(runtime_func == nullptr || compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            execute_compiled_defined(call_stack, runtime_func, compiled_func, param_bytes, result_bytes, stack_top_ptr);
        }

        inline constexpr void
            execute_interpreter_defined(call_stack_tls_state& call_stack, compiled_defined_func_info const& info, ::std::byte** stack_top_ptr) noexcept
        {
            execute_interpreter_defined(call_stack,
                                        info.module_id,
                                        info.function_index,
                                        info.runtime_func,
                                        info.compiled_func,
                                        info.param_bytes,
                                        info.result_bytes,
                                        stack_top_ptr);
        }

        inline constexpr void execute_interpreter_defined(
            call_stack_tls_state& call_stack,
            ::uwvm2::runtime::compiler::uwvm_int::optable::compiled_defined_call_info const& info,
            ::std::byte** stack_top_ptr) noexcept
        {
            execute_interpreter_defined(call_stack,
                                        info.module_id,
                                        info.function_index,
                                        static_cast<runtime_local_func_storage_t const*>(info.runtime_func),
                                        info.compiled_func,
                                        info.param_bytes,
                                        info.result_bytes,
                                        stack_top_ptr);
        }

#endif


#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[maybe_unused]] UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_FUNC_ATTR inline constexpr void
            llvm_jit_raw_call_cached_import_entry(::std::uintptr_t context_address,
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
                        // LLVM-generated import thunks may call only eagerly materialized native Wasm entries.
                        if(try_invoke_runtime_llvm_jit_raw_defined_entry(tgt->frame.module_id,
                                                                         tgt->frame.function_index,
                                                                         result_buffer,
                                                                         tgt->result_bytes,
                                                                         param_buffer,
                                                                         tgt->param_bytes))
                        {
                            return;
                        }
#endif
                        ::fast_io::fast_terminate();
                    }
                    case cached_import_target::kind::local_imported:
                    {
                        auto const local_imported_module{tgt->u.local_imported.module_ptr};
                        if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        call_stack_guard g{call_stack, tgt->frame.module_id, tgt->frame.function_index};
                        call_local_imported_with_wasip1_env(*local_imported_module,
                                                            tgt->u.local_imported.index,
                                                            tgt->llvm_wasm_fp_control_policy,
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
                for(::std::size_t steps{};; ++steps)
                {
                    // Initialization rejects alias cycles. Retain a hard bound here so corrupted embedding state fails closed
                    // instead of hanging the runtime during table-view publication or call_indirect dispatch.
                    if(steps > 8192uz) [[unlikely]] { return nullptr; }
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

#if defined(UWVM_RUNTIME_LLVM_JIT)
        inline constexpr void clear_llvm_jit_call_indirect_table_views() noexcept
        {
            // Runtime-module views point into per-record target vectors owned by g_runtime. Clear the borrowed addresses before
            // destroying those vectors so reset/reload never leaves a dangling native dispatch view in otherwise-live storage.
            details::clear_borrowed_llvm_jit_call_indirect_table_views(g_runtime.modules);
        }

        [[nodiscard]] inline constexpr runtime_llvm_jit_raw_call_target_t make_llvm_jit_call_indirect_target(
            compiled_module_record const& caller_rec,
            runtime_table_elem_storage_t const& elem) noexcept
        {
            using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
            runtime_llvm_jit_raw_call_target_t target{};
            switch(elem.type)
            {
                case table_elem_type::func_ref_defined:
                {
                    auto const defined_func_ptr{elem.storage.defined_ptr};
                    if(defined_func_ptr == nullptr) { break; }

                    auto const defined_info{find_defined_func_info(defined_func_ptr)};
                    if(defined_info == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                    // Publish the eagerly materialized native executable address.
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
                    {
                        ::fast_io::fast_terminate();
                    }
                    target.encoded_type_id = find_canonical_type_id_for_sig(caller_rec, func_sig_from_defined(defined_info->runtime_func));
                    break;
                }
                case table_elem_type::func_ref_imported:
                {
                    auto const imported_func_ptr{elem.storage.imported_ptr};
                    if(imported_func_ptr == nullptr) { break; }
                    auto const cached_target_ptr{find_cached_import_target(imported_func_ptr)};
                    if(cached_target_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& cached_target{*cached_target_ptr};
                    target.entry_address = reinterpret_cast<::std::uintptr_t>(llvm_jit_raw_call_cached_import_entry);
                    target.context_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(cached_target));
                    target.encoded_type_id = find_canonical_type_id_for_sig(caller_rec, cached_target.signature());
                    break;
                }
                [[unlikely]] default:
                {
                    ::fast_io::fast_terminate();
                }
            }
            return target;
        }

        inline constexpr void populate_llvm_jit_call_indirect_table_views() noexcept
        {
            // LLVM call_indirect lowers through compact table-view arrays. They are rebuilt after initialization/materialization so
            // every table element points at an eagerly materialized raw entry or a runtime import bridge.
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
                    if(resolved_table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(resolved_table->table_type_ptr->reftype !=
                       ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref)
                    {
                        // call_indirect is defined only for funcref tables. Externref tables intentionally have no native
                        // indirect-call view; their opaque host references remain owned by the runtime table itself.
                        continue;
                    }

                    auto& target_vec{caller_rec.llvm_jit_call_indirect_targets.index_unchecked(table_index)};
                    target_vec.clear();
                    target_vec.resize(resolved_table->elems.size());

                    for(::std::size_t elem_index{}; elem_index != resolved_table->elems.size(); ++elem_index)
                    {
                        target_vec.index_unchecked(elem_index) =
                            make_llvm_jit_call_indirect_target(caller_rec, resolved_table->elems.index_unchecked(elem_index));
                    }

                    table_view.data_address = reinterpret_cast<::std::uintptr_t>(target_vec.data());
                    table_view.size = target_vec.size();
                }
            }
        }

        inline constexpr void update_llvm_jit_call_indirect_table_views(
            runtime_table_storage_t* mutated_table,
            ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind kind,
            ::std::size_t begin,
            ::std::size_t count) noexcept
        {
            using mutation_kind = ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind;
            if(mutated_table == nullptr || mutated_table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(mutated_table->table_type_ptr->reftype !=
               ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
            if(count == 0uz) { return; }

            bool grow_mutation{};
            switch(kind)
            {
                case mutation_kind::set: [[fallthrough]];
                case mutation_kind::init: [[fallthrough]];
                case mutation_kind::copy: [[fallthrough]];
                case mutation_kind::fill: break;
                case mutation_kind::grow: grow_mutation = true; break;
                [[unlikely]] default: ::fast_io::fast_terminate();
            }

            auto const table_size{mutated_table->elems.size()};
            if(begin > table_size || count > table_size - begin) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(grow_mutation && count != table_size - begin) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const affected_view_count{details::for_each_borrowed_llvm_jit_call_indirect_table_view_alias(
                g_runtime.modules,
                mutated_table,
                [](runtime_module_storage_t const& module, ::std::size_t table_index) constexpr noexcept
                { return resolve_table(module, table_index); },
                [&](compiled_module_record& caller_rec, runtime_module_storage_t& caller_module, ::std::size_t table_index) constexpr noexcept
                {
                    if(caller_rec.llvm_jit_call_indirect_targets.size() != caller_module.llvm_jit_call_indirect_table_views.size()) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }

                    auto& target_vec{caller_rec.llvm_jit_call_indirect_targets.index_unchecked(table_index)};
                    if(grow_mutation)
                    {
                        // A successful table.grow has already appended exactly [begin, table_size) to the resolved table.
                        // Keep the old prefix and encode only the new suffix before publishing the relocated view once.
                        if(target_vec.size() != begin) [[unlikely]] { ::fast_io::fast_terminate(); }
                        target_vec.resize(table_size);
                    }
                    else if(target_vec.size() != table_size) [[unlikely]]
                    {
                        ::fast_io::fast_terminate();
                    }

                    for(::std::size_t elem_index{begin}; elem_index != begin + count; ++elem_index)
                    {
                        auto const target{make_llvm_jit_call_indirect_target(
                            caller_rec, mutated_table->elems.index_unchecked(elem_index))};
                        target_vec.index_unchecked(elem_index) = target;
                    }

                    if(grow_mutation)
                    {
                        auto& table_view{caller_module.llvm_jit_call_indirect_table_views.index_unchecked(table_index)};
                        table_view = {reinterpret_cast<::std::uintptr_t>(target_vec.data()), target_vec.size()};
                    }
                })};
            if(affected_view_count == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }
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
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only: return u8"llvm_jit_only";
#endif
                [[unlikely]] default:
                    return u8"<unknown-runtime-compiler>";
            }
        }

        [[nodiscard]] inline constexpr details::runtime_state_signature make_runtime_state_signature() noexcept
        {
            namespace runtime_mode = ::uwvm2::uwvm::runtime::runtime_mode;

            details::runtime_state_signature signature{};
            signature.kind = details::runtime_state_kind::full;
            signature.runtime_compiler = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_compiler);
            signature.runtime_mode = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_mode);
            signature.runtime_compile_threads_existed = runtime_mode::runtime_compile_threads_existed;
            signature.runtime_compile_threads_policy = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_compile_threads_policy);
            signature.runtime_compile_threads_resolved = runtime_mode::global_runtime_compile_threads_resolved;
            signature.runtime_scheduling_policy_existed = runtime_mode::runtime_scheduling_policy_existed;
            signature.runtime_scheduling_policy = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_scheduling_policy);
            signature.runtime_scheduling_size = runtime_mode::global_runtime_scheduling_size;

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            signature.uwvm_int_disable_loop_unwind = runtime_mode::runtime_uwvm_int_disable_loop_unwind;
            signature.uwvm_int_opcode_conbination_level =
                static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_uwvm_int_opcode_conbination_level);
            signature.uwvm_int_disable_delay_local = runtime_mode::runtime_uwvm_int_disable_delay_local;
            signature.uwvm_int_enable_instruction_reorder = runtime_mode::runtime_uwvm_int_enable_instruction_reorder;
            signature.uwvm_int_loop_unwind_max_size = runtime_mode::global_runtime_uwvm_int_loop_unwind_max_size;
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
            signature.llvm_jit_policy_existed = runtime_mode::runtime_llvm_jit_policy_existed;
            signature.llvm_jit_policy = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_llvm_jit_policy);
            signature.llvm_jit_full_policy_existed = runtime_mode::runtime_llvm_jit_full_policy_existed;
            signature.llvm_jit_full_policy = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_llvm_jit_full_policy);
            signature.llvm_jit_call_stack_existed = runtime_mode::runtime_llvm_jit_call_stack_existed;
            signature.llvm_jit_call_stack = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_llvm_jit_call_stack);
            signature.llvm_jit_cache_path_existed = runtime_mode::runtime_llvm_jit_cache_path_existed;
            signature.llvm_jit_cache_path_mode = static_cast<::std::uint_least32_t>(runtime_mode::global_runtime_llvm_jit_cache_path_mode);
            auto const configured_cache_path{::uwvm2::runtime::llvm_jit_cache::configured_cache_directory()};
            signature.llvm_jit_cache_path_size = configured_cache_path.size();
            signature.llvm_jit_cache_path_hash =
                details::stable_runtime_state_u8_hash(configured_cache_path.data(), configured_cache_path.size());
            auto const cache_policy{::uwvm2::runtime::llvm_jit_cache::default_cache_policy()};
            signature.llvm_jit_cache_enabled = cache_policy.enable;
            signature.llvm_jit_cache_generate_signature = cache_policy.generate_signature;
            signature.llvm_jit_cache_verify_signature = cache_policy.verify_signature;
#endif
            return signature;
        }

        inline constexpr void require_runtime_state_transition(details::runtime_state_signature const& requested, bool eager_ready) noexcept
        {
            auto const transition{details::classify_runtime_state_transition(g_runtime.published_runtime_state, requested)};
            if(eager_ready)
            {
                if(transition != details::runtime_state_transition::reuse) [[unlikely]]
                {
                    // Generated code and caches cannot be retargeted in place. The embedding host must quiesce execution and
                    // call reset_runtime_state_host_api() before changing any backend or compilation setting.
                    ::fast_io::fast_terminate();
                }
                return;
            }
            if(transition != details::runtime_state_transition::initialize) [[unlikely]] { ::fast_io::fast_terminate(); }
        }

        enum class default_runtime_scheduling_profile_t : unsigned
        {
            // Default split policies differ by backend because interpreter translation and LLVM codegen scale differently.
            uwvm_int,
            llvm_jit
        };

        // Keep runtime scheduling state backend-neutral. In a combined build the selected LLVM backend must not inherit uwvm-int's
        // resolver merely because interpreter support is also present in the binary (and vice versa). Adapters below translate this
        // small stable representation at the compiler boundary.
        enum class runtime_compile_task_split_policy_t : unsigned
        {
            function_count,
            code_size
        };

        struct runtime_compile_task_split_config_t
        {
            runtime_compile_task_split_policy_t policy{runtime_compile_task_split_policy_t::code_size};
            ::std::size_t split_size{4096uz};
            bool adjust_for_default_policy{true};
        };

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        [[nodiscard]] inline constexpr ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config
            make_uwvm_int_compile_task_split_config(runtime_compile_task_split_config_t const& config) noexcept
        {
            using policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t;
            return {.policy = config.policy == runtime_compile_task_split_policy_t::function_count ? policy_t::function_count : policy_t::code_size,
                    .split_size = config.split_size,
                    .adjust_for_default_policy = config.adjust_for_default_policy};
        }

        [[nodiscard]] inline constexpr runtime_compile_task_split_config_t from_uwvm_int_compile_task_split_config(
            ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config const& config) noexcept
        {
            using policy_t = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t;
            return {.policy = config.policy == policy_t::function_count ? runtime_compile_task_split_policy_t::function_count
                                                                        : runtime_compile_task_split_policy_t::code_size,
                    .split_size = config.split_size,
                    .adjust_for_default_policy = config.adjust_for_default_policy};
        }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config
            make_llvm_jit_compile_task_split_config(runtime_compile_task_split_config_t const& config) noexcept
        {
            using policy_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t;
            return {.policy = config.policy == runtime_compile_task_split_policy_t::function_count ? policy_t::function_count : policy_t::code_size,
                    .split_size = config.split_size,
                    .adjust_for_default_policy = config.adjust_for_default_policy};
        }

        [[nodiscard]] inline constexpr runtime_compile_task_split_config_t from_llvm_jit_compile_task_split_config(
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_config const& config) noexcept
        {
            using policy_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_task_split_policy_t;
            return {.policy = config.policy == policy_t::function_count ? runtime_compile_task_split_policy_t::function_count
                                                                        : runtime_compile_task_split_policy_t::code_size,
                    .split_size = config.split_size,
                    .adjust_for_default_policy = config.adjust_for_default_policy};
        }
#endif

        [[nodiscard]] inline constexpr runtime_compile_task_split_config_t resolve_effective_runtime_compile_task_split_config(
            runtime_module_storage_t const& runtime_module,
            runtime_compile_task_split_config_t config,
            ::std::size_t extra_compile_threads,
            ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t compiler) noexcept
        {
            using compiler_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t;
            switch(compiler)
            {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                case compiler_t::uwvm_interpreter_only:
                    return from_uwvm_int_compile_task_split_config(
                        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_compile_task_split_config(
                            runtime_module, make_uwvm_int_compile_task_split_config(config), extra_compile_threads));
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case compiler_t::llvm_jit_only:
                    return from_llvm_jit_compile_task_split_config(
                        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::resolve_effective_compile_task_split_config(
                            runtime_module, make_llvm_jit_compile_task_split_config(config), extra_compile_threads));
#endif
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

        [[nodiscard]] inline constexpr ::std::size_t resolve_effective_runtime_adaptive_extra_compile_threads(
            runtime_module_storage_t const& runtime_module,
            runtime_compile_task_split_config_t config,
            ::std::size_t extra_compile_threads_upper_bound,
            ::std::size_t target_task_groups_per_adjusted_compile_thread,
            bool split_was_adjusted,
            ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t compiler) noexcept
        {
            using compiler_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t;
            switch(compiler)
            {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                case compiler_t::uwvm_interpreter_only:
                    return ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::resolve_effective_adaptive_extra_compile_threads(
                        runtime_module,
                        make_uwvm_int_compile_task_split_config(config),
                        extra_compile_threads_upper_bound,
                        target_task_groups_per_adjusted_compile_thread,
                        split_was_adjusted);
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case compiler_t::llvm_jit_only:
                    return ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::resolve_effective_adaptive_extra_compile_threads(
                        runtime_module,
                        make_llvm_jit_compile_task_split_config(config),
                        extra_compile_threads_upper_bound,
                        target_task_groups_per_adjusted_compile_thread,
                        split_was_adjusted);
#endif
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

        [[nodiscard]] inline constexpr ::std::size_t resolve_runtime_target_task_groups_per_adjusted_compile_thread(
            bool aggressive,
            ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t compiler) noexcept
        {
            using compiler_t = ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t;
            switch(compiler)
            {
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                case compiler_t::uwvm_interpreter_only:
                    return aggressive
                               ? ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::aggressive_target_task_groups_per_adjusted_compile_thread
                               : ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::default_target_task_groups_per_adjusted_compile_thread;
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
                case compiler_t::llvm_jit_only:
                    return aggressive
                               ? ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::aggressive_target_task_groups_per_adjusted_compile_thread
                               : ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::default_target_task_groups_per_adjusted_compile_thread;
#endif
                [[unlikely]] default: ::fast_io::fast_terminate();
            }
        }

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
            return ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                   ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::uwvm_interpreter_only;
        }
#else
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_uwvm_int_translation() noexcept { return false; }
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_llvm_jit_translation() noexcept
        {
            return ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler ==
                   ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only;
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
            // Raw entry symbols use the buffer ABI exposed to hosts and import thunks.
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            return llvm_jit_translate_details::get_llvm_wasm_raw_function_name(
                runtime_module,
                static_cast<llvm_jit_translate_details::validation_module_traits_t::wasm_u32>(func_index));
        }

        template <typename ValueType>
        [[nodiscard]] inline constexpr bool
            runtime_llvm_jit_cache_range_bytes(ValueType const* begin, ValueType const* end, ::std::byte const*& first, ::std::size_t& size) noexcept
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

        inline constexpr void runtime_llvm_jit_cache_sha_update(::fast_io::sha256_context& sha, ::std::byte const* first, ::std::size_t size) noexcept
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
        inline constexpr void runtime_llvm_jit_cache_sha_update_function_type(::fast_io::sha256_context& sha, FunctionType const* function_type) noexcept
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

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string runtime_llvm_jit_cache_sha256_hex(::fast_io::sha256_context& sha) noexcept
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
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(key,
                                                                                  u8"parallel-object-count",
                                                                                  static_cast<::std::uint_least64_t>(object_count));
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(key,
                                                                                  u8"parallel-object-index",
                                                                                  static_cast<::std::uint_least64_t>(object_index));
            ctx.cache_key = ::std::move(key);
            ctx.cache_key_is_complete = true;
            return ctx;
        }

        [[nodiscard]] inline constexpr bool
            load_runtime_llvm_jit_parallel_object_cache(::uwvm2::runtime::llvm_jit_cache::cache_context const& base_context,
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

                loaded_objects.emplace_back(
                    ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(load.object.cbegin()), load.object.size()});
            }

            object_outputs = ::std::move(loaded_objects);
            return true;
        }

        inline constexpr void
            store_runtime_llvm_jit_parallel_object_cache(::uwvm2::runtime::llvm_jit_cache::cache_context const& base_context,
                                                         ::uwvm2::runtime::llvm_jit_cache::cache_policy const& cache_policy,
                                                         ::uwvm2::utils::container::u8string_view module_name,
                                                         ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& object_outputs) noexcept
        {
            if(!cache_policy.enable) { return; }

            auto const object_count{object_outputs.size()};
            for(::std::size_t object_index{}; object_index != object_count; ++object_index)
            {
                auto object_context{runtime_llvm_jit_parallel_object_cache_context(base_context, object_count, object_index)};
                auto const& object_output{object_outputs.index_unchecked(object_index)};
                auto const first{reinterpret_cast<::std::byte const*>(object_output.cbegin())};
                auto const status{::uwvm2::runtime::llvm_jit_cache::store_object_async(object_context,
                                                                                       first,
                                                                                       object_output.size(),
                                                                                       cache_policy,
                                                                                       module_name,
                                                                                       true,
                                                                                       object_index,
                                                                                       object_count)};
                ::uwvm2::runtime::llvm_jit_cache::details::runtime_cache_log_line(u8"object-cache-store-enqueue module=\"",
                                                                                  module_name,
                                                                                  u8"\" status=",
                                                                                  ::uwvm2::runtime::llvm_jit_cache::cache_status_name(status),
                                                                                  u8" bytes=",
                                                                                  object_output.size(),
                                                                                  u8" object_index=",
                                                                                  object_index,
                                                                                  u8" object_count=",
                                                                                  object_count);
            }
        }

        [[nodiscard]] inline constexpr bool initialize_llvm_jit_process_target() noexcept
        {
# if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
            // Register the target selected by the compiler for this executable. LLVM's InitializeNativeTarget() follows the
            // LLVM_NATIVE_TARGET recorded in llvm-config's generated headers, which can describe the build host instead when uwvm2
            // is cross-compiled (for example, x86_64 Linux -> AArch64 Linux) and then makes MCJIT target selection fail at runtime.
            ::LLVMInitializeX86TargetInfo();
            ::LLVMInitializeX86Target();
            ::LLVMInitializeX86TargetMC();
            ::LLVMInitializeX86AsmPrinter();
            return true;
# elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
            ::LLVMInitializeAArch64TargetInfo();
            ::LLVMInitializeAArch64Target();
            ::LLVMInitializeAArch64TargetMC();
            ::LLVMInitializeAArch64AsmPrinter();
            return true;
# elif defined(__arm__) || defined(_M_ARM)
            ::LLVMInitializeARMTargetInfo();
            ::LLVMInitializeARMTarget();
            ::LLVMInitializeARMTargetMC();
            ::LLVMInitializeARMAsmPrinter();
            return true;
# elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
            ::LLVMInitializePowerPCTargetInfo();
            ::LLVMInitializePowerPCTarget();
            ::LLVMInitializePowerPCTargetMC();
            ::LLVMInitializePowerPCAsmPrinter();
            return true;
# elif defined(__riscv)
            ::LLVMInitializeRISCVTargetInfo();
            ::LLVMInitializeRISCVTarget();
            ::LLVMInitializeRISCVTargetMC();
            ::LLVMInitializeRISCVAsmPrinter();
            return true;
# elif defined(__s390x__)
            ::LLVMInitializeSystemZTargetInfo();
            ::LLVMInitializeSystemZTarget();
            ::LLVMInitializeSystemZTargetMC();
            ::LLVMInitializeSystemZAsmPrinter();
            return true;
# elif defined(__loongarch__)
            ::LLVMInitializeLoongArchTargetInfo();
            ::LLVMInitializeLoongArchTarget();
            ::LLVMInitializeLoongArchTargetMC();
            ::LLVMInitializeLoongArchAsmPrinter();
            return true;
# elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
            ::LLVMInitializeMipsTargetInfo();
            ::LLVMInitializeMipsTarget();
            ::LLVMInitializeMipsTargetMC();
            ::LLVMInitializeMipsAsmPrinter();
            return true;
# elif defined(__sparc__)
            ::LLVMInitializeSparcTargetInfo();
            ::LLVMInitializeSparcTarget();
            ::LLVMInitializeSparcTargetMC();
            ::LLVMInitializeSparcAsmPrinter();
            return true;
# else
            return !::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter();
# endif
        }

        inline constexpr bool ensure_llvm_jit_native_target_initialized() noexcept
        {
            // LLVM native target setup is process-global and not idempotent in all versions.
            static ::std::atomic_bool initialized{};
            static ::std::atomic_bool success{};
            static ::std::atomic_flag init_lock = ATOMIC_FLAG_INIT;

            if(initialized.load(::std::memory_order_acquire)) { return success.load(::std::memory_order_acquire); }

            while(init_lock.test_and_set(::std::memory_order_acquire))
            {
                if(initialized.load(::std::memory_order_acquire)) { return success.load(::std::memory_order_acquire); }
                ::fast_io::this_thread::yield();
            }

            if(!initialized.load(::std::memory_order_relaxed))
            {
                auto& pass_registry{*::llvm::PassRegistry::getPassRegistry()};
                ::llvm::initializeCore(pass_registry);
                ::llvm::initializeTransformUtils(pass_registry);
                ::llvm::initializeScalarOpts(pass_registry);
                ::llvm::initializeInstCombine(pass_registry);
                ::llvm::initializeAnalysis(pass_registry);
                ::llvm::initializeTarget(pass_registry);
                success.store(initialize_llvm_jit_process_target(), ::std::memory_order_release);
                initialized.store(true, ::std::memory_order_release);
            }

            init_lock.clear(::std::memory_order_release);
            return success.load(::std::memory_order_acquire);
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
# if (defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)) && !defined(__mips_msa)
            // Match optional JIT FP units to the runtime's protected controls and MSA-compatible ABI.
            mattrs.emplace_back(u8"-msa");
# endif
# if defined(__riscv) && !defined(__riscv_flen)
            mattrs.emplace_back(u8"-f");
            mattrs.emplace_back(u8"-d");
            mattrs.emplace_back(u8"-v");
# endif
            if(::llvm::Triple{::llvm::sys::getProcessTriple()}.getArch() == ::llvm::Triple::ve)
            {
                // LLVM 23 VE's VPU legalizer widens Wasm's short vectors to
                // 256 lanes, has missing widening patterns, and may spill a
                // kilobyte for a 16-byte operation. Use its scalar backend for
                // these generated entries, preserving guarded volatile memory
                // operations rather than splitting them prematurely in IR.
                // This MUST configure TargetMachine: VE ignores per-function
                // target-features, so adding -vpu only to IR is ineffective.
                // Apply to full/lazy preoptimization and final MCJIT alike;
                // the resulting target feature string is also part of the cache.
                mattrs.emplace_back(u8"-vpu");
            }
# if (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)) && !defined(__ALTIVEC__) && !defined(__linux__)
            mattrs.emplace_back(u8"-altivec");
            mattrs.emplace_back(u8"-vsx");
# endif
            return mattrs;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string get_llvm_jit_host_cpu_name_storage() noexcept
        {
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            auto const host_cpu_name{::llvm::sys::getHostCPUName()};
            return ::uwvm2::utils::container::u8string{llvm_jit_translate_details::get_uwvm_u8string_view(host_cpu_name)};
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

        [[nodiscard]] inline ::llvm::Triple get_runtime_llvm_jit_mcjit_target_triple()
        {
            // Do not use EngineBuilder::selectTarget()'s implicit getProcessTriple().
            // LLVM narrows a 64-bit architecture when sizeof(void*) == 4, but N32
            // and x32 are ILP32 ABIs on 64-bit ISAs, not MIPS32/i386 code. For
            // example mips64el-linux-gnuabin32 becomes mipsel-linux-gnuabin32,
            // which aborts when its N32 ABI requests registers the ISA lacks.
            // The bundled LLVM build's configured host/default triple retains
            // the ISA, endian, ABI environment and R6 subarchitecture together.
            // Reuse this choice for preoptimization, materialization AND live
            // unwind probes; a probe must not abort before a valid JIT runs.
            // Normalize three-component spellings too: LLVM's getter itself
            // does not normalize, and ABI environment parsing must be exact.
            return ::llvm::Triple{::llvm::Triple::normalize(::llvm::sys::getDefaultTargetTriple())};
        }

        [[nodiscard]] inline ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> select_runtime_llvm_jit_target(
            ::llvm::EngineBuilder& target_builder,
            ::uwvm2::utils::container::u8string const& host_cpu_name,
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& host_target_attribute_storage)
        {
            namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            // This explicit EngineBuilder overload owns strings, unlike setMAttrs.
            // Preserve every host feature (including VE -vpu) in the same order.
            ::llvm::SmallVector<::std::string, 16> attributes{};
            attributes.reserve(host_target_attribute_storage.size());
            for(auto const& attribute: host_target_attribute_storage)
            {
                auto const ref{emit::get_llvm_string_ref(attribute)};
                attributes.emplace_back(ref.data(), ref.size());
            }
            auto triple{get_runtime_llvm_jit_mcjit_target_triple()};
            // RuntimeDyld has MIPS far-call stubs, but they jump through $at
            // without establishing the PIC host callee's required $t9/GP.
            // Generate full-width C-ABI register calls instead. O32/N32 ignore
            // long-calls with ABICalls enabled, so explicitly use noabicalls
            // for MCJIT's static relocation model (N64 already does so).
            // Append both after host features: these are required JIT ABI
            // policies, not optional CPU capabilities. Also avoid assuming
            // separately allocated code shares a JAL 256-MiB address region.
            // Keep full materialization and the live unwind probe aligned.
            if(triple.isMIPS())
            {
                attributes.emplace_back("+noabicalls");
                attributes.emplace_back("+long-calls");
            }
            // A registered backend (or even a working object encoder) is not
            // necessarily a native JIT implementation. VE/SPARC and bytecode
            // targets can pass AOT probes without a supported MCJIT loader.
            // EngineBuilder::create only warns about hasJIT()==false; it does
            // not refuse construction, so that warning is not a safety gate.
            // Fail through the caller's normal materialization/probe handling
            // before LLVM aborts in an unsupported object/relocation path.
            // This gate is native-only: AOT inventories must remain unfiltered.
            ::std::string error{};
            // The empty-architecture overload preserves the explicit triple
            // and is also available before LLVM 23's two-argument API change.
            auto const target{::llvm::TargetRegistry::lookupTarget({}, triple, error)};
            // BPF advertises hasJIT(), but its bytecode is not CPU code for our
            // in-process C ABI; UWVM has no BPF loading/execution boundary.
            if(triple.isBPF() || target == nullptr || !target->hasJIT() || !target->hasMCAsmBackend() ||
               !::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_object_format_supported(triple)) { return {}; }
            auto machine{::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine>{
                target_builder.selectTarget(triple, {}, emit::get_llvm_string_ref(host_cpu_name), attributes)}};
            if(machine != nullptr && !::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_subtarget_supported(*machine)) { return {}; }
            return machine;
        }

        inline constexpr void apply_runtime_llvm_jit_native_target_function_attrs(::llvm::Module& module,
                                                                                  ::uwvm2::utils::container::u8string const& target_cpu_name,
                                                                                  ::uwvm2::utils::container::u8string const& tune_cpu_name,
                                                                                  ::llvm::TargetMachine const& target_machine) noexcept
        {
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

            auto const target_features{target_machine.getTargetFeatureString()};
            auto const target_cpu_ref{llvm_jit_translate_details::get_llvm_string_ref(target_cpu_name)};
            auto const tune_cpu_ref{llvm_jit_translate_details::get_llvm_string_ref(tune_cpu_name)};
            auto const target_features_ref{llvm_jit_translate_details::get_llvm_string_ref(target_features)};

            for(auto& function: module)
            {
                if(function.isDeclaration()) { continue; }
                if(!target_cpu_ref.empty()) { function.addFnAttr(llvm_jit_translate_details::get_llvm_string_ref(u8"target-cpu"), target_cpu_ref); }
                if(!tune_cpu_ref.empty()) { function.addFnAttr(llvm_jit_translate_details::get_llvm_string_ref(u8"tune-cpu"), tune_cpu_ref); }
                if(!target_features_ref.empty())
                {
                    function.addFnAttr(llvm_jit_translate_details::get_llvm_string_ref(u8"target-features"), target_features_ref);
                }
            }
        }

        inline constexpr void append_runtime_llvm_jit_native_target_codegen_policy(::uwvm2::utils::container::u8string& policy,
                                                                                   ::uwvm2::utils::container::u8string const& target_cpu_name,
                                                                                   ::uwvm2::utils::container::u8string const& tune_cpu_name) noexcept
        {
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(policy, u8"target-cpu", target_cpu_name);
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(policy, u8"tune-cpu", tune_cpu_name);
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

            return select_runtime_llvm_jit_target(target_builder, host_cpu_name, host_target_attribute_storage);
        }

        struct runtime_llvm_jit_legacy_light_task_preopt_context
        {
            // Shared context for pre-link optimization of per-task LLVM modules before they are linked into the full module.
            ::uwvm2::utils::container::u8string host_cpu_name{};
            ::uwvm2::utils::container::u8string host_tune_cpu_name{};
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

            set_llvm_module_target_triple_from_machine(*module_storage.llvm_module, *target_machine);
            module_storage.llvm_module->setDataLayout(target_machine->createDataLayout());
            apply_runtime_llvm_jit_native_target_function_attrs(*module_storage.llvm_module,
                                                                context->host_cpu_name,
                                                                context->host_tune_cpu_name,
                                                                *target_machine);
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

        [[nodiscard]] inline constexpr ::std::size_t runtime_llvm_jit_parallel_object_task_count(::std::size_t function_count,
                                                                                                 ::std::size_t extra_compile_threads) noexcept
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
            ::uwvm2::utils::container::u8string const& host_tune_cpu_name,
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

                set_llvm_module_target_triple_from_machine(*module, *target_machine);
                module->setDataLayout(target_machine->createDataLayout());
                apply_runtime_llvm_jit_native_target_function_attrs(*module, host_cpu_name, host_tune_cpu_name, *target_machine);
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
                                                   ::uwvm2::utils::container::u8string const& host_tune_cpu_name,
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
                                                                          host_tune_cpu_name,
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
            // Raw wrappers and their typed callees must be lowered consistently:
            // on i386 this includes the private no-x87 result ABI, not just rounding
            // instructions. Cache fingerprints/symbol rebinding cover cached objects.
            ::uwvm2::runtime::compiler::shared::strict_float_jit::lower(module);
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

            if(pipeline == runtime_llvm_jit_full_pipeline_kind::none)
            {
                ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::legalize_llvm_jit_native_vectors(module);
                return !verify_llvm_jit_ir || !::llvm::verifyModule(module);
            }

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
                // LLVM 23.1 changed OptimizationLevel from a class (with
                // getSpeedupLevel()) to an enum. We select only O0/O1/O2/O3,
                // so comparing the public levels preserves the previous >1
                // policy on both APIs without guessing an enum's encoding or
                // accidentally treating a size-optimization level as O2.
                auto const optimize_for_speed{pipeline_opt_level == ::llvm::OptimizationLevel::O2 ||
                                              pipeline_opt_level == ::llvm::OptimizationLevel::O3};
                pipeline_tuning_options.LoopUnrolling = optimize_for_speed;
                pipeline_tuning_options.LoopInterleaving = pipeline_tuning_options.LoopUnrolling;
                pipeline_tuning_options.LoopVectorization = optimize_for_speed;
                pipeline_tuning_options.SLPVectorization = optimize_for_speed;
                ::llvm::PassBuilder pass_builder{::std::addressof(target_machine), pipeline_tuning_options};
                details::register_runtime_llvm_jit_expanded_lane_unroll_policy(pass_builder);

                pass_builder.registerModuleAnalyses(module_analysis_manager);
                pass_builder.registerCGSCCAnalyses(cgscc_analysis_manager);
                pass_builder.registerFunctionAnalyses(function_analysis_manager);
                pass_builder.registerLoopAnalyses(loop_analysis_manager);
                pass_builder.crossRegisterProxies(loop_analysis_manager, function_analysis_manager, cgscc_analysis_manager, module_analysis_manager);

                auto module_pass_manager{pass_builder.buildPerModuleDefaultPipeline(pipeline_opt_level)};
                module_pass_manager.run(module, module_analysis_manager);
            }

            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::legalize_llvm_jit_native_vectors(module);
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
                                                                                    ::llvm::CodeGenOptLevel default_codegen_opt_level,
                                                                                    ::std::size_t extra_materialize_threads) noexcept
        {
            // Full-module LLVM materialization consumes emitted IR, optionally optimizes/splits it, creates an MCJIT engine, then
            // publishes typed/raw entry addresses into the runtime record.
            ::uwvm2::runtime::compiler::shared::strict_float_jit::register_symbols();
            if(extra_materialize_threads == ::std::numeric_limits<::std::size_t>::max())
            {
                extra_materialize_threads = ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compile_threads_resolved;
            }
            auto const llvm_jit_materialize_runtime_log_now{[]() constexpr noexcept
                                                            {
                                                                // Logging timestamps are optional; the materializer must remain usable without a monotonic
                                                                // clock.
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

            rec.llvm_jit_ready = false;
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
                rec.llvm_jit_ready = true;
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

            auto const host_cpu_name_storage{get_llvm_jit_host_cpu_name_storage()};
            auto const host_tune_cpu_name_storage{host_cpu_name_storage};
            auto const& host_cpu_name{host_cpu_name_storage};
            auto const& host_tune_cpu_name{host_tune_cpu_name_storage};
            namespace llvm_jit_translate_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
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
            target_builder.setEngineKind(::llvm::EngineKind::JIT)
                .setOptLevel(codegen_opt_level)
                .setMCPU(llvm_jit_translate_details::get_llvm_string_ref(host_cpu_name))
                .setMAttrs(host_target_attributes);

            auto target_machine{select_runtime_llvm_jit_target(target_builder, host_cpu_name, host_target_attribute_storage)};
            if(target_machine == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT target selection failed for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }
            if(codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target_machine->setFastISel(true); }

            set_llvm_module_target_triple_from_machine(*merged_module, *target_machine);
            merged_module->setDataLayout(target_machine->createDataLayout());
            apply_runtime_llvm_jit_native_target_function_attrs(*merged_module, host_cpu_name, host_tune_cpu_name, *target_machine);
            auto llvm_jit_cache_key{::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"full-module")};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module", rec.module_name);
            auto full_module_wasm_hash{runtime_llvm_jit_full_module_cache_fingerprint(*runtime_module)};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module-wasm-hash", full_module_wasm_hash);
            // The hand-built Wasm fingerprint is a cheap namespace hint, not a complete code identity: table, memory,
            // global, element/data and proposal metadata can change emitted IR without changing function-body bytes.
            // Hash the complete pre-optimization module after target attributes are installed. This also qualifies the
            // manual parallel-object key, whose cache lookup happens before the normal LLVM ObjectCache callback.
            auto full_module_ir_hash{::uwvm2::runtime::llvm_jit_cache::details::module_bitcode_hash(*merged_module)};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                llvm_jit_cache_key, u8"module-ir-hash", full_module_ir_hash);
            auto llvm_jit_cache_codegen_policy{::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"codegen-policy")};
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy, u8"cache-unit", u8"full");
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy,
                                                                              u8"pipeline",
                                                                              get_runtime_llvm_jit_full_pipeline_name(full_materialize_strategy.pipeline));
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy,
                                                                              u8"codegen-opt-level",
                                                                              get_runtime_llvm_jit_codegen_opt_level_name(codegen_opt_level));
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy, u8"policy", full_materialize_strategy.policy_name);
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy,
                                                                              u8"call-stack",
                                                                              get_runtime_llvm_jit_call_stack_mode_name());
            append_runtime_llvm_jit_native_target_codegen_policy(llvm_jit_cache_codegen_policy, host_cpu_name, host_tune_cpu_name);
            auto llvm_jit_cache_context{::uwvm2::runtime::llvm_jit_cache::default_cache_context(
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_key.data(), llvm_jit_cache_key.size()},
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_codegen_policy.data(), llvm_jit_cache_codegen_policy.size()},
                *target_machine)};
            // `module-ir-hash` above covers every IR input consumed by both the ordinary and partitioned emitters.
            llvm_jit_cache_context.cache_key_is_complete = true;
            auto llvm_jit_cache_policy{::uwvm2::runtime::llvm_jit_cache::default_cache_policy()};
# if defined(__i386__) || defined(_M_IX86) || (defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64))
            // Full-module objects on these MCJIT targets can contain process-local runtime storage or bridge addresses.
            // Reusing such an object across processes would turn those constants into stale pointers.
            llvm_jit_cache_policy.enable = false;
# endif

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

            if(use_parallel_objects) { llvm_jit_materialize_runtime_log_line(u8"optimize-skip module=\"", rec.module_name, u8"\" reason=object-cache-hit"); }
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
                                                      !runtime_llvm_jit_unwind_call_stack_requested() ? ::uwvm2::utils::container::u8string_view{u8"off"}
                                                      : get_runtime_llvm_jit_effective_call_stack_mode() ==
                                                            ::uwvm2::uwvm::runtime::runtime_mode::runtime_llvm_jit_call_stack_t::unwind_uncheck
                                                            ? ::uwvm2::utils::container::u8string_view{u8"unchecked"}
# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE
                                                      : ::uwvm2::utils::container::u8string_view{u8"live"},
# else
                                                      : ::uwvm2::utils::container::u8string_view{u8"static"},
# endif
                                                      u8" unwind_replace_frames=",
                                                      runtime_llvm_jit_unwind_can_replace_instruction_frames()
                                                          ? ::uwvm2::utils::container::u8string_view{u8"yes"}
                                                          : ::uwvm2::utils::container::u8string_view{u8"no"},
                                                      u8" call_stack_frames=",
                                                      runtime_llvm_jit_uses_instruction_call_stack_frames() ? u8"emit" : u8"omit");
                if(!optimize_runtime_llvm_jit_module(*merged_module,
                                                     *target_machine,
                                                     full_materialize_strategy.pipeline,
                                                     codegen_opt_level,
                                                     true,
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
                auto const object_emit_result{
                    emit_runtime_llvm_jit_objects_parallel(*merged_module,
                                                           host_cpu_name,
                                                           host_tune_cpu_name,
                                                           host_target_attribute_storage,
                                                           codegen_opt_level,
                                                           true,
                                                           extra_materialize_threads,
                                                           parallel_object_outputs,
                                                           parallel_object_defined_function_count)};
                if(object_emit_result == runtime_llvm_jit_parallel_object_emit_result::success)
                {
                    use_parallel_objects = true;
                    ::std::size_t object_bytes{};
                    for(auto const& object_output: parallel_object_outputs) { object_bytes += object_output.size(); }
                    store_runtime_llvm_jit_parallel_object_cache(llvm_jit_cache_context, llvm_jit_cache_policy, rec.module_name, parallel_object_outputs);
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

            auto memory_manager{
                ::uwvm2::utils::container::make_delete_owned<
                    ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()};
            auto const memory_manager_observer{memory_manager.get()};
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
                set_llvm_module_target_triple_from_machine(*engine_module, *target_machine);
                engine_module->setDataLayout(target_machine->createDataLayout());
                merged_module.reset();
                raw_engine = ::llvm::EngineBuilder(llvm_module_owner_t{engine_module.release()})
                                 .setEngineKind(::llvm::EngineKind::JIT)
                                 .setOptLevel(codegen_opt_level)
                                 .setMCPU(llvm_jit_translate_details::get_llvm_string_ref(host_cpu_name))
                                 .setMAttrs(host_target_attributes)
                                 .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{memory_manager.release()})
                                 .create(target_machine.release());
            }
            else
            {
                // Normal path: hand the optimized LLVM module directly to MCJIT.
                raw_engine = ::llvm::EngineBuilder(llvm_module_owner_t{merged_module.release()})
                                 .setEngineKind(::llvm::EngineKind::JIT)
                                 .setOptLevel(codegen_opt_level)
                                 .setMCPU(llvm_jit_translate_details::get_llvm_string_ref(host_cpu_name))
                                 .setMAttrs(host_target_attributes)
                                 .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{memory_manager.release()})
                                 .create(target_machine.release());
            }
            if(raw_engine == nullptr) [[unlikely]]
            {
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT engine creation failed for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }
            // From this point the ExecutionEngine owns the target machine and all executable allocations are tracked by rec.

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> llvm_jit_engine{raw_engine};
            ::uwvm2::runtime::llvm_jit_cache::llvm_jit_object_cache llvm_jit_object_cache{::std::move(llvm_jit_cache_context), llvm_jit_cache_policy};
            if(!use_parallel_objects) { llvm_jit_engine->setObjectCache(::std::addressof(llvm_jit_object_cache)); }
            if(runtime_llvm_jit_unwind_call_stack_requested())
            {
                llvm_jit_engine->RegisterJITEventListener(::std::addressof(get_uwvm_llvm_jit_code_range_listener()));
            }
            if(use_parallel_objects)
            {
                // Feed each externally emitted object back into the MCJIT engine so symbol lookup and code-range registration work normally.
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
            if(memory_manager_observer->has_finalization_failure()) [[unlikely]]
            {
                if(!use_parallel_objects) { llvm_jit_engine->setObjectCache(nullptr); }
                if(::uwvm2::uwvm::io::show_verbose) [[unlikely]]
                {
                    llvm_jit_materialize_error(u8"LLVM JIT memory finalization failed for module=\"", rec.module_name, u8"\".");
                }
                return false;
            }
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

                auto const raw_function_name{get_runtime_llvm_jit_wasm_raw_function_name(*runtime_module, function_index)};
                auto const raw_function_address{resolve_function_address(raw_function_name)};
                if(raw_function_address == 0u) [[unlikely]] { return false; }
                rec.llvm_jit_local_raw_entry_addresses.index_unchecked(local_index) = raw_function_address;

                auto const& runtime_func{runtime_module->local_defined_function_vec_storage.index_unchecked(local_index)};
                auto const typed_entry_supported{
                    runtime_func.function_type_ptr != nullptr &&
                    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::is_runtime_wasm_function_type_llvm_typed_entry_abi_supported(
                        *runtime_func.function_type_ptr)};
                ::std::uintptr_t function_address{};
                if(typed_entry_supported)
                {
                    auto const function_name{get_runtime_llvm_jit_wasm_function_name(*runtime_module, function_index)};
                    function_address = resolve_function_address(function_name);
                    if(function_address == 0u) [[unlikely]] { return false; }
                }
                rec.llvm_jit_local_entry_addresses.index_unchecked(local_index) = function_address;

                if(materialized_module_id != ::std::numeric_limits<::std::size_t>::max())
                {
                    if(function_address != 0u) { record_llvm_jit_unwind_entry(materialized_module_id, function_index, function_address, false); }
                    record_llvm_jit_unwind_entry(materialized_module_id, function_index, raw_function_address, true, typed_entry_supported);
                }
            }

            rec.llvm_jit_context_holder = ::std::move(llvm_context_holder);
            rec.llvm_jit_engine = ::std::move(llvm_jit_engine);
            rec.llvm_jit_ready = true;
            if(materialized_module_id != ::std::numeric_limits<::std::size_t>::max() && materialized_module_id < g_runtime.defined_func_cache.size())
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

#else
        [[nodiscard]] inline constexpr bool runtime_compiler_requests_llvm_jit_translation() noexcept { return false; }

#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // =========================================================================
        // Interpreter callback bridges
        // -------------------------------------------------------------------------
        // Translated opfunc streams call these callbacks for traps, direct calls, and
        // call_indirect. The callbacks are process-global optable entries, so they
        // must be initialized before any interpreter body executes.
        //
        // Coverage invariants:
        // - Trap callbacks terminate through the shared fatal-reporting path.
        // - Direct-call bridges accept either a compact call-info pointer or a module/function pair.
        // - call_indirect bridges perform table, null, and signature checks before dispatch.
        // - Inline caches are thread-local because table state and trap context are thread-sensitive.
        // =========================================================================

        // These callbacks are entered from uwvm-int opfuncs. Keep their ABI exactly synchronized with
        // UWVM_INTERPRETER_OPFUNC_TYPE_MACRO, otherwise Windows x86_64 SysV opfuncs will corrupt runtime callback calls.
        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void unreachable_trap() noexcept { trap_fatal(trap_kind::unreachable); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void trap_invalid_conversion_to_integer() noexcept
        { trap_fatal(trap_kind::invalid_conversion_to_integer); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void trap_integer_divide_by_zero() noexcept
        { trap_fatal(trap_kind::integer_divide_by_zero); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void trap_integer_overflow() noexcept { trap_fatal(trap_kind::integer_overflow); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void trap_table_out_of_bounds() noexcept
        { trap_fatal(trap_kind::table_out_of_bounds); }

        UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr void
            trap_memory_out_of_bounds(::uwvm2::object::memory::error::memory_error_t const& memerr) noexcept
        { print_memory_out_of_bounds_trap(memerr); }

        template <typename... Args>
        UWVM_ALWAYS_INLINE inline constexpr void execute_defined_for_bridge(Args&&... args) noexcept
        {
            execute_interpreter_defined(static_cast<Args&&>(args)...);
        }

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
                execute_defined_for_bridge(call_stack, *info, stack_top_ptr);
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
                        execute_defined_for_bridge(call_stack,
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
                        invoke_local_imported(tgt.u.local_imported,
                                              tgt.llvm_wasm_fp_control_policy,
                                              tgt.param_bytes,
                                              tgt.result_bytes,
                                              stack_top_ptr);
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
            execute_defined_for_bridge(call_stack, info, stack_top_ptr);
        }

        [[nodiscard]] UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr ::std::byte*
            call_bridge(::std::size_t wasm_module_id, ::std::size_t func_index, ::std::byte* stack_top) UWVM_THROWS
        {
            // The callback ABI returns the updated top in a register.  Only this non-tail bridge takes the address of its
            // parameter while adapting to the runtime implementation's synchronous pointer-to-pointer interface.
            call_bridge_impl(wasm_module_id, func_index, ::std::addressof(stack_top));
            return stack_top;
        }

        inline constexpr void call_indirect_bridge_impl(::std::size_t wasm_module_id,
                                                        ::std::size_t type_index,
                                                        ::std::size_t table_index,
                                                        ::std::byte** stack_top_ptr) UWVM_THROWS
        {
            // call_indirect must enforce wasm table bounds, null-element, and signature rules before dispatch.
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
            if(!g_runtime.compiled_all.load(::std::memory_order_acquire)) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
# endif
            auto const runtime_generation{current_runtime_generation()};
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
                if(details::call_indirect_cache_generation_matches(ic.runtime_generation, runtime_generation) && ic.table == table &&
                   ic.elems_data == elems_data && ic.selector == selector_u32 &&
                   ic.expected_ft_ptr == expected_ft_ptr &&
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
                            execute_defined_for_bridge(call_stack, info, stack_top_ptr);
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
                                    execute_defined_for_bridge(call_stack,
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
                                    invoke_local_imported(tgt.u.local_imported,
                                                          tgt.llvm_wasm_fp_control_policy,
                                                          tgt.param_bytes,
                                                          tgt.result_bytes,
                                                          stack_top_ptr);
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

                    auto const info_ptr{find_defined_func_info(def_ptr)};
                    if(info_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& info{*info_ptr};

                    auto const actual_ft_ptr{info.runtime_func->function_type_ptr};
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
                                trap_fatal(trap_kind::call_indirect_type_mismatch);
                            }
                        }
                    }

                    // Update inline cache.
                    {
                        auto& ic{call_stack.call_indirect_cache[cache_index]};
                        ic.runtime_generation = 0u;
                        ic.table = table;
                        ic.elems_data = table->elems.data();
                        ic.selector = selector_u32;
                        ic.expected_ft_ptr = expected_ft_ptr;
                        ic.elem_type = elem.type;
                        ic.target_ptr = static_cast<void const*>(def_ptr);
                        ic.defined_info = info.compiled_call_info;
                        ic.imported_tgt = nullptr;
                        ic.runtime_generation = runtime_generation;
                    }

                    if(try_execute_trivial_defined_call(info, stack_top_ptr)) { return; }

                    call_stack_guard g{call_stack, info.module_id, info.function_index};
                    auto const rf{static_cast<runtime_local_func_storage_t const*>(info.runtime_func)};
                    if(rf == nullptr || info.compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    execute_defined_for_bridge(call_stack, info, stack_top_ptr);
                    return;
                }
                case ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t::func_ref_imported:
                {
                    auto const imp_ptr{elem.storage.imported_ptr};
                    if(imp_ptr == nullptr) [[unlikely]] { trap_fatal(trap_kind::call_indirect_null_element); }

                    auto const tgt_ptr{find_cached_import_target(imp_ptr)};
                    if(tgt_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                    auto const& tgt{*tgt_ptr};

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
                                trap_fatal(trap_kind::call_indirect_type_mismatch);
                            }
                        }
                    }

                    // Update inline cache.
                    {
                        auto& ic{call_stack.call_indirect_cache[cache_index]};
                        ic.runtime_generation = 0u;
                        ic.table = table;
                        ic.elems_data = table->elems.data();
                        ic.selector = selector_u32;
                        ic.expected_ft_ptr = expected_ft_ptr;
                        ic.elem_type = elem.type;
                        ic.target_ptr = static_cast<void const*>(imp_ptr);
                        ic.defined_info = nullptr;
                        ic.imported_tgt = ::std::addressof(tgt);
                        ic.runtime_generation = runtime_generation;
                    }

                    call_stack_guard g{call_stack, tgt.frame.module_id, tgt.frame.function_index};
                    switch(tgt.k)
                    {
                        case cached_import_target::kind::defined:
                        {
                            execute_defined_for_bridge(call_stack,
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
                            invoke_local_imported(tgt.u.local_imported,
                                                  tgt.llvm_wasm_fp_control_policy,
                                                  tgt.param_bytes,
                                                  tgt.result_bytes,
                                                  stack_top_ptr);
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

        [[nodiscard]] UWVM2_RUNTIME_INTERPRETER_CALLBACK_FUNC_ATTR inline constexpr ::std::byte* call_indirect_bridge(
            ::std::size_t wasm_module_id, ::std::size_t type_index, ::std::size_t table_index, ::std::byte* stack_top) UWVM_THROWS
        {
            call_indirect_bridge_impl(wasm_module_id, type_index, table_index, ::std::addressof(stack_top));
            return stack_top;
        }

        inline constexpr void configure_interpreter_call_bridges_for_current_runtime() noexcept
        {
            ::uwvm2::runtime::compiler::uwvm_int::optable::call_func = call_bridge;
            ::uwvm2::runtime::compiler::uwvm_int::optable::call_indirect_func = call_indirect_bridge;
        }

        inline constexpr void ensure_bridges_initialized() noexcept
        {
            // Trap and call callbacks are process-global optable entries. Initialize them once with release/acquire publication.
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
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_table_out_of_bounds_func = trap_table_out_of_bounds;
                ::uwvm2::runtime::compiler::uwvm_int::optable::trap_memory_out_of_bounds_func = trap_memory_out_of_bounds;

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
            // - The publication lock protects compilation and reset of all global registries as one unit.
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
            auto const compile_llvm_jit_translation{runtime_compiler_requests_llvm_jit_translation()};
            auto const compile_uwvm_int_translation{runtime_compiler_requests_uwvm_int_translation()};

            // Compilation and reset share one publication lock. A live registry may be reused only by the exact configuration
            // that built it; policy/backend changes require the documented quiescent reset instead of silently retaining old code.
            runtime_state_publication_guard runtime_state_guard{};
            auto const requested_runtime_state{make_runtime_state_signature()};
            auto const eager_ready{g_runtime.compiled_all.load(::std::memory_order_relaxed)};
            require_runtime_state_transition(requested_runtime_state, eager_ready);
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
            // Check the publication signature before touching backend-global bridge slots. A compatible eager state may still need
            // bridges initialized when its first host-side build requested metadata only.
            if(initialize_interpreter_bridges && compile_uwvm_int_translation) { ensure_bridges_initialized(); }
# endif
            if(eager_ready) { return; }

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
                resolve_runtime_target_task_groups_per_adjusted_compile_thread(aggressive_runtime_compile_threads_policy_active, runtime_compiler)};
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
                                                                           [[unlikely]] default:
                                                                           {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                                                                               ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                                                                               ::std::unreachable();
                                                                           }
                                                                       }
                                                                   }()};

            runtime_compile_task_split_config_t const compile_task_split_conf{
                .policy = runtime_scheduling_policy == runtime_scheduling_policy_t::function_count
                              ? runtime_compile_task_split_policy_t::function_count
                              : runtime_compile_task_split_policy_t::code_size,
                .split_size = runtime_scheduling_size,
                .adjust_for_default_policy = allow_default_runtime_scheduling_adjustment};

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
# if defined(UWVM_RUNTIME_LLVM_JIT)
            clear_llvm_jit_call_indirect_table_views();
# endif
            g_runtime.modules.clear();
            g_runtime.module_name_to_id.clear();
            g_runtime.defined_func_cache.clear();
            g_runtime.defined_func_ptr_ranges.clear();
# if defined(UWVM_RUNTIME_LLVM_JIT)
            g_runtime.llvm_jit_unwind_entries.clear();
            g_runtime.llvm_jit_code_ranges.clear();
# endif
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
                        u8"LLVM JIT full IR translation will be generated without uwvm-int full artifacts. ");
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
# endif
                // Resolve split sizing through the selected backend. A combined binary must make the same scheduling decision as the
                // corresponding pure-backend binary rather than selecting an adapter at preprocessing time.
                auto const thread_resolution_compile_task_split_conf{resolve_effective_runtime_compile_task_split_config(
                    *rec.runtime_module, compile_task_split_conf, effective_extra_compile_threads, runtime_compiler)};
                auto const runtime_scheduling_policy_adjusted_for_thread_resolution{
                    !runtime_scheduling_policy_existed && compile_task_split_conf.policy == runtime_compile_task_split_policy_t::code_size &&
                    thread_resolution_compile_task_split_conf.split_size != runtime_scheduling_size};
                // Thread-resolution can shrink default code-size batches before adaptive per-module worker reduction is calculated.
                auto effective_module_extra_compile_threads{effective_extra_compile_threads};
                if(adaptive_runtime_compile_threads_policy_active)
                {
                    effective_module_extra_compile_threads = resolve_effective_runtime_adaptive_extra_compile_threads(
                        *rec.runtime_module,
                        thread_resolution_compile_task_split_conf,
                        effective_extra_compile_threads,
                        adaptive_target_task_groups_per_adjusted_compile_thread,
                        runtime_scheduling_policy_adjusted_for_thread_resolution,
                        runtime_compiler);
                }
                auto const effective_compile_task_split_conf{resolve_effective_runtime_compile_task_split_config(
                    *rec.runtime_module, compile_task_split_conf, effective_module_extra_compile_threads, runtime_compiler)};
                auto const default_runtime_scheduling_policy_adjusted{!runtime_scheduling_policy_existed &&
                                                                      compile_task_split_conf.policy == runtime_compile_task_split_policy_t::code_size &&
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

                    if(effective_compile_task_split_conf.policy == runtime_compile_task_split_policy_t::function_count)
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
                        auto const wasm_feature_parameter{find_validator_feature_parameter_storage(rec.module_name)};
                        if(wasm_feature_parameter == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        rec.compiled = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_all_from_uwvm<kTranslateOpt>(
                            *rec.runtime_module,
                            opt,
                            err,
                            effective_module_extra_compile_threads,
                            make_uwvm_int_compile_task_split_config(effective_compile_task_split_conf),
                            wasm_feature_parameter);

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
                        // Full LLVM mode emits IR for all local functions first, then materializes as a module. Failure in
                        // either phase aborts AOT compilation instead of changing execution backends.
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
                        // The reduced runtime never accepts unchecked generated IR.
                        llvm_jit_opt.verify_llvm_jit_ir = true;
                        auto const llvm_jit_wasm_feature_parameter{find_validator_feature_parameter_storage(rec.module_name)};
                        if(llvm_jit_wasm_feature_parameter == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                        llvm_jit_opt.validator_feature_parameter = llvm_jit_wasm_feature_parameter;
                        configure_runtime_llvm_jit_call_stack_policy(llvm_jit_opt);
                        runtime_llvm_jit_legacy_light_task_preopt_context legacy_light_task_preopt_context{};
                        bool legacy_light_task_preopt_enabled{};
                        if(effective_module_extra_compile_threads != 0uz)
                        {
                            auto const full_translation_strategy{resolve_runtime_llvm_jit_full_materialize_strategy(::llvm::CodeGenOptLevel::Aggressive)};
                            if(full_translation_strategy.pipeline == runtime_llvm_jit_full_pipeline_kind::legacy_light &&
                               ensure_llvm_jit_native_target_initialized())
                            {
                                legacy_light_task_preopt_context.host_cpu_name = get_llvm_jit_host_cpu_name_storage();
                                legacy_light_task_preopt_context.host_tune_cpu_name = legacy_light_task_preopt_context.host_cpu_name;
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
                            make_llvm_jit_compile_task_split_config(effective_compile_task_split_conf));
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
                    if(!try_materialize_runtime_module_llvm_jit(rec, ::llvm::CodeGenOptLevel::Aggressive, effective_module_extra_compile_threads))
                        [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"LLVM AOT materialization failed for module=\"",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                            rec.module_name,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"\".\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
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

                // Canonical type ids and defined-function metadata are backend-neutral runtime indexes used by calls, host APIs, and traps.
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
                            if(tgt.u.local_imported.module_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                            tgt.llvm_wasm_fp_control_policy =
                                query_local_imported_function_wasm_fp_control_policy(tgt.u.local_imported.module_ptr, tgt.u.local_imported.index);
                            tgt.local_imported_signature = func_sig_from_local_imported(tgt.u.local_imported.module_ptr, tgt.u.local_imported.index);
                            auto const tgt_sig{tgt.signature()};
                            tgt.param_bytes = total_abi_bytes(tgt_sig.params);
                            tgt.result_bytes = total_abi_bytes(tgt_sig.results);
                            if((tgt.param_bytes == 0uz && tgt_sig.params.size != 0uz) || (tgt.result_bytes == 0uz && tgt_sig.results.size != 0uz)) [[unlikely]]
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

                    cache.index_unchecked(i) = ::std::move(tgt);
                }
            }

# if defined(UWVM_RUNTIME_LLVM_JIT)
            if(compile_llvm_jit_translation) { populate_llvm_jit_call_indirect_table_views(); }
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

            g_runtime.published_runtime_state = requested_runtime_state;
            g_runtime.compiled_all.store(true, ::std::memory_order_release);
#endif
        }



    }  // namespace

#if defined(UWVM_RUNTIME_LLVM_JIT)
    namespace details
    {
        // Local-imported metadata, global, and memory operations are extensible native-provider calls. LLVM compilation
        // and generated Wasm both use these entry points; the common host-callback scope suspends any live generated-
        // bridge depth token for the complete virtual call and restores Wasm FP controls (globals use the light guard).
        // Combined LLVM/interpreter builds share this boundary. Pure-interpreter optables use a small guarded native
        // boundary with header-only linkage instead.
        template <typename Callable>
        inline void invoke_host_preserving_wasm_fp_control(Callable&& callable) noexcept
        {
            llvm_jit_generated_bridge_scope_suspend_guard generated_bridge_scope_suspend{
                get_llvm_jit_generated_bridge_scope_depth()};
            // Header-driven runners have no production-entry marker; always guard native globals without a TLS lookup.
            scoped_wasm_host_fp_control_restore fp_control_guard{};
            if(!fp_control_guard.ready()) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::std::forward<Callable>(callable)();
        }

        extern "C++" void invoke_local_imported_provider_global_get(void* opaque_module,
                                                                     ::std::size_t global_index,
                                                                     ::std::byte* out) noexcept
        {
            auto module{static_cast<local_imported_t*>(opaque_module)};
            if(module == nullptr || out == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            invoke_host_preserving_wasm_fp_control([&]() noexcept { module->global_get_from_index(global_index, out); });
        }

        extern "C++" bool invoke_local_imported_provider_global_set(void* opaque_module,
                                                                     ::std::size_t global_index,
                                                                     ::std::byte const* in) noexcept
        {
            auto module{static_cast<local_imported_t*>(opaque_module)};
            if(module == nullptr || in == nullptr) [[unlikely]] { return false; }
            bool result{};
            invoke_host_preserving_wasm_fp_control([&]() noexcept { result = module->global_set_from_index(global_index, in); });
            return result;
        }

        extern "C++" bool invoke_local_imported_provider_function_signature(
            void const* opaque_module,
            ::std::size_t function_index,
            local_imported_provider_function_signature_t& out) noexcept
        {
            auto module{static_cast<local_imported_t const*>(opaque_module)};
            if(module == nullptr) [[unlikely]] { return false; }

            runtime_compilation_metadata_callback_scope metadata_callback_scope{};
            bool result{};
            invoke_host_preserving_llvm_wasm_fp_environment(
                [&]() noexcept
                {
                    auto const info{module->get_function_information_from_index(function_index)};
                    if(!info.successed) [[unlikely]] { return; }

                    auto const& function_type{info.function_type};
                    auto const copy_range{
                        [](auto const& values, ::std::vector<::std::uint_least8_t>& types_out) noexcept
                        {
                            if(values.begin == nullptr || values.end == nullptr)
                            {
                                if(values.begin != values.end) [[unlikely]] { return false; }
                                types_out.clear();
                                return true;
                            }

                            // A provider is outside the runtime's allocation domain. Relational comparison or subtraction
                            // of unrelated pointers is undefined in C++; validate the representation as integer addresses
                            // first, including element alignment, and copy each value while the callback remains guarded.
                            using element_type = ::std::remove_cv_t<::std::remove_pointer_t<decltype(values.begin)>>;
                            auto const begin_address{reinterpret_cast<::std::uintptr_t>(values.begin)};
                            auto const end_address{reinterpret_cast<::std::uintptr_t>(values.end)};
                            if(end_address < begin_address) [[unlikely]] { return false; }
                            auto const byte_count{end_address - begin_address};
                            if(begin_address % alignof(element_type) != 0uz || byte_count % sizeof(element_type) != 0uz) [[unlikely]] { return false; }
                            auto const element_count{byte_count / sizeof(element_type)};
                            if constexpr(sizeof(::std::uintptr_t) > sizeof(::std::size_t))
                            {
                                if(element_count > static_cast<::std::uintptr_t>((::std::numeric_limits<::std::size_t>::max)())) [[unlikely]] { return false; }
                            }
                            auto const count{static_cast<::std::size_t>(element_count)};
                            types_out.resize(count);
                            for(::std::size_t i{}; i != count; ++i)
                            {
                                types_out[i] = static_cast<::std::uint_least8_t>(values.begin[i]);
                            }
                            return true;
                        }};

                    local_imported_provider_function_signature_t snapshot{};
                    if(!copy_range(function_type.parameter, snapshot.parameter_types) ||
                       !copy_range(function_type.result, snapshot.result_types)) [[unlikely]]
                    {
                        return;
                    }

                    // Commit both vectors together only after the complete provider result has validated. Runtime caches
                    // may keep this owned snapshot without depending on provider object or callback-local lifetimes.
                    out = ::std::move(snapshot);
                    result = true;
                });
            return result;
        }

        extern "C++" ::std::uint_least8_t
            invoke_local_imported_provider_function_wasm_fp_control_policy(void const* opaque_module,
                                                                           ::std::size_t function_index) noexcept
        {
            auto module{static_cast<local_imported_t const*>(opaque_module)};
            using policy_type = ::uwvm2::uwvm::wasm::type::local_imported_wasm_fp_control_policy_t;
            if(module == nullptr) [[unlikely]] { return static_cast<::std::uint_least8_t>(policy_type::may_modify); }
            runtime_compilation_metadata_callback_scope metadata_callback_scope{};
            policy_type result{policy_type::may_modify};
            invoke_host_preserving_llvm_wasm_fp_environment(
                [&]() noexcept { result = module->function_wasm_fp_control_policy_from_index(function_index); });
            return static_cast<::std::uint_least8_t>(result);
        }

        extern "C++" ::std::uint_least64_t
            invoke_local_imported_provider_memory_page_size(void const* opaque_module, ::std::size_t memory_index) noexcept
        {
            auto module{static_cast<local_imported_t const*>(opaque_module)};
            if(module == nullptr) [[unlikely]] { return 0u; }
            ::std::uint_least64_t result{};
            invoke_host_preserving_llvm_wasm_fp_environment([&]() noexcept { result = module->memory_page_size_from_index(memory_index); });
            return result;
        }

        extern "C++" ::std::uint_least64_t
            invoke_local_imported_provider_memory_page_size_for_compilation(void const* opaque_module,
                                                                            ::std::size_t memory_index) noexcept
        {
            // Unlike generated memory.size/load/store helpers, lazy single-CU translation may query this provider
            // without publication-lock ownership. Mark only that metadata path so ordinary execution callbacks keep
            // their supported public-raw re-entry surface.
            runtime_compilation_metadata_callback_scope metadata_callback_scope{};
            return invoke_local_imported_provider_memory_page_size(opaque_module, memory_index);
        }

        extern "C++" bool invoke_local_imported_provider_memory_access_snapshot(
            void* opaque_module,
            ::std::size_t memory_index,
            local_imported_provider_memory_snapshot_t& out) noexcept
        {
            auto module{static_cast<local_imported_t*>(opaque_module)};
            if(module == nullptr) [[unlikely]] { return false; }
            ::uwvm2::uwvm::wasm::type::memory_access_snapshot_result_t provider_snapshot{};
            bool result{};
            invoke_host_preserving_llvm_wasm_fp_environment(
                [&]() noexcept { result = module->memory_access_snapshot_from_index(memory_index, provider_snapshot); });
            if(result)
            {
                out.memory_begin = provider_snapshot.memory_begin;
                out.page_count = provider_snapshot.page_count;
            }
            return result;
        }

        extern "C++" bool invoke_local_imported_provider_memory_read(void* opaque_module,
                                                                      ::std::size_t memory_index,
                                                                      ::std::uint_least64_t offset,
                                                                      void* destination,
                                                                      ::std::size_t size) noexcept
        {
            auto module{static_cast<local_imported_t*>(opaque_module)};
            if(module == nullptr) [[unlikely]] { return false; }
            bool result{};
            invoke_host_preserving_llvm_wasm_fp_environment(
                [&]() noexcept { result = module->memory_read_from_index(memory_index, offset, destination, size); });
            return result;
        }

        extern "C++" bool invoke_local_imported_provider_memory_write(void* opaque_module,
                                                                       ::std::size_t memory_index,
                                                                       ::std::uint_least64_t offset,
                                                                       void const* source,
                                                                       ::std::size_t size) noexcept
        {
            auto module{static_cast<local_imported_t*>(opaque_module)};
            if(module == nullptr) [[unlikely]] { return false; }
            bool result{};
            invoke_host_preserving_llvm_wasm_fp_environment(
                [&]() noexcept { result = module->memory_write_to_index(memory_index, offset, source, size); });
            return result;
        }

        extern "C++" bool invoke_local_imported_provider_memory_try_grow(void* opaque_module,
                                                                          ::std::size_t memory_index,
                                                                          ::std::uint_least64_t delta_pages,
                                                                          ::std::size_t max_limit_memory_length,
                                                                          ::std::uint_least64_t* old_page_size_out) noexcept
        {
            auto module{static_cast<local_imported_t*>(opaque_module)};
            if(module == nullptr || old_page_size_out == nullptr) [[unlikely]] { return false; }
            bool result{};
            invoke_host_preserving_llvm_wasm_fp_environment([&]() noexcept
                                                            {
                                                                result = module->memory_try_grow_from_index(
                                                                    memory_index, delta_pages, max_limit_memory_length, old_page_size_out);
                                                            });
            return result;
        }
    }  // namespace details

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
    extern "C++" void llvm_jit_refresh_call_indirect_table_views(
        runtime_table_storage_t* mutated_table,
        ::uwvm2::uwvm::runtime::storage::llvm_jit_call_indirect_table_mutation_kind kind,
        ::std::size_t begin,
        ::std::size_t count) noexcept
    {
        // The bridge receives the already-resolved destination table, so imported aliases in every caller are updated while
        // unrelated tables retain both their target allocation and contents.
        update_llvm_jit_call_indirect_table_views(mutated_table, kind, begin, count);
    }

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
        store_llvm_jit_trap_context(k, return_address);
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        store_llvm_jit_win64_trap_caller_context(return_address, explicit_frame_address, explicit_stack_pointer);
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
            case llvm_jit_trap_kind::table_out_of_bounds:
            {
                trap_fatal(trap_kind::table_out_of_bounds);
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
        store_llvm_jit_trap_context(llvm_jit_trap_kind::memory_out_of_bounds, return_address);
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        store_llvm_jit_win64_trap_caller_context(return_address, explicit_frame_address, explicit_stack_pointer);
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


    // =========================================================================
    // Full-compile host entry point
    // -------------------------------------------------------------------------
    // The host calls this when all runtime artifacts should be ready before entry
    // execution. LLVM AOT execution requires a materialized raw native entry.
    //
    // Coverage invariants:
    // - Eager caches are built before entry lookup.
    // - Imported wasm entries are redirected to their provider module/function.
    // - LLVM AOT treats native entry unavailability as fatal.
    // - Independent interpreter execution uses the same ABI stack staging as internal calls.
    // =========================================================================
    extern "C++" void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config cfg) noexcept
    {
        // Declare the entry scope first so every FP/bridge guard is destroyed before outermost cleanup. Recursive
        // full execution is unsupported and fails before it can disturb the active entry's per-thread state.
        runtime_execution_entry_scope execution_entry_scope{};
        ::uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment fp_environment_guard{
            get_llvm_wasm_fp_environment_active_marker()};
        if(!fp_environment_guard.ready()) [[unlikely]] { ::fast_io::fast_terminate(); }

#if defined(UWVM_RUNTIME_LLVM_JIT)
        auto native_unwind_execution_guard{
            g_runtime.llvm_jit_native_unwind_execution_gate.enter_if(runtime_llvm_jit_unwind_call_stack_requested())};
#endif

        // Full execution forces the selected backend artifacts to be ready before selecting the entry.
        compile_all_modules_if_needed();

#if defined(UWVM_RUNTIME_LLVM_JIT)
        // Publish the generated-only import bridge capability only after compilation and all runtime caches are ready.
        llvm_jit_generated_bridge_scope_guard generated_bridge_scope{
            get_llvm_jit_generated_bridge_scope_depth(), runtime_compiler_requests_llvm_jit_translation()};
#endif

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
                if(cfg.module_start && tgt.param_bytes == 0uz && tgt.result_bytes == 0uz &&
                   tgt.sig.params.size == 0uz && tgt.sig.results.size == 0uz &&
                   cfg.entry_abi_buffers.param_bytes == 0uz && cfg.entry_abi_buffers.result_bytes == 0uz)
                {
#if defined(UWVM_RUNTIME_LLVM_JIT)
                    if(runtime_compiler_requests_llvm_jit_translation())
                    {
                        llvm_jit_raw_call_cached_import_entry(reinterpret_cast<::std::uintptr_t>(::std::addressof(tgt)),
                                                               0u, 0uz, 0u, 0uz);
                        return;
                    }
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
                    ::std::byte empty_stack{};
                    (void)call_bridge(main_id, cfg.entry_function_index, ::std::addressof(empty_stack));
                    return;
#endif
                }

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
            // AOT execution is native-only: failure to resolve the materialized raw entry is fatal.
            ::uwvm2::uwvm::global::record_total_wasm_time_start();
            if(try_invoke_runtime_llvm_jit_raw_defined_entry(llvm_jit_entry_module_id,
                                                             llvm_jit_entry_function_index,
                                                             cfg.entry_abi_buffers.result_buffer,
                                                             result_bytes,
                                                             cfg.entry_abi_buffers.param_buffer,
                                                             param_bytes))
            {
                ::uwvm2::uwvm::global::record_total_wasm_time_end();
                return;
            }
            ::uwvm2::uwvm::global::discard_total_wasm_time_record();

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
#endif

#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
        // The independently selected interpreter uses the same stack-buffer staging as internal call bridges.
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
            stack_top_ptr = call_bridge(main_id, cfg.entry_function_index, stack_top_ptr);
            ::uwvm2::uwvm::global::record_total_wasm_time_end();
        }
# ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error)
        {
            trap_fatal(trap_kind::uncatched_int_tag);
        }
# endif
        if(result_bytes != 0uz) { ::std::memcpy(cfg.entry_abi_buffers.result_buffer, host_stack_base, result_bytes); }

#endif
    }

    // =========================================================================
    // Host-facing runtime maintenance APIs
    // -------------------------------------------------------------------------
    // These functions are called by embedding code rather than by wasm execution.
    // They reset runtime registries, bridge raw host calls into the active backend,
    // and expose preload memory descriptors/copy operations.
    //
    // Coverage invariants:
    // - Raw host calls validate the runtime module pointer against current registries.
    // - Import calls preserve WASI/preload context through cached target metadata.
    // - Preload memory APIs are thin wrappers over the active call context.
    // =========================================================================
    extern "C++" void runtime_stop_before_proc_exit_host_api() noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT)
        // ROS has no lazy/tiered compiler, but its MCJIT object cache still owns one asynchronous writer.
        ::uwvm2::runtime::llvm_jit_cache::shutdown_async_store_objects();
#endif
    }

    extern "C++" void reset_runtime_state_host_api() noexcept
    {
        // A compilation-metadata provider callback can run outside the publication lock (notably lazy single-CU
        // materialization). Reset there would destroy state owned by the active compiler, so reject it before taking
        // any gate or consulting backend readiness.
        if(!::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_access_allowed(
               get_runtime_compilation_metadata_callback_depth())) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        // Provider callbacks may run while this thread owns the non-recursive publication lock. A reset from
        // such a callback would otherwise spin forever before reaching any registry validation.
        if(!::uwvm2::runtime::lib::details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth())) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        // Reset destroys both interpreter and LLVM per-thread state. It is never legal from any active execution
        // entry, including a host callback whose generated-bridge token is temporarily suspended.
        if(!::uwvm2::runtime::lib::details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth())) [[unlikely]]
        { ::fast_io::fast_terminate(); }
#if defined(UWVM_RUNTIME_LLVM_JIT)
        // Reset may erase this thread's TLS/map-backed state. Reject same-thread callback reset before a suspended
        // generated-bridge guard can retain a pointer into that state and later restore a stale capability.
        if(::uwvm2::runtime::lib::details::is_llvm_wasm_fp_environment_active(get_llvm_wasm_fp_environment_active_marker()) ||
           get_llvm_jit_generated_bridge_scope_depth() != 0uz) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        // The selected backend/call-stack policy may already have changed. Always take the gate so a previously published
        // unwind-backed execution cannot overlap destruction of the native address maps.
        auto native_unwind_execution_guard{g_runtime.llvm_jit_native_unwind_execution_gate.enter_if(true)};
#endif
        {
            runtime_state_publication_guard runtime_state_guard{};
            // Reset is intended for embedding hosts that reload runtime storage in the same process. ROS has no background compiler to
            // join, so its scheduler phase is empty, but the caller must still quiesce execution and host API calls. Advance the generation
            // before dropping registries so surviving caller TLS caches cannot match addresses reused by the next module set.
            advance_runtime_generation();

#if defined(UWVM_RUNTIME_LLVM_JIT)
            clear_llvm_jit_call_indirect_table_views();
#endif
            // Drop every registry that borrows module-owned storage before destroying the module records and their LLVM engines.
            g_import_call_cache.clear();
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            g_wasip1_runtime_module_context_cache.clear();
            details::clear_wasip1_memory_bindings(::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env,
                                                  ::uwvm2::uwvm::imported::wasi::wasip1::storage::configured_wasip1_groups);
            default_wasip1_memory_runtime_module_id = invalid_default_wasip1_memory_runtime_module_id;
#endif
            g_runtime.defined_func_cache.clear();
            g_runtime.defined_func_ptr_ranges.clear();
#if defined(UWVM_RUNTIME_LLVM_JIT)
            g_runtime.llvm_jit_unwind_entries.clear();
            g_runtime.llvm_jit_code_ranges.clear();
#endif
            g_runtime.modules.clear();
            g_runtime.module_name_to_id.clear();

            g_runtime.compiled_all.store(false, ::std::memory_order_release);
            g_runtime.bridges_initialized.store(false, ::std::memory_order_release);
            g_runtime.published_runtime_state = {};
        }

        // Release current-thread publication ownership before erasing its TLS/map-backed node. The guard's destructor
        // re-acquires the depth field, so letting it cross this erase would recreate a node or observe invalid storage.
        erase_current_thread_runtime_state();
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_USE_THREAD_LOCAL)
        llvm_jit_trap_return_address = 0u;
        llvm_jit_last_trap_kind = {};
# if UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE
        llvm_jit_win64_trap_caller_context = {};
        llvm_jit_win64_trap_caller_context_valid = false;
# endif
#endif
    }

#if defined(UWVM_RUNTIME_LLVM_JIT)
    extern "C++" void llvm_jit_reset_runtime_state_host_api() noexcept { reset_runtime_state_host_api(); }

    extern "C++" void details::llvm_jit_call_raw_from_generated_wasm(void const* runtime_module_ptr,
                                                                     ::std::uint_least32_t func_index,
                                                                     void* result_buffer,
                                                                     ::std::size_t result_bytes,
                                                                     void const* param_buffer,
                                                                     ::std::size_t param_bytes) noexcept
    {
        // This address is embedded only into generated Wasm. Reusing the outer execution scope avoids a full fenv
        // save/default/restore cycle and recursive gate lock for every import. A direct host call to this internal bridge
        // is rejected; the public API below establishes both invariants before forwarding here.
        auto& call_stack{get_call_stack()};
        if(!::uwvm2::runtime::lib::details::is_llvm_wasm_fp_environment_active(call_stack.llvm_wasm_fp_environment_active) ||
           call_stack.llvm_jit_generated_bridge_scope_depth == 0uz) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

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
            switch(tgt.k)
            {
                case cached_import_target::kind::defined:
                {
                    if(try_invoke_runtime_llvm_jit_raw_defined_entry(tgt.frame.module_id,
                                                                     tgt.frame.function_index,
                                                                     result_buffer,
                                                                     result_bytes,
                                                                     param_buffer,
                                                                     param_bytes))
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
                                                        tgt.llvm_wasm_fp_control_policy,
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

        if(try_invoke_runtime_llvm_jit_raw_defined_entry(wasm_module_id,
                                                         func_index,
                                                         result_buffer,
                                                         result_bytes,
                                                         param_buffer,
                                                         param_bytes))
        {
            return;
        }

        ::fast_io::fast_terminate();
    }

    extern "C++" void details::llvm_jit_call_raw_from_generated_wasm_abi_bridge(::std::uintptr_t runtime_module_address,
                                                                                 ::std::uintptr_t func_index,
                                                                                 ::std::uintptr_t result_buffer_address,
                                                                                 ::std::size_t result_bytes,
                                                                                 ::std::uintptr_t param_buffer_address,
                                                                                 ::std::size_t param_bytes) noexcept
    {
        // Generated LLVM IR declares an integer-only ABI. Convert to the runtime's typed interface in a real C++
        // wrapper instead of relying on pointer/integer register equivalence, which is not a C++ ABI guarantee.
        if constexpr(::std::numeric_limits<::std::uintptr_t>::digits > ::std::numeric_limits<::std::uint32_t>::digits)
        {
            if(func_index > static_cast<::std::uintptr_t>((::std::numeric_limits<::std::uint32_t>::max)())) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
        }
        details::llvm_jit_call_raw_from_generated_wasm(
            runtime_module_address == 0u ? nullptr : reinterpret_cast<void const*>(runtime_module_address),
            static_cast<::std::uint_least32_t>(func_index),
            result_buffer_address == 0u ? nullptr : reinterpret_cast<void*>(result_buffer_address),
            result_bytes,
            param_buffer_address == 0u ? nullptr : reinterpret_cast<void const*>(param_buffer_address),
            param_bytes);
    }

    extern "C++" void llvm_jit_call_raw_host_api(void const* runtime_module_ptr,
                                                 ::std::uint_least32_t func_index,
                                                 void* result_buffer,
                                                 ::std::size_t result_bytes,
                                                 void const* param_buffer,
                                                 ::std::size_t param_bytes) noexcept
    {
        // A metadata provider may be serving the very lazy compilation unit this raw call would wait for. This phase
        // can exist without publication-lock ownership, so reject it independently and before any readiness/lock path.
        if(!::uwvm2::runtime::lib::details::runtime_compilation_metadata_callback_access_allowed(
               get_runtime_compilation_metadata_callback_depth())) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        // Compilation-time/provider callbacks execute while this same thread owns the non-recursive publication
        // lock. Re-entry there cannot observe a complete registry and must fail before compile_all attempts to relock.
        if(!::uwvm2::runtime::lib::details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth())) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        // This is the sole public execution API that may recursively enter generated Wasm from a host callback.
        // Nested exit must not erase state still owned by the surrounding full execution.
        runtime_execution_entry_scope execution_entry_scope{runtime_execution_entry_reentry::allow_public_llvm_raw};
        // Public host calls, including hostile-FP callback re-entry, must establish a fresh canonical Wasm FP
        // environment and participate in native-unwind registry serialization. Generated import calls use the
        // preconditioned bridge above because their outer execution entry already owns both scopes.
        ::uwvm2::runtime::lib::details::scoped_llvm_wasm_fp_environment fp_environment_guard{
            get_llvm_wasm_fp_environment_active_marker()};
        if(!fp_environment_guard.ready()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto native_unwind_execution_guard{
            g_runtime.llvm_jit_native_unwind_execution_gate.enter_if(runtime_llvm_jit_unwind_call_stack_requested())};

        compile_all_modules_if_needed(false);
        llvm_jit_generated_bridge_scope_guard generated_bridge_scope{get_llvm_jit_generated_bridge_scope_depth()};
        details::llvm_jit_call_raw_from_generated_wasm(runtime_module_ptr, func_index, result_buffer, result_bytes, param_buffer, param_bytes);
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
