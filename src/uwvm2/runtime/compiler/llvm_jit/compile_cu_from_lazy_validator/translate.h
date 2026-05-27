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
            case lazy_compile_unit_kind::function:
                return u8"function";
            [[unlikely]] default:
                return u8"unknown";
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
            case ::uwvm2::utils::thread::lazy_compile_state::failed:
                return u8"failed";
            [[unlikely]] default:
                return u8"unknown";
        }
    }

    namespace lazy_runtime_log
    {
        [[nodiscard]] inline constexpr bool enabled() noexcept
        {
# ifdef UWVM
            return ::uwvm2::uwvm::io::enable_runtime_log;
# else
            return false;
# endif
        }

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

    struct lazy_split_config
    { ::std::size_t cu_code_size{4096uz}; };

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
        ::uwvm2::utils::container::owned_ptr<::llvm::LLVMContext> llvm_context_holder{};
        ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> llvm_jit_engine{};
        ::std::uintptr_t entry_address{};
        ::std::uintptr_t raw_entry_address{};
        ::uwvm2::utils::container::deque<::std::uintptr_t> tiered_osr_entry_addresses{};
        ::uwvm2::utils::container::deque<::std::size_t> tiered_osr_loop_slots{};
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
        ::llvm::CodeGenOptLevel codegen_opt_level{::llvm::CodeGenOptLevel::Less};
    };

    struct lazy_compile_request_context
    {
        runtime_module_storage_t const* curr_module{};
        lazy_module_storage_t* lazy_storage{};
        lazy_compile_options options{};
        ::std::size_t compile_unit_index{};
        ::uwvm2::validation::error::code_validation_error_impl* err{};
        ::uwvm2::utils::container::u8string_view module_name{};
        void (*publish_materialized_function)(void*, ::std::size_t) noexcept {};
        void* publish_user_data{};
    };

    namespace details
    {
        template <typename>
        struct member_function_first_argument;

        template <typename R, typename C, typename A0>
        struct member_function_first_argument<R (C::*)(A0)>
        {
            using type = A0;
        };

        using llvm_module_owner_t = typename member_function_first_argument<decltype(&::llvm::ExecutionEngine::addModule)>::type;

        namespace all_compile = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
        namespace all_details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

        struct llvm_jit_native_target_config
        {
            ::uwvm2::utils::container::string cpu_name{};
            ::uwvm2::utils::container::vector<::uwvm2::utils::container::string> feature_storage{};
        };

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

            storage.functions.index_unchecked(
                local_function_index) = {.function_index = function_index, .local_function_index = local_function_index, .primary_cu_index = cu_index};
            storage.compiled.local_funcs.index_unchecked(local_function_index) = local_func_storage;
        }

        inline void mark_function_compile_units_state(lazy_module_storage_t& storage,
                                                      lazy_function_storage_t& fn,
                                                      ::uwvm2::utils::thread::lazy_compile_state state) noexcept
        {
            fn.materialization_state.state.store(state, ::std::memory_order_release);
            ::uwvm2::utils::thread::lazy_compile_notify_unit(fn.materialization_state);
            if(fn.primary_cu_index < storage.compile_units.size())
            {
                auto& cu_state{storage.compile_units.index_unchecked(fn.primary_cu_index).state};
                cu_state.state.store(state, ::std::memory_order_release);
                ::uwvm2::utils::thread::lazy_compile_notify_unit(cu_state);
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
                auto const ok{!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() && !::llvm::InitializeNativeTargetAsmParser()};
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
                    mattrs.push_back(::uwvm2::utils::container::concat_uwvm(feature_enabled ? "+" : "-",
                                                                            ::uwvm2::utils::container::string_view{feature_name.data(), feature_name.size()}));
                }
            }
            return mattrs;
        }

        [[nodiscard]] inline llvm_jit_native_target_config const& get_llvm_jit_native_target_config()
        {
            static llvm_jit_native_target_config config{.cpu_name =
                                                            []()
                                                        {
                                                            auto const host_cpu_name{::llvm::sys::getHostCPUName()};
                                                            return ::uwvm2::utils::container::string{
                                                                ::uwvm2::utils::container::string_view{host_cpu_name.data(), host_cpu_name.size()}
                                                            };
                                                        }(),
                                                        .feature_storage = get_llvm_jit_host_target_attribute_storage()};
            return config;
        }

        inline void append_llvm_jit_host_target_attribute_refs(::uwvm2::utils::container::vector<::uwvm2::utils::container::string> const& attr_storage,
                                                               ::llvm::SmallVector<::llvm::StringRef, 16>& attr_refs)
        {
            attr_refs.clear();
            attr_refs.reserve(attr_storage.size());
            for(auto const& attr: attr_storage) { attr_refs.push_back(all_details::get_llvm_string_ref(attr)); }
        }

        [[nodiscard]] inline bool skip_wasm_instruction_for_direct_call_scan(::std::byte const*& code_curr, ::std::byte const* code_end) noexcept
        {
            if(code_curr == code_end) [[unlikely]] { return false; }

            all_details::wasm1_code op{};
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

        inline void append_unique_local_function_index(::uwvm2::utils::container::vector<::std::size_t>& out, ::std::size_t local_function_index)
        {
            if(::std::find(out.begin(), out.end(), local_function_index) == out.end()) { out.push_back(local_function_index); }
        }

        [[nodiscard]] inline bool collect_direct_defined_callees(runtime_module_storage_t const& curr_module,
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

        [[nodiscard]] inline bool optimize_lazy_llvm_jit_module(::llvm::Module& module,
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
                                                                  lazy_compile_options const& options,
                                                                  ::std::size_t local_function_index,
                                                                  llvm_jit_module_storage_t& llvm_ir_storage) noexcept
        {
            if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }
            auto& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
            materialized.ready = false;
            materialized.entry_address = 0u;
            materialized.raw_entry_address = 0u;
            materialized.tiered_osr_entry_addresses.clear();
            materialized.tiered_osr_loop_slots.clear();
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

            llvm_module->setTargetTriple(target_machine->getTargetTriple());
            llvm_module->setDataLayout(target_machine->createDataLayout());
            if(!optimize_lazy_llvm_jit_module(*llvm_module, *target_machine, options.codegen_opt_level, options.compile_options.verify_llvm_jit_ir))
                [[unlikely]]
            {
                return false;
            }

            auto raw_engine{::llvm::EngineBuilder(details::llvm_module_owner_t{llvm_module.release()})
                                .setEngineKind(::llvm::EngineKind::JIT)
                                .setOptLevel(options.codegen_opt_level)
                                .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                                .setMAttrs(host_target_attributes)
                                .create(target_machine.get())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }
            static_cast<void>(target_machine.release());

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};
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

            materialized.tiered_osr_loop_slots = materialized.local_func.tiered_osr_loop_slots;
            for(auto const slot: materialized.tiered_osr_loop_slots)
            {
                auto const osr_function_name{all_details::get_llvm_wasm_tiered_osr_function_name(curr_module, function_index_u32, slot)};
                auto const osr_entry_address{resolve_llvm_function_address(*engine, osr_function_name)};
                if(osr_entry_address == 0u) [[unlikely]]
                {
                    materialized.tiered_osr_entry_addresses.clear();
                    materialized.tiered_osr_loop_slots.clear();
                    return false;
                }
                materialized.tiered_osr_entry_addresses.push_back(osr_entry_address);
            }

            materialized.entry_address = entry_address;
            materialized.raw_entry_address = raw_entry_address;
            materialized.llvm_context_holder = ::std::move(llvm_context_holder);
            materialized.llvm_jit_engine = ::std::move(engine);
            materialized.ready = true;
            return true;
        }

        [[nodiscard]] inline bool materialize_lazy_local_function_group(runtime_module_storage_t const& curr_module,
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
                materialized.ready = false;
                materialized.entry_address = 0u;
                materialized.raw_entry_address = 0u;
                materialized.tiered_osr_entry_addresses.clear();
                materialized.tiered_osr_loop_slots.clear();
                materialized.llvm_jit_engine.reset();
                materialized.llvm_context_holder.reset();
            }

            if(!llvm_ir_storage.emitted || llvm_ir_storage.llvm_context_holder == nullptr || llvm_ir_storage.llvm_module == nullptr) [[unlikely]]
            {
                return false;
            }
            if(!ensure_llvm_jit_native_target_initialized()) [[unlikely]] { return false; }

            auto llvm_context_holder{::std::move(llvm_ir_storage.llvm_context_holder)};
            auto llvm_module{::std::move(llvm_ir_storage.llvm_module)};
            llvm_ir_storage.emitted = false;

            if(llvm_context_holder == nullptr || llvm_module == nullptr) [[unlikely]] { return false; }

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

            llvm_module->setTargetTriple(target_machine->getTargetTriple());
            llvm_module->setDataLayout(target_machine->createDataLayout());
            if(!optimize_lazy_llvm_jit_module(*llvm_module, *target_machine, options.codegen_opt_level, options.compile_options.verify_llvm_jit_ir))
                [[unlikely]]
            {
                return false;
            }

            auto raw_engine{::llvm::EngineBuilder(details::llvm_module_owner_t{llvm_module.release()})
                                .setEngineKind(::llvm::EngineKind::JIT)
                                .setOptLevel(options.codegen_opt_level)
                                .setMCPU(all_details::get_llvm_string_ref(target_config.cpu_name))
                                .setMAttrs(host_target_attributes)
                                .create(target_machine.get())};
            if(raw_engine == nullptr) [[unlikely]] { return false; }
            static_cast<void>(target_machine.release());

            ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine{raw_engine};
            engine->finalizeObject();

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
                materialized.tiered_osr_loop_slots = materialized.local_func.tiered_osr_loop_slots;
                for(auto const slot: materialized.tiered_osr_loop_slots)
                {
                    auto const osr_function_name{all_details::get_llvm_wasm_tiered_osr_function_name(curr_module, function_index_u32, slot)};
                    auto const osr_entry_address{resolve_llvm_function_address(*engine, osr_function_name)};
                    if(osr_entry_address == 0u) [[unlikely]]
                    {
                        materialized.tiered_osr_entry_addresses.clear();
                        materialized.tiered_osr_loop_slots.clear();
                        return false;
                    }
                    materialized.tiered_osr_entry_addresses.push_back(osr_entry_address);
                }
                materialized.entry_address = entry_address;
                materialized.raw_entry_address = raw_entry_address;
                materialized.ready = true;
            }

            auto const owner_local_function_index{local_function_indices.front_unchecked()};
            auto& owner{storage.materialized_functions.index_unchecked(owner_local_function_index)};
            owner.llvm_context_holder = ::std::move(llvm_context_holder);
            owner.llvm_jit_engine = ::std::move(engine);
            return true;
        }

        [[nodiscard]] inline ::std::size_t lazy_group_function_code_size(lazy_module_storage_t const& storage, ::std::size_t local_function_index) noexcept
        {
            if(local_function_index >= storage.functions.size()) [[unlikely]] { return 0uz; }
            auto const& fn{storage.functions.index_unchecked(local_function_index)};
            if(fn.primary_cu_index >= storage.compile_units.size()) [[unlikely]] { return 0uz; }
            return storage.compile_units.index_unchecked(fn.primary_cu_index).code_size;
        }

        [[nodiscard]] inline bool lazy_group_contains_function(::uwvm2::utils::container::vector<::std::size_t> const& out,
                                                               ::std::size_t local_function_index) noexcept
        { return ::std::find(out.begin(), out.end(), local_function_index) != out.end(); }

        [[nodiscard]] inline bool should_consider_lazy_group_function(lazy_module_storage_t const& storage, ::std::size_t local_function_index) noexcept
        {
            if(local_function_index >= storage.functions.size()) [[unlikely]] { return false; }
            auto const st{storage.functions.index_unchecked(local_function_index).materialization_state.state.load(::std::memory_order_acquire)};
            return st == ::uwvm2::utils::thread::lazy_compile_state::uncompiled || st == ::uwvm2::utils::thread::lazy_compile_state::queued;
        }

        [[nodiscard]] inline bool try_append_lazy_group_function(::uwvm2::utils::container::vector<::std::size_t>& out,
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

        inline void append_lazy_group_adjacent_functions(lazy_module_storage_t& storage,
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

        inline void collect_lazy_direct_call_group(runtime_module_storage_t const& curr_module,
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

        inline void compile_lazy_local_function_group(runtime_module_storage_t const& curr_module,
                                                      lazy_module_storage_t& storage,
                                                      lazy_compile_options& options,
                                                      ::std::size_t entry_local_function_index,
                                                      ::uwvm2::validation::error::code_validation_error_impl& err,
                                                      void (*publish_materialized_function)(void*, ::std::size_t) noexcept = nullptr,
                                                      void* publish_user_data = nullptr) UWVM_THROWS
        {
            ::uwvm2::utils::container::vector<::std::size_t> candidate_group{};
            collect_lazy_direct_call_group(curr_module, storage, entry_local_function_index, candidate_group);
            if(candidate_group.empty()) { candidate_group.push_back(entry_local_function_index); }

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
                for(auto const local_function_index: claimed_group) { validate_function_if_needed(curr_module, options, local_function_index, err); }

                llvm_jit_module_storage_t llvm_ir_storage{};
                if(!all_details::try_prepare_runtime_llvm_jit_module_storage(curr_module, llvm_ir_storage)) [[unlikely]] { ::fast_io::fast_terminate(); }

                compile_option emit_options{options.compile_options};
                emit_options.route_wasm_calls_through_runtime_bridge = true;
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

                if(!materialize_lazy_local_function_group(curr_module, storage, options, claimed_group, llvm_ir_storage)) [[unlikely]]
                {
                    ::fast_io::fast_terminate();
                }

                for(auto const local_function_index: claimed_group)
                {
                    auto& fn{storage.functions.index_unchecked(local_function_index)};
                    if(publish_materialized_function != nullptr) { publish_materialized_function(publish_user_data, local_function_index); }
                    mark_function_compile_units_state(storage, fn, ::uwvm2::utils::thread::lazy_compile_state::compiled);
                }
            }
# ifdef UWVM_CPP_EXCEPTIONS
            catch(...)
            {
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
            if(!all_details::try_prepare_runtime_llvm_jit_module_storage(curr_module, llvm_ir_storage)) [[unlikely]] { ::fast_io::fast_terminate(); }

            compile_option emit_options{options.compile_options};
            emit_options.route_wasm_calls_through_runtime_bridge = true;
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

    [[nodiscard]] inline bool collect_direct_defined_callees(runtime_module_storage_t const& curr_module,
                                                             ::std::size_t local_function_index,
                                                             ::uwvm2::utils::container::vector<::std::size_t>& callees) noexcept
    { return details::collect_direct_defined_callees(curr_module, local_function_index, callees); }

    inline void compile_cu_from_lazy_validator(runtime_module_storage_t const& curr_module,
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

        details::compile_lazy_local_function_group(curr_module, storage, options, cu.local_function_index, err);
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

    [[nodiscard]] inline ::uwvm2::utils::thread::lazy_compile_request make_lazy_compile_request(lazy_compile_request_context & ctx,
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

    [[nodiscard]] inline bool try_get_lazy_tiered_osr_entries(lazy_module_storage_t const& storage,
                                                              ::std::size_t local_function_index,
                                                              ::uwvm2::utils::container::deque<::std::size_t> const*& slots,
                                                              ::uwvm2::utils::container::deque<::std::uintptr_t> const*& entry_addresses) noexcept
    {
        slots = nullptr;
        entry_addresses = nullptr;
        if(local_function_index >= storage.materialized_functions.size()) [[unlikely]] { return false; }

        auto const& materialized{storage.materialized_functions.index_unchecked(local_function_index)};
        if(!materialized.ready || materialized.tiered_osr_loop_slots.empty() ||
           materialized.tiered_osr_loop_slots.size() != materialized.tiered_osr_entry_addresses.size())
        {
            return false;
        }

        slots = ::std::addressof(materialized.tiered_osr_loop_slots);
        entry_addresses = ::std::addressof(materialized.tiered_osr_entry_addresses);
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
