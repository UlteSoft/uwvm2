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
# include <atomic>
# include <cstddef>
# include <cstdint>
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
# endif
// import
# include <fast_io.h>
# include <uwvm2/uwvm_predefine/io/impl.h>
# include <uwvm2/uwvm_predefine/utils/ansies/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/thread/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/validation/error/impl.h>
# include <uwvm2/validation/standard/wasm1/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_RUNTIME_LLVM_JIT)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator
{
    using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
    using parser_module_storage_t = ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_module_storage_t;
    using full_function_symbol_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::full_function_symbol_t;
    using local_func_storage_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t;
    using compile_option = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option;
    using llvm_jit_module_storage_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_module_storage_t;
    using validation_module_storage_t = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details::validation_module_storage_t;

    enum class lazy_validation_mode : unsigned
    {
        validate_on_lazy_compile,
        assume_full_code_verified
    };

    enum class lazy_compile_unit_kind : unsigned
    {
        function
    };

    [[nodiscard]] inline constexpr ::fast_io::u8string_view lazy_compile_unit_kind_name(lazy_compile_unit_kind kind) noexcept
    {
        switch(kind)
        {
            case lazy_compile_unit_kind::function: return u8"function";
            [[unlikely]] default: return u8"unknown";
        }
    }

    [[nodiscard]] inline constexpr ::fast_io::u8string_view compile_state_name(::uwvm2::utils::thread::lazy_compile_state st) noexcept
    {
        switch(st)
        {
            case ::uwvm2::utils::thread::lazy_compile_state::uncompiled: return u8"uncompiled";
            case ::uwvm2::utils::thread::lazy_compile_state::queued: return u8"queued";
            case ::uwvm2::utils::thread::lazy_compile_state::compiling: return u8"compiling";
            case ::uwvm2::utils::thread::lazy_compile_state::compiled: return u8"compiled";
            case ::uwvm2::utils::thread::lazy_compile_state::failed: return u8"failed";
            [[unlikely]] default: return u8"unknown";
        }
    }

    namespace lazy_runtime_log
    {
        [[nodiscard]] inline bool enabled() noexcept
        {
# ifdef UWVM
            return ::uwvm2::uwvm::io::enable_runtime_log;
# else
            return false;
# endif
        }

        template <typename... Args>
        inline void line(Args&&... args) noexcept
        {
# ifdef UWVM
            if(!enabled()) { return; }

            auto u8runtime_log_output_osr{::fast_io::operations::output_stream_ref(::uwvm2::uwvm::io::u8runtime_log_output)};
            ::fast_io::operations::decay::stream_ref_decay_lock_guard u8runtime_log_output_lg{
                ::fast_io::operations::decay::output_stream_mutex_ref_decay(u8runtime_log_output_osr)};
            auto u8runtime_log_output_ul{::fast_io::operations::decay::output_stream_unlocked_ref_decay(u8runtime_log_output_osr)};

            ::fast_io::io::perrln(u8runtime_log_output_ul, u8"[llvm-jit-lazy] ", ::std::forward<Args>(args)...);
# else
            ((void)args, ...);
# endif
        }
    }  // namespace lazy_runtime_log

    struct lazy_split_config
    {
        ::std::size_t cu_code_size{4096uz};
    };

    struct lazy_compile_unit_storage_t
    {
        ::uwvm2::utils::thread::lazy_compile_unit_state state{};
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        ::std::byte const* code_begin{};
        ::std::byte const* code_end{};
        ::std::size_t code_offset{};
        ::std::size_t code_size{};
        lazy_compile_unit_kind kind{lazy_compile_unit_kind::function};
    };

    struct lazy_function_storage_t
    {
        ::uwvm2::utils::thread::lazy_compile_unit_state materialization_state{};
        ::std::size_t function_index{};
        ::std::size_t local_function_index{};
        ::std::size_t primary_cu_index{SIZE_MAX};
    };

    struct lazy_materialized_function_storage_t
    {
        local_func_storage_t local_func{};
        ::std::unique_ptr<::llvm::LLVMContext> llvm_context_holder{};
        ::std::unique_ptr<::llvm::ExecutionEngine> llvm_jit_engine{};
        ::std::uintptr_t entry_address{};
        ::std::uintptr_t raw_entry_address{};
        bool ready{};

        lazy_materialized_function_storage_t() = default;
        lazy_materialized_function_storage_t(lazy_materialized_function_storage_t const&) = delete;
        lazy_materialized_function_storage_t& operator= (lazy_materialized_function_storage_t const&) = delete;
        lazy_materialized_function_storage_t(lazy_materialized_function_storage_t&&) noexcept = default;
        lazy_materialized_function_storage_t& operator= (lazy_materialized_function_storage_t&&) noexcept = default;
    };

    struct lazy_module_storage_t
    {
        full_function_symbol_t compiled{};
        validation_module_storage_t validation_module{};
        ::uwvm2::utils::container::vector<lazy_function_storage_t> functions{};
        ::uwvm2::utils::container::vector<lazy_compile_unit_storage_t> compile_units{};
        ::uwvm2::utils::container::vector<lazy_materialized_function_storage_t> materialized_functions{};
    };

    struct lazy_compile_options
    {
        compile_option compile_options{};
        parser_module_storage_t const* validator_module_storage{};
        lazy_validation_mode validation_mode{lazy_validation_mode::validate_on_lazy_compile};
    };

    struct lazy_compile_request_context
    {
        runtime_module_storage_t const* curr_module{};
        lazy_module_storage_t* lazy_storage{};
        lazy_compile_options options{};
        ::std::size_t compile_unit_index{};
        ::uwvm2::validation::error::code_validation_error_impl* err{};
        ::uwvm2::utils::container::u8string_view module_name{};
    };

    namespace details
    {
        namespace all_compile = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
        namespace all_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

        [[nodiscard]] inline constexpr ::std::size_t byte_offset(::std::byte const* base, ::std::byte const* curr) noexcept
        { return static_cast<::std::size_t>(curr - base); }

        inline void append_lazy_function(runtime_module_storage_t const& curr_module,
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

            storage.functions.index_unchecked(local_function_index) = {.function_index = function_index,
                                                                       .local_function_index = local_function_index,
                                                                       .primary_cu_index = cu_index};
            storage.compiled.local_funcs.index_unchecked(local_function_index) = local_func_storage;
        }

        inline void mark_function_compile_units_state(lazy_module_storage_t& storage,
                                                      lazy_function_storage_t& fn,
                                                      ::uwvm2::utils::thread::lazy_compile_state state) noexcept
        {
            fn.materialization_state.state.store(state, ::std::memory_order_release);
            if(fn.primary_cu_index < storage.compile_units.size())
            {
                storage.compile_units.index_unchecked(fn.primary_cu_index).state.state.store(state, ::std::memory_order_release);
            }
        }

        inline void validate_function_if_needed(runtime_module_storage_t const& curr_module,
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

        [[nodiscard]] inline bool ensure_llvm_jit_native_target_initialized() noexcept
        {
            static ::std::atomic_bool initialized{};
            static ::std::atomic_bool success{};
            static ::std::atomic_flag init_lock = ATOMIC_FLAG_INIT;

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
                auto const ok{!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() &&
                              !::llvm::InitializeNativeTargetAsmParser()};
                success.store(ok, ::std::memory_order_release);
                initialized.store(true, ::std::memory_order_release);
            }

            init_lock.clear(::std::memory_order_release);
            return success.load(::std::memory_order_acquire);
        }

        [[nodiscard]] inline ::uwvm2::utils::container::vector<::uwvm2::utils::container::string> get_llvm_jit_host_target_attribute_storage()
        {
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::string> mattrs{};
            auto host_features{::llvm::sys::getHostCPUFeatures()};
            if(!host_features.empty())
            {
                mattrs.reserve(host_features.size());
                for(auto const& [feature_name, feature_enabled]: host_features)
                {
                    mattrs.push_back(::uwvm2::utils::container::concat_uwvm(
                        feature_enabled ? "+" : "-",
                        ::uwvm2::utils::container::string_view{feature_name.data(), feature_name.size()}));
                }
            }
            return mattrs;
        }

        inline void append_llvm_jit_host_target_attribute_refs(
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::string> const& attr_storage,
            ::llvm::SmallVector<::llvm::StringRef, 16>& attr_refs)
        {
            attr_refs.clear();
            attr_refs.reserve(attr_storage.size());
            for(auto const& attr: attr_storage) { attr_refs.push_back(all_details::get_llvm_string_ref(attr)); }
        }

        [[nodiscard]] inline bool optimize_lazy_llvm_jit_module(::llvm::Module& module, ::llvm::TargetMachine& target_machine) noexcept
        {
            static_cast<void>(target_machine);
            if(::llvm::verifyModule(module)) [[unlikely]] { return false; }

            return true;
        }

        [[nodiscard]] inline ::std::uintptr_t resolve_llvm_function_address(::llvm::ExecutionEngine& engine,
                                                                            ::uwvm2::utils::container::string const& function_name) noexcept
        {
            auto found_function{engine.FindFunctionNamed(all_details::get_llvm_string_ref(function_name))};
            if(found_function == nullptr || found_function->isDeclaration()) [[unlikely]] { return 0u; }

            auto const function_address{engine.getPointerToFunction(found_function)};
            return function_address == nullptr ? 0u : reinterpret_cast<::std::uintptr_t>(function_address);
        }

        [[nodiscard]] inline bool materialize_lazy_local_function(runtime_module_storage_t const& curr_module,
                                                                  lazy_module_storage_t& storage,
                                                                  ::std::size_t local_function_index,
                                                                  llvm_jit_module_storage_t& llvm_ir_storage) noexcept
        {
            if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }
            auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
            materialized.ready = false;
            materialized.entry_address = 0u;
            materialized.raw_entry_address = 0u;
            materialized.llvm_jit_engine.reset();
            materialized.llvm_context_holder.reset();

            if(!llvm_ir_storage.emitted || llvm_ir_storage.llvm_context_holder == nullptr || llvm_ir_storage.llvm_module == nullptr) [[unlikely]]
            {
                return false;
            }
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]] { return false; }

            auto llvm_context_holder{::std::move(llvm_ir_storage.llvm_context_holder)};
            auto llvm_module{::std::move(llvm_ir_storage.llvm_module)};
            llvm_ir_storage.emitted = false;

            if(llvm_context_holder == nullptr || llvm_module == nullptr) [[unlikely]] { return false; }

            auto const host_cpu_name{::llvm::sys::getHostCPUName()};
            auto const host_target_attribute_storage{get_llvm_jit_host_target_attribute_storage()};
            ::llvm::SmallVector<::llvm::StringRef, 16> host_target_attributes{};
            append_llvm_jit_host_target_attribute_refs(host_target_attribute_storage, host_target_attributes);

            ::llvm::EngineBuilder target_builder{};
            target_builder.setEngineKind(::llvm::EngineKind::JIT)
                .setOptLevel(::llvm::CodeGenOptLevel::None)
                .setMCPU(host_cpu_name)
                .setMAttrs(host_target_attributes);

            ::std::unique_ptr<::llvm::TargetMachine> target_machine{target_builder.selectTarget()};
            if(target_machine == nullptr) [[unlikely]] { return false; }

            llvm_module->setTargetTriple(target_machine->getTargetTriple());
            llvm_module->setDataLayout(target_machine->createDataLayout());
            if(!optimize_lazy_llvm_jit_module(*llvm_module, *target_machine)) [[unlikely]] { return false; }

            auto raw_engine{::llvm::EngineBuilder(::std::move(llvm_module))
                                .setEngineKind(::llvm::EngineKind::JIT)
                                .setOptLevel(::llvm::CodeGenOptLevel::None)
                                .setMCPU(host_cpu_name)
                                .setMAttrs(host_target_attributes)
                                .setMCJITMemoryManager(::std::make_unique<::llvm::SectionMemoryManager>())
                                .create(target_machine.release())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }

            ::std::unique_ptr<::llvm::ExecutionEngine> engine{raw_engine};
            engine->finalizeObject();

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

            materialized.entry_address = entry_address;
            materialized.raw_entry_address = raw_entry_address;
            materialized.llvm_context_holder = ::std::move(llvm_context_holder);
            materialized.llvm_jit_engine = ::std::move(engine);
            materialized.ready = true;
            return true;
        }

        inline void compile_lazy_local_function(runtime_module_storage_t const& curr_module,
                                                lazy_module_storage_t& storage,
                                                lazy_compile_options& options,
                                                ::std::size_t local_function_index,
                                                ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
        {
            if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
            auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
            if(materialized.ready) { return; }

            validate_function_if_needed(curr_module, options, local_function_index, err);

            llvm_jit_module_storage_t llvm_ir_storage{};
            if(!all_details::try_prepare_runtime_llvm_jit_module_storage(curr_module, llvm_ir_storage)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }

            compile_option emit_options{options.compile_options};
            emit_options.route_wasm_calls_through_runtime_bridge = true;
            materialized.local_func = all_details::compile_all_from_uwvm_local_func(
                curr_module,
                storage.validation_module,
                emit_options,
                local_function_index,
                err,
                ::std::addressof(llvm_ir_storage));
            llvm_ir_storage.emitted = llvm_ir_storage.llvm_context_holder != nullptr && llvm_ir_storage.llvm_module != nullptr;

            if(!materialize_lazy_local_function(curr_module, storage, local_function_index, llvm_ir_storage)) [[unlikely]]
            {
                ::fast_io::fast_terminate();
            }
        }

        inline void lazy_compile_request_entry(void* user_data) noexcept
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
                compile_lazy_local_function(*ctx->curr_module, storage, ctx->options, cu.local_function_index, err);
                mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
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
                                       u8" state=compiled");
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
                mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::failed);
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
                                       u8" state=failed");
            }
