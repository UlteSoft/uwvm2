// Private portable producer/consumer. Included inside the native capsule.
// Only the coherent manager can construct native_capture_key.
[[nodiscard]] static result portable_current(native_capture_key const& key,request const& selected,
    ::std::uint64_t epoch,::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile,
    prepared_current_environment* prepared=nullptr,void const** captured_identity=nullptr)
{
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
    namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
    namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
    namespace abi=::uwvm2::imported::wasi::wasip1::abi;
    namespace fn=::uwvm2::imported::wasi::wasip1::func;
    namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
    result out{};
    try
    {
        if((selected.portable_metadata_only && selected.portable_restore) || selected.portable_rebindings.size()>pp::max_rows ||
           (selected.portable_restore && (prepared==nullptr || prepared->target!=nullptr)))
        { out.status=status::invalid_portable_snapshot;return out; }
        // Authenticate actual target source/provider/profile in the original
        // closure. A detached file is input DATA, never a fabricated live owner.
        auto proof=capture_current(key,selected,epoch,profile,false);
        if(proof.status!=status::captured || !proof.capsule) { return proof; }out.observed_runtime_epoch=epoch;
        auto& environment=resolve_wasip1_env_for_runtime_module_id(static_cast<::std::size_t>(selected.module));
        auto* arguments=::std::addressof(storage::wasip1_argument_storage);
        auto* variables=::std::addressof(storage::wasip1_environment_storage);
        if(::std::addressof(environment)!=::std::addressof(storage::default_wasip1_env))
        {
            auto group=find_wasip1_override_for_runtime_module_id_slow(static_cast<::std::size_t>(selected.module));
            if(group==nullptr || ::std::addressof(group->env)!=::std::addressof(environment)) { out.status=status::stale_environment;return out; }
            arguments=::std::addressof(group->argument_storage);variables=::std::addressof(group->environment_storage);
        }
        auto& table=environment.fd_storage;
        if(table.opens.size()>pp::max_rows || table.renumber_map.size()>pp::max_rows-table.opens.size() || table.closes.size()>table.opens.size())
        { out.status=status::resource_limit;return out; }
        ::uwvm2::object::memory::linear::native_memory_t scratch{};scratch.init_by_page_count(1u);
        auto clone=[](fd const& original)
        {
            fm::wasi_fd_unique_ptr_t copy{};copy.fd_p->wasi_fd=original.wasi_fd;
            copy.fd_p->rights_base=original.rights_base;copy.fd_p->rights_inherit=original.rights_inherit;return copy;
        };
        auto stat=[&](fd_ref const& resource,pp::resource& row)
        {
            wasip1_env_type stage{};stage.wasip1_memory=::std::addressof(scratch);stage.fd_storage.fd_limit=2u;
            fm::wasi_fd_unique_ptr_t cell{};cell.fd_p->wasi_fd=resource;stage.fd_storage.opens.push_back(::std::move(cell));
            if(fn::fd_fdstat_get(stage,0,0u)!=abi::errno_t::esuccess) { return false; }
            auto type=::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory<::std::uint8_t>(scratch,0u);
            row.flags=::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory<::std::uint16_t>(scratch,2u);
            if(type==static_cast<::std::uint8_t>(abi::filetype_t::filetype_directory)) { row.type=pp::kind::directory;return true; }
            if(type!=static_cast<::std::uint8_t>(abi::filetype_t::filetype_regular_file))
            { row.type=pp::kind::external_stream;row.stream_type=type;return true; }
            row.type=pp::kind::file;
            if(resource.ptr==nullptr || resource.ptr->wasi_fd_storage.type!=fd_kind::file) { return false; }
            auto offset=::fast_io::operations::io_stream_seek_bytes(runtime_wasip1_debug_environment::managed_file(resource),0,::fast_io::seekdir::cur);
            if(offset<0 || static_cast<::std::uint64_t>(offset)>INT64_MAX) { return false; }
            row.offset=static_cast<::std::uint64_t>(offset);return true;
        };
        auto fail=[&](status s,::std::size_t index,::fast_io::u8string_view why)
        { out.status=s;out.diagnostic=::fast_io::u8concat_fast_io(u8"resource=",index,u8" ",why);return out; };
        if(!selected.portable_restore)
        {
            auto s=::std::make_shared<pp::snapshot>();s->recording_label=selected.recording_label;
            s->original_wasm=proof.capsule->data_.original_wasm;s->builtin_interface=proof.capsule->data_.builtin_interface;
            if(!runtime_wasip1_debug_environment::bound_text(*arguments,environment.argv) ||
               !runtime_wasip1_debug_environment::bound_text(*variables,environment.envs)) { out.status=status::unavailable_owned_text;return out; }
            ::std::size_t text_bytes{},directory_entries{};
            auto copy_text=[&](auto const& source,auto& destination)
            { for(auto const& t:source) { if(t.size()+1u>selected.maximum_owned_text_bytes-text_bytes) { return false; }text_bytes+=t.size()+1u;destination.emplace_back(pp::text_view{t.data(),t.size()}); }return true; };
            if(!copy_text(*arguments,s->arguments) || !copy_text(*variables,s->environment)) { out.status=status::unavailable_owned_text;return out; }
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t lock{table.fds_rwlock};
            if(!table.fits_occupied_slot_limit(selected.maximum_descriptors))
            { out.status=status::resource_limit;return out; }
            s->opens_size=static_cast<::std::uint32_t>(table.opens.size());
            for(auto n:table.closes) { s->closed.push_back(static_cast<::std::uint32_t>(n)); }
            ::std::vector<::std::uint64_t> numbers{};
            for(::std::size_t n{};n<table.opens.size();++n) { numbers.push_back(n); }
            for(auto const& [n,unused]:table.renumber_map) { (void)unused;if(n<0) { out.status=status::unavailable_environment;return out; } numbers.push_back(n); }
            ::std::sort(numbers.begin(),numbers.end());::std::vector<fd_ref> actuals{};
            ::uwvm2::utils::container::map<void const*,::std::size_t> resource_indices{};
            for(auto n:numbers)
            {
                auto cell=find_actual_descriptor(table,n);if(cell==nullptr) { out.status=status::unavailable_environment;return out; }
                ::uwvm2::utils::mutex::mutex_guard_t cell_lock{cell->fd_mutex};if(cell->close_pos!=SIZE_MAX) { continue; }
                auto const base=static_cast<::std::uint64_t>(cell->rights_base)&0x3fffffffu;
                auto const inherit=static_cast<::std::uint64_t>(cell->rights_inherit)&0x3fffffffu;
                auto const& ref=cell->wasi_fd;
                if(ref.ptr==nullptr || ref.ptr->wasi_fd_storage.type==fd_kind::null)
                { s->reserved.push_back({static_cast<::std::uint32_t>(n),base,inherit,ref.ptr!=nullptr});continue; }
                auto [position,inserted]=resource_indices.emplace(ref.ptr,actuals.size());auto index=position->second;
                if(inserted)
                {
                    pp::resource r{};auto const& rc=*ref.ptr;
                    if(rc.checkpoint_stdio_index>=0 && rc.checkpoint_stdio_index<=2)
                    { r.type=pp::kind::stdio;r.stdio_index=static_cast<::std::uint8_t>(rc.checkpoint_stdio_index); }
                    else if(rc.wasi_fd_storage.type==fd_kind::dir)
                    {
                        auto const& chain=rc.wasi_fd_storage.storage.dir_stack.dir_stack;
                        if(chain.empty() || chain.front_unchecked().ptr==nullptr) { return fail(status::unsupported_resource,index,u8"directory has no mount"); }
                        if(chain.size()>selected.maximum_directory_entries-directory_entries) { return fail(status::resource_limit,index,u8"directory entry quota"); }directory_entries+=chain.size();
                        r.type=pp::kind::directory;r.follow=rc.checkpoint_follow;if(!stat(ref,r)) { return fail(status::native_operation_failed,index,u8"directory flags unavailable"); }
                        auto const& root=chain.front_unchecked().ptr->dir_stack.name;
                        r.mount.assign(pp::text_view{root.data(),root.size()});
                        for(::std::size_t j{1u};j<chain.size();++j)
                        {
                            if(chain.index_unchecked(j).ptr==nullptr) { return fail(status::unsupported_resource,index,u8"invalid directory chain"); }
                            auto const& component=chain.index_unchecked(j).ptr->dir_stack.name;
                            if(component.size()>pp::max_text-r.path.size() || (!r.path.empty() && component.size()==pp::max_text-r.path.size())) { return fail(status::resource_limit,index,u8"directory path limit"); }
                            if(!r.path.empty()) { r.path.push_back(u8'/'); }r.path.append(pp::text_view{component.data(),component.size()});
                        }
                    }
                    else
                    {
                        if(!stat(ref,r) || r.type==pp::kind::directory) { return fail(status::unsupported_resource,index,u8"resource needs a typed external provider"); }
                        if(r.type!=pp::kind::external_stream) { r.type=rc.checkpoint_reopenable ? pp::kind::file : pp::kind::external; }
                        if(rc.checkpoint_reopenable && r.type==pp::kind::file)
                        { r.mount.assign(pp::text_view{rc.checkpoint_mount.data(),rc.checkpoint_mount.size()});r.path.assign(pp::text_view{rc.checkpoint_path.data(),rc.checkpoint_path.size()});r.follow=rc.checkpoint_follow; }
                    }
                    if(r.mount.size()+1u>selected.maximum_owned_text_bytes-text_bytes) { return fail(status::resource_limit,index,u8"mount text quota"); }text_bytes+=r.mount.size()+1u;
                    if(r.path.size()+1u>selected.maximum_owned_text_bytes-text_bytes) { return fail(status::resource_limit,index,u8"path text quota"); }text_bytes+=r.path.size()+1u;
                    actuals.push_back(ref);s->resources.push_back(::std::move(r));
                }
                s->bindings.push_back({static_cast<::std::uint32_t>(n),static_cast<::std::uint32_t>(index),base,inherit});
            }
            if(!pp::valid(*s)) { out.status=status::invalid_portable_snapshot;return out; }
            if(captured_identity!=nullptr) { *captured_identity=proof.capsule->environment_identity_; }
            out.portable=::std::move(s);out.status=status::captured;return out;
        }
        auto const& s=*selected.portable_restore;
        if(!pp::valid(s)) { out.status=status::invalid_portable_snapshot;return out; }
        // The request and its detached metadata must identify one recording,
        // just as in group import. A label is DATA, not restoration authority.
        if(selected.recording_label!=s.recording_label)
        { out.status=status::invalid_portable_snapshot;out.diagnostic=::fast_io::u8string{u8"recording label differs from import metadata"};return out; }
        if(s.bindings.size()+s.reserved.size()>selected.maximum_descriptors)
        { out.status=status::resource_limit;return out; }
        ::std::size_t portable_text{},portable_directories{};
        auto charge=[&](auto const& t) { if(t.size()+1u>selected.maximum_owned_text_bytes-portable_text) { return false; }portable_text+=t.size()+1u;return true; };
        for(auto const& t:s.arguments) { if(!charge(t)) { out.status=status::resource_limit;return out; } }
        for(auto const& t:s.environment) { if(!charge(t)) { out.status=status::resource_limit;return out; } }
        for(auto const& r:s.resources)
        {
            if(!charge(r.mount) || !charge(r.path)) { out.status=status::resource_limit;return out; }
            if(r.type==pp::kind::directory)
            { ++portable_directories;for(auto c:r.path) { if(c==u8'/') { ++portable_directories; } }if(!r.path.empty()) { ++portable_directories; } }
        }
        if(portable_directories>selected.maximum_directory_entries) { out.status=status::resource_limit;return out; }
        if(s.original_wasm!=proof.capsule->data_.original_wasm || s.builtin_interface!=proof.capsule->data_.builtin_interface)
        { out.status=status::stale_environment;return out; }
        if(s.bindings.size()+s.reserved.size()>table.fd_limit) { out.status=status::resource_limit;return out; }
        // Index saved empty cells once; every target reserved FD is protected.
        ::uwvm2::utils::container::map<::std::uint32_t,pp::empty_binding const*> reserved{};
        for(auto const& e:s.reserved) { reserved.emplace(e.descriptor,::std::addressof(e)); }
        auto protected_cell=[&](::std::uint64_t n,fm::wasi_fd_unique_ptr_t const& owner)
        {
            auto cell=owner.fd_p;
            if(cell==nullptr || cell->close_pos!=SIZE_MAX || (cell->wasi_fd.ptr!=nullptr && cell->wasi_fd.ptr->wasi_fd_storage.type!=fd_kind::null)) { return true; }
            auto found=reserved.find(static_cast<::std::uint32_t>(n));
            return found!=reserved.end() && found->second->base==static_cast<::std::uint64_t>(cell->rights_base) &&
                found->second->inheriting==static_cast<::std::uint64_t>(cell->rights_inherit) && found->second->null_resource==(cell->wasi_fd.ptr!=nullptr);
        };
        for(::std::size_t n{};n<table.opens.size();++n)
        { if(!protected_cell(n,table.opens.index_unchecked(n))) { return fail(status::capability_denied,n,u8"target reserved descriptor must remain reserved"); } }
        for(auto const& [n,cell]:table.renumber_map)
        { if(n<0 || !protected_cell(static_cast<::std::uint64_t>(n),cell)) { return fail(status::capability_denied,static_cast<::std::size_t>(n),u8"target reserved descriptor must remain reserved"); } }
        ::std::vector<::std::uint64_t> bases(s.resources.size()),inherits(s.resources.size());
        for(auto const& b:s.bindings) { bases[b.resource_index]|=b.base;inherits[b.resource_index]|=b.inheriting; }
        ::std::vector<fd*> targets(s.resources.size());
        for(auto const& b:selected.portable_rebindings)
        {
            if(b.resource_index>=s.resources.size() || b.target_descriptor>INT32_MAX || targets[b.resource_index]!=nullptr)
            { out.status=status::invalid_portable_snapshot;return out; }
            auto p=find_actual_descriptor(table,b.target_descriptor);
            if(p==nullptr || p->close_pos!=SIZE_MAX || p->wasi_fd.ptr==nullptr || p->wasi_fd.ptr->wasi_fd_storage.type==fd_kind::null)
            { return fail(status::missing_rebind,b.resource_index,u8"target FD unavailable"); }
            targets[b.resource_index]=p;
        }
        using indexed_target=::uwvm2::runtime::lib::wasip1_mount_identity::descriptor_index;
        ::uwvm2::utils::container::map<pp::text_view,indexed_target> mounts{};
        ::std::array<indexed_target,3u> channels{};
        auto add=[](indexed_target& into,fd* cell,bool mount=false)
        {
            if(into.cell!=nullptr && into.cell->wasi_fd.ptr!=cell->wasi_fd.ptr &&
               (!mount || !::uwvm2::runtime::lib::wasip1_mount_identity::same(into.cell->wasi_fd,cell->wasi_fd)))
            { into.ambiguous=true; }
            if(into.cell==nullptr) { into.cell=cell; }into.aliases.push_back(cell);
        };
        auto index_target=[&](fm::wasi_fd_unique_ptr_t const& owner)
        {
            auto cell=owner.fd_p;if(cell==nullptr || cell->close_pos!=SIZE_MAX || cell->wasi_fd.ptr==nullptr) { return; }
            auto const& rc=*cell->wasi_fd.ptr;
            if(rc.checkpoint_stdio_index>=0 && rc.checkpoint_stdio_index<=2) { add(channels[rc.checkpoint_stdio_index],cell); }
            if(rc.wasi_fd_storage.type!=fd_kind::dir) { return; }
            auto const& chain=rc.wasi_fd_storage.storage.dir_stack.dir_stack;
            if(chain.size()!=1u || chain.front_unchecked().ptr==nullptr) { return; }
            auto const& name=chain.front_unchecked().ptr->dir_stack.name;
            add(mounts[pp::text_view{name.data(),name.size()}],cell,true);
        };
        for(auto const& cell:table.opens) { index_target(cell); }
        for(auto const& [n,cell]:table.renumber_map) { (void)n;index_target(cell); }
        ::std::vector<fd_ref> replacements{};replacements.reserve(s.resources.size());
        for(::std::size_t i{};i<s.resources.size();++i)
        {
            auto r=s.resources[i];fd* target=targets[i];
            if(r.type==pp::kind::stdio && target==nullptr)
            {
                auto& channel=channels[r.stdio_index];
                if(channel.ambiguous) { return fail(status::missing_rebind,i,u8"ambiguous stdio binding"); }
                target=channel.select(bases[i],inherits[i]);
                if(target==nullptr && channel.cell!=nullptr) { return fail(status::capability_denied,i,u8"target stdio rights insufficient"); }
            }
            if(r.type==pp::kind::file && target!=nullptr)
            {
                if(!::uwvm2::uwvm::debugger::wasip1_state::rights_subset(bases[i],inherits[i],static_cast<::std::uint64_t>(target->rights_base),static_cast<::std::uint64_t>(target->rights_inherit)))
                { return fail(status::capability_denied,i,u8"target FD rights insufficient"); }
                auto const& rc=*target->wasi_fd.ptr;
                if(rc.checkpoint_reopenable && rc.wasi_fd_storage.type==fd_kind::file)
                { r.mount.assign(pp::text_view{rc.checkpoint_mount.data(),rc.checkpoint_mount.size()});r.path.assign(pp::text_view{rc.checkpoint_path.data(),rc.checkpoint_path.size()});r.follow=rc.checkpoint_follow;target=nullptr; }
                else
                { r.type=pp::kind::external;r.mount.clear();r.path.clear(); }
            }
            if(r.type==pp::kind::external || r.type==pp::kind::external_stream)
            {
                if(target==nullptr) { return fail(status::missing_rebind,i,u8"anonymous or untracked file requires resource=targetFD"); }
                if(!::uwvm2::uwvm::debugger::wasip1_state::rights_subset(bases[i],inherits[i],static_cast<::std::uint64_t>(target->rights_base),static_cast<::std::uint64_t>(target->rights_inherit)))
                { return fail(status::capability_denied,i,u8"target FD rights insufficient"); }
                auto const& rc=*target->wasi_fd.ptr;
                if(rc.checkpoint_reopenable && r.type==pp::kind::external)
                { r.type=pp::kind::file;r.mount.assign(pp::text_view{rc.checkpoint_mount.data(),rc.checkpoint_mount.size()});r.path.assign(pp::text_view{rc.checkpoint_path.data(),rc.checkpoint_path.size()});r.follow=rc.checkpoint_follow;target=nullptr; }
                else
                {
                    pp::resource state{};
                    if(!stat(target->wasi_fd,state) || state.offset!=r.offset || state.flags!=r.flags || state.stream_type!=r.stream_type ||
                       (r.type==pp::kind::external_stream ? state.type!=pp::kind::external_stream : state.type!=pp::kind::file))
                    { return fail(status::unsupported_resource,i,u8"supplied anonymous file must already have matching cursor and flags"); }
                    replacements.push_back(target->wasi_fd);continue;
                }
            }
            if(r.type==pp::kind::stdio)
            {
                if(target==nullptr || target->wasi_fd.ptr==nullptr || target->wasi_fd.ptr->wasi_fd_storage.type==fd_kind::null)
                { return fail(status::missing_rebind,i,u8"stdio unavailable"); }
                if(!::uwvm2::uwvm::debugger::wasip1_state::rights_subset(bases[i],inherits[i],static_cast<::std::uint64_t>(target->rights_base),static_cast<::std::uint64_t>(target->rights_inherit)))
                { return fail(status::capability_denied,i,u8"target stdio rights insufficient"); }
                replacements.push_back(target->wasi_fd);continue;
            }
            if(target!=nullptr) { return fail(status::invalid_portable_snapshot,i,u8"only external and stdio resources accept explicit bindings"); }
            // Resolve a unique actual configured preopen by guest name. Host
            // paths in source OS are absent. Existing path_open enforces the
            // original symlink/component boundary and target capability policy.
            auto found=mounts.find(pp::text_view{r.mount.data(),r.mount.size()});
            if(found==mounts.end()) { return fail(status::missing_mount,i,u8"configure target mnt with saved guest mount name"); }
            if(found->second.ambiguous) { return fail(status::missing_mount,i,u8"ambiguous mount name"); }
            bool const root_directory=r.type==pp::kind::directory && r.path.empty();
            // Select ONE actual FD for the entire operation. Child rights
            // derive from this parent's inheriting rights, never from its base
            // rights or a union of separately reduced aliases.
            bool holding_root{};
            if(root_directory)
            {
                target=found->second.select_flags(bases[i],inherits[i],r.flags,[&](fd const* candidate)->::std::optional<::std::uint16_t>
                {
                    pp::resource observed{};if(!stat(candidate->wasi_fd,observed)) { return ::std::nullopt; }
                    return observed.flags;
                });
                holding_root=target!=nullptr;
            }
            if(!holding_root)
            {
                auto required=static_cast<::std::uint64_t>(abi::rights_t::right_path_open);
                if((r.flags&24u)!=0u) { required|=static_cast<::std::uint64_t>(abi::rights_t::right_fd_sync); }
                auto inherited=inherits[i]|bases[i];
                if((r.flags&2u)!=0u)
                {
                    target=found->second.select(required|static_cast<::std::uint64_t>(abi::rights_t::right_fd_datasync),inherited);
                    if(target==nullptr) { target=found->second.select(required|static_cast<::std::uint64_t>(abi::rights_t::right_fd_sync),inherited); }
                }
                else { target=found->second.select(required,inherited); }
            }
            if(target==nullptr) { return fail(status::capability_denied,i,u8"target mount FD rights insufficient"); }
            if(r.type==pp::kind::directory && r.path.empty())
            {
                if(holding_root && !::uwvm2::uwvm::debugger::wasip1_state::rights_subset(bases[i],inherits[i],static_cast<::std::uint64_t>(target->rights_base),static_cast<::std::uint64_t>(target->rights_inherit)))
                { return fail(status::capability_denied,i,u8"target preopen rights insufficient"); }
                pp::resource observed{};if(!stat(target->wasi_fd,observed)) { return fail(status::native_operation_failed,i,u8"target directory flags unavailable"); }
#if defined(_WIN32) && !defined(__CYGWIN__)
                if(r.flags!=0u) { return fail(status::incompatible_flags,i,u8"Windows directory flags unsupported"); }
#endif
                // One held root has complete saved rights and exact or
                // independently mutable flags. Otherwise the parent selection
                // above authorized the entire fresh reopen.
                auto replacement=::uwvm2::runtime::lib::wasip1_mount_identity::copy_root(target->wasi_fd,r.follow);
                auto mode=::fast_io::open_mode::in|::fast_io::open_mode::directory|::fast_io::open_mode::explicit_disposition;
                if(r.flags&1u) { mode|=::fast_io::open_mode::app; }
                if(r.flags&2u) { mode|=::fast_io::open_mode::dsync; }
                if(r.flags&4u) { mode|=::fast_io::open_mode::no_block; }
                if(r.flags&8u) { mode|=::fast_io::open_mode::rsync; }
                if(r.flags&16u) { mode|=::fast_io::open_mode::sync; }
                auto error=fn::path_open_independent_directory(replacement.ptr->wasi_fd_storage.storage.dir_stack,mode);
                if(error!=abi::errno_t::esuccess)
                { return fail(error==abi::errno_t::enotsup || error==abi::errno_t::einval ? status::incompatible_flags : status::native_operation_failed,i,u8"target directory cannot be reopened"); }
                if(!stat(replacement,observed) || observed.type!=pp::kind::directory || observed.flags!=r.flags)
                { return fail(status::incompatible_flags,i,u8"target directory flags cannot be restored exactly"); }
                replacements.push_back(::std::move(replacement));continue;
            }
#if defined(_WIN32) && !defined(__CYGWIN__)
            if((r.flags&~1u)!=0u) { return fail(status::incompatible_flags,i,u8"Windows native provider cannot restore these flags exactly"); }
#endif
            wasip1_env_type stage{};stage.wasip1_memory=::std::addressof(scratch);stage.fd_storage.fd_limit=2u;
            // Portable paths must have one UTF-8 interpretation on every OS.
            // The target's ordinary byte-path opt-out cannot turn malformed
            // saved bytes into a replacement-character filename on Windows.
            stage.disable_utf8_check=false;stage.fd_storage.opens.push_back(clone(*target));
            auto first=reinterpret_cast<::std::byte const*>(r.path.data());
            ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory(scratch,32u,first,first+r.path.size());
            auto open_flags=r.flags;
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
            if(r.type==pp::kind::file) { open_flags|=4u; } // WASI nonblock during qualification only.
#endif
            auto error=fn::path_open(stage,0,r.follow ? abi::lookupflags_t::lookup_symlink_follow : abi::lookupflags_t{},32u,
                static_cast<abi::wasi_size_t>(r.path.size()),r.type==pp::kind::directory ? abi::oflags_t::o_directory : abi::oflags_t{},
                static_cast<abi::rights_t>(bases[i]),static_cast<abi::rights_t>(inherits[i]),static_cast<abi::fdflags_t>(open_flags),16u);
            if(error!=abi::errno_t::esuccess)
            { out.status=error==abi::errno_t::enotcapable ? status::capability_denied : status::native_operation_failed;out.diagnostic=::fast_io::u8concat_fast_io(u8"resource=",i,u8" path_open errno=",static_cast<unsigned>(error));return out; }
            auto number=::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory<::std::uint32_t>(scratch,16u);
            auto opened=find_actual_descriptor(stage.fd_storage,number);
            if(opened==nullptr || opened->wasi_fd.ptr==nullptr) { return fail(status::native_operation_failed,i,u8"reopen failed"); }
            if(r.type==pp::kind::file)
            {
                pp::resource observed{};if(!stat(opened->wasi_fd,observed) || observed.type!=pp::kind::file) { return fail(status::unsupported_resource,i,u8"target is not a regular file"); }
#if defined(__linux__) || defined(__FreeBSD__) || (defined(__APPLE__) && defined(__MACH__))
                if((r.flags&4u)==0u)
                {
                    auto& file=runtime_wasip1_debug_environment::managed_file(opened->wasi_fd);
                    ::uwvm2::runtime::lib::wasip1_native_file::set_flags(file,::uwvm2::runtime::lib::wasip1_native_file::flags(file)&~O_NONBLOCK);
                    if(!stat(opened->wasi_fd,observed)) { return fail(status::native_operation_failed,i,u8"flag verification failed"); }
                }
#endif
                if(observed.flags!=r.flags) { return fail(status::incompatible_flags,i,u8"target flag semantics differ"); }
                ::fast_io::operations::io_stream_seek_bytes(runtime_wasip1_debug_environment::managed_file(opened->wasi_fd),static_cast<::fast_io::intfpos_t>(r.offset),::fast_io::seekdir::beg);
            }
            if(r.type==pp::kind::directory)
            { pp::resource observed{};if(!stat(opened->wasi_fd,observed) || observed.type!=pp::kind::directory || observed.flags!=r.flags) { return fail(status::incompatible_flags,i,u8"target directory flags differ"); } }
            replacements.push_back(opened->wasi_fd);
        }
        // Preserve the saved alias graph. Two different saved resource rows
        // cannot reuse one authentic RC via the same FD or different FD aliases.
        // This checks privately prepared owners before any live table/text swap.
        if(auto conflict=::uwvm2::runtime::lib::wasip1_resource_identity::conflict(replacements))
        { return fail(status::unsupported_resource,*conflict,u8"distinct saved resources require distinct target owners"); }
        using strings=runtime_wasip1_debug_environment::strings;using views=runtime_wasip1_debug_environment::string_views;
        strings args{},envs{};views arg_views{},env_views{};
        auto copy=[](auto const& source,strings& owners,views& borrowed)
        { owners.reserve(source.size());borrowed.reserve(source.size());for(auto const& t:source) { owners.emplace_back(pp::text_view{t.data(),t.size()}); }for(auto const& t:owners) { borrowed.emplace_back(t.data(),t.size()); } };
        copy(s.arguments,args,arg_views);copy(s.environment,envs,env_views);
        ::uwvm2::utils::container::vector<fm::wasi_fd_unique_ptr_t> opens{};decltype(table.renumber_map) renumber{};
        ::uwvm2::utils::container::vector<::std::size_t> closes{};opens.reserve(s.opens_size);closes.reserve(s.closed.size());
        for(::std::size_t i{};i<s.opens_size;++i) { fm::wasi_fd_unique_ptr_t cell{};cell.fd_p->wasi_fd=fd_ref{fm::wasi_no_construct};opens.push_back(::std::move(cell)); }
        for(auto const& b:s.bindings)
        {
            fm::wasi_fd_unique_ptr_t cell{};cell.fd_p->wasi_fd=replacements[b.resource_index];cell.fd_p->rights_base=static_cast<abi::rights_t>(b.base);cell.fd_p->rights_inherit=static_cast<abi::rights_t>(b.inheriting);
            if(b.descriptor<opens.size()) { opens.index_unchecked(b.descriptor)=::std::move(cell); } else { renumber.emplace(static_cast<::std::int32_t>(b.descriptor),::std::move(cell)); }
        }
        for(auto const& e:s.reserved)
        {
            fm::wasi_fd_unique_ptr_t cell{};
            if(!e.null_resource) { cell.fd_p->wasi_fd=fd_ref{fm::wasi_no_construct}; }
            cell.fd_p->rights_base=static_cast<abi::rights_t>(e.base);cell.fd_p->rights_inherit=static_cast<abi::rights_t>(e.inheriting);
            // Reserved cells can be moved into the sparse map by the original
            // WASI fd_renumber, just like live bindings. Never index the dense
            // vector with an untrusted high guest FD.
            if(e.descriptor<opens.size()) { opens.index_unchecked(e.descriptor)=::std::move(cell); }
            else { renumber.emplace(static_cast<::std::int32_t>(e.descriptor),::std::move(cell)); }
        }
        for(auto n:s.closed) { opens.index_unchecked(n).fd_p->close_pos=closes.size();closes.push_back(n); }
        // Return private owned replacements only. The enclosing manager
        // rechecks its SAME lease/closed-host proof after native IO, then
        // publishes either this environment or the entire prepared group.
        // No live target text, mapping, native cursor or flags changed here.
        prepared->opens.swap(opens);prepared->renumber.swap(renumber);prepared->closes.swap(closes);
        prepared->new_arguments.swap(args);prepared->argument_views.swap(arg_views);
        prepared->new_variables.swap(envs);prepared->variable_views.swap(env_views);
        prepared->target=::std::addressof(environment);
        prepared->argument_target=arguments;prepared->variable_target=variables;
        out.status=status::captured;return out;
    }
    catch(::fast_io::error const&) { out.status=status::native_operation_failed;return out; }
    catch(...) { out.status=status::allocation_failed;return out; }
#else
    (void)key;(void)selected;(void)epoch;(void)profile;(void)prepared;(void)captured_identity;return {status::unavailable_environment,{}};
#endif
}

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
[[nodiscard]] static result prepare_portable_group_current(native_capture_key const& key,
    ::std::span<request const> selected, ::std::uint64_t epoch,
    ::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile,
    prepared_environment_group& prepared)
{
    namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
    result out{};out.observed_runtime_epoch=epoch;
    if(selected.empty() || selected.size()>registry_limit || !prepared.empty())
    { out.status=status::invalid_portable_snapshot;return out; }
    try
    {
        ::std::size_t text{},descriptors{};
        ::std::array<::std::byte,16u> label{};bool have_label{};
        // Bound the entire operation and validate its complete roster BEFORE
        // allocating replacement files or borrowing target FD capabilities.
        for(::std::size_t index{};index!=selected.size();++index)
        {
            auto const& row{selected[index]};auto const& snapshot{row.portable_restore};
            auto fail=[&](status value,::fast_io::u8string_view why)
            { out.status=value;out.diagnostic=::fast_io::u8concat_fast_io(u8"request=",index,u8" ",why);return out; };
            if(!snapshot || row.portable_metadata_only || !pp::valid(*snapshot) || row.recording_label!=snapshot->recording_label)
            { return fail(status::invalid_portable_snapshot,u8"valid import metadata and matching recording label required"); }
            if(have_label && label!=snapshot->recording_label)
            { return fail(status::invalid_portable_snapshot,u8"environment recording labels differ"); }
            label=snapshot->recording_label;have_label=true;
            if(snapshot->bindings.size()>descriptor_limit-descriptors)
            { return fail(status::resource_limit,u8"aggregate descriptor quota"); }
            descriptors+=snapshot->bindings.size();
            if(snapshot->reserved.size()>descriptor_limit-descriptors)
            { return fail(status::resource_limit,u8"aggregate reserved descriptor quota"); }
            descriptors+=snapshot->reserved.size();
            auto charge=[&](auto const& value)
            { if(value.size()>=text_limit-text) { return false; }text+=value.size()+1u;return true; };
            for(auto const& value:snapshot->arguments) { if(!charge(value)) { return fail(status::resource_limit,u8"aggregate text quota"); } }
            for(auto const& value:snapshot->environment) { if(!charge(value)) { return fail(status::resource_limit,u8"aggregate text quota"); } }
            for(auto const& resource:snapshot->resources)
            { if(!charge(resource.mount) || !charge(resource.path)) { return fail(status::resource_limit,u8"aggregate path quota"); } }
        }
        prepared.reserve(selected.size());
        for(::std::size_t index{};index!=selected.size();++index)
        {
            auto next{::std::make_unique<prepared_current_environment>()};
            auto checked{portable_current(key,selected[index],epoch,profile,next.get())};
            if(checked.status!=status::captured)
            {
                prepared.clear();checked.diagnostic=::fast_io::u8concat_fast_io(u8"request=",index,u8" ",
                    ::fast_io::u8string_view{checked.diagnostic.data(),checked.diagnostic.size()});return checked;
            }
            for(auto const& prior:prepared)
            {
                if(prior->target==next->target)
                {
                    prepared.clear();out.status=status::invalid_portable_snapshot;
                    out.diagnostic=::fast_io::u8concat_fast_io(u8"request=",index,u8" duplicate target environment");return out;
                }
            }
            prepared.push_back(::std::move(next));
        }
        out.status=status::captured;return out;
    }
    catch(::fast_io::error const&) { prepared.clear();out.status=status::native_operation_failed;return out; }
    catch(...) { prepared.clear();out.status=status::allocation_failed;return out; }
}
#endif

