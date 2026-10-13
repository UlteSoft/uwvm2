// Private live-environment consumer. Included inside the capsule class.
// This restores a same-generation managed WASI environment, never a whole VM.
struct prepared_current_environment;
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
struct prepared_current_environment
{
    using table_type=::uwvm2::imported::wasi::wasip1::fd_manager::wasm_fd_storage_t;
    using environment_type=::std::remove_reference_t<decltype(resolve_wasip1_env_for_runtime_module_id(::std::size_t{}))>;
    using strings=runtime_wasip1_debug_environment::strings;
    using views=runtime_wasip1_debug_environment::string_views;
    environment_type* target{};
    strings* argument_target{};strings* variable_target{};
    strings new_arguments{},new_variables{};
    views argument_views{},variable_views{};
    ::std::vector<fd_ref> resources{};
    decltype(::std::declval<table_type>().opens) opens{};
    decltype(::std::declval<table_type>().renumber_map) renumber{};
    decltype(::std::declval<table_type>().closes) closes{};
    prepared_current_environment()=default;
    prepared_current_environment(prepared_current_environment const&)=delete;
    prepared_current_environment& operator=(prepared_current_environment const&)=delete;
};
struct prepared_managed_resource
{
    resource_pin const* snapshot{};
    fd_ref resource{};
};
// A staged resource table is private owned DATA. Only the real manager's SAME
// cohort/closed-host/N/publication transaction can prepare and publish it.
// Preparing every environment first makes a later group commit non-fallible.
[[nodiscard]] static status prepare_current(native_capture_key const& key, owner const& supplied,
    llvm_jit_wasip1_environment_capsule_request const& selected, bool strict,
    ::std::uint_least64_t epoch, ::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile,
    prepared_current_environment& prepared, ::std::vector<prepared_managed_resource>* shared=nullptr)
{
    try
    {
    if(prepared.target!=nullptr) { return status::invalid_capsule_owner; }
    auto const saved{canonical(supplied)};
    if(!saved) { return status::invalid_capsule_owner; }
    // Obtain the current original source/provider/profile proof through the SAME
    // private producer. No new registered capsule or current payload is needed.
    auto current{capture_current(key,selected,epoch,profile,false)};
    if(current.status!=status::captured || !current.capsule) { return current.status; }
    auto const& actual{*current.capsule};
    if(saved->data_.observed_runtime_epoch!=epoch || saved->data_.module!=selected.module ||
       saved->environment_identity_!=actual.environment_identity_ || saved->data_.original_wasm!=actual.data_.original_wasm ||
       saved->data_.builtin_interface!=actual.data_.builtin_interface)
    { return status::stale_environment; }
    if(strict && saved->data_.retained_external_resources!=0u) { return status::unsupported_resource; }
    if(saved->data_.descriptors.size()!=saved->bindings_.size() || saved->opens_size_>descriptor_limit)
    { return status::invalid_capsule_owner; }
    namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
    namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
    auto& environment{resolve_wasip1_env_for_runtime_module_id(static_cast<::std::size_t>(selected.module))};
    auto* arguments{::std::addressof(storage::wasip1_argument_storage)};
    auto* variables{::std::addressof(storage::wasip1_environment_storage)};
    if(::std::addressof(environment)!=::std::addressof(storage::default_wasip1_env))
    {
        auto const group{find_wasip1_override_for_runtime_module_id_slow(static_cast<::std::size_t>(selected.module))};
        if(group==nullptr || ::std::addressof(group->env)!=::std::addressof(environment)) { return status::stale_environment; }
        arguments=::std::addressof(group->argument_storage);variables=::std::addressof(group->environment_storage);
    }
    using strings=runtime_wasip1_debug_environment::strings;
    using views=runtime_wasip1_debug_environment::string_views;
    auto& new_arguments{prepared.new_arguments};auto& new_variables{prepared.new_variables};
    auto& argument_views{prepared.argument_views};auto& variable_views{prepared.variable_views};
    auto const copy_text{[](auto const& source, strings& target, views& borrowed)
    {
        target.reserve(source.size());borrowed.reserve(source.size());
        for(auto const& entry:source) { target.emplace_back(::fast_io::u8string_view{entry.data(),entry.size()}); }
        for(auto const& entry:target) { borrowed.emplace_back(entry.data(),entry.size()); }
    }};
    copy_text(saved->data_.arguments,new_arguments,argument_views);copy_text(saved->data_.environment,new_variables,variable_views);
    auto& resources{prepared.resources};resources.reserve(saved->resources_.size());
    for(auto const& pin:saved->resources_)
    {
        if(pin.managed_identity!=0u)
        {
            if(shared!=nullptr)
            {
                auto const prior{::std::find_if(shared->begin(),shared->end(),[&](auto const& row)
                { return row.snapshot->managed_identity==pin.managed_identity; })};
                if(prior!=shared->end())
                {
                    auto const& original{*prior->snapshot};
                    if(original.actual.ptr!=pin.actual.ptr || original.managed_offset!=pin.managed_offset ||
                       original.managed_flags!=pin.managed_flags || original.managed_content!=pin.managed_content)
                    { return status::stale_environment; }
                    resources.push_back(prior->resource);continue;
                }
            }
            fd_ref replacement{};
            runtime_wasip1_debug_environment::create_managed_file(replacement);
            auto& file{runtime_wasip1_debug_environment::managed_file(replacement)};
            if(!pin.managed_content.empty())
            { ::fast_io::operations::pwrite_all_bytes(file,pin.managed_content.data(),pin.managed_content.data()+pin.managed_content.size(),0); }
            ::fast_io::operations::io_stream_seek_bytes(file,pin.managed_offset,::fast_io::seekdir::beg);
            runtime_wasip1_debug_environment::set_managed_flags(replacement,pin.managed_flags);
            replacement.ptr->checkpoint_managed_identity=pin.managed_identity;
            if(shared!=nullptr) { shared->push_back({::std::addressof(pin),replacement}); }
            resources.push_back(::std::move(replacement));
        }
        else
        {
            // Owned refs can restore guest bindings, but external content,
            // shared kernel offsets/flags and effects remain unchanged.
            if(pin.original_kind==fd_kind::file_observer) { return status::unsupported_resource; }
#if defined(_WIN32) && !defined(__CYGWIN__)
            if(pin.original_kind==fd_kind::socket_observer) { return status::unsupported_resource; }
#endif
            for(auto const& directory:pin.directories) { if(directory.was_observer) { return status::unsupported_resource; } }
            if(pin.actual.ptr==nullptr || pin.actual.ptr->wasi_fd_storage.type!=pin.original_kind)
            { return status::stale_environment; }
            resources.push_back(pin.actual);
        }
    }
    auto& opens{prepared.opens};auto& renumber{prepared.renumber};auto& closes{prepared.closes};
    opens.reserve(saved->opens_size_);closes.reserve(saved->opens_size_);
    for(::std::size_t n{};n!=saved->opens_size_;++n)
    {
        fm::wasi_fd_unique_ptr_t slot{};slot.fd_p->wasi_fd=fd_ref{fm::wasi_no_construct};
        opens.push_back(::std::move(slot));
    }
    for(::std::size_t n{};n!=saved->bindings_.size();++n)
    {
        auto const& binding{saved->bindings_[n]};auto const& row{saved->data_.descriptors[n]};
        if(binding.resource>=resources.size() || binding.guest_number!=row.descriptor || binding.guest_number>INT32_MAX)
        { return status::invalid_capsule_owner; }
        fm::wasi_fd_unique_ptr_t slot{};slot.fd_p->wasi_fd=resources[binding.resource];
        slot.fd_p->rights_base=static_cast<decltype(slot.fd_p->rights_base)>(row.base_rights);
        slot.fd_p->rights_inherit=static_cast<decltype(slot.fd_p->rights_inherit)>(row.inheriting_rights);
        if(binding.guest_number<opens.size()) { opens.index_unchecked(binding.guest_number)=::std::move(slot); }
        else { renumber.emplace(static_cast<::std::int32_t>(binding.guest_number),::std::move(slot)); }
    }
    // Empty reserved cells are not free-list entries. In particular, the
    // debugger management input must never become a guest stdin capability.
    for(auto const& empty:saved->reserved_empty_)
    {
        auto const number{empty.number};
        if(number>INT32_MAX) { return status::invalid_capsule_owner; }
        fm::wasi_fd_unique_ptr_t slot{};
        if(!empty.null_resource) { slot.fd_p->wasi_fd=fd_ref{fm::wasi_no_construct}; }
        slot.fd_p->rights_base=static_cast<decltype(slot.fd_p->rights_base)>(empty.base);
        slot.fd_p->rights_inherit=static_cast<decltype(slot.fd_p->rights_inherit)>(empty.inheriting);
        if(number<opens.size())
        {
            if(opens.index_unchecked(number).fd_p->wasi_fd.ptr!=nullptr) { return status::invalid_capsule_owner; }
            opens.index_unchecked(number)=::std::move(slot);
        }
        else if(!renumber.emplace(static_cast<::std::int32_t>(number),::std::move(slot)).second)
        { return status::invalid_capsule_owner; }
    }
    // Recreate the ORIGINAL allocation/free-list order, not just the live rows.
    closes.reserve(saved->closed_order_.size());
    for(auto number:saved->closed_order_)
    {
        if(number>=opens.size() || opens.index_unchecked(number).fd_p->wasi_fd.ptr!=nullptr ||
           opens.index_unchecked(number).fd_p->close_pos!=SIZE_MAX)
        { return status::invalid_capsule_owner; }
        opens.index_unchecked(number).fd_p->close_pos=closes.size();closes.push_back(number);
    }
    if(closes.size()+saved->data_.descriptors.size()+saved->reserved_empty_.size()!=opens.size()+renumber.size()) { return status::invalid_capsule_owner; }
    auto& table{environment.fd_storage};
    ::uwvm2::utils::mutex::rw_fair_unique_guard_t table_lock{table.fds_rwlock};
    // Reserved empty cells consume allocator slots as well, just as they do
    // in the original FD table and managed construction policy check.
    if(saved->data_.descriptors.size()+saved->reserved_empty_.size()>table.fd_limit) { return status::resource_limit; }
    // All allocation and native IO finished, with the original table untouched.
    // The actual enclosing proof excludes actors across preparation AND commit.
    prepared.target=::std::addressof(environment);
    prepared.argument_target=arguments;prepared.variable_target=variables;
    return status::captured;
    }
    catch(::fast_io::error const&) { return status::native_operation_failed; }
    catch(...) { return status::allocation_failed; }
}
using prepared_environment_group=::std::vector<::std::unique_ptr<prepared_current_environment>>;
[[nodiscard]] static status prepare_group_current(native_capture_key const& key,
    ::std::span<owner const> supplied, bool strict, ::std::uint_least64_t epoch,
    ::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile,
    prepared_environment_group& prepared)
{
    if(supplied.empty() || supplied.size()>registry_limit || !prepared.empty()) { return status::invalid_capsule_owner; }
    try
    {
        ::std::vector<owner> canonical_saved{};canonical_saved.reserve(supplied.size());
        ::std::size_t descriptors{},text{},bytes{};
        for(auto const& supplied_owner:supplied)
        {
            auto saved{canonical(supplied_owner)};
            if(!saved) { return status::invalid_capsule_owner; }
            if(!canonical_saved.empty() && saved->data_.recording_label!=canonical_saved.front()->data_.recording_label)
            { return status::invalid_capsule_owner; }
            if(saved->data_.descriptors.size()>descriptor_limit-descriptors ||
               saved->owned_text_bytes_>text_limit-text || saved->data_.managed_content_bytes>67108864u-bytes)
            { return status::resource_limit; }
            descriptors+=saved->data_.descriptors.size();
            if(saved->reserved_empty_.size()>descriptor_limit-descriptors) { return status::resource_limit; }
            descriptors+=saved->reserved_empty_.size();text+=saved->owned_text_bytes_;bytes+=saved->data_.managed_content_bytes;
            canonical_saved.push_back(::std::move(saved));
        }
        // Cross-module aliases retain one cloned anonymous file/cursor/flags.
        // Saved payload disagreement is rejected before publishing any table.
        ::std::vector<prepared_managed_resource> shared{};prepared.reserve(canonical_saved.size());
        for(auto const& saved:canonical_saved)
        {
            llvm_jit_wasip1_environment_capsule_request request{};
            request.module=saved->data_.module;request.recording_label=saved->data_.recording_label;
            auto next{::std::make_unique<prepared_current_environment>()};
            auto const result{prepare_current(key,saved,request,strict,epoch,profile,*next,::std::addressof(shared))};
            if(result!=status::captured) { prepared.clear();return result; }
            for(auto const& prior:prepared)
            { if(prior->target==next->target) { prepared.clear();return status::invalid_capsule_owner; } }
            prepared.push_back(::std::move(next));
        }
        return status::captured; // Original environments still untouched.
    }
    catch(::fast_io::error const&) { prepared.clear();return status::native_operation_failed; }
    catch(...) { prepared.clear();return status::allocation_failed; }
}
static void publish_current_prepared(prepared_current_environment& prepared) noexcept
{
    if(prepared.target==nullptr || prepared.argument_target==nullptr || prepared.variable_target==nullptr)
    { ::fast_io::fast_terminate(); }
    auto& environment{*prepared.target};auto& table{environment.fd_storage};
    ::uwvm2::utils::mutex::rw_fair_unique_guard_t table_lock{table.fds_rwlock};
    // No allocation, IO, callback or rejection follows the first publication.
    // The real private manager retains the SAME proof throughout all swaps.
    table.opens.swap(prepared.opens);table.renumber_map.swap(prepared.renumber);table.closes.swap(prepared.closes);
    environment.argv.swap(prepared.argument_views);prepared.argument_target->swap(prepared.new_arguments);
    environment.envs.swap(prepared.variable_views);prepared.variable_target->swap(prepared.new_variables);
    prepared.target=nullptr;prepared.argument_target=nullptr;prepared.variable_target=nullptr;
}
#endif
[[nodiscard]] static status restore_current(native_capture_key const& key, owner const& supplied,
    llvm_jit_wasip1_environment_capsule_request const& selected, bool strict,
    ::std::uint_least64_t epoch, ::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile)
{
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
    prepared_current_environment prepared{};
    auto const result{prepare_current(key,supplied,selected,strict,epoch,profile,prepared)};
    if(result!=status::captured) { return result; }
    publish_current_prepared(prepared);return status::restored;
#else
    (void)key;(void)supplied;(void)selected;(void)strict;(void)epoch;(void)profile;return status::unavailable_environment;
#endif
}