# endif
        }
    }  // namespace details

    inline lazy_module_storage_t initialize_lazy_module_storage(runtime_module_storage_t const& curr_module,
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

    inline void compile_cu_from_lazy_validator(runtime_module_storage_t const& curr_module,
                                               lazy_module_storage_t& storage,
                                               lazy_compile_options& options,
                                               ::std::size_t compile_unit_index,
                                               ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        if(compile_unit_index >= storage.compile_units.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

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

        details::compile_lazy_local_function(curr_module, storage, options, cu.local_function_index, err);
        details::mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
        lazy_runtime_log::line(u8"compile-cu-end module_id=",
                               options.compile_options.curr_wasm_id,
                               u8" local_fn=",
                               cu.local_function_index,
                               u8" cu=",
                               compile_unit_index,
                               u8" state=compiled");
    }

    [[nodiscard]] inline ::uwvm2::utils::thread::lazy_compile_request make_lazy_compile_request(lazy_compile_request_context& ctx,
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

    [[nodiscard]] inline bool try_get_lazy_raw_entry_address(lazy_module_storage_t const& storage,
                                                             ::std::size_t local_function_index,
                                                             ::std::uintptr_t& function_address) noexcept
    {
        function_address = 0u;
        if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }

        auto const& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
        if(!materialized.ready || materialized.raw_entry_address == 0u) { return false; }

        function_address = materialized.raw_entry_address;
        return true;
    }

    [[nodiscard]] inline bool try_get_lazy_entry_address(lazy_module_storage_t const& storage,
                                                         ::std::size_t local_function_index,
                                                         ::std::uintptr_t& function_address) noexcept
    {
        function_address = 0u;
        if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }

        auto const& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
        if(!materialized.ready || materialized.entry_address == 0u) { return false; }

        function_address = materialized.entry_address;
        return true;
    }
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/uwvm_predefine/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
