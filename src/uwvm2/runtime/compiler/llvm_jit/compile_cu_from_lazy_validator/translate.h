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
 * | |_| |  \ V  V /    \ V / | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <algorithm>
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <limits>
# include <memory>
# include <string>
# include <type_traits>
# include <utility>
# include <vector>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/Analysis/TargetTransformInfo.h>
#  include <llvm/Bitcode/BitcodeReader.h>
#  include <llvm/Bitcode/BitcodeWriter.h>
#  include <llvm/Linker/Linker.h>
#  include <llvm/Support/MemoryBuffer.h>
#  include <llvm/Support/raw_ostream.h>
#  include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/MCJIT.h>
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#  include <uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h>
#  include <llvm/Config/llvm-config.h>
#  include <llvm/InitializePasses.h>
#  include <llvm/IR/BasicBlock.h>
#  include <llvm/IR/Constants.h>
#  include <llvm/IR/Instructions.h>
#  include <llvm/IR/Metadata.h>
#  include "checked_whole_local_target_specialization.h"
#  include <llvm/IR/LegacyPassManager.h>
#  include <llvm/IR/Verifier.h>
#  include <llvm/MC/TargetRegistry.h>
#  include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#  include <llvm/PassRegistry.h>
#  include <llvm/Support/TargetSelect.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/TargetParser/Host.h>
#  include <llvm/TargetParser/Triple.h>
#  include <llvm/Transforms/InstCombine/InstCombine.h>
#  include <llvm/Transforms/Scalar.h>
#  include <llvm/Transforms/Scalar/GVN.h>
#  include <llvm/Transforms/Utils.h>
#  include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
# endif
// import
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/call_indirect_immediate.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/standard/wasm1/impl.h>
# include <uwvm2/validation/standard/wasm1p1/impl.h>
# include <uwvm2/validation/standard/wasm3/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/compiler/shared/wasm1p1_simd.h>
# include <uwvm2/runtime/exception/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <uwvm2/runtime/llvm_jit_cache/impl.h>
# endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)

