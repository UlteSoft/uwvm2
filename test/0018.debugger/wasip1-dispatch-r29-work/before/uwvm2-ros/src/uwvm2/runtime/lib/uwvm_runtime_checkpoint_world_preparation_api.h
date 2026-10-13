// Private preparation bridge. Include after manager/world inside runtime::lib.
// No guest import, saved-file proof, execution token or native address.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
extern "C++"
{
    void runtime_checkpoint_coherent_manager::prepare_and_discard_actual_world(
        ::uwvm2::uwvm::debugger::checkpoint::state const& saved,source_owner const& source,
        ::std::uint_least64_t epoch,::uwvm2::runtime::checkpoint::compilation_profile::owner const& current_profile,
        llvm_jit_checkpoint_prepare_request const& request,llvm_jit_checkpoint_prepare_result& out,
        ::std::span<llvm_jit_wasip1_environment_capsule_owner const> environments) noexcept
    {
        using world=runtime_checkpoint_world_transaction;
        namespace checkpoint=::uwvm2::runtime::checkpoint;
        namespace compile=::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
        out.status=llvm_jit_checkpoint_prepare_status::resource_preparation_declined;
        try
        {
            // Private candidates live THROUGH Wasm resource/engine preparation.
            // A later failure destroys all FD clones/text/tables before the
            // same actual closed-host/N/publication scope is released. No swap.
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            llvm_jit_wasip1_environment_capsule::prepared_environment_group wasip1{};
            if(request.include_wasip1 && !environments.empty())
            {
                out.wasip1_status=llvm_jit_wasip1_environment_capsule::prepare_group_current(
                    llvm_jit_wasip1_environment_capsule::native_capture_key{},environments,
                    request.require_managed_wasip1_resources,epoch,current_profile,wasip1);
                if(out.wasip1_status!=llvm_jit_wasip1_environment_capsule_status::captured)
                { out.status=llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined;return; }
            }
#else
            if(!environments.empty())
            { out.status=llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined;return; }
#endif
            auto prepared{world::prepare_actual_resources(saved,current_profile,source,epoch,
                request.maximum_native_payload_bytes,request.maximum_original_source_bytes,request.graph_budget,out)};
            if(!prepared) { return; }
            out.modules=prepared->modules_.size();
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            if(request.include_wasip1 && !environments.empty())
            {
                try
                { out.wasip1_status=prepared->install_private_wasip1_environments(environments,wasip1,request.require_managed_wasip1_resources); }
                catch(::fast_io::error const&)
                { out.wasip1_status=llvm_jit_wasip1_environment_capsule_status::native_operation_failed; }
                if(out.wasip1_status!=llvm_jit_wasip1_environment_capsule_status::captured)
                { out.status=llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined;out.native_payload_bytes=prepared->native_bytes_;return; }
            }
#endif
            out.status=llvm_jit_checkpoint_prepare_status::engine_preparation_declined;
            // No silent conversion of unlimited observation budgets into resume
            // budgets. Every actual schema/ABI/slot/budget field stays identical.
            auto profile{checkpoint::compilation_profile::create_for_trusted_manager(
                current_profile->limits(),checkpoint::compilation_purpose::resumable)};
            auto const policy{runtime_llvm_jit_unwind_call_stack_requested()};
            auto const granularity{g_runtime.debug_granularity==llvm_jit_debug_safe_point_granularity::instruction?
                compile::llvm_jit_debug_safe_point_granularity::instruction:compile::llvm_jit_debug_safe_point_granularity::entry_loop};
            if(!profile || !prepared->prepare_all_native_engines(profile,policy,granularity))
            { out.engine_diagnostic=prepared->engine_diagnostic_;out.native_payload_bytes=prepared->native_bytes_;return; }
            out.engines=prepared->engines_.size();out.functions=prepared->prepared_functions_;
            out.status=llvm_jit_checkpoint_prepare_status::frame_preparation_declined;
            if(!prepared->prepare_all_world_frames())
            { out.frame_diagnostic=static_cast<unsigned>(prepared->phase_);out.native_payload_bytes=prepared->native_bytes_;return; }
            out.prepared_threads=prepared->prepared_threads_.size();out.prepared_frames=prepared->prepared_frames_;
            out.prepared_root_carriers=prepared->prepared_root_carriers_;
            out.native_payload_bytes=prepared->native_bytes_;
            out.status=llvm_jit_checkpoint_prepare_status::root_preparation_declined;
            if(!prepared->validate_private_world_root_installation(request.maximum_private_root_frames,out)) { return; }
            out.status=llvm_jit_checkpoint_prepare_status::prepared_and_discarded;
            out.wasip1_environments=environments.size();
            out.wasip1_prepared_together=request.include_wasip1;
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            out.prepared_wasip1_modules=prepared->wasip1_bound_modules_;
            out.prepared_wasip1_memories=prepared->wasip1_bound_memories_;
            out.prepared_wasip1_shared_modules=prepared->wasip1_shared_modules_;
            out.wasip1_installed_privately=request.include_wasip1 && !environments.empty();
#endif
            // Genuine private root scopes have left before engine/source/store
            // destruction. No restored worker or execution activation was seeded.
            // Engine/range destruction precedes source/stores. Never commit,
            // drain original workers, change VM epoch/initializer or execute.
        }
        catch(...) { out={};out.status=llvm_jit_checkpoint_prepare_status::allocation_failed; }
    }
    extern "C++" llvm_jit_checkpoint_prepare_result llvm_jit_checkpoint_prepare_instance_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        llvm_jit_checkpoint_prepare_request const& requested) noexcept
    {
        namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
        llvm_jit_checkpoint_prepare_result out{};
        if(mode::global_runtime_mode!=mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only)
        { out.status=llvm_jit_checkpoint_prepare_status::requires_llvm_jit_full;return out; }
        if(!ticket || supplied.empty() || supplied.size()>256u)
        { out.status=llvm_jit_checkpoint_prepare_status::unavailable_capture;return out; }
        runtime_checkpoint_thread_capture::owner captured[256u]{};
        for(::std::size_t index{};index<supplied.size();++index)
        {
            auto const actual{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!actual) { out.status=llvm_jit_checkpoint_prepare_status::unavailable_capture;return out; }
            captured[index]=actual->actual_;
        }
        return runtime_checkpoint_coherent_manager::prepare_current_instance(ticket,{captured,supplied.size()},requested);
    }
}
#else
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++" void runtime_checkpoint_coherent_manager::prepare_and_discard_actual_world(
    ::uwvm2::uwvm::debugger::checkpoint::state const&,source_owner const&,
    ::std::uint_least64_t,::uwvm2::runtime::checkpoint::compilation_profile::owner const&,
    llvm_jit_checkpoint_prepare_request const&,llvm_jit_checkpoint_prepare_result& out,
    ::std::span<llvm_jit_wasip1_environment_capsule_owner const>) noexcept
{ out.status=llvm_jit_checkpoint_prepare_status::resource_preparation_declined; }
#endif
extern "C++" llvm_jit_checkpoint_prepare_result llvm_jit_checkpoint_prepare_instance_host_api(
    ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
    ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
    llvm_jit_checkpoint_prepare_request const&) noexcept
{
    llvm_jit_checkpoint_prepare_result out{};out.status=llvm_jit_checkpoint_prepare_status::unavailable_capture;return out;
}
#endif
