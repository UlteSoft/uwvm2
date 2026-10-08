// Private native-lifetime producer; included inside the runtime namespace
// after the original WASIp1 environment resolver and cold debug adapter.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++"
{
    class llvm_jit_wasip1_environment_capsule final
    {
        friend class runtime_checkpoint_coherent_manager;
        friend class runtime_checkpoint_world_transaction;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(
            llvm_jit_wasip1_environment_capsule_owner const&) noexcept -> llvm_jit_wasip1_environment_capsule_data_result;
        using status = llvm_jit_wasip1_environment_capsule_status;
        using request = llvm_jit_wasip1_environment_capsule_request;
        using result = llvm_jit_wasip1_environment_capsule_result;
        using owner = llvm_jit_wasip1_environment_capsule_owner;
        // A future world consumer may canonicalize/read this native owner,
        // but cannot normally mint one: only the actual manager can construct
        // this lexical key, and only AFTER its complete current proof closure.
        class native_capture_key final
        {
            friend class runtime_checkpoint_coherent_manager;
            native_capture_key()=default;
        };
        static constexpr ::std::size_t registry_limit{16u}, descriptor_limit{65536u}, text_limit{1048576u};
        inline static ::std::mutex registry_mutex_{};
        inline static ::std::weak_ptr<llvm_jit_wasip1_environment_capsule const> registry_[registry_limit]{};
        inline static ::std::uint64_t next_serial_{1u};
        llvm_jit_wasip1_environment_capsule_data data_{};
        void const* environment_identity_{}; // comparison only, never dereferenced or serialized
        ::std::size_t opens_size_{};
        ::std::vector<::std::size_t> closed_order_{};
        struct empty_binding { ::std::uint64_t number{},base{},inheriting{}; bool null_resource{}; };
        ::std::vector<empty_binding> reserved_empty_{}; // non-allocatable placeholders, including debugger fd0
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        using fd = ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t;
        using fd_ref = ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_ref_t;
        using fd_kind = ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e;
        using dir_ref = ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t;
        struct directory_pin
        {
            dir_ref actual;
            explicit directory_pin(dir_ref const& original) noexcept : actual{original} {}
            directory_pin(directory_pin const&)=delete;
            directory_pin& operator=(directory_pin const&)=delete;
            directory_pin(directory_pin&&) noexcept=default;
            directory_pin& operator=(directory_pin&&) noexcept=default;
            ::fast_io::dir_file owned_observer_duplicate{};
            ::uwvm2::uwvm::debugger::wasip1_state::text original_name{};
            bool was_observer{};
            // Actual RC is retained even for an observer, but only the typed
            // duplicate owns its native lifetime. Original observer is never a
            // historical restore handle. Names remain private native metadata.
        };
        struct resource_pin
        {
            resource_pin()=default;
            resource_pin(resource_pin const&)=delete;
            resource_pin& operator=(resource_pin const&)=delete;
            resource_pin(resource_pin&&) noexcept=default;
            resource_pin& operator=(resource_pin&&) noexcept=default;
            fd_ref actual{::uwvm2::imported::wasi::wasip1::fd_manager::wasi_no_construct};
            fd_kind original_kind{};
            ::std::uint64_t managed_identity{};
            ::fast_io::intfpos_t managed_offset{};
            int managed_flags{};
            ::std::vector<::std::byte> managed_content{};
            ::fast_io::native_file owned_observer_duplicate{};
#if defined(_WIN32) && !defined(__CYGWIN__)
            ::fast_io::win32_socket_file owned_socket_observer_duplicate{};
#endif
            ::std::vector<directory_pin> directories{};
            // Genuine refs retain owned FastIO RAII resources without an old
            // module, source, engine, host gate or VM generation owner.
        };
        struct descriptor_binding { ::std::uint64_t guest_number{}; ::std::size_t resource{}; };
        ::std::vector<resource_pin> resources_{};
        ::std::vector<descriptor_binding> bindings_{};
        [[nodiscard]] static fd* find_actual_descriptor(
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasm_fd_storage_t& table, ::std::uint64_t number) noexcept
        { return runtime_wasip1_debug_environment::find_descriptor(table,number); }
        [[nodiscard]] status pin_actual_resource(fd_ref const& actual, request const& selected, ::std::size_t& index)
        {
            if(actual.ptr == nullptr || actual.ptr->refcount.load(::std::memory_order_relaxed) >= SIZE_MAX / 2u)
            { return status::unavailable_environment; }
            for(::std::size_t i{}; i != resources_.size(); ++i)
            {
                // [private retained actual RC vector ... i<N] end
                // [safe] compare authentic RC identity inside the real closed
                // host/N transaction; no caller address or descriptor ID enters.
                if(resources_[i].actual.ptr == actual.ptr) { index=i; return status::captured; }
            }
            if(resources_.size() == selected.maximum_descriptors) { return status::resource_limit; }
            resource_pin candidate{}; candidate.actual=actual;
            auto const& storage{actual.ptr->wasi_fd_storage}; // Actual locked FD owns this authenticated RC.
            candidate.original_kind=storage.type;
            switch(storage.type)
            {
                case fd_kind::file:
#if defined(_WIN32) && !defined(__CYGWIN__)
                    if(!storage.storage.file_fd.file) { return status::unavailable_environment; }
#else
                    if(!storage.storage.file_fd) { return status::unavailable_environment; }
#endif
                    break; // Actual RC owns the native_file; no syscall/copy of its storage union.
                case fd_kind::file_observer:
                    try { candidate.owned_observer_duplicate=::fast_io::native_file{::fast_io::io_dup,storage.storage.file_observer}; }
                    catch(...) { return status::native_duplicate_failed; }
                    ++data_.duplicated_observers; break;
                case fd_kind::dir:
                {
                    auto const& chain{storage.storage.dir_stack.dir_stack};
                    if(chain.empty() || chain.size()>selected.maximum_directory_entries-data_.captured_directory_entries)
                    { return status::resource_limit; }
                    candidate.directories.reserve(chain.size());
                    for(::std::size_t n{}; n!=chain.size(); ++n)
                    {
                        // [actual locked directory chain ... n<size] end
                        // [safe] count checked BEFORE index; retain RC BEFORE
                        // any entry payload read, with all host actors excluded.
                        auto const& entry{chain.index_unchecked(n)};
                        if(entry.ptr==nullptr || entry.ptr->refcount.load(::std::memory_order_relaxed)>=SIZE_MAX/2u)
                        { return status::unavailable_environment; }
                        auto const& current{entry.ptr->dir_stack};
                        if(current.name.size()>selected.maximum_owned_text_bytes-owned_text_bytes_)
                        { return status::resource_limit; }
                        directory_pin captured{entry}; captured.was_observer=current.is_observer;
                        captured.original_name.assign(::fast_io::u8string_view{current.name.data(),current.name.size()});
                        owned_text_bytes_+=current.name.size(); // Subtraction bound checked BEFORE advance.
                        if(current.is_observer)
                        {
                            try
                            {
                                // FastIO directory wrapper does not inherit the
                                // io_dup constructor. Duplicate through its real
                                // typed platform owner, then transfer that owned
                                // result into the noexcept directory wrapper.
#if (defined(_WIN32) || defined(__CYGWIN__)) && defined(_WIN32_WINDOWS)
                                ::fast_io::win32_9xa_dir_file duplicate{::fast_io::io_dup,current.storage.observer};
#else
                                ::fast_io::native_file duplicate{::fast_io::io_dup,current.storage.observer};
#endif
                                captured.owned_observer_duplicate=::fast_io::dir_file{duplicate.release()};
                            }
                            catch(...) { return status::native_duplicate_failed; }
                            ++data_.duplicated_observers;
                        }
                        else if(!current.storage.file) { return status::unavailable_environment; }
                        candidate.directories.push_back(::std::move(captured));
                    }
                    data_.captured_directory_entries+=chain.size(); // Checked remaining count BEFORE advance.
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case fd_kind::socket:
                    if(storage.storage.socket_fd.native_handle()==0u) { return status::unavailable_environment; }
                    break;
                case fd_kind::socket_observer:
                    try { candidate.owned_socket_observer_duplicate=::fast_io::win32_socket_file{::fast_io::io_dup,storage.storage.socket_observer}; }
                    catch(...) { return status::native_duplicate_failed; }
                    ++data_.duplicated_observers; break;
#endif
                default: return status::unavailable_environment;
            }
            if(actual.ptr->checkpoint_managed_identity!=0u)
            {
                if(storage.type!=fd_kind::file) { return status::unavailable_environment; }
                try
                {
                auto& file{runtime_wasip1_debug_environment::managed_file(actual)};
                auto const file_status{::fast_io::status(file)};
                if(file_status.size>selected.maximum_managed_file_bytes ||
                   file_status.size>selected.maximum_managed_total_bytes-data_.managed_content_bytes)
                { return status::resource_limit; }
                candidate.managed_identity=actual.ptr->checkpoint_managed_identity;
                candidate.managed_offset=::fast_io::operations::io_stream_seek_bytes(file,0,::fast_io::seekdir::cur);
                candidate.managed_flags=runtime_wasip1_debug_environment::managed_flags(actual);
                candidate.managed_content.resize(static_cast<::std::size_t>(file_status.size));
                if(!candidate.managed_content.empty())
                { ::uwvm2::runtime::lib::wasip1_native_file::read_content(file,candidate.managed_content.data(),candidate.managed_content.data()+candidate.managed_content.size()); }
                data_.managed_content_bytes+=candidate.managed_content.size();++data_.managed_resources;
                }
                catch(::fast_io::error const&) { return status::native_operation_failed; }
            }
            else { ++data_.retained_external_resources; }
            index=resources_.size();resources_.push_back(::std::move(candidate));return status::captured;
        }
        ::std::size_t owned_text_bytes_{};
        [[nodiscard]] bool copy_actual_text(runtime_wasip1_debug_environment::strings const& backing,
            runtime_wasip1_debug_environment::string_views const& views,
            ::std::vector<::uwvm2::uwvm::debugger::wasip1_state::text>& output, request const& selected)
        {
            if(!runtime_wasip1_debug_environment::bound_text(backing,views)) { return false; }
            output.reserve(backing.size());
            for(auto const& actual:backing)
            {
                if(actual.size()>selected.maximum_owned_text_bytes-owned_text_bytes_) { return false; }
                output.emplace_back(::fast_io::u8string_view{actual.data(),actual.size()});
                owned_text_bytes_+=actual.size(); // Remaining byte budget checked BEFORE advance.
            }
            return true;
        }
        [[nodiscard]] status copy_actual_descriptors(wasip1_env_type& environment,request const& selected)
        {
            auto& table{environment.fd_storage};
            // Exact original lock order, nested ONLY under manager's actual
            // closed-host/cohort/N/publication scope. No provider callback.
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t table_lock{table.fds_rwlock};
            if(table.opens.size()>descriptor_limit || table.renumber_map.size()>descriptor_limit-table.opens.size())
            { return status::resource_limit; }
            if(table.closes.size()>table.opens.size()) { return status::unavailable_environment; }
            if(!table.fits_occupied_slot_limit(selected.maximum_descriptors)) { return status::resource_limit; }
            opens_size_=table.opens.size();
            closed_order_.assign(table.closes.begin(),table.closes.end());
            ::std::vector<::std::uint64_t> numbers{};numbers.reserve(table.opens.size()+table.renumber_map.size());
            for(::std::size_t n{};n!=table.opens.size();++n) { numbers.push_back(n); }
            for(auto const& [number,ignored]:table.renumber_map)
            {
                (void)ignored;
                if(number<0 || static_cast<::std::uint64_t>(number)<table.opens.size()) { return status::unavailable_environment; }
                numbers.push_back(static_cast<::std::uint64_t>(number));
            }
            if(numbers.size()>1u) { ::std::sort(numbers.begin(),numbers.end()); }
            for(::std::size_t n{1u};n<numbers.size();++n)
            {
                // [sorted owned bounded ID vector ... n-1,n<N] end
                // [safe] n>=1 and n<N BEFORE either read.
                if(numbers[n-1u]==numbers[n]) { return status::unavailable_environment; }
            }
            for(auto const number:numbers)
            {
                auto const actual{find_actual_descriptor(table,number)};
                if(actual==nullptr) { reserved_empty_.push_back({number});continue; }
                ::uwvm2::utils::mutex::mutex_guard_t fd_lock{actual->fd_mutex};
                if(actual->close_pos!=SIZE_MAX) { continue; }
                if(actual->wasi_fd.ptr==nullptr || actual->wasi_fd.ptr->wasi_fd_storage.type==fd_kind::null)
                { reserved_empty_.push_back({number,static_cast<::std::uint64_t>(actual->rights_base),
                    static_cast<::std::uint64_t>(actual->rights_inherit),actual->wasi_fd.ptr!=nullptr});continue; }
                if(data_.descriptors.size()==selected.maximum_descriptors) { return status::resource_limit; }
                ::uwvm2::uwvm::debugger::wasip1_state::descriptor_entry row{};
                if(!runtime_wasip1_debug_environment::descriptor_row(*actual,number,row)) { return status::unavailable_environment; }
                ::std::size_t resource{};auto const pinned{pin_actual_resource(actual->wasi_fd,selected,resource)};
                if(pinned!=status::captured) { return pinned; }
                if(row.guest_preopen_name.size()>selected.maximum_owned_text_bytes-owned_text_bytes_) { return status::resource_limit; }
                owned_text_bytes_+=row.guest_preopen_name.size(); // Count copied alias rows, checked BEFORE advance.
                data_.descriptors.push_back(::std::move(row));bindings_.push_back({number,resource});
            }
            data_.distinct_native_bindings=resources_.size();return status::captured;
        }
#endif
#include "uwvm_runtime_wasip1_environment_restore.h"
#include "uwvm_runtime_wasip1_portable_environment.h"
        explicit llvm_jit_wasip1_environment_capsule(native_capture_key const&) noexcept {}
        [[nodiscard]] static owner canonical(owner const& supplied) noexcept
        {
            if(!supplied) { return {}; }
            ::std::lock_guard lock{registry_mutex_};
            for(auto const& weak:registry_)
            {
                auto actual{weak.lock()};
                // [private weak registry -> genuine immutable object] [request]
                // [safe] pointer AND control block compared BEFORE supplied
                // pointee access. Same-address alias does not own the capsule.
                if(actual && actual.get()==supplied.get() && !actual.owner_before(supplied) && !supplied.owner_before(actual)) { return actual; }
            }
            return {};
        }
        [[nodiscard]] static result capture_current(native_capture_key const& actual_key,request const& selected,::std::uint_least64_t epoch,
            ::uwvm2::runtime::checkpoint::compilation_profile::owner const& actual_profile, bool capture_resources=true)
        {
            bool labelled{};for(auto byte:selected.recording_label) { labelled=labelled || byte!=::std::byte{}; }
            if(!labelled) { return {status::invalid_recording_label,{}}; }
            if(selected.maximum_descriptors>descriptor_limit || selected.maximum_directory_entries>descriptor_limit ||
               selected.maximum_owned_text_bytes>text_limit || selected.maximum_managed_file_bytes>16777216u ||
               selected.maximum_managed_total_bytes>67108864u) { return {status::resource_limit,{}}; }
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            namespace source_ns=::uwvm2::uwvm::runtime::full;
            namespace storage=::uwvm2::uwvm::imported::wasi::wasip1::storage;
            if(epoch==0u || epoch!=current_runtime_generation() || selected.module>=g_runtime.modules.size() ||
               g_runtime.modules.size()>4096u || !is_wasip1_import_visible_for_runtime_module_id_slow(static_cast<::std::size_t>(selected.module)))
            { return {status::unavailable_environment,{}}; }
            auto const id{static_cast<::std::size_t>(selected.module)};
            // [actual generation module vector ... id<N] end
            // [safe] narrowing and read only AFTER bound and genuine manager
            // lease/ONE/hostclose/N/publication preflight, never caller pointers.
            auto const& record{g_runtime.modules.index_unchecked(id)};auto const* code{record.llvm_jit_full_publication.get()};
            auto const* module{record.runtime_module};
            if(module==nullptr || code==nullptr || !record.llvm_jit_ready || !code->engine || !code->context || code->plan ||
               code->debug_source_runtime_epoch!=epoch || record.llvm_jit_debug_source_fused_epoch!=epoch ||
               !actual_profile || !code->checkpoint_profile || code->checkpoint_profile.get()!=actual_profile.get() ||
               code->checkpoint_profile.owner_before(actual_profile) || actual_profile.owner_before(code->checkpoint_profile) ||
               !source_ns::full_source_instance::has_canonical_owner(code->source))
            { return {status::unavailable_environment,{}}; }
            auto const& source{code->source};auto const* file{source->actual_validated_file(id,epoch,module)};
            if(file==nullptr || !file->has_owned_source_image() ||
               !source->actual_no_unadapted_native_memory_provider(id,epoch,module))
            { return {status::unavailable_environment,{}}; }
            auto const member{source->registry().find(record.module_name)};
            if(member==source->registry().end() || ::std::addressof(member->second)!=module) { return {status::unavailable_environment,{}}; }
            // A transitive receiver need not directly import the native tuple.
            // Prove at least one REAL direct builtin member of THIS canonical
            // initialized source graph; visibility/name alone is insufficient.
            bool builtin{};source_ns::builtin_wasip1_function_data witness{};::std::size_t imports{};
            for(::std::size_t n{};n!=g_runtime.modules.size();++n)
            {
                auto const& other{g_runtime.modules.index_unchecked(n)};auto const* publication{other.llvm_jit_full_publication.get()};
                if(publication==nullptr || publication->source.get()!=source.get() || publication->source.owner_before(source) || source.owner_before(publication->source)) { continue; }
                auto const* native{other.runtime_module};
                if(native==nullptr || source->actual_validated_file(n,epoch,native)==nullptr) { return {status::unavailable_environment,{}}; }
                auto const size{native->imported_function_vec_storage.size()};
                if(size>descriptor_limit-imports) { return {status::resource_limit,{}}; }imports+=size;
                for(::std::size_t i{};i!=size;++i)
                {
                    source_ns::builtin_wasip1_function_data current{};
                    if(source->actual_builtin_wasip1_function(n,epoch,native,i,current))
                    {
                        if(builtin && current.interface_sha256!=witness.interface_sha256) { return {status::unavailable_environment,{}}; }
                        witness=current;builtin=true;
                    }
                }
            }
            if(!builtin) { return {status::unavailable_environment,{}}; }
            ::std::shared_ptr<llvm_jit_wasip1_environment_capsule> result{new llvm_jit_wasip1_environment_capsule{actual_key}};
            result->data_.recording_label=selected.recording_label;result->data_.observed_runtime_epoch=epoch;result->data_.module=selected.module;
            result->data_.builtin_interface=witness.interface_sha256;
            if(file->source_cbegin()==nullptr || file->source_size()==0u ||
               file->source_size()>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()))
            { return {status::unavailable_environment,{}}; }
            auto const* bytes{reinterpret_cast<::std::byte const*>(file->source_cbegin())};
            ::fast_io::sha256_context original_hash{};
            // [actual immutable owned Wasm bytes ... size<=PTRDIFF_MAX] end
            // [safe] nonnull, nonempty original image and full source ownership
            // were checked BEFORE forming its exclusive-end cursor below.
            original_hash.update(bytes,bytes+file->source_size());
            original_hash.do_final();original_hash.digest_to_byte_ptr(result->data_.original_wasm.data());
            auto& environment{resolve_wasip1_env_for_runtime_module_id(id)};
            result->environment_identity_=::std::addressof(environment);
            if(!capture_resources) { return {status::captured,::std::move(result)}; }
            auto const* arguments{::std::addressof(storage::wasip1_argument_storage)};
            auto const* variables{::std::addressof(storage::wasip1_environment_storage)};
            if(::std::addressof(environment)!=::std::addressof(storage::default_wasip1_env))
            {
                auto const group{find_wasip1_override_for_runtime_module_id_slow(id)};
                if(group==nullptr || ::std::addressof(group->env)!=::std::addressof(environment)) { return {status::unavailable_owned_text,{}}; }
                arguments=::std::addressof(group->argument_storage);variables=::std::addressof(group->environment_storage);
            }
            if(!result->copy_actual_text(*arguments,environment.argv,result->data_.arguments,selected) ||
               !result->copy_actual_text(*variables,environment.envs,result->data_.environment,selected)) { return {status::unavailable_owned_text,{}}; }
            ::std::size_t aliases{};
            for(::std::size_t n{};n!=g_runtime.modules.size();++n)
            { if(is_wasip1_import_visible_for_runtime_module_id_slow(n) && ::std::addressof(resolve_wasip1_env_for_runtime_module_id(n))==::std::addressof(environment)) { ++aliases; } }
            result->data_.shared_environment=aliases>1u;
            auto const pinned{result->copy_actual_descriptors(environment,selected)};if(pinned!=status::captured) { return {pinned,{}}; }
            // Publish only a complete immutable native capsule. Registry owns
            // weak refs only; releasing final trusted owner closes its duplicates.
            // No native/source/engine/domain pointers are serialized as DATA.
            ::std::lock_guard lock{registry_mutex_};
            if(next_serial_==0u) { return {status::registry_exhausted,{}}; }
            for(auto& weak:registry_)
            {
                if(weak.expired())
                {
                    result->data_.issuer_serial=next_serial_;
                    next_serial_=next_serial_==UINT64_MAX ? 0u : next_serial_+1u; // Checked before advance; exhausted IDs never reused.
                    weak=result;return {status::captured,::std::move(result)};
                }
            }
            return {status::registry_exhausted,{}};
#else
            (void)actual_key;(void)epoch;(void)actual_profile;return {status::unavailable_environment,{}};
#endif
        }
    public:
        llvm_jit_wasip1_environment_capsule(llvm_jit_wasip1_environment_capsule const&)=delete;
        llvm_jit_wasip1_environment_capsule& operator=(llvm_jit_wasip1_environment_capsule const&)=delete;
        ~llvm_jit_wasip1_environment_capsule()=default;
    };
}
#endif
