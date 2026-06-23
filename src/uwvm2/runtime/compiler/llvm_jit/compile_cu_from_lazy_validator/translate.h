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
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_push_macro.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// platform
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/Analysis/TargetTransformInfo.h>
#  include <llvm/ExecutionEngine/ExecutionEngine.h>
#  include <llvm/ExecutionEngine/MCJIT.h>
#  include <llvm/ExecutionEngine/SectionMemoryManager.h>
#  include <llvm/Config/llvm-config.h>
#  include <llvm/InitializePasses.h>
#  include <llvm/IR/LegacyPassManager.h>
#  include <llvm/IR/Verifier.h>
#  include <llvm/PassRegistry.h>
#  include <llvm/Support/TargetSelect.h>
#  include <llvm/Target/TargetMachine.h>
#  include <llvm/TargetParser/Host.h>
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
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/standard/wasm1/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <uwvm2/runtime/llvm_jit_cache/impl.h>
# endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)

// apple unwind
# if defined(__APPLE__) && !defined(_WIN32) && __has_include(<unwind.h>)
extern "C" void __register_frame(void const*);
extern "C" void __deregister_frame(void const*);
# endif

// Lazy LLVM JIT compilation support for runtime-local Wasm functions.
//
// This header builds a light-weight lazy compilation index from the already-finalized runtime module, validates function
// bodies on demand when requested, emits LLVM IR through the full-module translator helpers, and materializes MCJIT
// entry addresses only when a function is first needed.  The current LLVM lazy backend uses whole functions as compile
// units; grouping is applied at materialization time to compile small direct-call neighborhoods together.
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator
{
    // Runtime module storage supplied by the loader/finalizer.  All pointers stored in lazy metadata borrow from it.
    using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;

    // Parser-side module storage used by the standard validator when lazy validation is enabled.
    using parser_module_storage_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t;

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

    [[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
        lazy_local_function_wasm_code_hash(runtime_module_storage_t const& curr_module, ::std::size_t local_function_index) noexcept
    {
        if(local_function_index >= curr_module.local_defined_function_vec_storage.size()) [[unlikely]]
        {
            return ::uwvm2::utils::container::u8concat_uwvm(u8"missing");
        }
        auto const& local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
        auto const wasm_code_ptr{local_func.wasm_code_ptr};
        if(wasm_code_ptr == nullptr) [[unlikely]] { return ::uwvm2::utils::container::u8concat_uwvm(u8"missing"); }

        auto const first{reinterpret_cast<::std::byte const*>(wasm_code_ptr->body.code_begin)};
        auto const last{reinterpret_cast<::std::byte const*>(wasm_code_ptr->body.code_end)};
        if(first == nullptr || last == nullptr || first > last) [[unlikely]] { return ::uwvm2::utils::container::u8concat_uwvm(u8"invalid"); }

        ::fast_io::sha256_context sha{};
        if(first != last) { sha.update(first, last); }
        sha.do_final();

        ::uwvm2::utils::container::array<::std::byte, 32uz> digest{};
        sha.digest_to_byte_ptr(digest.data());

        ::uwvm2::utils::container::u8string out{};
        out.reserve(digest.size() * 2uz);
        ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(out)};
        for(auto b: digest) { ::fast_io::io::print(ref, ::fast_io::mnp::hex<false, true>(::std::to_integer<::std::uint_least8_t>(b))); }
        return out;
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

    // Complete lazy LLVM JIT state for one runtime module.
    struct lazy_module_storage_t
    {
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

        // Borrowed parser module required when `validation_mode` is `validate_on_lazy_compile`.
        parser_module_storage_t const* validator_module_storage{};

        // Validation policy for lazy compilation.
        lazy_validation_mode validation_mode{lazy_validation_mode::validate_on_lazy_compile};

        // LLVM code-generation level used for the MCJIT engine and local optimization pipeline.
        ::llvm::CodeGenOptLevel codegen_opt_level{::llvm::CodeGenOptLevel::Less};

        // Optional listener for object/debug registration events emitted by MCJIT.
        ::llvm::JITEventListener* jit_event_listener{};
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
#  if LLVM_VERSION_MAJOR >= 22
            module.setTargetTriple(target_machine.getTargetTriple());
#  else
            module.setTargetTriple(target_machine.getTargetTriple().str());
#  endif
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
        inline constexpr void mark_function_compile_units_state(lazy_module_storage_t& storage,
                                                                lazy_function_storage_t& fn,
                                                                ::uwvm2::utils::thread::lazy_compile_state state) noexcept
        {
            // This release store is the function-level publication barrier.  By the time a waiter observes `compiled`
            // with an acquire load/wait, the materialized record, its `ready` publication, and any runtime target-table
            // stores sequenced before this call are visible.  `failed` is also published with release so waiters never
            // miss diagnostics or cleanup state written before failure.
            fn.materialization_state.state.store(state, ::std::memory_order_release);
            ::uwvm2::utils::thread::lazy_compile_notify_unit(fn.materialization_state);
            if(fn.primary_cu_index < storage.compile_units.size())
            {
                auto& cu_state{storage.compile_units.index_unchecked(fn.primary_cu_index).state};
                // The compile-unit state is a mirror for observers that do not hold the function record directly.  Use the
                // same release order so either state object can be used as the synchronization point for terminal states.
                cu_state.state.store(state, ::std::memory_order_release);
                ::uwvm2::utils::thread::lazy_compile_notify_unit(cu_state);
            }
        }

        // Runs standard Wasm code validation for one local function when the lazy mode requires it.
        inline constexpr void validate_function_if_needed(runtime_module_storage_t const& curr_module,
                                                          lazy_compile_options const& options,
                                                          ::std::size_t local_function_index,
                                                          ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            if(options.validation_mode == lazy_validation_mode::validate_on_lazy_compile)
            {
                if(options.validator_module_storage == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

                auto const import_func_count{curr_module.imported_function_vec_storage.size()};
                auto const function_index{import_func_count + local_function_index};
                auto const& curr_local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
                auto const& curr_code{*curr_local_func.wasm_code_ptr};
                auto const code_begin{reinterpret_cast<::std::byte const*>(curr_code.body.expr_begin)};
                auto const code_end{reinterpret_cast<::std::byte const*>(curr_code.body.code_end)};

                ::uwvm2::validation::standard::wasm1::validate_code(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                                                                    *options.validator_module_storage,
                                                                    function_index,
                                                                    code_begin,
                                                                    code_end,
                                                                    err);
            }
        }

        [[nodiscard]] inline constexpr bool initialize_llvm_jit_process_target() noexcept
        {
# if defined(__APPLE__)
            // Cross-built JIT binaries must register the target of the running process, not the target baked into llvm-config's
            // LLVM_NATIVE_TARGET. This is especially important for x86_64 Darwin binaries executed through Rosetta on Apple Silicon.
#  if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            ::LLVMInitializeX86TargetInfo();
            ::LLVMInitializeX86Target();
            ::LLVMInitializeX86TargetMC();
            ::LLVMInitializeX86AsmPrinter();
            return true;
#  elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
            ::LLVMInitializeAArch64TargetInfo();
            ::LLVMInitializeAArch64Target();
            ::LLVMInitializeAArch64TargetMC();
            ::LLVMInitializeAArch64AsmPrinter();
            return true;
#  elif defined(__arm__) || defined(_M_ARM)
            ::LLVMInitializeARMTargetInfo();
            ::LLVMInitializeARMTarget();
            ::LLVMInitializeARMTargetMC();
            ::LLVMInitializeARMAsmPrinter();
            return true;
#  elif defined(__powerpc__) || defined(__powerpc64__) || defined(__ppc__) || defined(__ppc64__)
            ::LLVMInitializePowerPCTargetInfo();
            ::LLVMInitializePowerPCTarget();
            ::LLVMInitializePowerPCTargetMC();
            ::LLVMInitializePowerPCAsmPrinter();
            return true;
#  else
            return !::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter();
#  endif
# else
            return !::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter();
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
            return mattrs;
        }

        // Returns the process-wide native target configuration used for lazy MCJIT materialization.
        [[nodiscard]] inline constexpr llvm_jit_native_target_config const& get_llvm_jit_native_target_config() noexcept
        {
            static llvm_jit_native_target_config config{.cpu_name =
                                                            []() constexpr noexcept
                                                        {
                                                            auto const host_cpu_name{::llvm::sys::getHostCPUName()};
                                                            return ::uwvm2::utils::container::u8string{all_details::get_uwvm_u8string_view(host_cpu_name)};
                                                        }(),
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

        // Advances over one Wasm instruction while scanning for direct `call` opcodes.  Structured control opcodes need
        // special handling because their block-result immediate is parsed by a different helper.
        [[nodiscard]] inline constexpr bool skip_wasm_instruction_for_direct_call_scan(::std::byte const*& code_curr, ::std::byte const* code_end) noexcept
        {
            if(code_curr == code_end) [[unlikely]] { return false; }

            all_details::wasm1_code op;  // no init necessary
            ::std::memcpy(::std::addressof(op), code_curr, sizeof(op));
            switch(op)
            {
                case all_details::wasm1_code::block:
                case all_details::wasm1_code::loop:
                case all_details::wasm1_code::if_:
                {
                    ++code_curr;
                    all_details::runtime_block_result_type block_result{};
                    return all_details::parse_wasm_block_result_type(code_curr, code_end, block_result);
                }
                default:
                {
                    return all_details::skip_wasm_unreachable_noncontrol_instruction(code_curr, code_end);
                }
            }
        }

        // Appends a local function index while preserving first-seen order and avoiding duplicates.
        inline constexpr void append_unique_local_function_index(::uwvm2::utils::container::vector<::std::size_t>& out,
                                                                 ::std::size_t local_function_index) noexcept
        {
            if(::std::find(out.begin(), out.end(), local_function_index) == out.end()) { out.push_back(local_function_index); }
        }

        // Finds directly-called local-defined functions in one Wasm body.  The scan is intentionally shallow: indirect
        // calls and calls to imports are not grouped because they do not identify a single local materialization target.
        [[nodiscard]] inline constexpr bool collect_direct_defined_callees(runtime_module_storage_t const& curr_module,
                                                                           ::std::size_t local_function_index,
                                                                           ::uwvm2::utils::container::vector<::std::size_t>& callees) noexcept
        {
            callees.clear();
            if(local_function_index >= curr_module.local_defined_function_vec_storage.size()) [[unlikely]] { return false; }

            auto const& local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
            if(local_func.wasm_code_ptr == nullptr) [[unlikely]] { return false; }

            auto code_curr{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.expr_begin)};
            auto const code_end{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.code_end)};
            if(code_curr == nullptr || code_end == nullptr || code_curr > code_end) [[unlikely]] { return false; }

            auto const import_count{curr_module.imported_function_vec_storage.size()};
            auto const local_count{curr_module.local_defined_function_vec_storage.size()};
            auto const all_function_count{import_count + local_count};

            while(code_curr < code_end)
            {
                all_details::wasm1_code op{};
                ::std::memcpy(::std::addressof(op), code_curr, sizeof(op));
                if(op == all_details::wasm1_code::call)
                {
                    ++code_curr;
                    all_details::validation_module_traits_t::wasm_u32 function_index{};
                    if(!all_details::parse_wasm_leb128_immediate(code_curr, code_end, function_index)) [[unlikely]] { return false; }

                    auto const function_index_uz{static_cast<::std::size_t>(function_index)};
                    if(function_index_uz >= import_count && function_index_uz < all_function_count)
                    {
                        append_unique_local_function_index(callees, function_index_uz - import_count);
                    }
                    continue;
                }

                if(!skip_wasm_instruction_for_direct_call_scan(code_curr, code_end)) [[unlikely]] { return false; }
            }

            return true;
        }

        [[nodiscard]] inline constexpr bool collect_call_indirect_defined_targets(runtime_module_storage_t const& curr_module,
                                                                                  all_details::validation_module_traits_t::wasm_u32 table_index,
                                                                                  ::uwvm2::utils::container::vector<::std::size_t>& targets) noexcept
        {
            auto const table{all_details::resolve_runtime_table_storage(curr_module, table_index)};
            if(table == nullptr) [[unlikely]] { return false; }

            auto const import_count{curr_module.imported_function_vec_storage.size()};
            auto const local_count{curr_module.local_defined_function_vec_storage.size()};
            for(auto const& elem: table->elems)
            {
                auto const callee{all_details::resolve_runtime_call_indirect_callee(curr_module, elem)};
                if(!callee.state_valid) [[unlikely]] { return false; }
                if(!callee.present || !callee.belongs_to_current_module) { continue; }

                auto const function_index{static_cast<::std::size_t>(callee.func_index)};
                if(function_index < import_count) { continue; }
                auto const local_function_index{function_index - import_count};
                if(local_function_index >= local_count) [[unlikely]] { return false; }

                append_unique_local_function_index(targets, local_function_index);
            }

            return true;
        }

        [[nodiscard]] inline constexpr bool collect_unwind_defined_callees(runtime_module_storage_t const& curr_module,
                                                                           ::std::size_t local_function_index,
                                                                           ::uwvm2::utils::container::vector<::std::size_t>& callees) noexcept
        {
            callees.clear();
            if(local_function_index >= curr_module.local_defined_function_vec_storage.size()) [[unlikely]] { return false; }

            auto const& local_func{curr_module.local_defined_function_vec_storage.index_unchecked(local_function_index)};
            if(local_func.wasm_code_ptr == nullptr) [[unlikely]] { return false; }

            auto code_curr{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.expr_begin)};
            auto const code_end{reinterpret_cast<::std::byte const*>(local_func.wasm_code_ptr->body.code_end)};
            if(code_curr == nullptr || code_end == nullptr || code_curr > code_end) [[unlikely]] { return false; }

            auto const import_count{curr_module.imported_function_vec_storage.size()};
            auto const local_count{curr_module.local_defined_function_vec_storage.size()};
            auto const all_function_count{import_count + local_count};
            auto const all_table_count{curr_module.imported_table_vec_storage.size() + curr_module.local_defined_table_vec_storage.size()};

            while(code_curr < code_end)
            {
                all_details::wasm1_code op{};
                ::std::memcpy(::std::addressof(op), code_curr, sizeof(op));
                if(op == all_details::wasm1_code::call)
                {
                    ++code_curr;
                    all_details::validation_module_traits_t::wasm_u32 function_index{};
                    if(!all_details::parse_wasm_leb128_immediate(code_curr, code_end, function_index)) [[unlikely]] { return false; }

                    auto const function_index_uz{static_cast<::std::size_t>(function_index)};
                    if(function_index_uz >= import_count && function_index_uz < all_function_count)
                    {
                        append_unique_local_function_index(callees, function_index_uz - import_count);
                    }
                    continue;
                }

                if(op == all_details::wasm1_code::call_indirect)
                {
                    ++code_curr;
                    all_details::validation_module_traits_t::wasm_u32 type_index{};
                    all_details::validation_module_traits_t::wasm_u32 table_index{};
                    if(!all_details::parse_wasm_leb128_immediate(code_curr, code_end, type_index) ||
                       !all_details::parse_wasm_leb128_immediate(code_curr, code_end, table_index))
                        [[unlikely]]
                    {
                        return false;
                    }

                    if(static_cast<::std::size_t>(table_index) >= all_table_count) [[unlikely]] { return false; }
                    // Native unwind mode must not discover a table target by entering the lazy raw trampoline from an
                    // active JIT frame: Rosetta and libunwind may then see only the callee object.  Pre-materialize all
                    // current-module targets of the selected table so call_indirect can take the typed native entry.
                    if(!collect_call_indirect_defined_targets(curr_module, table_index, callees)) [[unlikely]] { return false; }
                    continue;
                }

                if(!skip_wasm_instruction_for_direct_call_scan(code_curr, code_end)) [[unlikely]] { return false; }
            }

            return true;
        }

        // Verifies and lightly optimizes a lazy LLVM module before MCJIT takes ownership of it.
        [[nodiscard]] inline constexpr bool optimize_lazy_llvm_jit_module(::llvm::Module& module,
                                                                          ::llvm::TargetMachine& target_machine,
                                                                          ::llvm::CodeGenOptLevel codegen_opt_level,
                                                                          bool verify_llvm_jit_ir) noexcept
        {
            if(!all_details::verify_llvm_jit_module(module, verify_llvm_jit_ir)) [[unlikely]] { return false; }

            if(codegen_opt_level == ::llvm::CodeGenOptLevel::None) { return true; }

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

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{target_builder.selectTarget()};
            if(target_machine == nullptr) [[unlikely]] { return false; }
            if(options.codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target_machine->setFastISel(true); }

            set_llvm_module_target_triple_from_machine(*llvm_module, *target_machine);
            llvm_module->setDataLayout(target_machine->createDataLayout());
            if(!optimize_lazy_llvm_jit_module(*llvm_module, *target_machine, options.codegen_opt_level, options.compile_options.verify_llvm_jit_ir))
                [[unlikely]]
            {
                return false;
            }

            ::uwvm2::utils::container::u8string llvm_jit_cache_key{};
            {
                llvm_jit_cache_key = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"lazy-single");
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module", curr_module.module_name);
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                    llvm_jit_cache_key, u8"local-fn", static_cast<::std::uint_least64_t>(local_function_index));
                auto wasm_code_hash{lazy_local_function_wasm_code_hash(curr_module, local_function_index)};
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"wasm-code-hash", wasm_code_hash);
            }
            ::uwvm2::utils::container::u8string llvm_jit_cache_codegen_policy{};
            {
                llvm_jit_cache_codegen_policy = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"codegen-policy");
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                    llvm_jit_cache_codegen_policy, u8"cache-unit", u8"lazy-single");
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                    llvm_jit_cache_codegen_policy, u8"codegen-opt-level", static_cast<::std::uint_least64_t>(options.codegen_opt_level));
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                    llvm_jit_cache_codegen_policy, u8"validation-mode", lazy_validation_mode_name(options.validation_mode));
            }
            auto llvm_jit_cache_context{::uwvm2::runtime::llvm_jit_cache::default_cache_context(
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_key.data(), llvm_jit_cache_key.size()},
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_codegen_policy.data(), llvm_jit_cache_codegen_policy.size()},
                *target_machine)};
            llvm_jit_cache_context.cache_key_is_complete = true;
            ::uwvm2::runtime::llvm_jit_cache::llvm_jit_object_cache llvm_jit_object_cache{
                ::std::move(llvm_jit_cache_context), ::uwvm2::runtime::llvm_jit_cache::default_cache_policy()};
            
                auto raw_engine{
                ::llvm::EngineBuilder(details::llvm_module_owner_t{llvm_module.release()})
                    .setEngineKind(::llvm::EngineKind::JIT)
                    .setOptLevel(options.codegen_opt_level)
                    .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                    .setMAttrs(host_target_attributes)
                    .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{
                        ::uwvm2::utils::container::make_delete_owned<::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()
                            .release()})
                    .create(target_machine.get())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }
            static_cast<void>(target_machine.release());

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};

            engine->setObjectCache(::std::addressof(llvm_jit_object_cache));
            if(options.jit_event_listener != nullptr)
            {
                // Lazy unwind call-stack mode needs DWARF sections as well as executable sections so optimized inline
                // Wasm frames can be reconstructed from the generated object.
                engine->setProcessAllSections(true);
                engine->RegisterJITEventListener(options.jit_event_listener);
            }
            engine->finalizeObject();
            engine->setObjectCache(nullptr);

            auto const import_func_count{curr_module.imported_function_vec_storage.size()};
            auto const function_index{import_func_count + local_function_index};
            using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;
            if(function_index > static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())) [[unlikely]] { return false; }

            auto const function_index_u32{static_cast<wasm_u32>(function_index)};
            auto const function_name{all_details::get_llvm_wasm_function_name(curr_module, function_index_u32)};
            auto const raw_function_name{all_details::get_llvm_wasm_raw_function_name(curr_module, function_index_u32)};
            auto const entry_address{resolve_llvm_function_address(*engine, function_name)};
            auto const raw_entry_address{resolve_llvm_function_address(*engine, raw_function_name)};
            if(entry_address == 0u || raw_entry_address == 0u) [[unlikely]] { return false; }

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

            materialized.entry_address = entry_address;
            materialized.raw_entry_address = raw_entry_address;
            materialized.llvm_context_holder = ::std::move(llvm_context_holder);
            materialized.llvm_jit_engine = ::std::move(engine);
            // Publish the fully resolved single-function record.  Acquire readers can now safely consume the addresses
            // and rely on the owner fields to keep MCJIT code alive.
            store_lazy_materialized_ready(materialized, true, ::std::memory_order_release);
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

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::TargetMachine> target_machine{target_builder.selectTarget()};
            if(target_machine == nullptr) [[unlikely]] { return false; }
            if(options.codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target_machine->setFastISel(true); }

            set_llvm_module_target_triple_from_machine(*llvm_module, *target_machine);
            llvm_module->setDataLayout(target_machine->createDataLayout());
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
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                        llvm_jit_cache_key, u8"local-fn", static_cast<::std::uint_least64_t>(local_function_index));
                    auto wasm_code_hash{lazy_local_function_wasm_code_hash(curr_module, local_function_index)};
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"wasm-code-hash", wasm_code_hash);
                }
                else
                {
                    llvm_jit_cache_key = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"lazy-group");
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"module", curr_module.module_name);
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                        llvm_jit_cache_key, u8"local-fn-count", static_cast<::std::uint_least64_t>(local_function_indices.size()));
                    for(auto const local_function_index: local_function_indices)
                    {
                        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                            llvm_jit_cache_key, u8"local-fn", static_cast<::std::uint_least64_t>(local_function_index));
                        auto wasm_code_hash{lazy_local_function_wasm_code_hash(curr_module, local_function_index)};
                        ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(llvm_jit_cache_key, u8"wasm-code-hash", wasm_code_hash);
                    }
                }
            }
            ::uwvm2::utils::container::u8string llvm_jit_cache_codegen_policy{};
            {
                auto const single_cache_unit{local_function_indices.size() == 1uz};
                llvm_jit_cache_codegen_policy = ::uwvm2::runtime::llvm_jit_cache::details::make_cache_key(u8"codegen-policy");
                if(single_cache_unit)
                {
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                        llvm_jit_cache_codegen_policy, u8"cache-unit", u8"lazy-single");
                }
                else
                {
                    ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                        llvm_jit_cache_codegen_policy, u8"cache-unit", u8"lazy-group");
                }
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value_u64(
                    llvm_jit_cache_codegen_policy, u8"codegen-opt-level", static_cast<::std::uint_least64_t>(options.codegen_opt_level));
                ::uwvm2::runtime::llvm_jit_cache::details::append_cache_key_value(
                    llvm_jit_cache_codegen_policy, u8"validation-mode", lazy_validation_mode_name(options.validation_mode));
            }
            auto llvm_jit_cache_context{::uwvm2::runtime::llvm_jit_cache::default_cache_context(
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_key.data(), llvm_jit_cache_key.size()},
                ::uwvm2::utils::container::u8string_view{llvm_jit_cache_codegen_policy.data(), llvm_jit_cache_codegen_policy.size()},
                *target_machine)};
            llvm_jit_cache_context.cache_key_is_complete = true;
            ::uwvm2::runtime::llvm_jit_cache::llvm_jit_object_cache llvm_jit_object_cache{
                ::std::move(llvm_jit_cache_context), ::uwvm2::runtime::llvm_jit_cache::default_cache_policy()};
            
                auto raw_engine{
                ::llvm::EngineBuilder(details::llvm_module_owner_t{llvm_module.release()})
                    .setEngineKind(::llvm::EngineKind::JIT)
                    .setOptLevel(options.codegen_opt_level)
                    .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                    .setMAttrs(host_target_attributes)
                    .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{
                        ::uwvm2::utils::container::make_delete_owned<::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()
                            .release()})
                    .create(target_machine.get())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }
            static_cast<void>(target_machine.release());

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};
            
            engine->setObjectCache(::std::addressof(llvm_jit_object_cache));
            if(options.jit_event_listener != nullptr)
            {
                // Lazy group materialization can inline or split functions across one MCJIT object; keep DWARF metadata
                // available to the listener so traps can recover the logical Wasm stack.
                engine->setProcessAllSections(true);
                engine->RegisterJITEventListener(options.jit_event_listener);
            }
            engine->finalizeObject();
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
                auto const entry_address{resolve_llvm_function_address(*engine, function_name)};
                auto const raw_entry_address{resolve_llvm_function_address(*engine, raw_function_name)};
                if(entry_address == 0u || raw_entry_address == 0u) [[unlikely]] { return false; }

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
                materialized.entry_address = entry_address;
                materialized.raw_entry_address = raw_entry_address;
            }

            // Store the shared LLVM owners on one record before publishing any member as ready.  Other records contain
            // only addresses into the same engine, so the owner record must remain alive for as long as any grouped
            // address can be called.
            auto const owner_local_function_index{local_function_indices.front_unchecked()};
            auto& owner{storage.materialized_functions.index_unchecked(owner_local_function_index)};
            owner.llvm_context_holder = ::std::move(llvm_context_holder);
            owner.llvm_jit_engine = ::std::move(engine);
            for(auto const local_function_index: local_function_indices)
            {
                auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
                // Release-publish after the shared owner has been installed.  This prevents a racing tiered probe from
                // observing a callable address before the object lifetime anchor is stored in the lazy table.
                store_lazy_materialized_ready(materialized, true, ::std::memory_order_release);
            }
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
                                                             ::uwvm2::utils::container::vector<::std::size_t>& out) noexcept
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
                if(!collect_direct_defined_callees(curr_module, out.index_unchecked(cursor), callees)) { continue; }

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
                                                                    ::uwvm2::utils::container::vector<::std::size_t>& out) noexcept
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
                if(!collect_unwind_defined_callees(curr_module, local_index, callees)) { continue; }

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
            ::uwvm2::utils::container::vector<::std::size_t> candidate_group{};
            if(options.compile_options.emit_unwind_call_stack_frames)
            {
                collect_lazy_unwind_direct_call_group(curr_module, storage, entry_local_function_index, candidate_group);
            }
            else if(::uwvm2::runtime::llvm_jit_cache::default_cache_policy().enable)
            {
                // Opportunistic lazy groups depend on scheduler timing and existing cache warmth.  Cache-enabled lazy JIT
                // uses one stable demanded function per cache object; cache-disabled mode keeps group warmup below.
                candidate_group.push_back(entry_local_function_index);
            }
            else
            {
                collect_lazy_direct_call_group(curr_module, storage, entry_local_function_index, candidate_group);
            }
            if(candidate_group.empty()) { candidate_group.push_back(entry_local_function_index); }

            // Convert heuristic candidates into owned compile claims.  The demanded entry function may already be in
            // `compiling` state because the caller claimed it before entering this helper.
            ::uwvm2::utils::container::vector<::std::size_t> claimed_group{};
            claimed_group.reserve(candidate_group.size());
            for(auto const local_function_index: candidate_group)
            {
                if(local_function_index >= storage.functions.size() || local_function_index >= storage.materialized_functions.size()) [[unlikely]] { continue; }

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
            auto const use_direct_wasm_calls_for_claimed_group{options.compile_options.emit_unwind_call_stack_frames &&
                                                               claimed_group.size() == candidate_group.size()};

# ifdef UWVM_CPP_EXCEPTIONS
            try
# endif
            {
                // Validation is performed before LLVM IR emission so validation errors cannot leave a partially ready
                // native code record behind.
                for(auto const local_function_index: claimed_group) { validate_function_if_needed(curr_module, options, local_function_index, err); }

                compile_option emit_options{options.compile_options};
                emit_options.route_wasm_calls_through_runtime_bridge = !use_direct_wasm_calls_for_claimed_group;
                llvm_jit_module_storage_t llvm_ir_storage{};
                if(!all_details::try_prepare_runtime_llvm_jit_module_storage(curr_module, llvm_ir_storage, emit_options.emit_unwind_call_stack_frames))
                    [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }
                for(auto const local_function_index: claimed_group)
                {
                    auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
                    materialized.local_func = all_details::compile_all_from_uwvm_local_func(curr_module,
                                                                                            storage.validation_module,
                                                                                            emit_options,
                                                                                            local_function_index,
                                                                                            err,
                                                                                            ::std::addressof(llvm_ir_storage));
                }
                llvm_ir_storage.emitted = llvm_ir_storage.llvm_context_holder != nullptr && llvm_ir_storage.llvm_module != nullptr;

                // MCJIT materialization consumes the IR module and resolves all function symbols before publication.
                if(!materialize_lazy_local_function_group(curr_module, storage, options, claimed_group, llvm_ir_storage)) [[unlikely]]
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
                // Wake any waiters with a terminal failure state before propagating the validation/emission exception.
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
            if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
            if(load_lazy_materialized_ready(materialized, ::std::memory_order_acquire)) { return; }

            validate_function_if_needed(curr_module, options, local_function_index, err);

            compile_option emit_options{options.compile_options};
            emit_options.route_wasm_calls_through_runtime_bridge = true;
            llvm_jit_module_storage_t llvm_ir_storage{};
            if(!all_details::try_prepare_runtime_llvm_jit_module_storage(curr_module, llvm_ir_storage, emit_options.emit_unwind_call_stack_frames)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
            materialized.local_func = all_details::compile_all_from_uwvm_local_func(curr_module,
                                                                                    storage.validation_module,
                                                                                    emit_options,
                                                                                    local_function_index,
                                                                                    err,
                                                                                    ::std::addressof(llvm_ir_storage));
            llvm_ir_storage.emitted = llvm_ir_storage.llvm_context_holder != nullptr && llvm_ir_storage.llvm_module != nullptr;

            if(!materialize_lazy_local_function(curr_module, storage, options, local_function_index, llvm_ir_storage)) [[unlikely]]
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
    inline constexpr lazy_module_storage_t initialize_lazy_module_storage(runtime_module_storage_t const& curr_module,
                                                                          compile_option const& options,
                                                                          ::uwvm2::validation::error::code_validation_error_impl& err,
                                                                          [[maybe_unused]] lazy_split_config split_config = {}) UWVM_THROWS
    {
        lazy_module_storage_t storage{};
        storage.validation_module = details::all_details::build_runtime_validation_module(curr_module);

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

    // Public wrapper for direct-callee discovery, primarily used by runtime prefetch/tiering heuristics.
    [[nodiscard]] inline constexpr bool collect_direct_defined_callees(runtime_module_storage_t const& curr_module,
                                                                       ::std::size_t local_function_index,
                                                                       ::uwvm2::utils::container::vector<::std::size_t>& callees) noexcept
    { return details::collect_direct_defined_callees(curr_module, local_function_index, callees); }

    // Synchronously compiles the compile unit identified by `compile_unit_index`.  Invalid indices and failed
    // materialization are fatal because callers use this path when execution cannot proceed without native code.
    inline constexpr void compile_cu_from_lazy_validator(runtime_module_storage_t const& curr_module,
                                                         lazy_module_storage_t& storage,
                                                         lazy_compile_options& options,
                                                         ::std::size_t compile_unit_index,
                                                         ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
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