// Lazy LLVM JIT compilation support for runtime-local Wasm functions.
//
// This header builds a light-weight lazy compilation index from the already-finalized runtime module, validates function
// bodies while emitting checked IR at admission, retains owned compiler artifacts, and materializes MCJIT
// entry addresses only when a function is first needed.  The current LLVM lazy backend uses whole functions as compile
// units; grouping is applied at materialization time to compile small direct-call neighborhoods together.
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator
{
    // Runtime module storage supplied by the loader/finalizer.  All pointers stored in lazy metadata borrow from it.
    using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;

    // Parser-side module storage used by the standard validator when lazy validation is enabled.
    using parser_module_storage_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t;
    using parser_feature_parameter_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t;

    // Shared LLVM JIT metadata and option types from the eager "compile all" path.
    using full_function_symbol_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::full_function_symbol_t;
    using local_func_storage_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t;
    using tiered_loop_reentry_storage_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::tiered_loop_reentry_storage_t;
    using compile_option = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option;
    using llvm_jit_module_storage_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_module_storage_t;
    using validation_module_storage_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::validation_module_storage_t;

    // Controls whether this lazy path re-runs Wasm code validation before JIT emission.
    enum class lazy_validation_mode : unsigned
    {
        // Validate the target function body immediately before lazy LLVM emission.
        validate_on_lazy_compile,

        // Skip lazy validation because the complete code section has already been validated by the caller.
        assume_full_code_verified
    };

    // Compile-unit categories exposed to logging and scheduler metadata.  The LLVM lazy backend currently materializes
    // complete functions only; this enum keeps the external shape compatible with future finer-grained splits.
    enum class lazy_compile_unit_kind : unsigned
    {
        function
    };

    // Stable UTF-8 label for lazy compile-unit diagnostics.
    [[nodiscard]] inline constexpr ::fast_io::u8string_view lazy_compile_unit_kind_name(lazy_compile_unit_kind kind) noexcept
    {
        switch(kind)
        {
            case lazy_compile_unit_kind::function:
                return u8"function";
            [[unlikely]] default:
                return u8"unknown";
        }
    }

    // Stable UTF-8 label for the thread scheduler's lazy compile state.
    [[nodiscard]] inline constexpr ::fast_io::u8string_view compile_state_name(::uwvm2::utils::thread::lazy_compile_state st) noexcept
    {
        switch(st)
        {
            case ::uwvm2::utils::thread::lazy_compile_state::uncompiled: return u8"uncompiled";
            case ::uwvm2::utils::thread::lazy_compile_state::queued: return u8"queued";
            case ::uwvm2::utils::thread::lazy_compile_state::compiling: return u8"compiling";
            case ::uwvm2::utils::thread::lazy_compile_state::compiled: return u8"compiled";
            case ::uwvm2::utils::thread::lazy_compile_state::failed:
                return u8"failed";
            [[unlikely]] default:
                return u8"unknown";
        }
    }

    [[nodiscard]] inline constexpr ::fast_io::u8string_view lazy_validation_mode_name(lazy_validation_mode mode) noexcept
    {
        switch(mode)
        {
            case lazy_validation_mode::validate_on_lazy_compile: return u8"validate_on_lazy_compile";
            case lazy_validation_mode::assume_full_code_verified:
                return u8"assume_full_code_verified";
            [[unlikely]] default:
                return u8"unknown";
        }
    }

    inline constexpr void append_lazy_llvm_jit_cache_shape_key(::uwvm2::utils::container::u8string& key, compile_option const& opt) noexcept
    {
        auto const bool_key_value{[](bool value) constexpr noexcept -> ::fast_io::u8string_view { return value ? u8"1" : u8"0"; }};

        // These flags alter emitted IR, generated symbol sets, or synchronization on lazy target-table loads.
        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
            key, u8"route-wasm-calls-through-runtime-bridge", bool_key_value(opt.route_wasm_calls_through_runtime_bridge));
        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
            key, u8"lazy-defined-targets-are-atomic", bool_key_value(opt.lazy_defined_targets_are_atomic));
        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
            key, u8"emit-tiered-loop-reentry-entries", bool_key_value(opt.emit_tiered_loop_reentry_entries));
        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
            key, u8"emit-call-stack-frames", bool_key_value(opt.emit_call_stack_frames));
        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
            key, u8"emit-unwind-call-stack-frames", bool_key_value(opt.emit_unwind_call_stack_frames));
    }

    [[nodiscard]] inline constexpr ::uwvm2::runtime::llvm_jit_cache::cache_policy lazy_llvm_jit_object_cache_policy() noexcept
    {
        auto policy{::uwvm2::runtime::llvm_jit_cache::default_cache_policy()};
        // Persistent objects rebind host storage/bridge symbols on every
        // materialization; the requested policy is shared by all targets.
        return policy;
    }

    namespace lazy_runtime_log
    {
        // Runtime logging is compiled as a no-op outside the UWVM executable build.
        [[nodiscard]] inline constexpr bool enabled() noexcept
        {
# ifdef UWVM
            return ::uwvm2::uwvm::io::enable_runtime_log;
# else
            return false;
# endif
        }

        // Emits one lazy LLVM JIT log record with a backend-specific prefix.
        template <typename... Args>
        inline constexpr void line(Args&&... args) noexcept
        {
# ifdef UWVM
            if(!enabled()) { return; }

            ::fast_io::io::perrln(::uwvm2::uwvm::io::u8runtime_log_output, u8"[llvm-jit-lazy] ", ::std::forward<Args>(args)...);
# else
            ((void)args, ...);
# endif
        }
    }  // namespace lazy_runtime_log

    // Requested split configuration.  The LLVM lazy path currently records whole-function compile units, so `cu_code_size`
    // is retained for API parity and future subdivision support.
    struct lazy_split_config
    {
        // Target Wasm byte budget for future compile-unit splitting.
        ::std::size_t cu_code_size{4096uz};
    };

    // Static metadata for one lazy compile unit.
    struct lazy_compile_unit_storage_t
    {
        // Scheduler-facing state for this compile unit.
        ::uwvm2::utils::thread::lazy_compile_unit_state state{};

        // Public function index in the runtime module, including imported functions before local definitions.
        ::std::size_t function_index{};

        // Index into local-defined-function storage and all per-local lazy vectors.
        ::std::size_t local_function_index{};

        // Borrowed half-open range of the Wasm expression bytes covered by this compile unit.
        ::std::byte const* code_begin{};
        ::std::byte const* code_end{};

        // Function-relative byte offset and length for diagnostics and grouping heuristics.
        ::std::size_t code_offset{};
        ::std::size_t code_size{};

        // Kind of compile unit represented by this record.
        lazy_compile_unit_kind kind{lazy_compile_unit_kind::function};
    };

    // Per-local-function lazy state.  Function materialization is tracked separately from the compile-unit record so all
    // direct entry, raw entry, and tiered reentry targets publish atomically as one function-level result.
    struct lazy_function_storage_t
    {
        // Canonical state used by scheduler requests and demand compilation.
        ::uwvm2::utils::thread::lazy_compile_unit_state materialization_state{};

        // Public function index in the runtime module.
        ::std::size_t function_index{};

        // Local-defined function index.
        ::std::size_t local_function_index{};

        // Primary compile unit for this function, or SIZE_MAX until initialization fills the record.
        ::std::size_t primary_cu_index{SIZE_MAX};
    };

    // Runtime-owned result of lazily materializing one local function.
    struct lazy_materialized_function_storage_t
    {
        // Validation/emission metadata returned by the full LLVM JIT local-function translator.
        local_func_storage_t local_func{};

        // The LLVM context and MCJIT engine must outlive all native entry addresses returned from the engine.
        ::uwvm2::utils::container::delete_owned_ptr<::llvm::LLVMContext> llvm_context_holder{};
        ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> llvm_jit_engine{};

        // Typed public entry point for normal Wasm-to-Wasm calls.
        ::std::uintptr_t entry_address{};

        // Raw ABI entry point used by the runtime bridge and tiered dispatcher.
        ::std::uintptr_t raw_entry_address{};

        // The internal Tiered body is the Wasm activation; public/raw/OSR entries only adapt its ABI. Publish its
        // concrete address with the other entries so native backtraces never infer body identity from a nearby wrapper.
        ::std::uintptr_t tiered_core_entry_address{};

        // Reentry descriptors and their raw native entry addresses, kept in matching index order.
        ::uwvm2::utils::container::vector<tiered_loop_reentry_storage_t> tiered_loop_reentries{};
        ::uwvm2::utils::container::vector<::std::uintptr_t> tiered_loop_reentry_raw_entry_addresses{};

        // Publication flag for the non-atomic payload above.  Writers fill `entry_address`, `raw_entry_address`,
        // reentry vectors, and the LLVM owner fields first, then publish `ready == true` with release semantics.  Readers
        // must use an acquire load through `std::atomic_ref<bool>` before touching those payload fields.
        //
        // Keep the storage type as `bool` rather than `std::atomic_bool`: the project vector type moves these records, and
        // `std::atomic_ref` gives the required synchronization without making the record itself non-movable.
        bool ready{};

        inline constexpr lazy_materialized_function_storage_t() noexcept = default;
        inline constexpr lazy_materialized_function_storage_t(lazy_materialized_function_storage_t const&) noexcept = delete;
        inline constexpr lazy_materialized_function_storage_t& operator= (lazy_materialized_function_storage_t const&) noexcept = delete;
        inline constexpr lazy_materialized_function_storage_t(lazy_materialized_function_storage_t&&) noexcept = default;
        inline constexpr lazy_materialized_function_storage_t& operator= (lazy_materialized_function_storage_t&&) noexcept = default;
    };

    namespace details { class checked_lazy_ir_plan; }

    // Complete lazy LLVM JIT state for one runtime module.
    struct lazy_module_storage_t
    {
        // Factory-minted owned compiler artifacts from the sole typed admission
        // traversal. The engine/publication work still occurs on first use.
        ::std::unique_ptr<details::checked_lazy_ir_plan> checked_ir_plan{};

        // Shared metadata container used by eager translator helpers; only `local_funcs` is filled eagerly here.
        full_function_symbol_t compiled{};

        // Parser-compatible validation view built from runtime storage and reused for lazy LLVM emission.
        validation_module_storage_t validation_module{};

        // Per-local-function scheduler/materialization records.
        ::uwvm2::utils::container::vector<lazy_function_storage_t> functions{};

        // Compile units exposed to background prefetch and demand requests.
        ::uwvm2::utils::container::vector<lazy_compile_unit_storage_t> compile_units{};

        // Native materialization results indexed by local-defined-function index.
        ::uwvm2::utils::container::vector<lazy_materialized_function_storage_t> materialized_functions{};
    };

    // Options consumed by the lazy LLVM JIT validator and materializer.
    struct lazy_compile_options
    {
        // Base LLVM JIT options shared with the eager full-module compiler.
        compile_option compile_options{};

        // Optional parser provenance retained for callers; ordinary lazy compilation
        // validates runtime metadata and emits each body in the same traversal.
        parser_module_storage_t const* validator_module_storage{};
        // Borrowed feature switches used when the parser produced `validator_module_storage`.
        parser_feature_parameter_t const* validator_feature_parameter{};

        // Validation policy for lazy compilation.
        lazy_validation_mode validation_mode{lazy_validation_mode::validate_on_lazy_compile};

        // Borrowed per-local-function diagnostics. Runtime preparation resizes this array once, then pins it for the
        // module execution lease. A group owner copies a validation failure into every claimed member before any
        // member's failed release-store; waiters read their own slot only after an acquire of that terminal state.
        ::uwvm2::validation::error::code_validation_error_impl* grouped_failure_diagnostics{};
        ::std::size_t grouped_failure_diagnostic_count{};

        // LLVM code-generation level used for the MCJIT engine and local optimization pipeline.
        ::llvm::CodeGenOptLevel codegen_opt_level{::llvm::CodeGenOptLevel::Less};

        // Optional listener for object/debug registration events emitted by MCJIT.
        ::llvm::JITEventListener* jit_event_listener{};

        // Called synchronously after all group addresses, native CFI and owning
        // objects are complete, before ANY member's ready flag is released.
        // The callback may read the compiler-owned records directly during this
        // call to prepare the entire diagnostic address map. It must not use a
        // ready-gated accessor, publish an entry, execute guest code, or retain
        // the borrowed index vector. A direct call may enter any group member
        // as soon as the first ready flag becomes visible.
        void (*prepare_materialized_group)(void*, lazy_module_storage_t const&,
                                           ::uwvm2::utils::container::vector<::std::size_t> const&) noexcept {};
        void* prepare_user_data{};
    };

    // User data passed to the generic lazy compile scheduler for one compile request.
    struct lazy_compile_request_context
    {
        // Borrowed runtime module containing function bodies and finalized type/storage metadata.
        runtime_module_storage_t const* curr_module{};

        // Mutable lazy state for the module being compiled.
        lazy_module_storage_t* lazy_storage{};

        // Request-local copy of lazy compile options.
        lazy_compile_options options{};

        // Compile-unit index that triggered this request.
        ::std::size_t compile_unit_index{};

        // Optional caller-owned validation error sink; a temporary sink is used when null.
        ::uwvm2::validation::error::code_validation_error_impl* err{};

        // Borrowed module name used only for runtime log records.
        ::uwvm2::utils::container::u8string_view module_name{};

        // Optional callback invoked after a local function has been materialized and before its state is published.
        void (*publish_materialized_function)(void*, ::std::size_t) noexcept {};

        // Opaque data passed to `publish_materialized_function`.
        void* publish_user_data{};
    };

    namespace details
    {
        inline void set_llvm_module_target_triple_from_machine(::llvm::Module& module, ::llvm::TargetMachine const& target_machine)
        {
# if LLVM_VERSION_MAJOR >= 21
            module.setTargetTriple(target_machine.getTargetTriple());
# else
            module.setTargetTriple(target_machine.getTargetTriple().str());
# endif
        }

        // Serializes LLVM MCJIT materialization and publication.  LLVM global initialization, MCJIT object finalization,
        // and this runtime's target-table publication are intentionally kept behind a narrow process-local lock because
        // those phases touch process-wide LLVM state and runtime dispatch tables.
        inline ::std::atomic_flag lazy_materialize_lock = ATOMIC_FLAG_INIT;

        // Spin-based guard used by scheduler worker callbacks, where blocking primitives may not be available in all
        // supported builds.
        struct lazy_materialize_lock_guard
        {
            inline constexpr lazy_materialize_lock_guard() noexcept
            {
                // Acquire pairs with the guard destructor's release clear.  This is a real lock boundary, not just a
                // contention flag: after the loop exits, this worker sees all materialization and target-table side
                // effects sequenced before the previous holder released the flag.
                while(lazy_materialize_lock.test_and_set(::std::memory_order_acquire)) { ::uwvm2::utils::thread::lazy_compile_thread_yield(); }
            }

            inline constexpr lazy_materialize_lock_guard(lazy_materialize_lock_guard const&) noexcept = delete;
            inline constexpr lazy_materialize_lock_guard& operator= (lazy_materialize_lock_guard const&) noexcept = delete;

            inline constexpr ~lazy_materialize_lock_guard()
            {
                // Release publishes every protected side effect before another worker's acquire succeeds.  Wait-free
                // readers still synchronize through their own `ready` or state acquire loads; this lock only orders
                // workers with respect to each other.
                lazy_materialize_lock.clear(::std::memory_order_release);
            }
        };

        // Extracts the first argument type from selected LLVM member functions.  LLVM has changed ownership parameter
        // spellings across versions, so these aliases preserve ABI compatibility without version-specific branches.
        template <typename>
        struct member_function_first_argument;

        template <typename R, typename C, typename A0>
        struct member_function_first_argument<R (C::*)(A0)>
        {
            using type = A0;
        };

        using llvm_module_owner_t = typename member_function_first_argument<decltype(&::llvm::ExecutionEngine::addModule)>::type;
        using llvm_jit_memory_manager_owner_t = typename member_function_first_argument<decltype(&::llvm::EngineBuilder::setMCJITMemoryManager)>::type;
        using llvm_jit_function_address_name_t =
            ::std::remove_cvref_t<typename member_function_first_argument<decltype(&::llvm::ExecutionEngine::getFunctionAddress)>::type>;

        // Shorthand aliases for the eager LLVM JIT compiler and its internal helper namespace.
        namespace all_compile = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
        namespace all_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

        // Cached host target description used for every lazy MCJIT engine in this process.
        struct llvm_jit_native_target_config
        {
            // Host CPU name as returned by LLVM, stored to keep StringRef inputs alive.
            ::uwvm2::utils::container::u8string cpu_name{};

            // Host CPU used for backend scheduling/tuning.  JIT code runs only on this machine, so tune natively too.
            ::uwvm2::utils::container::u8string tune_cpu_name{};

            // Host feature attributes such as "+sse2" or "-avx512f", also stored for stable StringRef lifetimes.
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> feature_storage{};
        };

        // Returns the byte distance between two pointers inside the same Wasm function body.
        [[nodiscard]] inline constexpr ::std::size_t byte_offset(::std::byte const* base, ::std::byte const* curr) noexcept
        { return static_cast<::std::size_t>(curr - base); }

        // Adds one local function to the lazy metadata tables and records its borrowed runtime function storage.
        inline constexpr void append_lazy_function(runtime_module_storage_t const& curr_module,
                                                   lazy_module_storage_t& storage,
                                                   compile_option const& options,
                                                   ::std::size_t local_function_index,
                                                   ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            auto const import_func_count{curr_module.imported_function_vec_storage.size()};
            auto const function_index{import_func_count + local_function_index};
            auto local_func_storage{all_details::get_runtime_local_func_storage(curr_module, local_function_index, err)};
            local_func_storage.module_id = options.curr_wasm_id;

            auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
            auto const& curr_code{*curr_local_func.wasm_code_ptr};
            auto const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
            auto const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};

            auto const cu_index{storage.compile_units.size()};
            storage.compile_units.push_back({.function_index = function_index,
                                             .local_function_index = local_function_index,
                                             .code_begin = code_begin,
                                             .code_end = code_end,
                                             .code_offset = 0uz,
                                             .code_size = byte_offset(code_begin, code_end),
                                             .kind = lazy_compile_unit_kind::function});

            storage.functions.index_unchecked(
                local_function_index) = {.function_index = function_index, .local_function_index = local_function_index, .primary_cu_index = cu_index};
            storage.compiled.local_funcs.index_unchecked(local_function_index) = local_func_storage;
        }

        // Publishes a function-level terminal state and mirrors it to the primary compile unit so both scheduler waiters
        // and compile-unit observers are notified.
        struct lazy_compile_state_notifier
        {
            inline constexpr void operator()(::uwvm2::utils::thread::lazy_compile_unit_state& unit) const noexcept
            { ::uwvm2::utils::thread::lazy_compile_notify_unit(unit); }
        };

        template <typename Notify = lazy_compile_state_notifier>
        inline constexpr void mark_function_compile_units_state(lazy_module_storage_t& storage,
                                                                lazy_function_storage_t& fn,
                                                                ::uwvm2::utils::thread::lazy_compile_state state,
                                                                Notify notify = {}) noexcept
        {
            static_assert(noexcept(notify(fn.materialization_state)), "lazy terminal-state notifiers must not throw");
            if(fn.primary_cu_index < storage.compile_units.size())
            {
                auto& cu_state{storage.compile_units.index_unchecked(fn.primary_cu_index).state};
                // Publish the compile-unit mirror first.  Acquiring the authoritative function state below then also
                // observes this mirror, in addition to all materialized payload written before this call.
                cu_state.state.store(state, ::std::memory_order_release);
                notify(cu_state);
            }
            // This release store is the function-level publication barrier.  By the time a waiter observes `compiled`
            // with an acquire load/wait, the materialized record, its `ready` publication, runtime target-table stores,
            // and the compile-unit mirror are visible.  `failed` likewise publishes diagnostics and cleanup state.
            fn.materialization_state.state.store(state, ::std::memory_order_release);
            notify(fn.materialization_state);
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
            ::LLVMInitializeX86AsmParser();
            return true;
# elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(__arm64ec__) || defined(_M_ARM64EC)
            ::LLVMInitializeAArch64TargetInfo();
            ::LLVMInitializeAArch64Target();
            ::LLVMInitializeAArch64TargetMC();
            ::LLVMInitializeAArch64AsmPrinter();
            ::LLVMInitializeAArch64AsmParser();
            return true;
# elif defined(__arm__) || defined(_M_ARM)
            ::LLVMInitializeARMTargetInfo();
            ::LLVMInitializeARMTarget();
            ::LLVMInitializeARMTargetMC();
            ::LLVMInitializeARMAsmPrinter();
            ::LLVMInitializeARMAsmParser();
            return true;
# elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
            ::LLVMInitializePowerPCTargetInfo();
            ::LLVMInitializePowerPCTarget();
            ::LLVMInitializePowerPCTargetMC();
            ::LLVMInitializePowerPCAsmPrinter();
            ::LLVMInitializePowerPCAsmParser();
            return true;
# elif defined(__riscv)
            ::LLVMInitializeRISCVTargetInfo();
            ::LLVMInitializeRISCVTarget();
            ::LLVMInitializeRISCVTargetMC();
            ::LLVMInitializeRISCVAsmPrinter();
            // Full-width host addresses use the production `li` inline-asm
            // materializer. Registering only the printer leaves MCJIT unable
            // to encode that IR, including calls in the live unwind probe.
            ::LLVMInitializeRISCVAsmParser();
            return true;
# elif defined(__s390x__)
            ::LLVMInitializeSystemZTargetInfo();
            ::LLVMInitializeSystemZTarget();
            ::LLVMInitializeSystemZTargetMC();
            ::LLVMInitializeSystemZAsmPrinter();
            ::LLVMInitializeSystemZAsmParser();
            return true;
# elif defined(__loongarch__)
            ::LLVMInitializeLoongArchTargetInfo();
            ::LLVMInitializeLoongArchTarget();
            ::LLVMInitializeLoongArchTargetMC();
            ::LLVMInitializeLoongArchAsmPrinter();
            ::LLVMInitializeLoongArchAsmParser();
            return true;
# elif defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)
            ::LLVMInitializeMipsTargetInfo();
            ::LLVMInitializeMipsTarget();
            ::LLVMInitializeMipsTargetMC();
            ::LLVMInitializeMipsAsmPrinter();
            ::LLVMInitializeMipsAsmParser();
            return true;
# elif defined(__sparc__)
            ::LLVMInitializeSparcTargetInfo();
            ::LLVMInitializeSparcTarget();
            ::LLVMInitializeSparcTargetMC();
            ::LLVMInitializeSparcAsmPrinter();
            ::LLVMInitializeSparcAsmParser();
            return true;
# else
            return !::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() &&
                   !::llvm::InitializeNativeTargetAsmParser();
# endif
        }

        // Initializes LLVM native-target support exactly once and records whether the initialization succeeded.
        [[nodiscard]] inline constexpr bool ensure_llvm_jit_native_target_initialized() noexcept
        {
            static ::std::atomic_bool initialized{};
            static ::std::atomic_bool success{};
            static ::std::atomic_flag init_lock = ATOMIC_FLAG_INIT;

            // `initialized` is the once flag.  An acquire load that observes `true` also observes LLVM's process-wide
            // target registration side effects and the preceding `success` publication.  The separate `success` atomic is
            // intentionally read after the acquire so callers never consume a stale result from before initialization.
            if(initialized.load(::std::memory_order_acquire)) { return success.load(::std::memory_order_acquire); }

            while(init_lock.test_and_set(::std::memory_order_acquire))
            {
                if(initialized.load(::std::memory_order_acquire)) { return success.load(::std::memory_order_acquire); }
                ::uwvm2::utils::thread::lazy_compile_thread_yield();
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
                auto const ok{initialize_llvm_jit_process_target()};
                // Publish the result before publishing completion.  Readers synchronize through the `initialized` flag,
                // whose release store is sequenced after this store.
                success.store(ok, ::std::memory_order_release);
                initialized.store(true, ::std::memory_order_release);
            }

            init_lock.clear(::std::memory_order_release);
            return success.load(::std::memory_order_acquire);
        }

        // Publishes or reads the materialized-function ready flag.
        //
        // `ready == true` is a release publication of the ordinary payload fields in `lazy_materialized_function_storage_t`.
        // The matching acquire load is intentionally inside the accessor functions below, because some tiered paths probe
        // materialized records before they have observed the outer function state as `compiled`.
        inline constexpr void store_lazy_materialized_ready(lazy_materialized_function_storage_t& materialized, bool ready, ::std::memory_order order) noexcept
        { ::std::atomic_ref<bool>{materialized.ready}.store(ready, order); }

        [[nodiscard]] inline constexpr bool load_lazy_materialized_ready(lazy_materialized_function_storage_t const& materialized,
                                                                         ::std::memory_order order) noexcept
        {
            auto& ready{const_cast<bool&>(materialized.ready)};
            return ::std::atomic_ref<bool>{ready}.load(order);
        }

        // The compiler owns every claimed record exclusively until publication.
        // Complete group-wide runtime metadata before opening any ready gate:
        // readers can bypass scheduler state through the public entry accessors.
        inline constexpr void publish_lazy_materialized_group_after_prepare(
            lazy_module_storage_t& storage,
            lazy_compile_options const& options,
            ::uwvm2::utils::container::vector<::std::size_t> const& local_function_indices) noexcept
        {
            if(options.prepare_materialized_group != nullptr)
            {
                options.prepare_materialized_group(options.prepare_user_data, storage, local_function_indices);
            }
            for(auto const local_function_index: local_function_indices)
            {
                auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
                // The release includes all group metadata prepared by the
                // callback, not merely this member's addresses and owner.
                store_lazy_materialized_ready(materialized, true, ::std::memory_order_release);
            }
        }

        // Copies LLVM's host feature map into owning strings so later EngineBuilder StringRefs remain valid.
        [[nodiscard]] inline constexpr ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>
            get_llvm_jit_host_target_attribute_storage() noexcept
        {
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> mattrs{};
            auto host_features{::llvm::sys::getHostCPUFeatures()};
            if(!host_features.empty())
            {
                mattrs.reserve(host_features.size());
                for(auto const& [feature_name, feature_enabled]: host_features)
                {
                    mattrs.push_back(
                        ::uwvm2::utils::container::u8concat_uwvm(feature_enabled ? u8"+" : u8"-", all_details::get_uwvm_u8string_view(feature_name)));
                }
            }
# if (defined(__mips__) || defined(__MIPS__) || defined(_MIPS_ARCH)) && !defined(__mips_msa)
            // The runtime must protect MSACSR and use an MSA-compatible ABI before native JIT enables MSA.
            mattrs.emplace_back(u8"-msa");
# endif
# if defined(__riscv) && !defined(__riscv_flen)
            mattrs.emplace_back(u8"-f");
            mattrs.emplace_back(u8"-d");
            mattrs.emplace_back(u8"-v");
# endif
# if (defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)) && !defined(__ALTIVEC__) && !defined(__linux__)
            mattrs.emplace_back(u8"-altivec");
            mattrs.emplace_back(u8"-vsx");
# endif
            return mattrs;
        }

        [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string get_llvm_jit_host_cpu_name() noexcept
        {
            auto const host_cpu_name{::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_abi_host_cpu_name(
                ::llvm::Triple{::llvm::sys::getProcessTriple()}, ::llvm::sys::getHostCPUName())};
            return ::uwvm2::utils::container::u8string{all_details::get_uwvm_u8string_view(host_cpu_name)};
        }

        // Returns the process-wide native target configuration used for lazy MCJIT materialization.
        [[nodiscard]] inline constexpr llvm_jit_native_target_config const& get_llvm_jit_native_target_config() noexcept
        {
            static llvm_jit_native_target_config config{.cpu_name = get_llvm_jit_host_cpu_name(),
                                                        .tune_cpu_name = get_llvm_jit_host_cpu_name(),
                                                        .feature_storage = get_llvm_jit_host_target_attribute_storage()};  // [global]
            return config;
        }

        // Converts owning host feature strings into the StringRef array shape expected by LLVM EngineBuilder.
        inline constexpr void
            append_llvm_jit_host_target_attribute_refs(::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& attr_storage,
                                                       ::llvm::SmallVector<::llvm::StringRef, 16>& attr_refs) noexcept
        {
            attr_refs.clear();
            attr_refs.reserve(attr_storage.size());
            for(auto const& attr: attr_storage) { attr_refs.push_back(all_details::get_llvm_string_ref(attr)); }
        }

        inline void
            append_llvm_jit_host_target_attribute_strings(::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string> const& attr_storage,
                                                          ::llvm::SmallVector<::std::string, 16>& attr_strings)
        {
            attr_strings.clear();
            attr_strings.reserve(attr_storage.size());
            for(auto const& attr: attr_storage)
            {
                auto const attr_ref{all_details::get_llvm_string_ref(attr)};
                attr_strings.emplace_back(attr_ref.data(), attr_ref.size());
            }
        }

        [[nodiscard]] inline ::llvm::Triple get_llvm_jit_mcjit_target_triple()
        {
            // N32/x32 have 32-bit pointers but execute a 64-bit ISA. Do not
            // regress to EngineBuilder's implicit getProcessTriple() narrowing.
            ::llvm::Triple triple{::llvm::Triple::normalize(::llvm::sys::getDefaultTargetTriple())};
#  if defined(__aarch64__) && defined(__linux__) && !defined(__ANDROID__)
            if(triple.getArch() == ::llvm::Triple::aarch64 && triple.isOSLinux())
            {
                // Keep lazy MCJIT aligned with the full-module path: Alpine's
                // aarch64-alpine-linux-musl triple can fault in LLVM 22 RuntimeDyld under qemu-user.
                triple.setVendor(::llvm::Triple::UnknownVendor);
                triple.setEnvironment(::llvm::Triple::GNU);
            }
#  endif
            return triple;
        }

        [[nodiscard]] inline ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine>
            select_llvm_jit_target(::llvm::EngineBuilder& target_builder, llvm_jit_native_target_config const& target_config)
        {
            ::llvm::SmallVector<::std::string, 16> host_target_attribute_strings{};
            append_llvm_jit_host_target_attribute_strings(target_config.feature_storage, host_target_attribute_strings);
            auto target_triple{get_llvm_jit_mcjit_target_triple()};
            ::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_append_native_host_vector_features(
                target_triple,all_details::get_llvm_string_ref(target_config.cpu_name),host_target_attribute_strings);
            // Match full/probe selection. RuntimeDyld's MIPS far-call stubs
            // jump through $at, not the PIC host callee's required $t9. Use
            // full-width C-ABI register calls. O32/N32 ignore long-calls while
            // ABICalls is enabled, so both features are required for MCJIT's
            // static model. This also avoids relying on separately allocated
            // lazy objects sharing one JAL 256-MiB address region.
            if(target_triple.isMIPS())
            {
                host_target_attribute_strings.emplace_back("+noabicalls");
                host_target_attribute_strings.emplace_back("+long-calls");
            }
            // Match full materialization: an AOT backend is not automatically
            // an in-process MCJIT loader. Refuse before unsupported relocations
            // can abort LLVM; never turn this into an AOT inventory skip.
            // LLVM's EngineBuilder warning on !hasJIT() does not stop creation.
            ::std::string error{};
            // The empty-architecture overload preserves the explicit triple
            // and is also available before LLVM 23's two-argument API change.
            auto const target{::llvm::TargetRegistry::lookupTarget({}, target_triple, error)};
            // BPF bytecode needs a different loader/execution ABI despite its JIT flag.
            if(target_triple.isBPF() || target == nullptr || !target->hasJIT() || !target->hasMCAsmBackend() ||
               !::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_object_format_supported(target_triple)) { return {}; }
            ::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_configure_host_unwind_abi(target_builder);
            ::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_configure_code_model(target_builder, target_triple);
            auto machine{::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine>{target_builder.selectTarget(
                target_triple, {}, all_details::get_llvm_string_ref(target_config.cpu_name), host_target_attribute_strings)}};
            if(machine != nullptr && !::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_subtarget_supported(*machine)) { return {}; }
            // Match full/probe emission: an external RuntimeDyld must not alias
            // repeated ELF local labels when lazy modules are materialized.
            if(machine != nullptr && ::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_needs_unique_temp_labels(target_triple))
            { machine->Options.MCOptions.MCSaveTempLabels = true; }
            return machine;
        }

        inline constexpr void apply_llvm_jit_native_target_function_attrs(::llvm::Module& module,
                                                                          llvm_jit_native_target_config const& target_config,
                                                                          ::llvm::TargetMachine const& target_machine) noexcept
        {
            auto const target_features{target_machine.getTargetFeatureString()};
            auto const target_cpu_ref{all_details::get_llvm_string_ref(target_config.cpu_name)};
            auto const tune_cpu_ref{all_details::get_llvm_string_ref(target_config.tune_cpu_name)};
            auto const target_features_ref{all_details::get_llvm_string_ref(target_features)};

            for(auto& function: module)
            {
                if(function.isDeclaration()) { continue; }
                if(!target_cpu_ref.empty()) { function.addFnAttr(all_details::get_llvm_string_ref(u8"target-cpu"), target_cpu_ref); }
                if(!tune_cpu_ref.empty()) { function.addFnAttr(all_details::get_llvm_string_ref(u8"tune-cpu"), tune_cpu_ref); }
                if(!target_features_ref.empty()) { function.addFnAttr(all_details::get_llvm_string_ref(u8"target-features"), target_features_ref); }
            }
        }

        inline constexpr void append_llvm_jit_native_target_codegen_policy(::uwvm2::utils::container::u8string& policy,
                                                                           llvm_jit_native_target_config const& target_config) noexcept
        {
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(policy, u8"target-cpu", target_config.cpu_name);
            ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(policy, u8"tune-cpu", target_config.tune_cpu_name);
        }

        struct call_indirect_scan_policy_t
        {
            bool mvp_reserved_byte{};
        };

        [[nodiscard]] inline constexpr call_indirect_scan_policy_t
            get_call_indirect_scan_policy(parser_feature_parameter_t const* validator_feature_parameter) noexcept
        {
            parser_feature_parameter_t const default_validator_feature_parameter{};
            auto const& effective_validator_feature_parameter{
                validator_feature_parameter == nullptr ? default_validator_feature_parameter : *validator_feature_parameter};
            auto const& wasm1p1_para{
                ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(effective_validator_feature_parameter)};
            return {.mvp_reserved_byte =
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::uses_mvp_call_indirect_reserved_byte(wasm1p1_para)};
        }

        [[nodiscard]] inline constexpr bool parse_call_indirect_table_index_for_scan(
            ::std::byte const*& code_curr,
            ::std::byte const* code_end,
            parser_feature_parameter_t const* validator_feature_parameter,
            all_details::validation_module_traits_t::wasm_u32& table_index) noexcept
        {
            auto const policy{get_call_indirect_scan_policy(validator_feature_parameter)};
            // The scanner caller has already consumed and decoded the opcode/type-index prefix. At entry, the trailing
            // immediate may already be section_end:
            // call_indirect type_index table_index ...
            // [          safe        ] unsafe (could be the section_end)
            //                          ^^ code_curr
            // On success the complete trailing immediate has been consumed:
            // call_indirect type_index table_index ...
            // [                safe              ] unsafe (could be the section_end)
            //                                      ^^ code_curr
            // This wrapper commits only that complete trailing immediate. On failure the shared decoder leaves code_curr
            // at the entry position above and leaves table_index unchanged.
            return ::uwvm2::parser::wasm::standard::wasm1p1::features::parse_call_indirect_trailing_immediate(
                code_curr, code_end, policy.mvp_reserved_byte, table_index);
        }

        // Decode one blocktype only far enough to establish its byte boundary. Direct value forms are literal one-byte
        // alternatives; the remaining grammar is a non-negative s33 type index. Runtime type-section bounds belong to
        // validation and are deliberately not consulted by this raw-body scanner. Failure leaves code_curr at entry.
        [[nodiscard]] inline constexpr bool skip_wasm_blocktype_for_callee_scan(::std::byte const*& code_curr,
                                                                                ::std::byte const* code_end) noexcept
        {
            // control_op blocktype ...
            // [  safe  ] unsafe (could be the section_end)
            //            ^^ code_curr

            if(code_curr == code_end) [[unlikely]] { return false; }
            // [control opcode][blocktype prefix ... code_end)
            // [safe          ] code_curr != code_end proves exactly one prefix readable.
            if(::uwvm2::validation::standard::wasm3::is_core3_extended_block_reference_prefix(
                ::std::to_integer<unsigned>(*code_curr)))
            {
                auto cursor{code_curr};
                ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
                // This shallow callee scan determines only the byte boundary.
                // The ordinary validator checks type indices and feature gates.
                auto const decoded{::uwvm2::validation::standard::wasm3::scan_core3_value_carrier(
                    cursor, code_end, true, (::std::numeric_limits<::std::size_t>::max)(),
                    empty_context, true, true)};
                if(decoded.error != ::uwvm2::validation::standard::wasm3::value_carrier_error::ok) [[unlikely]]
                { return false; }
                // [control opcode][complete checked Core 3 valtype] next ... code_end
                // [safe                                       ] unsafe (possibly code_end)
                //                                               ^^ code_curr: the decoder bounded
                //                                                  every byte before this commit.
                code_curr = cursor;
                // control_op checked Core 3 valtype ... code_end
                // [safe                              ] unsafe (could be code_end)
                //                                    ^^ code_curr; no byte is read by this commit.
                return true;
            }

            auto cursor{code_curr};
            auto const blocktype_begin{cursor};
            ::std::int_least64_t blocktype{};
            if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, blocktype)) [[unlikely]] { return false; }

            auto const encoded_size{static_cast<::std::size_t>(cursor - blocktype_begin)};
            if(encoded_size > 5uz) [[unlikely]] { return false; }
            if(blocktype < 0)
            {
                if(encoded_size != 1uz) [[unlikely]] { return false; }
                switch(blocktype)
                {
                    case -64:  // empty
                    case -1:   // i32
                    case -2:   // i64
                    case -3:   // f32
                    case -4:   // f64
                    case -5:   // v128
                    case -16:  // funcref
                    case -17:  // externref
                        break;
                    [[unlikely]] default: return false;
                }
            }
            else if(static_cast<::std::uint_least64_t>(blocktype) >
                    static_cast<::std::uint_least64_t>((::std::numeric_limits<all_details::validation_module_traits_t::wasm_u32>::max)()))
                [[unlikely]]
            {
                return false;
            }

            // control_op blocktype ... code_end
            // [       safe       ] unsafe (could be code_end)
            //         ^^ code_curr: the bounded s33 decoder returned cursor in this body.
            code_curr = cursor;

            // control_op blocktype ...
            // [       safe       ] unsafe (could be the section_end)
            //                      ^^ code_curr
            return true;
        }

        // Fixed-width fields first prove the complete range. Truncation leaves code_curr at entry.
        [[nodiscard]] inline constexpr bool skip_wasm_fixed_immediate_for_callee_scan(::std::byte const*& code_curr,
                                                                                      ::std::byte const* code_end,
                                                                                      ::std::size_t byte_count) noexcept
        {
            // fixed_immediate[byte_count] ...
            // unsafe (could be the section_end)
            // ^^ code_curr

            // A zero-width field does not form a pointer difference, including
            // when an empty synthetic scan uses equal null endpoints.
            if(byte_count == 0uz) { return true; }
            if(code_curr == code_end) [[unlikely]] { return false; }
            // [body bytes from code_curr through code_end) | one-past
            // [safe                                   ] | byte_count must fit.
            //  ^^ code_curr and code_end borrow the same function-body allocation.
            if(static_cast<::std::size_t>(code_end - code_curr) < byte_count) [[unlikely]] { return false; }
            // fixed_immediate[byte_count] ... code_end
            // [       safe                ] unsafe (could be code_end)
            //         ^^ code_curr: remaining-byte proof covers the full field.
            code_curr += byte_count;

            // instruction fixed_immediate[byte_count] ...
            // [                safe                 ] unsafe (could be the section_end)
            //                                         ^^ code_curr
            return true;
        }

        // MVP memory instructions and the bulk-memory encodings below use literal reserved zero bytes, not u32 indices.
        // At entry, the immediate may already be section_end:
        // reserved_zero ...
        // unsafe (could be the section_end)
        // ^^ code_curr
        // A missing/nonzero byte fails without advancing code_curr. On success:
        // reserved_zero ...
        // [   safe    ] unsafe (could be the section_end)
        //               ^^ code_curr
        [[nodiscard]] inline constexpr bool parse_wasm_memory_index_for_callee_scan(::std::byte const*& code_curr,
                                                                                     ::std::byte const* code_end) noexcept
        {
            ::std::uint_least32_t index{};
            // [opcode] memidx ... (code_end): bounded structural scanning; full validation owns feature policy.
            return ::uwvm2::validation::standard::wasm3::scan_memory_index(code_curr, code_end, true, index) == ::uwvm2::validation::standard::wasm3::memory_immediate_error::ok;
            // On success [opcode memidx] is safe, code_curr may equal code_end; failure leaves it unchanged.
        }

        // Alignment/index u32 and offset u64 use a local cursor; failure leaves code_curr at entry.
        [[nodiscard]] inline constexpr bool skip_wasm_memarg_for_callee_scan(::std::byte const*& code_curr,
                                                                             ::std::byte const* code_end) noexcept
        {
            // [opcode] memarg ... (code_end): use the same transactional encoding decoder as full validation.
            return ::uwvm2::validation::standard::wasm3::scan_memory_argument64(code_curr, code_end, true).error == ::uwvm2::validation::standard::wasm3::memory_immediate_error::ok;
            // [opcode memarg] safe; code_curr may equal code_end on success, unchanged on failure.
        }

        // Decode the vector immediate of typed select. The binary vector may have any encoded count; the validator owns
        // the current semantic restriction to exactly one result type. Every element must still be a grammatical valtype.
        // Count truncation, insufficient remaining bytes, or an invalid valtype leaves code_curr at entry.
        [[nodiscard]] inline constexpr bool skip_wasm_typed_select_for_callee_scan(::std::byte const*& code_curr,
                                                                                   ::std::byte const* code_end) noexcept
        {
            using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;

            // result_type_count result_type ...
            // unsafe (could be the section_end)
            // ^^ code_curr

            auto cursor{code_curr};
            wasm_u32 result_type_count{};
            if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, result_type_count)) [[unlikely]] { return false; }

            auto const result_type_count_uz{static_cast<::std::size_t>(result_type_count)};
            if(static_cast<wasm_u32>(result_type_count_uz) != result_type_count ||
               result_type_count_uz > static_cast<::std::size_t>(code_end - cursor)) [[unlikely]]
            {
                return false;
            }

            for(::std::size_t i{}; i != result_type_count_uz; ++i)
            {
                // [select_t count][previous complete valtypes] next valtype ... | code_end
                // [safe                                     ] unsafe (possibly one-past)
                //                                             ^^ cursor: each type may include an s33 heap.
                // Decode the same Core 3 binary valtype grammar used by the fused
                // validator. This boundary-only pass has no type-index or feature
                // authority; the actual compiler checks its retained module context.
                ::uwvm2::validation::standard::wasm3::recursive_binary_details::reader input{
                    {cursor, cursor == code_end ? 0uz : static_cast<::std::size_t>(code_end - cursor)}};
                ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type value{};
                if(!input.value(value)) [[unlikely]] { return false; }
                // [select_t count][complete valtypes through i] next ... | code_end
                // [safe                                      ] unsafe (possibly one-past)
                //                                              ^^ cursor: value() bounded every byte and proved
                //                                                 0 < input.position <= the original slice size.
                cursor += input.position;
                // [select_t count][complete valtypes through i] next ... | code_end
                // [safe                                      ] unsafe (possibly one-past)
                //                                              ^^ cursor: outer code_curr is still unchanged.
            }
            // select_t result_type_count complete_valtypes ... code_end
            // [safe                                        ] unsafe (possibly one-past)
            //                                                ^^ cursor: no additional byte-count advance;
            //                                                   full reference valtypes can occupy several bytes.
            // select_t result_type_count result_type ... code_end
            // [                safe                ] unsafe (could be code_end)
            //                  ^^ code_curr: the checked result vector ends at cursor.
            code_curr = cursor;

            // select_t result_type_count result_type ...
            // [                safe                ] unsafe (could be the section_end)
            //                                        ^^ code_curr
            return true;
        }

        // Decode one 0xfc instruction. Feature gates and index bounds are semantic checks performed by the validator;
        // this helper recognizes every retained subopcode and consumes only its binary immediates. Unknown/truncated
        // subopcodes or immediates leave code_curr at entry.
        [[nodiscard]] inline constexpr bool skip_wasm_numeric_prefix_for_callee_scan(::std::byte const*& code_curr,
                                                                                     ::std::byte const* code_end) noexcept
        {
            using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;
            using numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;

            // subopcode immediates ...
            // unsafe (could be the section_end)
            // ^^ code_curr

            auto cursor{code_curr};
            wasm_u32 subopcode{};
            if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, subopcode)) [[unlikely]] { return false; }

            wasm_u32 index{};
            switch(static_cast<numeric_code>(subopcode))
            {
                case numeric_code::i32_trunc_sat_f32_s:
                case numeric_code::i32_trunc_sat_f32_u:
                case numeric_code::i32_trunc_sat_f64_s:
                case numeric_code::i32_trunc_sat_f64_u:
                case numeric_code::i64_trunc_sat_f32_s:
                case numeric_code::i64_trunc_sat_f32_u:
                case numeric_code::i64_trunc_sat_f64_s:
                case numeric_code::i64_trunc_sat_f64_u: break;
                case numeric_code::memory_init:
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, index) ||
                       !parse_wasm_memory_index_for_callee_scan(cursor, code_end)) [[unlikely]]
                    {
                        return false;
                    }
                    break;
                case numeric_code::data_drop:
                case numeric_code::elem_drop:
                case numeric_code::table_grow:
                case numeric_code::table_size:
                case numeric_code::table_fill:
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, index)) [[unlikely]] { return false; }
                    break;
                case numeric_code::memory_copy:
                    if(!parse_wasm_memory_index_for_callee_scan(cursor, code_end) ||
                       !parse_wasm_memory_index_for_callee_scan(cursor, code_end)) [[unlikely]]
                    {
                        return false;
                    }
                    break;
                case numeric_code::memory_fill:
                    if(!parse_wasm_memory_index_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                    break;
                case numeric_code::table_init:
                case numeric_code::table_copy:
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, index) ||
                       !all_details::parse_wasm_leb128_immediate(cursor, code_end, index)) [[unlikely]]
                    {
                        return false;
                    }
                    break;
                [[unlikely]] default: return false;
            }

            // numeric_prefix subopcode immediates ... code_end
            // [              safe               ] unsafe (could be code_end)
            //                ^^ code_curr: each selected decoder bounded cursor.
            code_curr = cursor;

            // numeric_prefix subopcode immediates ...
            // [              safe               ] unsafe (could be the section_end)
            //                                     ^^ code_curr
            return true;
        }

        // Decode one 0xfd instruction through the same exhaustive opcode visitor used by LLVM lowering. The visitor
        // supplies the immediate shape; lane/alignment ranges remain validator semantics and do not affect boundaries.
        // Unknown/truncated subopcodes or immediates leave code_curr at entry.
        [[nodiscard]] inline constexpr bool skip_wasm_simd_prefix_for_callee_scan(::std::byte const*& code_curr,
                                                                                  ::std::byte const* code_end) noexcept
        {
            using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;
            namespace shared_simd = ::uwvm2::runtime::compiler::shared;

            // subopcode immediates ...
            // unsafe (could be the section_end)
            // ^^ code_curr

            auto cursor{code_curr};
            wasm_u32 subopcode{};
            if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, subopcode)) [[unlikely]] { return false; }

            auto const valid_instruction{shared_simd::visit_wasm1p1_simd_instruction(
                static_cast<shared_simd::wasm1p1_simd_details::simd_code>(subopcode),
                [&]<shared_simd::wasm1p1_simd_details::simd_code Op,
                    shared_simd::wasm1p1_simd_instruction_kind Kind,
                    shared_simd::wasm1p1_simd_scalar_kind ScalarKind,
                    ::std::size_t LaneCount,
                    ::std::uint_least32_t MaxAlign>() constexpr noexcept -> bool
                {
                    static_cast<void>(Op);
                    static_cast<void>(ScalarKind);
                    static_cast<void>(MaxAlign);
                    if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_load ||
                                 Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_store)
                    {
                        if(!skip_wasm_memarg_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                        if constexpr(LaneCount != 0uz)
                        {
                            return skip_wasm_fixed_immediate_for_callee_scan(cursor, code_end, 1uz);
                        }
                    }
                    else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::constant ||
                                      Kind == shared_simd::wasm1p1_simd_instruction_kind::shuffle)
                    {
                        return skip_wasm_fixed_immediate_for_callee_scan(cursor, code_end, 16uz);
                    }
                    else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::extract_lane ||
                                      Kind == shared_simd::wasm1p1_simd_instruction_kind::replace_lane)
                    {
                        return skip_wasm_fixed_immediate_for_callee_scan(cursor, code_end, 1uz);
                    }
                    return true;
                })};
            if(!valid_instruction) [[unlikely]] { return false; }

            // simd_prefix subopcode immediates ... code_end
            // [             safe             ] unsafe (could be code_end)
            //               ^^ code_curr: the selected SIMD decoder bounded cursor.
            code_curr = cursor;

            // simd_prefix subopcode immediates ...
            // [             safe             ] unsafe (could be the section_end)
            //                                  ^^ code_curr
            return true;
        }

        // Advance over exactly one raw Wasm instruction while scanning a complete function body. The outer cursor is
        // committed only after the opcode and all of its immediates have been decoded; malformed or unknown opcodes leave
        // it at the instruction start so a caller can fail closed without interpreting immediate bytes as opcodes.
        [[nodiscard]] inline constexpr bool skip_wasm_instruction_for_direct_call_scan(
            runtime_module_storage_t const& curr_module,
            ::std::byte const*& code_curr,
            ::std::byte const* code_end,
            parser_feature_parameter_t const* validator_feature_parameter) noexcept
        {
            // instruction opcode immediates ...
            // unsafe (could be the section_end)
            // ^^ code_curr

            if(code_curr == code_end) [[unlikely]] { return false; }

            using wasm1_code = all_details::wasm1_code;
            using wasm1p1_code = all_details::wasm1p1_code;
            using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;

            auto cursor{code_curr};
            auto const opcode{::std::to_integer<::std::uint_least8_t>(*cursor)};
            // [opcode byte] next bytes ... | end
            // [safe consumed bytes]       | one-past is never dereferenced here
            // ^^ cursor: code_curr != code_end proves one byte; this increment does not dereference, and later immediate reads are bounded.
            ++cursor;

            auto const commit{[&]() constexpr noexcept
                              {
                                  // decoded instruction ... code_end
                                  // [safe consumed bytes] unsafe (could be code_end)
                                  // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
                                  code_curr = cursor;

                                  // instruction opcode immediates ...
                                  // [           safe            ] unsafe (could be the section_end)
                                  //                               ^^ code_curr
                                  return true;
                              }};

            if(opcode >= static_cast<::std::uint_least8_t>(wasm1_code::i32_load) &&
               opcode <= static_cast<::std::uint_least8_t>(wasm1_code::i64_store32))
            {
                if(!skip_wasm_memarg_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                return commit();
            }

            // MVP scalar comparisons, numeric operators, conversions, and reinterpretations are one-byte instructions.
            if(opcode >= static_cast<::std::uint_least8_t>(wasm1_code::i32_eqz) &&
               opcode <= static_cast<::std::uint_least8_t>(wasm1_code::f64_reinterpret_i64))
            {
                return commit();
            }

            // The sign-extension proposal adds a contiguous no-immediate range immediately after MVP numeric opcodes.
            if(opcode >= static_cast<::std::uint_least8_t>(wasm1p1_code::i32_extend8_s) &&
               opcode <= static_cast<::std::uint_least8_t>(wasm1p1_code::i64_extend32_s))
            {
                return commit();
            }

            switch(opcode)
            {
                // throw_ref has no immediate. This raw boundary scan does not validate reachability,
                // operands or features; whole-function compilation performs all three checks.
                case 0x0au: return commit();
                case static_cast<::std::uint_least8_t>(wasm1_code::unreachable):
                case static_cast<::std::uint_least8_t>(wasm1_code::nop):
                case static_cast<::std::uint_least8_t>(wasm1_code::else_):
                case static_cast<::std::uint_least8_t>(wasm1_code::end):
                case static_cast<::std::uint_least8_t>(wasm1_code::return_):
                case static_cast<::std::uint_least8_t>(wasm1_code::drop):
                case static_cast<::std::uint_least8_t>(wasm1_code::select):
                case 0xd3u: // ref.eq has no immediate; whole-function validation enforces the GC gate.
                case 0xd4u: // ref.as_non_null has no immediate.
                case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_is_null): return commit();
                case static_cast<::std::uint_least8_t>(wasm1_code::block):
                case static_cast<::std::uint_least8_t>(wasm1_code::loop):
                case static_cast<::std::uint_least8_t>(wasm1_code::if_):
                    if(!skip_wasm_blocktype_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                    return commit();
                case 0x1fu: // Core 3 try_table: blocktype followed by the catch vector.
                {
                    // [try_table] blocktype catches ... | code_end
                    // [safe     ] unsafe (possibly one-past)
                    //             ^^ cursor: opcode was proved above; outer code_curr remains at it.
                    // Boundary discovery neither resolves outer labels/tags nor accepts a
                    // function. The shared fused validator performs those checks exactly once.
                    if(!skip_wasm_blocktype_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                    // [try_table blocktype] catches ... | code_end
                    // [safe               ] unsafe (possibly one-past)
                    //                       ^^ cursor: blocktype decoder committed only its bounded immediate.
                    auto const catches{::uwvm2::validation::standard::wasm3::scan_exception_catches(cursor, code_end)};
                    if(catches.error != ::uwvm2::validation::standard::wasm3::exception_immediate_error::ok) [[unlikely]]
                    { return false; }
                    // [try_table complete bounded immediate] next ... | code_end
                    // [safe                                 ] unsafe (possibly one-past)
                    //                                         ^^ cursor; commit advances outer code_curr only now.
                    return commit();
                }
                case 0x08u: // Core 3 throw: one bounded u32 tag index, not legacy try/catch.
                case static_cast<::std::uint_least8_t>(wasm1_code::br):
                case static_cast<::std::uint_least8_t>(wasm1_code::br_if):
                case 0x12u: // return_call has the same bounded u32 immediate
                case 0x14u: // call_ref type index
                case 0x15u: // return_call_ref type index
                case 0xd5u: // br_on_null label index
                case 0xd6u: // br_on_non_null label index
                case static_cast<::std::uint_least8_t>(wasm1_code::call):
                case static_cast<::std::uint_least8_t>(wasm1_code::local_get):
                case static_cast<::std::uint_least8_t>(wasm1_code::local_set):
                case static_cast<::std::uint_least8_t>(wasm1_code::local_tee):
                case static_cast<::std::uint_least8_t>(wasm1_code::global_get):
                case static_cast<::std::uint_least8_t>(wasm1_code::global_set):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::table_get):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::table_set):
                case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_func):
                {
                    wasm_u32 index{};
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, index)) [[unlikely]] { return false; }
                    return commit();
                }
                case 0x13u:
                case static_cast<::std::uint_least8_t>(wasm1_code::call_indirect):
                {
                    // call_indirect type_index table_index ...
                    // [    safe   ] unsafe (could be the section_end)
                    //               ^^ cursor

                    wasm_u32 type_index{};
                    wasm_u32 table_index{};
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, type_index) ||
                       !(opcode == 0x13u ? all_details::parse_wasm_leb128_immediate(cursor, code_end, table_index) :
                         parse_call_indirect_table_index_for_scan(cursor, code_end, validator_feature_parameter, table_index))) [[unlikely]]
                    {
                        return false;
                    }

                    // call_indirect type_index table_index ...
                    // [                safe              ] unsafe (could be the section_end)
                    //                                      ^^ cursor
                    return commit();
                }
                case static_cast<::std::uint_least8_t>(wasm1_code::br_table):
                {
                    wasm_u32 target_count{};
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, target_count)) [[unlikely]] { return false; }

                    auto const target_count_uz{static_cast<::std::size_t>(target_count)};
                    auto const remaining_bytes{static_cast<::std::size_t>(code_end - cursor)};
                    if(static_cast<wasm_u32>(target_count_uz) != target_count || target_count_uz >= remaining_bytes) [[unlikely]]
                    {
                        // Each target and the mandatory default label need at least one byte. This guard also makes
                        // `target_count + 1` safe in size_t before the bounded decode loop below.
                        return false;
                    }

                    auto const label_count{target_count_uz + 1uz};
                    for(::std::size_t i{}; i != label_count; ++i)
                    {
                        wasm_u32 label_index{};
                        if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, label_index)) [[unlikely]] { return false; }
                    }
                    return commit();
                }
                case static_cast<::std::uint_least8_t>(wasm1_code::memory_size):
                case static_cast<::std::uint_least8_t>(wasm1_code::memory_grow):
                    if(!parse_wasm_memory_index_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                    return commit();
                case static_cast<::std::uint_least8_t>(wasm1_code::i32_const):
                {
                    ::std::int_least32_t immediate{};
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, immediate)) [[unlikely]] { return false; }
                    return commit();
                }
                case static_cast<::std::uint_least8_t>(wasm1_code::i64_const):
                {
                    ::std::int_least64_t immediate{};
                    if(!all_details::parse_wasm_leb128_immediate(cursor, code_end, immediate)) [[unlikely]] { return false; }
                    return commit();
                }
                case static_cast<::std::uint_least8_t>(wasm1_code::f32_const):
                {
                    ::std::uint_least32_t immediate{};
                    if(!all_details::parse_wasm_little_endian_immediate(cursor, code_end, immediate)) [[unlikely]] { return false; }
                    return commit();
                }
                case static_cast<::std::uint_least8_t>(wasm1_code::f64_const):
                {
                    ::std::uint_least64_t immediate{};
                    if(!all_details::parse_wasm_little_endian_immediate(cursor, code_end, immediate)) [[unlikely]] { return false; }
                    return commit();
                }
                case static_cast<::std::uint_least8_t>(wasm1p1_code::select_t):
                    if(!skip_wasm_typed_select_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                    return commit();
                case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_null):
                {
                    parser_feature_parameter_t const default_features{};
                    auto const& effective_features{validator_feature_parameter == nullptr ?
                        default_features : *validator_feature_parameter};
                    auto const& wasm1p1_para{
                        ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(effective_features)};
                    // This is a boundary-only pre-scan. With no feature record, defer feature
                    // rejection to whole-function validation while still decoding the full heap.
                    auto const gc_enabled{validator_feature_parameter == nullptr || !wasm1p1_para.disable_gc};
                    auto const function_references_enabled{
                        validator_feature_parameter == nullptr || !wasm1p1_para.disable_function_references};
                    auto const exceptions_enabled{
                        validator_feature_parameter == nullptr || !wasm1p1_para.disable_exceptions};
                    auto const* const type_begin{curr_module.type_section_storage.type_section_begin};
                    auto const* const type_end{curr_module.type_section_storage.type_section_end};
                    if((type_begin == nullptr) != (type_end == nullptr)) [[unlikely]] { return false; }
                    // Both non-null endpoints borrow one initializer-owned contiguous type allocation.
                    // [type_begin ... type_end) is stable during this synchronous pre-scan; no pointer moves.
                    auto const type_count{type_begin == nullptr ? 0uz : static_cast<::std::size_t>(type_end - type_begin)};
                    auto const* const retained_context{curr_module.type_section_storage.core3_context_ptr};
                    if(retained_context != nullptr || (type_count == 0uz && gc_enabled))
                    {
                        ::uwvm2::validation::standard::wasm3::recursive_type_context const empty_context{};
                        auto const& context{retained_context == nullptr ? empty_context : *retained_context};
                        // [ref.null][signed-33 heap immediate] next ... code_end
                        // [safe   ] unsafe (possibly code_end)
                        //          ^^ cursor: the Core 3 decoder checks every byte and feature before committing.
                        auto const decoded{::uwvm2::validation::standard::wasm3::scan_core3_ref_null_heap(
                            cursor, code_end, context, gc_enabled,
                            function_references_enabled, exceptions_enabled)};
                        if(decoded.error != ::uwvm2::validation::standard::wasm3::core3_ref_null_error::ok) [[unlikely]]
                        { return false; }
                    }
                    else
                    {
                        // Legacy type storage has function-only indices. This bounded decoder rejects GC heaps.
                        // [ref.null][signed-33 heap immediate] next ... code_end
                        // [safe   ] unsafe (possibly code_end)
                        //          ^^ cursor: the decoder commits only a complete enabled function/exn heap.
                        auto const decoded{::uwvm2::validation::standard::wasm3::scan_function_ref_null_heap(
                            cursor, code_end, function_references_enabled, type_count, exceptions_enabled, gc_enabled)};
                        if(decoded.error != ::uwvm2::validation::standard::wasm3::function_heap_immediate_error::ok) [[unlikely]]
                        { return false; }
                    }
                    // [ref.null][checked complete heap] next ... code_end
                    // [safe                          ] unsafe (possibly code_end)
                    //                                 ^^ cursor: commit publishes only the bounded advance.
                    return commit();
                }
                case static_cast<::std::uint_least8_t>(wasm1p1_code::numeric_prefix):
                    if(!skip_wasm_numeric_prefix_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                    return commit();
                case static_cast<::std::uint_least8_t>(wasm1p1_code::atomic_prefix):
                    // [FE] subopcode memarg/reserved ... (code_end)
                    // [safe] unsafe (could be code_end)
                    //        ^^ cursor; the decoder checks every consumed byte.
                    if(::uwvm2::validation::standard::wasm3::scan_atomic_instruction64(cursor, code_end, true).error !=
                       ::uwvm2::validation::standard::wasm3::atomic_immediate_error::ok) [[unlikely]] { return false; }
                    // [FE complete immediate] ... (code_end)
                    // [safe                 ] unsafe (could be code_end)
                    //                         ^^ cursor; commit publishes the bounded advance.
                    return commit();
                case 0xfbu:
                    // [FB] subopcode/immediate ... code_end
                    // [safe] unsafe (could be code_end)
                    //        ^^ cursor: the caller consumed the checked prefix.
                    if(::uwvm2::validation::standard::wasm3::scan_gc_instruction(cursor, code_end).error !=
                       ::uwvm2::validation::standard::wasm3::gc_immediate_error::ok) [[unlikely]] { return false; }
                    // [FB][checked complete immediate] next ... code_end
                    // [safe                          ] unsafe (could be code_end)
                    //                                 ^^ cursor: commit sees the bounded advancement.
                    return commit();
                case static_cast<::std::uint_least8_t>(wasm1p1_code::simd_prefix):
                    if(!skip_wasm_simd_prefix_for_callee_scan(cursor, code_end)) [[unlikely]] { return false; }
                    return commit();
                [[unlikely]] default: return false;
            }
        }

        // Appends a local function index while preserving first-seen order and avoiding duplicates.
        inline constexpr void append_unique_local_function_index(::uwvm2::utils::container::vector<::std::size_t>& out,
                                                                 ::std::size_t local_function_index) noexcept
        {
            if(::std::find(out.begin(), out.end(), local_function_index) == out.end()) { out.push_back(local_function_index); }
        }

        #include "checked_lazy_ir_plan.h"

        // Cold dependency queries consume DATA recorded by the original typed
        // instruction checks; neither query decodes the body/immediates again.
        [[nodiscard]] inline bool collect_checked_defined_callees(runtime_module_storage_t const& module,
            lazy_module_storage_t const& storage, ::std::size_t index,
            ::uwvm2::utils::container::vector<::std::size_t>& callees, bool unwind)
        {
            callees.clear();
            return storage.checked_ir_plan != nullptr && storage.checked_ir_plan->append_callees(module, index, callees, unwind);
        }

        // Verifies and lightly optimizes a lazy LLVM module before MCJIT takes ownership of it.
        [[nodiscard]] inline constexpr bool optimize_lazy_llvm_jit_module(::llvm::Module& module,
                                                                          ::llvm::TargetMachine& target_machine,
                                                                          ::llvm::CodeGenOptLevel codegen_opt_level,
                                                                          bool verify_llvm_jit_ir) noexcept
        {
            // Lazy materialization is another code-generation entry, not exempt
            // from strict rounding, native-NaN repair or the i386 private FP-bit ABI.
            // Run the same module-level lowering used by eager/raw-wrapper code.
            ::uwvm2::runtime::compiler::shared::strict_float_jit::lower(module);
            if(!all_details::verify_llvm_jit_module(module, verify_llvm_jit_ir)) [[unlikely]] { return false; }

            if(codegen_opt_level == ::llvm::CodeGenOptLevel::None)
            {
                all_details::legalize_llvm_jit_o0_vector_phis(module);
                all_details::legalize_llvm_jit_native_vectors(module);
                return all_details::verify_llvm_jit_module(module, verify_llvm_jit_ir);
            }

            ::llvm::legacy::FunctionPassManager function_pass_manager(::std::addressof(module));
            function_pass_manager.add(::llvm::createTargetTransformInfoWrapperPass(target_machine.getTargetIRAnalysis()));
            function_pass_manager.add(::llvm::createPromoteMemoryToRegisterPass());
            function_pass_manager.add(::llvm::createEarlyCSEPass());
            function_pass_manager.add(::llvm::createCFGSimplificationPass());
            function_pass_manager.add(::llvm::createDeadCodeEliminationPass());

            function_pass_manager.doInitialization();
            for(auto& function: module)
            {
                if(function.isDeclaration()) { continue; }
                function_pass_manager.run(function);
            }
            function_pass_manager.doFinalization();

            all_details::legalize_llvm_jit_native_vectors(module);
            return all_details::verify_llvm_jit_module(module, verify_llvm_jit_ir);
        }

        // Resolves an emitted LLVM function symbol to a native address, with a fallback for older MCJIT paths that need
        // an IR Function pointer before materializing the address.
        [[nodiscard]] inline constexpr ::std::uintptr_t resolve_llvm_function_address(::llvm::ExecutionEngine& engine,
                                                                                      ::uwvm2::utils::container::u8string const& function_name) noexcept
        {
            llvm_jit_function_address_name_t function_address_name{function_name.data(), function_name.data() + function_name.size()};
            auto const direct_function_address{engine.getFunctionAddress(function_address_name)};
            if(direct_function_address != 0u) [[likely]] { return static_cast<::std::uintptr_t>(direct_function_address); }

            auto found_function{engine.FindFunctionNamed(all_details::get_llvm_string_ref(function_name))};
            if(found_function == nullptr || found_function->isDeclaration()) [[unlikely]] { return 0u; }

            auto const function_address{engine.getPointerToFunction(found_function)};
            return function_address == nullptr ? 0u : reinterpret_cast<::std::uintptr_t>(function_address);
        }

        // Takes a single-function LLVM IR module, creates an MCJIT engine, resolves all public/raw entry points, and
        // stores the owning LLVM objects in the materialized function record.
        [[nodiscard]] inline constexpr bool materialize_lazy_local_function(runtime_module_storage_t const& curr_module,
                                                                            lazy_module_storage_t& storage,
                                                                            lazy_compile_options const& options,
                                                                            ::std::size_t local_function_index,
                                                                            llvm_jit_module_storage_t& llvm_ir_storage) noexcept
        {
            if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }
            auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
            // Closing the ready flag does not publish data, so relaxed is sufficient.  The owning compile claim prevents
            // another writer from rebuilding this record concurrently; readers that see `false` return before touching
            // the ordinary payload fields.
            store_lazy_materialized_ready(materialized, false, ::std::memory_order_relaxed);
            materialized.entry_address = 0u;
            materialized.raw_entry_address = 0u;
            materialized.tiered_core_entry_address = 0u;
            materialized.tiered_loop_reentries.clear();
            materialized.tiered_loop_reentry_raw_entry_addresses.clear();
            materialized.llvm_jit_engine.reset();
            materialized.llvm_context_holder.reset();

            // The emitter hands over a context/module pair.  Materialization consumes both and clears `llvm_ir_storage`
            // so failed callers cannot accidentally reuse moved LLVM objects.
            if(!llvm_ir_storage.emitted || llvm_ir_storage.llvm_context_holder == nullptr || llvm_ir_storage.llvm_module == nullptr) [[unlikely]]
            {
                return false;
            }
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]] { return false; }

            auto llvm_context_holder{::std::move(llvm_ir_storage.llvm_context_holder)};
            auto llvm_module{::std::move(llvm_ir_storage.llvm_module)};
            llvm_ir_storage.emitted = false;

            if(llvm_context_holder == nullptr || llvm_module == nullptr) [[unlikely]] { return false; }

            // Match the eager JIT target setup but keep the target machine temporary; MCJIT takes ownership through
            // ExecutionEngine creation below.
            auto const& target_config{get_llvm_jit_native_target_config()};
            ::llvm::SmallVector<::llvm::StringRef, 16> host_target_attributes{};
            append_llvm_jit_host_target_attribute_refs(target_config.feature_storage, host_target_attributes);

            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT)
                .setOptLevel(options.codegen_opt_level)
                .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                .setMAttrs(host_target_attributes);

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{select_llvm_jit_target(target_builder, target_config)};
            if(target_machine == nullptr) [[unlikely]] { return false; }
            if(options.codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target_machine->setFastISel(true); }

            set_llvm_module_target_triple_from_machine(*llvm_module, *target_machine);
            llvm_module->setDataLayout(target_machine->createDataLayout());
            apply_llvm_jit_native_target_function_attrs(*llvm_module, target_config, *target_machine);
            if(!optimize_lazy_llvm_jit_module(*llvm_module, *target_machine, options.codegen_opt_level, options.compile_options.verify_llvm_jit_ir))
                [[unlikely]]
            {
                return false;
            }

            ::uwvm2::utils::container::u8string llvm_jit_cache_key{};
            {
                llvm_jit_cache_key = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"lazy-single");
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module", curr_module.module_name);
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(llvm_jit_cache_key,
                                                                                      u8"local-fn",
                                                                                      static_cast<::std::uint_least64_t>(local_function_index));
                auto wasm_code_hash{storage.checked_ir_plan->artifact_hash(local_function_index)};
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"checked-ir-hash", wasm_code_hash);
            }
            ::uwvm2::utils::container::u8string llvm_jit_cache_codegen_policy{};
            {
                llvm_jit_cache_codegen_policy = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"codegen-policy");
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy, u8"cache-unit", u8"lazy-single");
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(llvm_jit_cache_codegen_policy,
                                                                                      u8"codegen-opt-level",
                                                                                      static_cast<::std::uint_least64_t>(options.codegen_opt_level));
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy,
                                                                                  u8"validation-mode",
                                                                                  lazy_validation_mode_name(options.validation_mode));
                append_llvm_jit_native_target_codegen_policy(llvm_jit_cache_codegen_policy, target_config);
                append_lazy_llvm_jit_cache_shape_key(llvm_jit_cache_codegen_policy, options.compile_options);
            }
            auto llvm_jit_cache_context{::uwvm2::runtime::llvm_jit_cache::default_cache_context(
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_key.data(), llvm_jit_cache_key.size()},
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_codegen_policy.data(), llvm_jit_cache_codegen_policy.size()},
                *target_machine)};
            // The source-level lazy key intentionally stays compact, but it does not encode imported globals,
            // memories/tables, proposal metadata, or every declaration copied into this compilation unit. Let the
            // ObjectCache append the complete emitted-module bitcode hash before accepting a persistent object.
            llvm_jit_cache_context.cache_key_is_complete = false;
            ::uwvm2::runtime::llvm_jit_cache::llvm_jit_object_cache llvm_jit_object_cache{::std::move(llvm_jit_cache_context),
                                                                                          lazy_llvm_jit_object_cache_policy()};

            ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declarations native_exception_imports{};
            ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::catch_runtime native_exception_catch{};
            if constexpr(::uwvm2::runtime::lib::details::native_exception_host::available)
            {
                if(::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::supports_itanium_dwarf_object(*target_machine))
                {
                    native_exception_imports =
                        ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declare_itanium_dwarf_symbols(*llvm_module, *target_machine);
                    native_exception_catch =
                        ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::declare_catch_runtime(*llvm_module);
                    if(native_exception_imports.status != ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::error::ok ||
                       !native_exception_catch) [[unlikely]] { return false; }
                }
            }
            if(native_exception_imports.type_info == nullptr &&
               llvm_module->getNamedValue(::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::type_info_symbol) != nullptr)
            { return false; }

            auto memory_manager{
                ::uwvm2::utils::container::make_delete_owned<
                    ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()};
            auto const memory_manager_observer{memory_manager.get()};
            auto raw_engine{
                ::llvm::EngineBuilder(details::llvm_module_owner_t{llvm_module.release()})
                    .setEngineKind(::llvm::EngineKind::JIT)
                    .setOptLevel(options.codegen_opt_level)
                    .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                    .setMAttrs(host_target_attributes)
                    .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{memory_manager.release()})
                    .create(target_machine.release())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};
            // The engine now owns the module without moving its declarations. Bind all five
            // host ABI symbols transactionally before cached/fresh objects can be finalized.
            if(native_exception_imports.type_info != nullptr &&
               !::uwvm2::runtime::lib::details::native_exception_host::bind<::uwvm2::runtime::exception::guest_exception>(
                   *engine, native_exception_imports, native_exception_catch,
                   +[]() UWVM_THROWS
                   {
                       return ::uwvm2::runtime::exception::guest_exception{
                           ::uwvm2::runtime::exception::value::make(::std::make_shared<unsigned char const>(0), {})};
                   })) [[unlikely]] { return false; }


            engine->setObjectCache(::std::addressof(llvm_jit_object_cache));
            if(options.jit_event_listener != nullptr)
            {
                // The runtime listener records executable ranges; MCJIT registers unwind metadata independently.
                engine->RegisterJITEventListener(options.jit_event_listener);
            }
            engine->finalizeObject();
            if(engine->hasError() || memory_manager_observer->has_finalization_failure()) [[unlikely]]
            {
                engine->setObjectCache(nullptr);
                return false;
            }
            engine->setObjectCache(nullptr);

            auto const import_func_count{curr_module.imported_function_vec_storage.size()};
            auto const function_index{import_func_count + local_function_index};
            using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;
            if(function_index > static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())) [[unlikely]] { return false; }

            auto const function_index_u32{static_cast<wasm_u32>(function_index)};
            auto const function_name{all_details::get_llvm_wasm_function_name(curr_module, function_index_u32)};
            auto const raw_function_name{all_details::get_llvm_wasm_raw_function_name(curr_module, function_index_u32)};
            auto const& runtime_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
            if(runtime_func.function_type_ptr == nullptr) [[unlikely]] { return false; }
            auto const typed_entry_required{all_details::is_runtime_wasm_function_type_llvm_typed_entry_abi_supported(*runtime_func.function_type_ptr)};
            auto const entry_address{typed_entry_required ? resolve_llvm_function_address(*engine, function_name) : 0u};
            auto const raw_entry_address{resolve_llvm_function_address(*engine, raw_function_name)};
            if(raw_entry_address == 0u || (typed_entry_required && entry_address == 0u)) [[unlikely]] { return false; }

            // Reentry wrapper symbols are generated only when the local-function translator discovered tiered loop
            // reentry points.  Preserve descriptor/address index alignment for later lookup by Wasm offset.
            materialized.tiered_loop_reentries = materialized.local_func.tiered_loop_reentries;
            materialized.tiered_loop_reentry_raw_entry_addresses.clear();
            materialized.tiered_loop_reentry_raw_entry_addresses.reserve(materialized.tiered_loop_reentries.size());
            for(auto const& reentry: materialized.tiered_loop_reentries)
            {
                auto const reentry_function_name{
                    all_details::get_llvm_wasm_tiered_loop_reentry_raw_function_name(curr_module, function_index_u32, reentry.wasm_code_offset)};
                auto const reentry_address{resolve_llvm_function_address(*engine, reentry_function_name)};
                if(reentry_address == 0u) [[unlikely]] { return false; }
                materialized.tiered_loop_reentry_raw_entry_addresses.push_back(reentry_address);
            }

            ::std::uintptr_t core_address{};
            if(typed_entry_required && options.compile_options.emit_tiered_loop_reentry_entries)
            {
                auto const core_name{all_details::get_llvm_wasm_tiered_core_function_name(curr_module, function_index_u32)};
                core_address = resolve_llvm_function_address(*engine, core_name);
                if(core_address == 0u) [[unlikely]] { return false; }
            }
            materialized.entry_address = entry_address;
            materialized.raw_entry_address = raw_entry_address;
            materialized.tiered_core_entry_address = core_address;
            materialized.llvm_context_holder = ::std::move(llvm_context_holder);
            materialized.llvm_jit_engine = ::std::move(engine);
            // The internal single-function path obeys the same preparation
            // contract as grouped materialization, including diagnostic maps.
            ::uwvm2::utils::container::vector<::std::size_t> published_indices{};
            published_indices.push_back(local_function_index);
            publish_lazy_materialized_group_after_prepare(storage, options, published_indices);
            return true;
        }

        // Materializes a group of local functions from one LLVM IR module.  All functions share one LLVMContext and one
        // MCJIT engine; the first materialized record owns those objects and therefore owns the native code lifetime for
        // the whole group.
        [[nodiscard]] inline constexpr bool
            materialize_lazy_local_function_group(runtime_module_storage_t const& curr_module,
                                                  lazy_module_storage_t& storage,
                                                  lazy_compile_options const& options,
                                                  ::uwvm2::utils::container::vector<::std::size_t> const& local_function_indices,
                                                  llvm_jit_module_storage_t& llvm_ir_storage) noexcept
        {
            if(local_function_indices.empty()) [[unlikely]] { return false; }

            for(auto const local_function_index: local_function_indices)
            {
                if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }
                auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
                // Reset each candidate before resolving the shared object.  This is only a fast-path gate for readers, so
                // relaxed is enough for the false transition.
                store_lazy_materialized_ready(materialized, false, ::std::memory_order_relaxed);
                materialized.entry_address = 0u;
                materialized.raw_entry_address = 0u;
                materialized.tiered_core_entry_address = 0u;
                materialized.tiered_loop_reentries.clear();
                materialized.tiered_loop_reentry_raw_entry_addresses.clear();
                materialized.llvm_jit_engine.reset();
                materialized.llvm_context_holder.reset();
            }

            // Consume the generated IR module exactly once; the resulting ExecutionEngine owns the compiled code.
            if(!llvm_ir_storage.emitted || llvm_ir_storage.llvm_context_holder == nullptr || llvm_ir_storage.llvm_module == nullptr) [[unlikely]]
            {
                return false;
            }
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]] { return false; }

            auto llvm_context_holder{::std::move(llvm_ir_storage.llvm_context_holder)};
            auto llvm_module{::std::move(llvm_ir_storage.llvm_module)};
            llvm_ir_storage.emitted = false;

            if(llvm_context_holder == nullptr || llvm_module == nullptr) [[unlikely]] { return false; }

            // Select and apply the native target before verification/optimization so data-layout-sensitive passes see
            // the same target description used by MCJIT.
            auto const& target_config{get_llvm_jit_native_target_config()};
            ::llvm::SmallVector<::llvm::StringRef, 16> host_target_attributes{};
            append_llvm_jit_host_target_attribute_refs(target_config.feature_storage, host_target_attributes);

            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT)
                .setOptLevel(options.codegen_opt_level)
                .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                .setMAttrs(host_target_attributes);

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{select_llvm_jit_target(target_builder, target_config)};
            if(target_machine == nullptr) [[unlikely]] { return false; }
            if(options.codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target_machine->setFastISel(true); }

            set_llvm_module_target_triple_from_machine(*llvm_module, *target_machine);
            llvm_module->setDataLayout(target_machine->createDataLayout());
            apply_llvm_jit_native_target_function_attrs(*llvm_module, target_config, *target_machine);
            if(!optimize_lazy_llvm_jit_module(*llvm_module, *target_machine, options.codegen_opt_level, options.compile_options.verify_llvm_jit_ir))
                [[unlikely]]
            {
                return false;
            }

            ::uwvm2::utils::container::u8string llvm_jit_cache_key{};
            {
                auto const single_cache_unit{local_function_indices.size() == 1uz};
                if(single_cache_unit)
                {
                    auto const local_function_index{local_function_indices.index_unchecked(0uz)};
                    llvm_jit_cache_key = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"lazy-single");
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module", curr_module.module_name);
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(llvm_jit_cache_key,
                                                                                          u8"local-fn",
                                                                                          static_cast<::std::uint_least64_t>(local_function_index));
                    auto wasm_code_hash{storage.checked_ir_plan->artifact_hash(local_function_index)};
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"checked-ir-hash", wasm_code_hash);
                }
                else
                {
                    llvm_jit_cache_key = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"lazy-group");
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module", curr_module.module_name);
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(llvm_jit_cache_key,
                                                                                          u8"local-fn-count",
                                                                                          static_cast<::std::uint_least64_t>(local_function_indices.size()));
                    for(auto const local_function_index: local_function_indices)
                    {
                        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(llvm_jit_cache_key,
                                                                                              u8"local-fn",
                                                                                              static_cast<::std::uint_least64_t>(local_function_index));
                        auto wasm_code_hash{storage.checked_ir_plan->artifact_hash(local_function_index)};
                        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"checked-ir-hash", wasm_code_hash);
                    }
                }
            }
            ::uwvm2::utils::container::u8string llvm_jit_cache_codegen_policy{};
            {
                auto const single_cache_unit{local_function_indices.size() == 1uz};
                llvm_jit_cache_codegen_policy = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"codegen-policy");
                if(single_cache_unit)
                {
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy, u8"cache-unit", u8"lazy-single");
                }
                else
                {
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy, u8"cache-unit", u8"lazy-group");
                }
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(llvm_jit_cache_codegen_policy,
                                                                                      u8"codegen-opt-level",
                                                                                      static_cast<::std::uint_least64_t>(options.codegen_opt_level));
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_codegen_policy,
                                                                                  u8"validation-mode",
                                                                                  lazy_validation_mode_name(options.validation_mode));
                append_llvm_jit_native_target_codegen_policy(llvm_jit_cache_codegen_policy, target_config);
                append_lazy_llvm_jit_cache_shape_key(llvm_jit_cache_codegen_policy, options.compile_options);
            }
            auto llvm_jit_cache_context{::uwvm2::runtime::llvm_jit_cache::default_cache_context(
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_key.data(), llvm_jit_cache_key.size()},
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_codegen_policy.data(), llvm_jit_cache_codegen_policy.size()},
                *target_machine)};
            // Keep the coroutine materialization path identical to the synchronous path: the surrounding key is only
            // a namespace hint, while the ObjectCache's bitcode hash is the authoritative native-code identity.
            llvm_jit_cache_context.cache_key_is_complete = false;
            ::uwvm2::runtime::llvm_jit_cache::llvm_jit_object_cache llvm_jit_object_cache{::std::move(llvm_jit_cache_context),
                                                                                          lazy_llvm_jit_object_cache_policy()};

            ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declarations native_exception_imports{};
            ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::catch_runtime native_exception_catch{};
            if constexpr(::uwvm2::runtime::lib::details::native_exception_host::available)
            {
                if(::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::supports_itanium_dwarf_object(*target_machine))
                {
                    native_exception_imports =
                        ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declare_itanium_dwarf_symbols(*llvm_module, *target_machine);
                    native_exception_catch =
                        ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::declare_catch_runtime(*llvm_module);
                    if(native_exception_imports.status != ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::error::ok ||
                       !native_exception_catch) [[unlikely]] { return false; }
                }
            }
            if(native_exception_imports.type_info == nullptr &&
               llvm_module->getNamedValue(::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::type_info_symbol) != nullptr)
            { return false; }

            auto memory_manager{
                ::uwvm2::utils::container::make_delete_owned<
                    ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()};
            auto const memory_manager_observer{memory_manager.get()};
            auto raw_engine{
                ::llvm::EngineBuilder(details::llvm_module_owner_t{llvm_module.release()})
                    .setEngineKind(::llvm::EngineKind::JIT)
                    .setOptLevel(options.codegen_opt_level)
                    .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                    .setMAttrs(host_target_attributes)
                    .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{memory_manager.release()})
                    .create(target_machine.release())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};
            // The engine now owns the module without moving its declarations. Bind all five
            // host ABI symbols transactionally before cached/fresh objects can be finalized.
            if(native_exception_imports.type_info != nullptr &&
               !::uwvm2::runtime::lib::details::native_exception_host::bind<::uwvm2::runtime::exception::guest_exception>(
                   *engine, native_exception_imports, native_exception_catch,
                   +[]() UWVM_THROWS
                   {
                       return ::uwvm2::runtime::exception::guest_exception{
                           ::uwvm2::runtime::exception::value::make(::std::make_shared<unsigned char const>(0), {})};
                   })) [[unlikely]] { return false; }


            engine->setObjectCache(::std::addressof(llvm_jit_object_cache));
            if(options.jit_event_listener != nullptr)
            {
                // Group materialization uses the same executable-range listener as single-function lazy compilation.
                engine->RegisterJITEventListener(options.jit_event_listener);
            }
            engine->finalizeObject();
            if(engine->hasError() || memory_manager_observer->has_finalization_failure()) [[unlikely]]
            {
                engine->setObjectCache(nullptr);
                return false;
            }
            engine->setObjectCache(nullptr);

            auto const import_func_count{curr_module.imported_function_vec_storage.size()};
            using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;
            for(auto const local_function_index: local_function_indices)
            {
                auto const function_index{import_func_count + local_function_index};
                if(function_index > static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())) [[unlikely]] { return false; }

                auto const function_index_u32{static_cast<wasm_u32>(function_index)};
                auto const function_name{all_details::get_llvm_wasm_function_name(curr_module, function_index_u32)};
                auto const raw_function_name{all_details::get_llvm_wasm_raw_function_name(curr_module, function_index_u32)};
                auto const& runtime_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
                if(runtime_func.function_type_ptr == nullptr) [[unlikely]] { return false; }
                auto const typed_entry_required{all_details::is_runtime_wasm_function_type_llvm_typed_entry_abi_supported(*runtime_func.function_type_ptr)};
                auto const entry_address{typed_entry_required ? resolve_llvm_function_address(*engine, function_name) : 0u};
                auto const raw_entry_address{resolve_llvm_function_address(*engine, raw_function_name)};
                if(raw_entry_address == 0u || (typed_entry_required && entry_address == 0u)) [[unlikely]] { return false; }

                auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
                materialized.tiered_loop_reentries = materialized.local_func.tiered_loop_reentries;
                materialized.tiered_loop_reentry_raw_entry_addresses.clear();
                materialized.tiered_loop_reentry_raw_entry_addresses.reserve(materialized.tiered_loop_reentries.size());
                for(auto const& reentry: materialized.tiered_loop_reentries)
                {
                    auto const reentry_function_name{
                        all_details::get_llvm_wasm_tiered_loop_reentry_raw_function_name(curr_module, function_index_u32, reentry.wasm_code_offset)};
                    auto const reentry_address{resolve_llvm_function_address(*engine, reentry_function_name)};
                    if(reentry_address == 0u) [[unlikely]] { return false; }
                    materialized.tiered_loop_reentry_raw_entry_addresses.push_back(reentry_address);
                }
                ::std::uintptr_t core_address{};
                if(typed_entry_required && options.compile_options.emit_tiered_loop_reentry_entries)
                {
                    auto const core_name{all_details::get_llvm_wasm_tiered_core_function_name(curr_module, function_index_u32)};
                    core_address = resolve_llvm_function_address(*engine, core_name);
                    if(core_address == 0u) [[unlikely]] { return false; }
                }
                materialized.entry_address = entry_address;
                materialized.raw_entry_address = raw_entry_address;
                materialized.tiered_core_entry_address = core_address;
            }

            // Store the shared LLVM owners on one record before publishing any member as ready.  Other records contain
            // only addresses into the same engine, so the owner record must remain alive for as long as any grouped
            // address can be called.
            auto const owner_local_function_index{local_function_indices.front_unchecked()};
            auto& owner{storage.materialized_functions.index_unchecked(owner_local_function_index)};
            owner.llvm_context_holder = ::std::move(llvm_context_holder);
            owner.llvm_jit_engine = ::std::move(engine);
            // Preparing the complete group must precede even its first ready
            // store: a newly visible entry can directly call another member.
            publish_lazy_materialized_group_after_prepare(storage, options, local_function_indices);
            return true;
        }

        // Returns the Wasm byte size used by the lazy grouping budget.
        [[nodiscard]] inline constexpr ::std::size_t lazy_group_function_code_size(lazy_module_storage_t const& storage,
                                                                                   ::std::size_t local_function_index) noexcept
        {
            if(local_function_index >= storage.functions.size()) [[unlikely]] { return 0uz; }
            auto const& fn{storage.functions.index_unchecked(local_function_index)};
            if(fn.primary_cu_index >= storage.compile_units.size()) [[unlikely]] { return 0uz; }
            return storage.compile_units.index_unchecked(fn.primary_cu_index).code_size;
        }

        // Checks whether a grouping candidate has already been selected.
        [[nodiscard]] inline constexpr bool lazy_group_contains_function(::uwvm2::utils::container::vector<::std::size_t> const& out,
                                                                         ::std::size_t local_function_index) noexcept
        { return ::std::find(out.begin(), out.end(), local_function_index) != out.end(); }

        // Only uncompiled or queued functions are safe to opportunistically include in a new group.
        [[nodiscard]] inline constexpr bool should_consider_lazy_group_function(lazy_module_storage_t const& storage,
                                                                                ::std::size_t local_function_index) noexcept
        {
            if(local_function_index >= storage.functions.size()) [[unlikely]] { return false; }
            // Acquire keeps this heuristic consistent with terminal-state publication.  A stale non-terminal read may
            // only skip an optimization candidate; a terminal read must observe the completed record if later reused.
            auto const st{storage.functions.index_unchecked(local_function_index).materialization_state.state.load(::std::memory_order_acquire)};
            return st == ::uwvm2::utils::thread::lazy_compile_state::uncompiled || st == ::uwvm2::utils::thread::lazy_compile_state::queued;
        }

        // Adds one candidate to the group if it fits the configured count and code-size budget.  `force` is used for the
        // demand-triggering function so it is always included even when it alone exceeds the heuristic budget.
        [[nodiscard]] inline constexpr bool try_append_lazy_group_function(::uwvm2::utils::container::vector<::std::size_t>& out,
                                                                           lazy_module_storage_t const& storage,
                                                                           ::std::size_t local_function_index,
                                                                           ::std::size_t function_budget,
                                                                           ::std::size_t code_size_budget,
                                                                           ::std::size_t& total_code_size,
                                                                           bool force) noexcept
        {
            if(local_function_index >= storage.functions.size()) [[unlikely]] { return false; }
            if(lazy_group_contains_function(out, local_function_index)) { return false; }

            auto const code_size{lazy_group_function_code_size(storage, local_function_index)};
            auto const accounted_code_size{code_size == 0uz ? 1uz : code_size};
            if(!force)
            {
                if(out.size() >= function_budget) { return false; }
                if(total_code_size >= code_size_budget) { return false; }
                if(accounted_code_size > code_size_budget - total_code_size) { return false; }
            }

            out.push_back(local_function_index);
            total_code_size += accounted_code_size;
            return true;
        }

        // Fills spare group budget with nearby local functions.  This improves locality for modules whose related
        // functions are laid out next to each other but do not form a direct-call chain.
        inline constexpr void append_lazy_group_adjacent_functions(lazy_module_storage_t& storage,
                                                                   ::std::size_t entry_local_function_index,
                                                                   ::std::size_t function_budget,
                                                                   ::std::size_t code_size_budget,
                                                                   ::std::size_t& total_code_size,
                                                                   ::uwvm2::utils::container::vector<::std::size_t>& out) noexcept
        {
            auto const local_count{storage.functions.size()};
            if(entry_local_function_index >= local_count) [[unlikely]] { return; }

            for(::std::size_t offset{1uz}; out.size() < function_budget && total_code_size < code_size_budget && offset < local_count; ++offset)
            {
                bool appended{};
                auto const forward_index{entry_local_function_index + offset};
                if(forward_index < local_count && should_consider_lazy_group_function(storage, forward_index))
                {
                    appended =
                        try_append_lazy_group_function(out, storage, forward_index, function_budget, code_size_budget, total_code_size, false) || appended;
                }

                if(out.size() == function_budget || total_code_size >= code_size_budget) { break; }

                if(entry_local_function_index >= offset && should_consider_lazy_group_function(storage, entry_local_function_index - offset))
                {
                    auto const backward_index{entry_local_function_index - offset};
                    appended =
                        try_append_lazy_group_function(out, storage, backward_index, function_budget, code_size_budget, total_code_size, false) || appended;
                }

                if(!appended && forward_index >= local_count && entry_local_function_index < offset) { break; }
            }
        }

        // Builds a small materialization group rooted at the demanded function.  Direct local callees are preferred, then
        // adjacent functions are added as a locality fallback.
        inline constexpr void collect_lazy_direct_call_group(runtime_module_storage_t const& curr_module,
                                                             lazy_module_storage_t& storage,
                                                             ::std::size_t entry_local_function_index,
                                                             ::uwvm2::utils::container::vector<::std::size_t>& out,
                                                             [[maybe_unused]] parser_feature_parameter_t const* validator_feature_parameter) noexcept
        {
            constexpr ::std::size_t group_function_budget{16uz};
            constexpr ::std::size_t group_code_size_budget{8uz * 1024uz};
            out.clear();
            if(entry_local_function_index >= storage.functions.size()) [[unlikely]] { return; }

            ::std::size_t total_code_size{};
            static_cast<void>(
                try_append_lazy_group_function(out, storage, entry_local_function_index, group_function_budget, group_code_size_budget, total_code_size, true));

            ::uwvm2::utils::container::vector<::std::size_t> callees{};
            for(::std::size_t cursor{}; cursor < out.size() && out.size() < group_function_budget && total_code_size < group_code_size_budget; ++cursor)
            {
                callees.clear();
                if(!collect_checked_defined_callees(curr_module, storage, out.index_unchecked(cursor), callees, false)) { continue; }

                for(auto remaining{callees.size()}; remaining != 0uz && out.size() < group_function_budget && total_code_size < group_code_size_budget;)
                {
                    --remaining;
                    auto const callee_local_index{callees.index_unchecked(remaining)};
                    if(!should_consider_lazy_group_function(storage, callee_local_index)) { continue; }
                    static_cast<void>(try_append_lazy_group_function(out,
                                                                     storage,
                                                                     callee_local_index,
                                                                     group_function_budget,
                                                                     group_code_size_budget,
                                                                     total_code_size,
                                                                     false));
                }
            }

            if(out.size() < group_function_budget && total_code_size < group_code_size_budget)
            {
                append_lazy_group_adjacent_functions(storage, entry_local_function_index, group_function_budget, group_code_size_budget, total_code_size, out);
            }
        }

        // In native-unwind call-stack mode, a Wasm call should correspond to a physical generated frame.  Build the
        // complete transitive graph rooted at the demanded entry, including current-module call_indirect table targets,
        // so one MCJIT object can avoid lazy raw-entry trampolines across Wasm-to-Wasm edges.
        inline constexpr void collect_lazy_unwind_direct_call_group(runtime_module_storage_t const& curr_module,
                                                                    lazy_module_storage_t& storage,
                                                                    ::std::size_t entry_local_function_index,
                                                                    ::uwvm2::utils::container::vector<::std::size_t>& out,
                                                                    parser_feature_parameter_t const* validator_feature_parameter) noexcept
        {
            out.clear();
            auto const local_count{curr_module.local_defined_function_vec_storage.size()};
            if(entry_local_function_index >= local_count || entry_local_function_index >= storage.functions.size()) [[unlikely]] { return; }

            ::uwvm2::utils::container::vector<bool> seen{};
            seen.resize(local_count);

            ::uwvm2::utils::container::vector<::std::size_t> stack{};
            stack.reserve(local_count);
            seen.index_unchecked(entry_local_function_index) = true;
            stack.push_back(entry_local_function_index);

            ::uwvm2::utils::container::vector<::std::size_t> callees{};
            while(!stack.empty())
            {
                auto const local_index{stack.back()};
                stack.pop_back();
                out.push_back(local_index);

                callees.clear();
                if(!collect_checked_defined_callees(curr_module, storage, local_index, callees, true)) { continue; }

                for(auto remaining{callees.size()}; remaining != 0uz;)
                {
                    --remaining;
                    auto const callee_local_index{callees.index_unchecked(remaining)};
                    if(callee_local_index >= local_count || callee_local_index >= storage.functions.size()) [[unlikely]] { continue; }
                    if(seen.index_unchecked(callee_local_index)) { continue; }
                    seen.index_unchecked(callee_local_index) = true;
                    stack.push_back(callee_local_index);
                }
            }
        }

        // Claims, validates, emits, materializes, publishes, and marks ready a group of lazy LLVM JIT functions.
        inline constexpr void compile_lazy_local_function_group(runtime_module_storage_t const& curr_module,
                                                                lazy_module_storage_t& storage,
                                                                lazy_compile_options& options,
                                                                ::std::size_t entry_local_function_index,
                                                                ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                void (*publish_materialized_function)(void*, ::std::size_t) noexcept = nullptr,
                                                                void* publish_user_data = nullptr) UWVM_THROWS
        {
            if(options.compile_options.emit_debug_safe_points) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(storage.checked_ir_plan == nullptr || !storage.checked_ir_plan->matches_source(curr_module) ||
               !storage.checked_ir_plan->matches_emission_options(options.compile_options) ||
               !storage.checked_ir_plan->admission_available()) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(entry_local_function_index >= storage.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(!storage.checked_ir_plan->native_ir_available(entry_local_function_index))
            {
                // Admission already typed this legal body and published the
                // original native lowering decline. Never claim/retranslate it.
                mark_function_compile_units_state(storage, storage.functions.index_unchecked(entry_local_function_index),
                    ::uwvm2::utils::thread::lazy_compile_state::failed);
                return;
            }
            ::uwvm2::utils::container::vector<::std::size_t> candidate_group{};
            if(options.compile_options.emit_unwind_call_stack_frames)
            {
                collect_lazy_unwind_direct_call_group(
                    curr_module, storage, entry_local_function_index, candidate_group, options.validator_feature_parameter);
            }
            else if(lazy_llvm_jit_object_cache_policy().enable)
            {
                // Opportunistic lazy groups depend on scheduler timing and existing cache warmth.  Cache-enabled lazy JIT
                // uses one stable demanded function per cache object; cache-disabled mode keeps group warmup below.
                candidate_group.push_back(entry_local_function_index);
            }
            else
            {
                collect_lazy_direct_call_group(
                    curr_module, storage, entry_local_function_index, candidate_group, options.validator_feature_parameter);
            }
            if(candidate_group.empty()) { candidate_group.push_back(entry_local_function_index); }

            // Convert heuristic candidates into owned compile claims.  The demanded entry function may already be in
            // `compiling` state because the caller claimed it before entering this helper.
            ::uwvm2::utils::container::vector<::std::size_t> claimed_group{};
            claimed_group.reserve(candidate_group.size());
            for(auto const local_function_index: candidate_group)
            {
                if(local_function_index >= storage.functions.size() || local_function_index >= storage.materialized_functions.size()) [[unlikely]] { continue; }
                // Mutable indirect/ref cohorts and adjacent heuristics can contain
                // legal bodies whose original native lowering declined. Filter
                // BEFORE any compile claim; an unused member cannot fail this
                // demanded function's already checked native fragment.
                if(!storage.checked_ir_plan->native_ir_available(local_function_index)) { continue; }

                auto& fn{storage.functions.index_unchecked(local_function_index)};
                auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
                if(local_function_index == entry_local_function_index)
                {
                    if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled) { continue; }
                    if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]] { ::fast_io::fast_terminate(); }
                    if(st == ::uwvm2::utils::thread::lazy_compile_state::compiling)
                    {
                        if(fn.primary_cu_index < storage.compile_units.size())
                        {
                            storage.compile_units.index_unchecked(fn.primary_cu_index)
                                .state.state.store(::uwvm2::utils::thread::lazy_compile_state::compiling, ::std::memory_order_release);
                        }
                        claimed_group.push_back(local_function_index);
                        continue;
                    }

                    if(st == ::uwvm2::utils::thread::lazy_compile_state::uncompiled || st == ::uwvm2::utils::thread::lazy_compile_state::queued)
                    {
                        auto expected{st};
                        // Successful acq_rel CAS owns materialization for this function.  The acquire half observes any
                        // prior queued-state publication; the release half makes the `compiling` claim visible to waiters.
                        // The failure order is acquire because a failed CAS may report a terminal state published by
                        // another compiler, and the caller may act on that state in the same loop.
                        if(fn.materialization_state.state.compare_exchange_strong(expected,
                                                                                  ::uwvm2::utils::thread::lazy_compile_state::compiling,
                                                                                  ::std::memory_order_acq_rel,
                                                                                  ::std::memory_order_acquire))
                        {
                            if(fn.primary_cu_index < storage.compile_units.size())
                            {
                                storage.compile_units.index_unchecked(fn.primary_cu_index)
                                    .state.state.store(::uwvm2::utils::thread::lazy_compile_state::compiling, ::std::memory_order_release);
                            }
                            claimed_group.push_back(local_function_index);
                        }
                    }
                    continue;
                }

                if(st != ::uwvm2::utils::thread::lazy_compile_state::uncompiled && st != ::uwvm2::utils::thread::lazy_compile_state::queued) { continue; }
                auto expected{st};
                // Opportunistic group members are claimed with the same ownership barrier as the demanded function.  A
                // failed acquire CAS is still useful: it observes another thread's state publication before this function
                // decides whether to skip the candidate.
                if(fn.materialization_state.state.compare_exchange_strong(expected,
                                                                          ::uwvm2::utils::thread::lazy_compile_state::compiling,
                                                                          ::std::memory_order_acq_rel,
                                                                          ::std::memory_order_acquire))
                {
                    if(fn.primary_cu_index < storage.compile_units.size())
                    {
                        storage.compile_units.index_unchecked(fn.primary_cu_index)
                            .state.state.store(::uwvm2::utils::thread::lazy_compile_state::compiling, ::std::memory_order_release);
                    }
                    claimed_group.push_back(local_function_index);
                }
            }

            if(claimed_group.empty()) { return; }
# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                // Every body was validated while emitting at admission. Parse
                // ONLY compiler-owned bitcode, merge the selected fragments, then
                // defer actual machine code/engine/entry publication to below.
                compile_option emit_options{storage.checked_ir_plan->emission_options()};
                llvm_jit_module_storage_t llvm_ir_storage{};
                if(!storage.checked_ir_plan->materialize_checked_ir(curr_module, emit_options, claimed_group, llvm_ir_storage))
                [[unlikely]] { ::fast_io::fast_terminate(); }
                // The original unwind policy only relaxed routed imports after
                // the entire candidate cohort had genuine compiling claims.
                // Consume compiler-owned alternative CFG; never rewalk Wasm.
                if(emit_options.emit_unwind_call_stack_frames && emit_options.route_wasm_calls_through_runtime_bridge &&
                   claimed_group.size() == candidate_group.size() &&
                   !storage.checked_ir_plan->select_complete_cohort_unwind_import_routes(curr_module, storage,
                       claimed_group, llvm_ir_storage)) [[unlikely]] { ::fast_io::fast_terminate(); }

                // MCJIT materialization consumes the IR module and resolves all function symbols before publication.
                lazy_compile_options materialize_options{options};
                materialize_options.compile_options = emit_options;
                if(!materialize_lazy_local_function_group(curr_module, storage, materialize_options, claimed_group, llvm_ir_storage)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                for(auto const local_function_index: claimed_group)
                {
                    auto& fn{storage.functions.index_unchecked(local_function_index)};
                    // Publish runtime target tables before releasing waiters with the `compiled` state.
                    if(publish_materialized_function != nullptr) { publish_materialized_function(publish_user_data, local_function_index); }
                    mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
                }
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
                // The group owner may validate local 0 while a waiter requested local 1. The validator wrote only
                // the owner's err sink, but both functions will become failed. Copy the meaningful diagnostic to
                // every claimed member's pinned slot before publishing ANY failed state: release/acquire then makes
                // that member's diagnostic visible without a shared mutable error object or hot-path lookup.
                if(err.err_code != ::uwvm2::validation::error::code_validation_error_code::ok &&
                   options.grouped_failure_diagnostics != nullptr)
                {
                    auto const group_error{err};  // Snapshot once; err may alias one destination slot.
                    for(auto const local_function_index: claimed_group)
                    {
                        if(local_function_index >= options.grouped_failure_diagnostic_count) [[unlikely]] { continue; }
                        // [parser-owned module bytes ...] module_end
                        // [safe to dereference          ] unsafe (one-past)
                        //  ^^ err_curr may be null or one-past; copy only, never dereference or advance it here.
                        //     All grouped functions share this pinned parser module for later diagnostics.
                        options.grouped_failure_diagnostics[local_function_index] = group_error;
                    }
                }
                for(auto const local_function_index: claimed_group)
                {
                    if(local_function_index >= storage.functions.size()) [[unlikely]] { continue; }
                    auto& fn{storage.functions.index_unchecked(local_function_index)};
                    mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::failed);
                }
                throw;
            }
