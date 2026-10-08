// Private candidate-world WASIp1 ownership. Include inside the transaction.
// No original environment pointer survives installation; no live table swaps.
#pragma once
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
using wasip1_capsule = llvm_jit_wasip1_environment_capsule;
using wasip1_environment = wasip1_capsule::prepared_current_environment::environment_type;
struct owned_world_wasip1_environment
{
    // These final string allocations own every view in the nonmoving env.
    wasip1_capsule::prepared_current_environment::strings arguments{}, variables{};
    wasip1_environment environment{
        .wasip1_memory_resolver = ::uwvm2::uwvm::imported::wasi::wasip1::storage::resolve_current_wasip1_memory};
};
struct world_wasip1_module_binding
{
    ::std::size_t environment{SIZE_MAX};
    native_memory_t* memory0{}; // genuine NEW module memory; no old memory borrow
};
// Before engines/frame carriers: their destruction precedes WASIp1 resources.
::std::vector<::std::unique_ptr<owned_world_wasip1_environment>> wasip1_environments_{};
::std::vector<world_wasip1_module_binding> wasip1_modules_{};
::std::size_t wasip1_bound_modules_{}, wasip1_bound_memories_{}, wasip1_shared_modules_{};
// Final native dispatch-cache shape, ready for a future non-fallible swap
// during COMPLETE world publication. Its raw pointers borrow only this world.
decltype(g_wasip1_runtime_module_context_cache) wasip1_dispatch_contexts_{};
::std::size_t wasip1_trace_bindings_{};

