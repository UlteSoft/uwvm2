/*************************************************************
 * Private source-only foundation candidate; actual project APIs.
 *************************************************************/

#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <memory>
# include <string>
# include <utility>
# include <vector>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/control/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
# include <uwvm2/uwvm/runtime/initializer/impl.h>
# include <uwvm2/uwvm/runtime/runtime_mode/impl.h>
# include <uwvm2/runtime/lib/uwvm_runtime.h>
# include "loader.h"
# include "retval.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::run
{
    // Actual owning CLI preparation: real parse/init occurs once in the
    // existing drained maintenance callback. Numeric eligibility is decided
    // later by the actual full validator/publisher, with native fallback.
    // Native administration/flags/preload mutations are externally serialized.
    [[nodiscard]] inline int prepare_owned_full_cli_source(bool retain_immutable_image = false) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           !::uwvm2::uwvm::cmdline::wasm_file_ppos ||
           ::uwvm2::uwvm::wasm::storage::preloaded_wasm.size() > 4095u)
        { return static_cast<int>(retval::parameter_error); }
# if defined(UWVM_SUPPORT_PRELOAD_DL)
        if(!::uwvm2::uwvm::wasm::storage::preloaded_dl.empty()) { return static_cast<int>(retval::parameter_error); }
# endif
# if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        if(!::uwvm2::uwvm::wasm::storage::weak_symbol.empty()) { return static_cast<int>(retval::parameter_error); }
# endif
        struct context
        {
            // Real owned inputs captured before the old selection is retired.
            ::std::u8string path, rename;
            ::uwvm2::utils::control::owned_file_image::owner image{};
            ::std::vector<::uwvm2::uwvm::runtime::full::full_preload_input> preloads{};
            mode::runtime_llvm_jit_exception_dispatch_t dispatch{mode::global_runtime_llvm_jit_exception_dispatch};
            int result{static_cast<int>(retval::parameter_error)};
        } state{};
        try
        {
            auto const path{::uwvm2::uwvm::cmdline::wasm_file_ppos->str};
            auto const rename{::uwvm2::uwvm::wasm::storage::execute_wasm.module_name};
            state.path = ::fast_io::u8concat_std(path);
            state.rename = ::fast_io::u8concat_std(rename);
            if(retain_immutable_image)
            {
                // Cold host input DATA only. Read into private owned bytes BEFORE
                // any parser/initializer body/type/segment pointer can escape.
                // Never hash/copy an already parsed map and pretend its borrows
                // were rebound. This performs no code-body validation/translation.
                auto loaded{::uwvm2::utils::control::owned_file_image::read(state.path,
                    ::uwvm2::utils::control::owned_file_image::maximum_bytes)};
                if(!loaded)
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                        u8"uwvm: [error] Unable to retain immutable debugger Wasm input before parsing; owned-image status=",
                        ::fast_io::mnp::dec(static_cast<unsigned>(loaded.error)), u8".\n\n");
                    return static_cast<int>(retval::load_main_module_error);
                }
                state.image = ::std::move(loaded.image);
            }
            // Copy command-line preload INPUTS before old source selection is
            // retired. Never move/reuse a previously parsed map/Core3 context:
            // every final source-owned preload below is parsed from scratch.
            auto const& original_preloads{::uwvm2::uwvm::wasm::storage::preloaded_wasm};
            state.preloads.reserve(original_preloads.size());
            auto remaining_bytes{::uwvm2::utils::control::owned_file_image::maximum_bytes};
            if(state.image)
            {
                if(state.image->size() > remaining_bytes) { return static_cast<int>(retval::load_main_module_error); }
                remaining_bytes -= state.image->size(); // BEFORE any next private-image allocation.
            }
            for(auto const& original : original_preloads)
            {
                ::uwvm2::uwvm::runtime::full::full_preload_input input{};
                input.file_name = ::fast_io::u8concat_std(original.file_name);
                input.module_name = ::fast_io::u8concat_std(original.module_name);
                input.parameters = original.wasm_parameter; // Exact original per-module feature policy.
                if(retain_immutable_image)
                {
                    auto loaded{::uwvm2::utils::control::owned_file_image::read(input.file_name, remaining_bytes)};
                    if(!loaded || loaded.image->size() > remaining_bytes)
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                            u8"uwvm: [error] Unable to retain immutable debugger preload before parsing; shared owned-image quota=1 GiB.\n\n");
                        return static_cast<int>(retval::load_main_module_error);
                    }
                    remaining_bytes -= loaded.image->size(); input.image = ::std::move(loaded.image);
                }
                state.preloads.push_back(::std::move(input));
            }
            auto const prepare{[](void* pointer) noexcept -> bool
            {
                auto& ctx{*static_cast<context*>(pointer)};
                try
                {
                    auto candidate{::uwvm2::uwvm::runtime::full::full_source_instance::create_unparsed(
                        ::std::move(ctx.path), ::std::move(ctx.rename), ::std::move(ctx.preloads))};
                    if(!::uwvm2::uwvm::runtime::full::select_unparsed_full_source_after_drain(candidate)) { return false; }
                    auto const loaded{::uwvm2::uwvm::wasm::loader::load_wasm_file(
                        candidate->file_for_native_initialization(), candidate->owned_file_name(), candidate->owned_rename(),
                        ::uwvm2::uwvm::wasm::storage::wasm_parameter, ::std::move(ctx.image))};
                    if(loaded != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok)
                    { ctx.result = static_cast<int>(retval::load_main_module_error); return false; }
                    auto& preloads{candidate->preloaded_files_for_native_initialization()};
                    for(::std::size_t index{}; index != preloads.size(); ++index)
                    {
                        // [FINAL source-owned preload vector] end
                        // [safe] index<size BEFORE WF access; vector never grows
                        // after any name/type/body pointer is installed by parser.
                        auto& file{preloads.index_unchecked(index)};
                        auto const loaded_preload{::uwvm2::uwvm::wasm::loader::load_wasm_file(file,
                            file.file_name, file.module_name, file.wasm_parameter,
                            candidate->take_preloaded_image_for_native_initialization(index))};
                        if(loaded_preload != ::uwvm2::uwvm::wasm::loader::load_wasm_file_rtl::ok)
                        { ctx.result = static_cast<int>(retval::load_main_module_error); return false; }
                    }
                    // Native standard pipeline, with original provider behavior.
                    // No guest may execute inside this maintenance callback.
                    if(auto const code{load_local_modules()}; code != static_cast<int>(retval::ok))
                    { ctx.result = code; return false; }
                    if(auto const code{load_weak_symbol_modules()}; code != static_cast<int>(retval::ok))
                    { ctx.result = code; return false; }
                    if(::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() !=
                       ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok ||
                       ::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() !=
                       ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok)
                    { ctx.result = static_cast<int>(retval::check_module_error); return false; }
                    ::uwvm2::uwvm::runtime::initializer::initialize_runtime(true);
                    if(!candidate->seal_actual_initializer()) { return false; }
                    auto const* main{candidate->initialized_main_module()};
                    if(main == nullptr) { return false; }
                    // Numeric admission is a later real full validation decision.
                    // Any unsupported body/import/schema keeps this sealed source
                    // and its normal native ABI; never throw away an initialized
                    // valid module just to rerun ordinary loader/initializer.
                    // This frozen cold request supplies no authority. The original
                    // real validator/factory still decides numeric eligibility.
                    if(ctx.dispatch != mode::runtime_llvm_jit_exception_dispatch_t::native_unwind)
                    { candidate->request_numeric_full_after_actual_initializer(); }
                    ctx.result = static_cast<int>(retval::ok);
                    return true;
                }
                catch(...) { return false; }
            }};
            if(!::uwvm2::runtime::lib::replace_full_source_after_drain_host_api(prepare, ::std::addressof(state)))
            { return state.result == static_cast<int>(retval::ok) ? static_cast<int>(retval::parameter_error) : state.result; }
            return state.result;
        }
        catch(...) { return state.result; }
#else
        static_cast<void>(retain_immutable_image);
        return static_cast<int>(retval::parameter_error);
#endif
    }
}
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
