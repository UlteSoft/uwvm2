// Included inside the authenticated current-stop WASIp1 controller operation.
if(query.operation==wasip1_state::action::portable_export || query.operation==wasip1_state::action::portable_import)
{
    namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;namespace lib=::uwvm2::runtime::lib;
    using cs=lib::llvm_jit_wasip1_environment_capsule_status;
    auto& out=result.wasip1_state_values;out.portable_operation=true;
#if defined(__cpp_exceptions)
    try
#endif
    {
        lib::llvm_jit_wasip1_environment_capsule_request selected{};selected.module=query.module;
        if(query.operation==wasip1_state::action::portable_export)
        {
            selected.portable_metadata_only=true;::fast_io::native_white_hole entropy{};
            ::fast_io::operations::read_all_bytes(entropy,selected.recording_label.data(),selected.recording_label.data()+selected.recording_label.size());
        }
        else
        {
            auto data=::std::make_shared<pp::snapshot>();
            if(!pp::parse_rebindings(pp::text_view{query.value.data(),query.value.size()},selected.portable_rebindings) ||
               !pp::load_file(pp::text_view{query.name.data(),query.name.size()},*data))
            { out.result=wasip1_state::status::invalid_request;out.diagnostic=pp::text{u8"invalid portable file or rebinding list"};return; }
            selected.recording_label=data->recording_label;out.total_entries=data->resources.size();selected.portable_restore=::std::move(data);
        }
        auto saved=lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(pause_,{captures.data(),count},selected);
        out.diagnostic=::std::move(saved.diagnostic);out.observed_runtime_epoch=saved.observed_runtime_epoch;
        if(saved.status==cs::captured && saved.portable)
        {
            out.total_entries=saved.portable->resources.size();
            if(!pp::save_file(*saved.portable,pp::text_view{query.name.data(),query.name.size()}))
            { out.result=wasip1_state::status::invalid_request;return; }
        }
        switch(saved.status)
        {
            case cs::captured:case cs::restored:out.result=wasip1_state::status::ok;out.mutation_applied=true;break;
            case cs::stale_environment:out.result=wasip1_state::status::stale_stop_or_generation;break;
            case cs::invalid_portable_snapshot:out.result=wasip1_state::status::invalid_request;break;
            case cs::capability_denied:out.result=wasip1_state::status::capability_increase;break;
            case cs::missing_mount:case cs::missing_rebind:case cs::incompatible_flags:case cs::unsupported_resource:
                out.result=wasip1_state::status::unavailable_resource_rollback;break;
            case cs::resource_limit:out.result=wasip1_state::status::resource_limit;break;
            case cs::allocation_failed:out.result=wasip1_state::status::allocation_failed;break;
            case cs::native_operation_failed:out.result=wasip1_state::status::native_operation_failed;break;
            default:out.result=wasip1_state::status::unavailable_environment;break;
        }
        return;
    }
#if defined(__cpp_exceptions)
    catch(::fast_io::error const&) { out.result=wasip1_state::status::native_operation_failed;return; }
    catch(...) { out.result=wasip1_state::status::allocation_failed;return; }
#endif
}

// Batch operations retain ONE authenticated cohort for all environments.
if(query.operation==wasip1_state::action::portable_export_group || query.operation==wasip1_state::action::portable_import_group)
{
    namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;namespace lib=::uwvm2::runtime::lib;
    using cs=lib::llvm_jit_wasip1_environment_capsule_status;
    auto& out=result.wasip1_state_values;out.portable_group_operation=true;out.environment_count=query.environments.size();
#if defined(__cpp_exceptions)
    try
#endif
    {
        ::std::vector<lib::llvm_jit_wasip1_environment_capsule_request> selected(query.environments.size());
        pp::group_snapshot data{};std::array<std::byte,16u> label{};
        bool const exporting{query.operation==wasip1_state::action::portable_export_group};
        // Validate every selector before opening even the metadata file.
        for(::std::size_t i{};i!=selected.size();++i)
        {
            auto& row=selected[i];auto const& selector=query.environments[i];row.module=selector.module;
            if(!exporting && !pp::parse_rebindings(pp::text_view{selector.rebindings.data(),selector.rebindings.size()},row.portable_rebindings))
            { out.result=wasip1_state::status::invalid_request;out.diagnostic=::fast_io::u8concat_fast_io(u8"request=",i,u8" invalid rebinding list");return; }
        }
        if(exporting)
        { ::fast_io::native_white_hole entropy{};::fast_io::operations::read_all_bytes(entropy,label.data(),label.data()+label.size()); }
        else if(!pp::load_group_file(pp::text_view{query.name.data(),query.name.size()},data) || data.environments.size()!=selected.size())
        { out.result=wasip1_state::status::invalid_request;out.diagnostic=pp::text{u8"invalid portable group or environment count"};return; }
        for(::std::size_t i{};i!=selected.size();++i)
        {
            auto& row=selected[i];
            if(exporting) { row.portable_metadata_only=true;row.recording_label=label; }
            else
            {
                row.recording_label=data.environments[i].recording_label;out.total_entries+=data.environments[i].resources.size();
                row.portable_restore=::std::make_shared<pp::snapshot>(::std::move(data.environments[i]));
            }
        }
        lib::llvm_jit_wasip1_environment_capsule_result saved{};
        if(exporting)
        {
            auto captured=lib::llvm_jit_checkpoint_capture_portable_wasip1_environment_group_host_api(pause_,{captures.data(),count},selected);
            saved.status=captured.status;saved.diagnostic=::std::move(captured.diagnostic);saved.observed_runtime_epoch=captured.observed_runtime_epoch;
            if(saved.status==cs::captured)
            {
                for(auto const& row:captured.portable.environments) { out.total_entries+=row.resources.size(); }
                if(!pp::save_group_file(captured.portable,pp::text_view{query.name.data(),query.name.size()}))
                { out.result=wasip1_state::status::invalid_request;out.diagnostic=pp::text{u8"portable group file could not be saved"};return; }
            }
        }
        else { saved=lib::llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(pause_,{captures.data(),count},selected); }
        out.diagnostic=::std::move(saved.diagnostic);out.observed_runtime_epoch=saved.observed_runtime_epoch;
        switch(saved.status)
        {
            case cs::captured:case cs::restored:out.result=wasip1_state::status::ok;out.mutation_applied=true;break;
            case cs::stale_environment:out.result=wasip1_state::status::stale_stop_or_generation;break;
            case cs::invalid_portable_snapshot:out.result=wasip1_state::status::invalid_request;break;
            case cs::capability_denied:out.result=wasip1_state::status::capability_increase;break;
            case cs::missing_mount:case cs::missing_rebind:case cs::incompatible_flags:case cs::unsupported_resource:
                out.result=wasip1_state::status::unavailable_resource_rollback;break;
            case cs::resource_limit:out.result=wasip1_state::status::resource_limit;break;
            case cs::allocation_failed:out.result=wasip1_state::status::allocation_failed;break;
            case cs::native_operation_failed:out.result=wasip1_state::status::native_operation_failed;break;
            default:out.result=wasip1_state::status::unavailable_environment;break;
        }
        return;
    }
#if defined(__cpp_exceptions)
    catch(::fast_io::error const&) { out.result=wasip1_state::status::native_operation_failed;return; }
    catch(...) { out.result=wasip1_state::status::allocation_failed;return; }
#endif
}
