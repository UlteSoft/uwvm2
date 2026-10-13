// Private cold resource factory. Included inside runtime_wasip1_debug_environment.
// Real cohort/host/N/publication guards are held by the caller, never DATA.
using fd_ref = ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_ref_t;
using fd_owner = ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t;
inline static ::std::uint64_t next_managed_identity_{1u};
[[nodiscard]] static ::fast_io::native_file& managed_file(fd_ref const& resource) noexcept
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    return resource.ptr->wasi_fd_storage.storage.file_fd.file;
#else
    return resource.ptr->wasi_fd_storage.storage.file_fd;
#endif
}
[[nodiscard]] static int managed_flags(fd_ref const& resource)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
    if(resource.ptr->wasi_fd_storage.storage.file_fd.fdflags!=::uwvm2::imported::wasi::wasip1::abi::fdflags_t{})
    { ::fast_io::throw_posix_error(ENOTSUP); }
#endif
    return ::uwvm2::runtime::lib::wasip1_native_file::flags(managed_file(resource));
}
static void set_managed_flags(fd_ref const& resource, int flags)
{
    ::uwvm2::runtime::lib::wasip1_native_file::set_flags(managed_file(resource),flags);
}
static void create_managed_file(fd_ref const& resource)
{
    auto& storage{resource.ptr->wasi_fd_storage};
    storage=::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_storage_t{fd_type::file};
    managed_file(resource)=::uwvm2::runtime::lib::wasip1_native_file::create();
    (void)managed_flags(resource); // Qualify before publication.
}
[[nodiscard]] static view manage_descriptor(request const& selected, wasip1_env_type& environment)
{
    namespace ws=::uwvm2::uwvm::debugger::wasip1_state;
    namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
    view out{};out.module=selected.module;out.operation=selected.operation;
    try
    {
    auto& table{environment.fd_storage};
    if(table.closes.size()>table.opens.size()) { out.result=status::unavailable_environment;return out; }
    ::uwvm2::utils::mutex::rw_fair_unique_guard_t table_lock{table.fds_rwlock};
    fd_owner candidate{fm::wasi_no_construct};
    if(selected.operation!=action::create_file)
    {
        auto const actual{find_descriptor(table,selected.descriptor)};
        if(actual==nullptr) { out.result=status::bad_descriptor;return out; }
        ::uwvm2::utils::mutex::mutex_guard_t fd_lock{actual->fd_mutex};
        if(actual->close_pos!=SIZE_MAX || actual->wasi_fd.ptr==nullptr || actual->wasi_fd.ptr->wasi_fd_storage.type==fd_type::null)
        { out.result=status::bad_descriptor;return out; }
        if(static_cast<::std::uint64_t>(actual->rights_base)!=selected.expected_base ||
           static_cast<::std::uint64_t>(actual->rights_inherit)!=selected.expected_inheriting)
        { out.result=status::changed_descriptor;return out; }
        if(selected.operation==action::close_descriptor)
        {
            // Retain a map-owned slot until its mutex guard releases. Closing a
            // guest binding never deletes a pathname or a checkpoint's owner.
            if(selected.descriptor<table.opens.size())
            {
                table.closes.reserve(table.closes.size()+1u);
                actual->close_pos=table.closes.size();table.closes.push_back(static_cast<::std::size_t>(selected.descriptor));
                actual->wasi_fd=fd_ref{fm::wasi_no_construct};actual->rights_base={};actual->rights_inherit={};
            }
            else
            {
                auto found{table.renumber_map.find(static_cast<::std::int32_t>(selected.descriptor))};
                candidate=::std::move(found->second);table.renumber_map.erase(found);
            }
            out.affected_descriptor=selected.descriptor;out.result=status::ok;out.mutation_applied=true;return out;
        }
        candidate=fd_owner{};candidate.fd_p->wasi_fd=actual->wasi_fd;
        candidate.fd_p->rights_base=actual->rights_base;candidate.fd_p->rights_inherit=actual->rights_inherit;
    }
    else
    {
        if(next_managed_identity_==0u) { out.result=status::resource_limit;return out; }
        candidate=fd_owner{};
        create_managed_file(candidate.fd_p->wasi_fd);
        auto& file{managed_file(candidate.fd_p->wasi_fd)};
        if(!selected.value.empty())
        {
            auto const* first{reinterpret_cast<::std::byte const*>(selected.value.data())};
            ::fast_io::operations::pwrite_all_bytes(file,first,first+selected.value.size(),0);
        }
        ::fast_io::operations::io_stream_seek_bytes(file,0,::fast_io::seekdir::beg);
        candidate.fd_p->wasi_fd.ptr->checkpoint_managed_identity=next_managed_identity_;
        candidate.fd_p->rights_base=static_cast<decltype(candidate.fd_p->rights_base)>(0x60006eu);
        candidate.fd_p->rights_inherit={};
    }
    if(!table.fits_allocation_scan_limit(ws::maximum_fd_scan))
    { out.result=status::resource_limit;return out; }
    if(table.opens.size()+table.renumber_map.size()-table.closes.size()>=table.fd_limit)
    { out.result=status::resource_limit;return out; }
    ::std::uint64_t number{};
    if(!table.closes.empty())
    {
        number=table.closes.back_unchecked();
        if(number>=table.opens.size() || table.opens.index_unchecked(number).fd_p==nullptr ||
           table.opens.index_unchecked(number).fd_p->close_pos!=table.closes.size()-1u)
        { out.result=status::unavailable_environment;return out; }
        table.opens.index_unchecked(number)=::std::move(candidate);table.closes.pop_back();
    }
    else
    {
        number=table.opens.size();
        while(number<=INT32_MAX && table.renumber_map.find(static_cast<::std::int32_t>(number))!=table.renumber_map.end()) { ++number; }
        if(number>INT32_MAX) { out.result=status::resource_limit;return out; }
        if(number==table.opens.size()) { table.opens.push_back(::std::move(candidate)); }
        else { table.renumber_map.emplace(static_cast<::std::int32_t>(number),::std::move(candidate)); }
    }
    if(selected.operation==action::create_file) { next_managed_identity_=next_managed_identity_==UINT64_MAX ? 0u : next_managed_identity_+1u; }
    out.affected_descriptor=number;out.result=status::ok;out.mutation_applied=true;return out;
    }
    catch(::fast_io::error const&) { out.result=status::native_operation_failed;return out; }
    catch(...) { out.result=status::allocation_failed;return out; }
}