[[nodiscard]] llvm_jit_wasip1_environment_capsule_status install_private_wasip1_environments(
    ::std::span<llvm_jit_wasip1_environment_capsule_owner const> supplied,
    wasip1_capsule::prepared_environment_group& prepared, bool strict)
{
    using status = llvm_jit_wasip1_environment_capsule_status;
    namespace storage = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
    namespace environment = ::uwvm2::imported::wasi::wasip1::environment;
    if(phase_ != preparation_status::resources_prepared || supplied.empty() || supplied.size() != prepared.size() ||
       !wasip1_environments_.empty() || !wasip1_modules_.empty() || !wasip1_dispatch_contexts_.empty() ||
       actual_dense_module_owners_.size() != modules_.size())
    { return status::invalid_capsule_owner; }
    // The actual manager retains its SAME cohort/closed-host/GC/publication
    // proof throughout this private installation and subsequent engine work.
    // Genuine capsule and prepared typed target equality precede any borrow.
    for(::std::size_t n{}; n < supplied.size(); ++n)
    {
        auto const saved{wasip1_capsule::canonical(supplied[n])};
        auto const* next{prepared[n].get()};
        if(!saved || next == nullptr || next->target == nullptr || next->argument_target == nullptr ||
           next->variable_target == nullptr || saved->environment_identity_ != next->target)
        { return status::invalid_capsule_owner; }
        bool actual_member{};
        for(auto const& module : modules_)
        { if(module.old_actual_id == saved->data_.module) { actual_member = true; break; } }
        if(!actual_member) { return status::stale_environment; }
        for(::std::size_t prior{}; prior < n; ++prior)
        { if(prepared[prior]->target == next->target) { return status::invalid_capsule_owner; } }
        auto const& original{*next->target};
        // Unowned arbitrary callbacks have no candidate-world lifetime adapter.
        // Preserve only this runtime's original known exit implementation.
        if((original.wasip1_proc_exit_func_ptr != nullptr &&
            original.wasip1_proc_exit_func_ptr != storage::uwvm_wasip1_proc_exit_func_ptr_overload) ||
           original.wasip1_proc_raise_func_ptr != nullptr || original.wasip1_sched_yield_func_ptr != nullptr)
        { return status::unsupported_resource; }
        if(strict && original.trace_wasip1_output_file) { return status::unsupported_resource; }
        if(original.trace_wasip1_output_file_path_storage.size() > wasip1_capsule::text_limit ||
           original.trace_wasip1_group_name_storage.size() > wasip1_capsule::text_limit)
        { return status::resource_limit; }
    }
    if(!charge_native(prepared.size(),sizeof(::std::unique_ptr<owned_world_wasip1_environment>)) ||
       !charge_native(prepared.size(),sizeof(owned_world_wasip1_environment)) ||
       !charge_native(modules_.size(),sizeof(world_wasip1_module_binding)) ||
       !charge_native(modules_.size(),sizeof(cached_wasip1_runtime_module_context)))
    { return status::resource_limit; }
    wasip1_environments_.reserve(prepared.size());
    wasip1_modules_.resize(modules_.size());
    wasip1_dispatch_contexts_.resize(modules_.size());
    for(auto& entry:wasip1_dispatch_contexts_) { entry.import_visible=false; }
    // Bind NEW dense module IDs from genuine source correspondence, preserving
    // the old sharing topology. Empty memory[0] is valid; it is never invented.
    // Do not mutate one shared env.wasip1_memory for several calling modules.
    for(auto const owner : actual_dense_module_owners_)
    {
        if(owner >= modules_.size()) { return status::stale_environment; }
        auto const& module{modules_[owner]};
        if(module.new_actual_id >= wasip1_modules_.size() || module.actual == nullptr ||
           context_->unpublished_file_for_module(module.actual) != module.file || module.old_actual_id >= g_runtime.modules.size())
        { return status::stale_environment; }
        auto& dispatch{wasip1_dispatch_contexts_.index_unchecked(module.new_actual_id)};
        dispatch.memory0 = const_cast<native_memory_t*>(resolve_memory0_ptr(*module.actual));
        if(!is_wasip1_import_visible_for_runtime_module_id_slow(static_cast<::std::size_t>(module.old_actual_id))) { continue; }
        dispatch.import_visible=true;
        auto const* original{::std::addressof(resolve_wasip1_env_for_runtime_module_id(static_cast<::std::size_t>(module.old_actual_id)))};
        ::std::size_t selected{SIZE_MAX};
        for(::std::size_t n{}; n < prepared.size(); ++n)
        { if(prepared[n]->target == original) { selected = n; break; } }
        if(selected == SIZE_MAX) { return status::stale_environment; }
        auto& binding{wasip1_modules_[module.new_actual_id]};
        if(binding.environment != SIZE_MAX) { return status::invalid_capsule_owner; }
        binding.environment = selected;
        binding.memory0 = const_cast<native_memory_t*>(resolve_memory0_ptr(*module.actual));
        ++wasip1_bound_modules_;
        if(binding.memory0 != nullptr) { ++wasip1_bound_memories_; }
    }
    for(::std::size_t n{}; n < prepared.size(); ++n)
    {
        ::std::size_t aliases{};
        for(auto const& binding : wasip1_modules_) { if(binding.environment == n) { ++aliases; } }
        if(aliases == 0u) { return status::stale_environment; }
        wasip1_shared_modules_ += aliases-1u; // total is bounded by module count
        auto& candidate{*prepared[n]}; auto const& original{*candidate.target};
        if(!charge_native(original.trace_wasip1_output_file_path_storage.size()+1u,sizeof(char8_t)) ||
           !charge_native(original.trace_wasip1_group_name_storage.size()+1u,sizeof(char8_t)))
        { return status::resource_limit; }
        auto owned{::std::make_unique<owned_world_wasip1_environment>()};
        auto& next{owned->environment};
        next.fd_storage.fd_limit = original.fd_storage.fd_limit;
        next.wasip1_proc_exit_func_ptr = original.wasip1_proc_exit_func_ptr;
        next.disable_utf8_check = original.disable_utf8_check;
        next.trace_wasip1_call = original.trace_wasip1_call;
        next.trace_wasip1_output_target = original.trace_wasip1_output_target;
        next.trace_wasip1_group_kind = original.trace_wasip1_group_kind;
        next.trace_wasip1_output_file_path_storage = original.trace_wasip1_output_file_path_storage;
        next.trace_wasip1_group_name_storage = original.trace_wasip1_group_name_storage;
        // Duplicate an already-owned trace handle; never reopen its path or
        // truncate the original log. Shared offsets/effects remain external.
        if(original.trace_wasip1_output_file)
        {
            next.trace_wasip1_output_file = ::fast_io::u8native_file{::fast_io::io_dup,original.trace_wasip1_output_file};
            if(!next.trace_wasip1_output_file || next.trace_wasip1_output_file.native_handle()==original.trace_wasip1_output_file.native_handle())
            { return status::stale_environment; }
            ++wasip1_trace_bindings_;
        }
        // Mount/preopen configuration was consumed by ORIGINAL initialization.
        // Only the saved live FD table is installed; re-running initialization
        // would reintroduce closed descriptors and extra host capabilities.
        next.fd_storage.opens.swap(candidate.opens);
        next.fd_storage.renumber_map.swap(candidate.renumber);
        next.fd_storage.closes.swap(candidate.closes);
        owned->arguments.swap(candidate.new_arguments); owned->variables.swap(candidate.new_variables);
        next.argv.swap(candidate.argument_views); next.envs.swap(candidate.variable_views);
        candidate.target = nullptr; candidate.argument_target = nullptr; candidate.variable_target = nullptr;
        wasip1_environments_.push_back(::std::move(owned));
    }
    // Verify the transferred private payload against the genuine capsules.
    // All pointer/extent equality checks precede reading borrowed text; actual
    // native IO below touches only newly cloned managed files and uses pread.
    for(::std::size_t n{}; n < supplied.size(); ++n)
    {
        auto const saved{wasip1_capsule::canonical(supplied[n])};
        if(!saved || n >= wasip1_environments_.size()) { return status::invalid_capsule_owner; }
        auto& owned{*wasip1_environments_[n]}; auto& env{owned.environment};
        auto const text_matches{[](auto const& owners,auto const& views,auto const& data) noexcept
        {
            if(owners.size()!=views.size() || owners.size()!=data.size()) { return false; }
            for(::std::size_t i{};i<owners.size();++i)
            {
                auto const& owner{owners.index_unchecked(i)};auto const& view{views.index_unchecked(i)};
                if(view.data()!=owner.data() || view.size()!=owner.size() || owner.size()!=data[i].size() ||
                   (owner.size()!=0u && ::fast_io::freestanding::my_memcmp(owner.data(),data[i].data(),owner.size())!=0)) { return false; }
            }
            return true;
        }};
        if(env.wasip1_memory!=nullptr || !env.mount_dir_roots.empty() ||
           !text_matches(owned.arguments,env.argv,saved->data_.arguments) ||
           !text_matches(owned.variables,env.envs,saved->data_.environment) ||
           env.fd_storage.opens.size()!=saved->opens_size_ || env.fd_storage.closes.size()!=saved->closed_order_.size())
        { return status::stale_environment; }
        for(::std::size_t i{};i<saved->closed_order_.size();++i)
        { if(env.fd_storage.closes.index_unchecked(i)!=saved->closed_order_[i]) { return status::stale_environment; } }
        using resource_identity=decltype(::std::declval<wasip1_capsule::fd_ref>().ptr);
        if(!charge_native(saved->resources_.size(),sizeof(resource_identity))) { return status::resource_limit; }
        ::std::vector<resource_identity> representatives(saved->resources_.size());
        for(::std::size_t i{};i<saved->bindings_.size();++i)
        {
            auto const& binding{saved->bindings_[i]};
            if(binding.resource>=saved->resources_.size() || i>=saved->data_.descriptors.size()) { return status::invalid_capsule_owner; }
            auto const* slot{wasip1_capsule::find_actual_descriptor(env.fd_storage,binding.guest_number)};
            auto const& expected{saved->data_.descriptors[i]};auto const& pin{saved->resources_[binding.resource]};
            if(slot==nullptr || slot->wasi_fd.ptr==nullptr || static_cast<::std::uint64_t>(slot->rights_base)!=expected.base_rights || static_cast<::std::uint64_t>(slot->rights_inherit)!=expected.inheriting_rights ||
               slot->wasi_fd.ptr->checkpoint_managed_identity!=pin.managed_identity) { return status::stale_environment; }
            auto& representative{representatives[binding.resource]};
            if(representative!=nullptr)
            { if(representative!=slot->wasi_fd.ptr) { return status::stale_environment; } continue; }
            representative=slot->wasi_fd.ptr;
            if(pin.managed_identity==0u)
            { if(slot->wasi_fd.ptr!=pin.actual.ptr) { return status::stale_environment; } continue; }
            if(slot->wasi_fd.ptr==pin.actual.ptr || slot->wasi_fd.ptr->wasi_fd_storage.type!=wasip1_capsule::fd_kind::file)
            { return status::stale_environment; }
            auto& file{runtime_wasip1_debug_environment::managed_file(slot->wasi_fd)};
            if(runtime_wasip1_debug_environment::managed_flags(slot->wasi_fd)!=pin.managed_flags ||
               ::fast_io::operations::io_stream_seek_bytes(file,0,::fast_io::seekdir::cur)!=pin.managed_offset ||
               ::fast_io::status(file).size!=pin.managed_content.size()) { return status::stale_environment; }
            ::std::array<::std::byte,4096u> bytes{};
            for(::std::size_t offset{};offset<pin.managed_content.size();)
            {
                auto const count{(::std::min)(bytes.size(),pin.managed_content.size()-offset)};
                ::fast_io::operations::pread_all_bytes(file,bytes.data(),bytes.data()+count,offset);
                if(::fast_io::freestanding::my_memcmp(bytes.data(),pin.managed_content.data()+offset,count)!=0) { return status::stale_environment; }
                offset+=count;
            }
            // Synchronous Windows pread may advance the kernel cursor.
            // Restore the NEW file cursor before any future activation.
            if(::fast_io::operations::io_stream_seek_bytes(file,pin.managed_offset,::fast_io::seekdir::beg)!=pin.managed_offset)
            { return status::stale_environment; }
        }
    }
    // Complete the ACTUAL runtime cache shape only after final owned envs
    // exist. No global cache/default selector is changed during this rehearsal.
    for(auto const owner:actual_dense_module_owners_)
    {
        auto const dense{modules_[owner].new_actual_id};auto const& binding{wasip1_modules_[dense]};
        auto& dispatch{wasip1_dispatch_contexts_.index_unchecked(dense)};
        if(binding.environment==SIZE_MAX)
        { if(dispatch.import_visible || dispatch.env!=nullptr) { return status::stale_environment; } continue; }
        if(binding.environment>=wasip1_environments_.size() || !dispatch.import_visible || dispatch.memory0!=binding.memory0)
        { return status::stale_environment; }
        dispatch.env=::std::addressof(wasip1_environments_[binding.environment]->environment);
        if(dispatch.env->wasip1_memory!=nullptr || details::wasip1_default_context_fast_path(
            wasip1_dispatch_contexts_,dense,::std::addressof(storage::default_wasip1_env),false))
        { return status::stale_environment; }
    }
    prepared.clear(); // extra RC pins drop; NEW tables retain every resource
    return status::captured; // private installation only, no runtime publication
}
#endif