#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
[[nodiscard]] static llvm_jit_wasip1_portable_environment_group_result capture_portable_group_current(
    native_capture_key const& key,::std::span<request const> selected,::std::uint64_t epoch,
    ::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile)
{
    namespace pp=::uwvm2::uwvm::debugger::wasip1_portable;
    llvm_jit_wasip1_portable_environment_group_result out{};out.observed_runtime_epoch=epoch;
    if(selected.empty() || selected.size()>pp::max_environments) { out.status=status::invalid_portable_snapshot;return out; }
    try
    {
        ::std::array<void const*,pp::max_environments> identities{};
        for(::std::size_t i{};i!=selected.size();++i)
        {
            auto fail=[&](status value,::fast_io::u8string_view why)
            { out.portable.environments.clear();out.status=value;out.diagnostic=::fast_io::u8concat_fast_io(u8"request=",i,u8" ",why);return ::std::move(out); };
            auto const& row=selected[i];
            if(!row.portable_metadata_only || row.portable_restore || !row.portable_rebindings.empty() || row.recording_label!=selected.front().recording_label)
            { return fail(status::invalid_portable_snapshot,u8"metadata-only export and common recording label required"); }
            auto checked=portable_current(key,row,epoch,profile,nullptr,::std::addressof(identities[i]));
            if(checked.status!=status::captured || !checked.portable)
            { return fail(checked.status,::fast_io::u8string_view{checked.diagnostic.data(),checked.diagnostic.size()}); }
            for(::std::size_t j{};j!=i;++j)
            { if(identities[j]==identities[i]) { return fail(status::invalid_portable_snapshot,u8"duplicate actual environment"); } }
            out.portable.environments.push_back(*checked.portable);
            if(!pp::valid_group(out.portable)) { return fail(status::resource_limit,u8"aggregate portable group quota"); }
        }
        out.status=status::captured;return out;
    }
    catch(::fast_io::error const&) { out.portable.environments.clear();out.status=status::native_operation_failed;return out; }
    catch(...) { out.portable.environments.clear();out.status=status::allocation_failed;return out; }
}
#endif