# endif
        }

        // Single-function materialization path retained for direct internal use.  The public lazy compile entry normally
        // uses the group path so direct callees can be warmed together.
        inline constexpr void compile_lazy_local_function(runtime_module_storage_t const& curr_module,
                                                          lazy_module_storage_t& storage,
                                                          lazy_compile_options& options,
                                                          ::std::size_t local_function_index,
                                                          ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            if(options.compile_options.emit_debug_safe_points) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(storage.checked_ir_plan == nullptr || !storage.checked_ir_plan->matches_source(curr_module) ||
               !storage.checked_ir_plan->matches_emission_options(options.compile_options) ||
               !storage.checked_ir_plan->admission_available()) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(local_function_index >= storage.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(!storage.checked_ir_plan->native_ir_available(local_function_index))
            {
                mark_function_compile_units_state(storage, storage.functions.index_unchecked(local_function_index),
                    ::uwvm2::utils::thread::lazy_compile_state::failed);
                return;
            }
            auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
            if(load_lazy_materialized_ready(materialized, ::std::memory_order_acquire)) { return; }

            compile_option emit_options{storage.checked_ir_plan->emission_options()};
            ::uwvm2::utils::container::vector<::std::size_t> indices{}; indices.push_back(local_function_index);
            llvm_jit_module_storage_t llvm_ir_storage{};
            if(!storage.checked_ir_plan->materialize_checked_ir(curr_module, emit_options, indices, llvm_ir_storage))
            [[unlikely]] { ::fast_io::fast_terminate(); }

            lazy_compile_options materialize_options{options};
            materialize_options.compile_options = emit_options;
            if(!materialize_lazy_local_function(curr_module, storage, materialize_options, local_function_index, llvm_ir_storage)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
        }

        // Scheduler callback for one lazy LLVM compile request.  The request context must remain alive until the
        // scheduler either executes or discards the request.
        inline constexpr void lazy_compile_request_entry(void* user_data) noexcept
        {
            auto const ctx{static_cast<lazy_compile_request_context*>(user_data)};
            if(ctx == nullptr || ctx->curr_module == nullptr || ctx->lazy_storage == nullptr) [[unlikely]] { return; }

            auto& storage{*ctx->lazy_storage};
            if(ctx->compile_unit_index >= storage.compile_units.size()) [[unlikely]] { return; }

            auto& cu{storage.compile_units.index_unchecked(ctx->compile_unit_index)};
            if(cu.local_function_index >= storage.functions.size()) [[unlikely]] { return; }
            auto& fn{storage.functions.index_unchecked(cu.local_function_index)};

            ::uwvm2::validation::error::code_validation_error_impl local_err{};
            auto& err{ctx->err == nullptr ? local_err : *ctx->err};
            ::fast_io::unix_timestamp compile_start_time{};
            // Timestamp collection is best-effort logging support and must not interfere with compilation.
            if(lazy_runtime_log::enabled()) [[unlikely]]
            {
# ifdef UWVM_CPP_EXCEPTIONS
                try
# endif
                {
                    compile_start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                }
# ifdef UWVM_CPP_EXCEPTIONS
                catch(::fast_io::error)
                {
                    // do nothing
                }
# endif
            }

            lazy_runtime_log::line(u8"compile-start module=\"",
                                   ctx->module_name,
                                   u8"\" module_id=",
                                   ctx->options.compile_options.curr_wasm_id,
                                   u8" local_fn=",
                                   cu.local_function_index,
                                   u8" fn=",
                                   fn.function_index,
                                   u8" cu=",
                                   ctx->compile_unit_index,
                                   u8" cu_kind=",
                                   lazy_compile_unit_kind_name(cu.kind),
                                   u8" offset=",
                                   cu.code_offset,
                                   u8" size=",
                                   cu.code_size);

# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                // LLVM materialization and runtime target publication are serialized across worker threads.
                lazy_materialize_lock_guard materialize_guard{};
                compile_lazy_local_function_group(*ctx->curr_module,
                                                  storage,
                                                  ctx->options,
                                                  cu.local_function_index,
                                                  err,
                                                  ctx->publish_materialized_function,
                                                  ctx->publish_user_data);
                ::fast_io::unix_timestamp compile_end_time{};
                if(lazy_runtime_log::enabled()) [[unlikely]]
                {
# ifdef UWVM_CPP_EXCEPTIONS
                    try
# endif
                    {
                        compile_end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                    }
# ifdef UWVM_CPP_EXCEPTIONS
                    catch(::fast_io::error)
                    {
                        // do nothing
                    }
# endif
                }
                lazy_runtime_log::line(u8"compile-end module=\"",
                                       ctx->module_name,
                                       u8"\" module_id=",
                                       ctx->options.compile_options.curr_wasm_id,
                                       u8" local_fn=",
                                       cu.local_function_index,
                                       u8" fn=",
                                       fn.function_index,
                                       u8" cu=",
                                       ctx->compile_unit_index,
                                       u8" state=",
                                       compile_state_name(fn.materialization_state.state.load(::std::memory_order_acquire)),
                                       u8" time=",
                                       compile_end_time - compile_start_time);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
                // Scheduler callbacks are noexcept; mark the request failed and log instead of rethrowing.
                mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::failed);
                ::fast_io::unix_timestamp compile_end_time{};
                if(lazy_runtime_log::enabled()) [[unlikely]]
                {
                    try
                    {
                        compile_end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
                    }
                    catch(::fast_io::error)
                    {
                        // do nothing
                    }
                }
                lazy_runtime_log::line(u8"compile-end module=\"",
                                       ctx->module_name,
                                       u8"\" module_id=",
                                       ctx->options.compile_options.curr_wasm_id,
                                       u8" local_fn=",
                                       cu.local_function_index,
                                       u8" fn=",
                                       fn.function_index,
                                       u8" cu=",
                                       ctx->compile_unit_index,
                                       u8" state=failed time=",
                                       compile_end_time - compile_start_time);
            }
# endif
        }
    }  // namespace details

    // Builds all lazy metadata for a runtime module without emitting native code.  Each local function receives one
    // whole-function compile unit and one materialization record indexed by local-defined-function index.
    inline constexpr lazy_module_storage_t prepare_lazy_module_storage_index(runtime_module_storage_t const& curr_module,
                                                                          compile_option const& options,
                                                                          ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                          [[maybe_unused]] lazy_split_config split_config = {}) UWVM_THROWS
    {
        // A copied full option cannot authorize lazy/tiered instrumentation.
        // The public runtime reports unsupported modes before this internal guard.
        if(options.emit_debug_safe_points) [[unlikely]] { ::fast_io::fast_terminate(); }
        lazy_module_storage_t storage{};
        storage.validation_module = details::all_details::build_runtime_validation_module(curr_module);
        parser_feature_parameter_t const default_feature_parameter{};
        auto const& effective_feature_parameter{options.validator_feature_parameter == nullptr ?
            default_feature_parameter : *options.validator_feature_parameter};
        details::all_details::require_runtime_module_declaration_policy(curr_module, effective_feature_parameter, err);

        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        storage.compiled.local_funcs.clear();
        storage.compiled.local_funcs.resize(local_func_count);
        storage.functions.clear();
        storage.functions.resize(local_func_count);
        storage.compile_units.clear();
        storage.compile_units.reserve(local_func_count);
        storage.materialized_functions.clear();
        storage.materialized_functions.resize(local_func_count);

        for(::std::size_t local_function_index{}; local_function_index != local_func_count; ++local_function_index)
        {
            details::append_lazy_function(curr_module, storage, options, local_function_index, err);
        }

        return storage;
    }

    // Admission is completed BEFORE any scheduler/guest execution publication.
    // Runtime target cells must already have their final stable allocation.
    inline void admit_lazy_module_storage(runtime_module_storage_t const& module, lazy_module_storage_t& storage,
        compile_option const& options, ::uwvm2::validation::error::code_validation_error_impl& error) UWVM_THROWS
    {
        if(storage.checked_ir_plan != nullptr) { ::fast_io::fast_terminate(); }
        auto pending{details::checked_lazy_ir_plan::admit(module, storage, options, error)};
        if(!pending || !pending->admission_available())
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                u8"uwvm: [fatal] LLVM lazy admission could not retain the complete checked plan (compiler resource quota).\n",
                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
            ::fast_io::fast_terminate();
        }
        // All typed records are sealed before any worker/guest publication.
        // A valid but native-unavailable function has no native ready entry. Its
        // demand path retains the existing native-unavailable diagnostic; only
        // the actual tiered T0 branch can consume its separately sealed exact
        // NTTP register-ring artifact. No fallback is synthesized here.
        for(::std::size_t index{}; index != storage.functions.size(); ++index)
        {
            // [actual pre-sized function/CU metadata0 ... index ... size] end
            // [safe] index<size before record access; initialization owns all
            // state and no scheduler/guest entry has been published yet.
            if(!pending->native_ir_available(index))
            { details::mark_function_compile_units_state(storage, storage.functions.index_unchecked(index),
                ::uwvm2::utils::thread::lazy_compile_state::failed); }
        }
        storage.checked_ir_plan = ::std::move(pending);
    }

    inline lazy_module_storage_t initialize_lazy_module_storage(runtime_module_storage_t const& module,
        compile_option const& options, ::uwvm2::validation::error::code_validation_error_impl& error,
        lazy_split_config split = {}) UWVM_THROWS
    {
        auto storage{prepare_lazy_module_storage_index(module, options, error, split)};
        admit_lazy_module_storage(module, storage, options, error);
        return storage;
    }

    [[nodiscard]] inline bool collect_direct_defined_callees(runtime_module_storage_t const& module,
        lazy_module_storage_t const& storage, ::std::size_t index,
        ::uwvm2::utils::container::vector<::std::size_t>& callees, [[maybe_unused]] parser_feature_parameter_t const* features)
    { return details::collect_checked_defined_callees(module, storage, index, callees, false); }

    // Synchronously compiles the compile unit identified by `compile_unit_index`.  Invalid indices and failed
    // materialization are fatal because callers use this path when execution cannot proceed without native code.
    inline constexpr void compile_cu_from_lazy_validator(runtime_module_storage_t const& curr_module,
                                                         lazy_module_storage_t& storage,
                                                         lazy_compile_options& options,
                                                         ::std::size_t compile_unit_index,
                                                         ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(storage.checked_ir_plan == nullptr || !storage.checked_ir_plan->matches_source(curr_module) ||
           !storage.checked_ir_plan->matches_emission_options(options.compile_options))
        [[unlikely]] { ::fast_io::fast_terminate(); }
        if(compile_unit_index >= storage.compile_units.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::fast_io::unix_timestamp compile_start_time{};
        if(lazy_runtime_log::enabled()) [[unlikely]]
        {
# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                compile_start_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                // do nothing
            }
# endif
        }

        auto& cu{storage.compile_units.index_unchecked(compile_unit_index)};
        if(cu.local_function_index >= storage.functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto& fn{storage.functions.index_unchecked(cu.local_function_index)};

        lazy_runtime_log::line(u8"compile-cu-start module_id=",
                               options.compile_options.curr_wasm_id,
                               u8" local_fn=",
                               cu.local_function_index,
                               u8" cu=",
                               compile_unit_index,
                               u8" cu_kind=",
                               lazy_compile_unit_kind_name(cu.kind),
                               u8" offset=",
                               cu.code_offset,
                               u8" size=",
                               cu.code_size);

        bool counted_wait{};
        for(;;)
        {
            // The function-level state is the canonical scheduler state.  Acquire is required when observing terminal
            // states because the compiled path immediately returns and callers may read the materialized payload next.
            auto const st{fn.materialization_state.state.load(::std::memory_order_acquire)};
            if(st == ::uwvm2::utils::thread::lazy_compile_state::compiled)
            {
                lazy_runtime_log::line(u8"compile-cu-hit module_id=",
                                       options.compile_options.curr_wasm_id,
                                       u8" local_fn=",
                                       cu.local_function_index,
                                       u8" cu=",
                                       compile_unit_index,
                                       u8" state=compiled");
                return;
            }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::failed) [[unlikely]] { ::fast_io::fast_terminate(); }
            if(st == ::uwvm2::utils::thread::lazy_compile_state::uncompiled)
            {
                auto expected{::uwvm2::utils::thread::lazy_compile_state::uncompiled};
                // Claim function-level materialization; all whole-function compile units alias this same state.  The
                // acq_rel success order publishes this inline owner and keeps direct claims equivalent to scheduler
                // claims.  Acquire failure preserves synchronization with a concurrent terminal publisher before retry.
                if(fn.materialization_state.state.compare_exchange_strong(expected,
                                                                          ::uwvm2::utils::thread::lazy_compile_state::compiling,
                                                                          ::std::memory_order_acq_rel,
                                                                          ::std::memory_order_acquire))
                {
                    break;
                }
                continue;
            }

            if(!counted_wait)
            {
                lazy_runtime_log::line(u8"compile-cu-wait module_id=",
                                       options.compile_options.curr_wasm_id,
                                       u8" local_fn=",
                                       cu.local_function_index,
                                       u8" cu=",
                                       compile_unit_index,
                                       u8" state=",
                                       compile_state_name(st));
                counted_wait = true;
            }
            ::uwvm2::utils::thread::lazy_compile_thread_yield();
        }

        {
            // Keep synchronous demand compilation serialized with background worker materialization.
            details::lazy_materialize_lock_guard materialize_guard{};
            details::compile_lazy_local_function_group(curr_module, storage, options, cu.local_function_index, err);
        }
        ::fast_io::unix_timestamp compile_end_time{};
        if(lazy_runtime_log::enabled()) [[unlikely]]
        {
# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                compile_end_time = ::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::monotonic_raw);
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(::fast_io::error)
            {
                // do nothing
            }
# endif
        }
        lazy_runtime_log::line(u8"compile-cu-end module_id=",
                               options.compile_options.curr_wasm_id,
                               u8" local_fn=",
                               cu.local_function_index,
                               u8" cu=",
                               compile_unit_index,
                               u8" state=",
                               compile_state_name(fn.materialization_state.state.load(::std::memory_order_acquire)),
                               u8" time=",
                               compile_end_time - compile_start_time);
    }

    // Creates a scheduler request for the function-level materialization state associated with `ctx.compile_unit_index`.
    // The returned request borrows `ctx`; callers must keep the context storage stable while the request may run.
    [[nodiscard]] inline constexpr ::uwvm2::utils::thread::lazy_compile_request make_lazy_compile_request(lazy_compile_request_context & ctx,
                                                                                                          unsigned priority = 0u) noexcept
    {
        if(ctx.lazy_storage == nullptr || ctx.compile_unit_index >= ctx.lazy_storage->compile_units.size()) [[unlikely]] { return {}; }

        auto& cu{ctx.lazy_storage->compile_units.index_unchecked(ctx.compile_unit_index)};
        if(cu.local_function_index >= ctx.lazy_storage->functions.size()) [[unlikely]] { return {}; }
        auto& fn{ctx.lazy_storage->functions.index_unchecked(cu.local_function_index)};

        return {.unit = ::std::addressof(fn.materialization_state),
                .compile = details::lazy_compile_request_entry,
                .user_data = ::std::addressof(ctx),
                .priority = priority};
    }

    // Retrieves the raw ABI entry address for a materialized local function.
    //
    // This accessor may be called either by the compiler thread after materialization or by a racing tiered probe before
    // it has observed the outer function state as `compiled`.  Therefore the `ready` flag is the synchronization point
    // here: if the acquire load observes `true`, the ordinary address fields are safe to read.
    [[nodiscard]] inline constexpr bool try_get_lazy_raw_entry_address(lazy_module_storage_t const& storage,
                                                                       ::std::size_t local_function_index,
                                                                       ::std::uintptr_t& function_address) noexcept
    {
        function_address = 0u;
        if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }

        auto const& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
        if(!details::load_lazy_materialized_ready(materialized, ::std::memory_order_acquire) || materialized.raw_entry_address == 0u) { return false; }

        function_address = materialized.raw_entry_address;
        return true;
    }

    // Retrieves the typed Wasm entry address for a materialized local function.
    //
    // The acquire `ready` load mirrors the raw-entry accessor.  It prevents a caller from consuming a typed entry address
    // while the materializer is still resolving symbols or installing the MCJIT owner.
    [[nodiscard]] inline constexpr bool try_get_lazy_entry_address(lazy_module_storage_t const& storage,
                                                                   ::std::size_t local_function_index,
                                                                   ::std::uintptr_t& function_address) noexcept
    {
        function_address = 0u;
        if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }

        auto const& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
        if(!details::load_lazy_materialized_ready(materialized, ::std::memory_order_acquire) || materialized.entry_address == 0u) { return false; }

        function_address = materialized.entry_address;
        return true;
    }

    // Retrieves the raw ABI address for a tiered loop reentry wrapper selected by function-relative Wasm code offset.
    //
    // Reentry descriptors and their raw addresses are ordinary vectors.  The acquire `ready` load must happen before the
    // vector size comparison and element reads so readers see a consistent descriptor/address pair.
    [[nodiscard]] inline constexpr bool try_get_lazy_tiered_loop_reentry_raw_entry_address(lazy_module_storage_t const& storage,
                                                                                           ::std::size_t local_function_index,
                                                                                           ::std::size_t wasm_code_offset,
                                                                                           ::std::uintptr_t& function_address) noexcept
    {
        function_address = 0u;
        if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }

        auto const& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
        if(!details::load_lazy_materialized_ready(materialized, ::std::memory_order_acquire) ||
           materialized.tiered_loop_reentries.size() != materialized.tiered_loop_reentry_raw_entry_addresses.size())
        {
            return false;
        }

        for(::std::size_t i{}; i != materialized.tiered_loop_reentries.size(); ++i)
        {
            if(materialized.tiered_loop_reentries.index_unchecked(i).wasm_code_offset != wasm_code_offset) { continue; }

            function_address = materialized.tiered_loop_reentry_raw_entry_addresses.index_unchecked(i);
            return function_address != 0u;
        }

        return false;
    }
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
