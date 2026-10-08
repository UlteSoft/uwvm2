// Private cold adapter. Included INSIDE the actual runtime namespace AFTER
// Wasip1 resolution, publication and private checkpoint admission definitions.
// Only the coherent manager can invoke it, inside its ONE actual proof scope.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++"
{
    class runtime_wasip1_debug_environment final
    {
        friend class runtime_checkpoint_coherent_manager;
        friend class llvm_jit_wasip1_environment_capsule;
        friend class runtime_checkpoint_world_transaction;
        runtime_wasip1_debug_environment() = delete;
        using request = ::uwvm2::uwvm::debugger::wasip1_state::request;
        using view = ::uwvm2::uwvm::debugger::wasip1_state::view;
        using status = ::uwvm2::uwvm::debugger::wasip1_state::status;
        using action = ::uwvm2::uwvm::debugger::wasip1_state::action;
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        using fd = ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_t;
        using fd_type = ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e;
        using strings = ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string>;
        using string_views = ::uwvm2::utils::container::vector<::uwvm2::utils::container::u8string_view>;
        [[nodiscard]] static bool bound_text(strings const& backing, string_views const& views) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
            if(backing.size() != views.size() || views.size() > ws::maximum_entries) { return false; }
            ::std::size_t bytes{};
            for(::std::size_t i{}; i != views.size(); ++i)
            {
                // [actual owned vectors with equal counts ... i<N] end
                // [safe] compare view addresses and extents BEFORE borrowing its
                // bytes. No arbitrary caller view is dereferenced as ownership.
                auto const& owner{backing.index_unchecked(i)}; auto const& borrowed{views.index_unchecked(i)};
                if(owner.data() != borrowed.data() || owner.size() != borrowed.size() ||
                   owner.size() > ws::maximum_text_bytes || owner.size() + 1u > ws::maximum_environment_bytes - bytes)
                { return false; }
                bytes += owner.size() + 1u; // Checked bounded accumulation BEFORE addition.
                for(char8_t c : owner) { if(c == u8'\0') { return false; } }
            }
            return true;
        }
        [[nodiscard]] static view text_operation(request const& selected,
            strings& backing, string_views& live) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
            view out{}; out.module = selected.module; out.operation = selected.operation;
            if(!bound_text(backing, live)) { out.result = status::unavailable_owned_text; return out; }
            out.total_entries = backing.size();
            bool const environment{selected.operation == action::environment || selected.operation == action::set_environment ||
                selected.operation == action::remove_environment};
            if(!ws::is_mutation(selected.operation))
            {
                auto const first{selected.first < backing.size() ? static_cast<::std::size_t>(selected.first) : backing.size()};
                auto const count{(::std::min)(static_cast<::std::size_t>(selected.count), backing.size() - first)};
                out.strings.reserve(count);
                ::std::size_t page_bytes{};
                out.next = first;
                for(::std::size_t i{}; i != count; ++i)
                {
                    // [checked page first..first+count<=N] end
                    // [safe] first+i<N; original storage, never request pointers.
                    auto const& entry{backing.index_unchecked(first + i)};
                    if(entry.size() > ws::maximum_page_text_bytes - page_bytes) { break; }
                    out.strings.push_back({first + i, ws::text{ws::text_view{entry.data(), entry.size()}}});
                    page_bytes += entry.size(); // Checked byte budget BEFORE addition.
                    out.next = first + i + 1u; // first+i<N proves this <=N.
                }
                out.more = out.next < backing.size(); out.result = status::ok; return out;
            }
            strings candidate{};
            bool const insert{selected.operation == action::insert_argument};
            bool const remove{selected.operation == action::remove_argument};
            if(insert && backing.size() == ws::maximum_entries) { out.result = status::resource_limit; return out; }
            if(insert && selected.index > backing.size()) { out.result = status::entry_not_found; return out; }
            candidate.reserve(backing.size() + static_cast<::std::size_t>(selected.operation == action::set_environment || insert));
            bool found{};
            if((selected.operation == action::replace_argument || remove) && selected.index >= backing.size())
            { out.result = status::entry_not_found; return out; }
            for(::std::size_t i{}; i != backing.size(); ++i)
            {
                auto const& entry{backing.index_unchecked(i)};
                if(insert && selected.index == i) { candidate.emplace_back(ws::text_view{selected.value.data(), selected.value.size()}); }
                if(remove && selected.index == i) { found = true; continue; }
                bool const matched{environment && ws::environment_key_matches(ws::text_view{entry.data(), entry.size()}, ws::text_view{selected.name.data(), selected.name.size()})};
                if(matched)
                {
                    if(found) { out.result = status::unavailable_owned_text; return out; } // Ambiguous duplicate key, leave every vector unchanged.
                    found = true;
                    if(selected.operation == action::remove_environment) { continue; }
                    ::uwvm2::utils::container::u8string replacement{};
                    ::fast_io::io::print(::uwvm2::utils::container::u8string_ref_uwvm{::std::addressof(replacement)},
                        ::fast_io::u8string_view{selected.name.data(), selected.name.size()}, ::fast_io::mnp::chvw(u8'='),
                        ::fast_io::u8string_view{selected.value.data(), selected.value.size()});
                    candidate.push_back(::std::move(replacement));
                }
                else if(!environment && !insert && selected.index == i)
                { candidate.emplace_back(ws::text_view{selected.value.data(), selected.value.size()}); found = true; }
                else { candidate.push_back(entry); }
            }
            if(insert && selected.index == backing.size())
            { candidate.emplace_back(ws::text_view{selected.value.data(), selected.value.size()}); }
            if(environment && !found)
            {
                if(selected.operation == action::remove_environment) { out.result = status::entry_not_found; return out; }
                if(candidate.size() == ws::maximum_entries) { out.result = status::resource_limit; return out; }
                ::uwvm2::utils::container::u8string replacement{};
                ::fast_io::io::print(::uwvm2::utils::container::u8string_ref_uwvm{::std::addressof(replacement)},
                    ::fast_io::u8string_view{selected.name.data(), selected.name.size()}, ::fast_io::mnp::chvw(u8'='),
                    ::fast_io::u8string_view{selected.value.data(), selected.value.size()});
                candidate.push_back(::std::move(replacement));
            }
            string_views candidate_views{}; candidate_views.reserve(candidate.size());
            ::std::size_t bytes{};
            for(auto const& entry : candidate)
            {
                if(entry.size() + 1u > ws::maximum_environment_bytes - bytes)
                { out.result = status::resource_limit; return out; }
                bytes += entry.size() + 1u;
                candidate_views.emplace_back(entry.data(), entry.size());
            }
            out.total_entries = candidate.size();
            // No allocate/throw/provider/guest callback after commit begins.
            // All real guest actors are parked and host admission is closed.
            // Candidate_views points into candidate's individually owned strings;
            // vector swap transfers buffers without relocating string objects.
            // First publish the new views, then their owning buffer. The retired
            // views/backing die only after BOTH swaps complete in this proof scope.
            live.swap(candidate_views); backing.swap(candidate);
            out.mutation_applied = true; out.result = status::ok; return out;
        }
        [[nodiscard]] static fd* find_descriptor(::uwvm2::imported::wasi::wasip1::fd_manager::wasm_fd_storage_t& storage,
            ::std::uint64_t number) noexcept
        {
            if(number > static_cast<::std::uint64_t>((::std::numeric_limits<::std::int32_t>::max)())) { return nullptr; }
            if(number < storage.opens.size())
            {
                // [actual manager opens vector ... number<size] end
                // [safe] bounded unsigned integer is checked BEFORE narrowing.
                return storage.opens.index_unchecked(static_cast<::std::size_t>(number)).fd_p;
            }
            auto const found{storage.renumber_map.find(static_cast<::std::int32_t>(number))};
            return found == storage.renumber_map.end() ? nullptr : found->second.fd_p;
        }
        [[nodiscard]] static bool descriptor_row(fd const& actual, ::std::uint64_t number,
            ::uwvm2::uwvm::debugger::wasip1_state::descriptor_entry& row) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
            if(actual.close_pos != SIZE_MAX || actual.wasi_fd.ptr == nullptr) { return false; }
            auto const& storage{actual.wasi_fd.ptr->wasi_fd_storage};
            row.managed_resource = actual.wasi_fd.ptr->checkpoint_managed_identity;
            row.descriptor = number; row.base_rights = static_cast<::std::uint64_t>(actual.rights_base);
            row.inheriting_rights = static_cast<::std::uint64_t>(actual.rights_inherit);
            switch(storage.type)
            {
                case fd_type::null: return false;
                case fd_type::file: row.kind = ws::descriptor_kind::native_file; break;
                case fd_type::file_observer: row.kind = ws::descriptor_kind::native_file_observer; break;
                case fd_type::dir:
                {
                    row.kind = ws::descriptor_kind::directory;
                    auto const& chain{storage.storage.dir_stack};
                    row.preopened = chain.is_preload_dir();
                    if(row.preopened)
                    {
                        // [actual nonempty one-element directory chain] end
                        // [safe] is_preload_dir proved size==1 before front().
                        auto const entry{chain.dir_stack.front_unchecked().ptr};
                        if(entry == nullptr || entry->dir_stack.name.size() > ws::maximum_text_bytes) { return false; }
                        row.guest_preopen_name.assign(ws::text_view{entry->dir_stack.name.data(), entry->dir_stack.name.size()});
                    }
                    break;
                }
#if defined(_WIN32) && !defined(__CYGWIN__)
                case fd_type::socket: row.kind = ws::descriptor_kind::socket; break;
                case fd_type::socket_observer: row.kind = ws::descriptor_kind::socket_observer; break;
#endif
                default: return false;
            }
            return true;
        }
#include "uwvm_runtime_wasip1_managed_files.h"
        [[nodiscard]] static view descriptor_operation(request const& selected, wasip1_env_type& environment) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
            view out{}; out.module = selected.module; out.operation = selected.operation;
            auto& storage{environment.fd_storage};
            // Cold management only: the actual gate/cohort proof surrounds this
            // ordinary FD storage -> per-FD lock order. No guest hot path changes.
            ::uwvm2::utils::mutex::rw_fair_shared_guard_t table_guard{storage.fds_rwlock};
            if(selected.operation == action::reduce_rights)
            {
                auto const actual{find_descriptor(storage, selected.descriptor)};
                if(actual == nullptr) { out.result = status::bad_descriptor; return out; }
                ::uwvm2::utils::mutex::mutex_guard_t descriptor_guard{actual->fd_mutex};
                if(actual->close_pos != SIZE_MAX || actual->wasi_fd.ptr == nullptr ||
                    actual->wasi_fd.ptr->wasi_fd_storage.type == fd_type::null)
                { out.result = status::bad_descriptor; return out; }
                auto const base{static_cast<::std::uint64_t>(actual->rights_base)}, inheriting{static_cast<::std::uint64_t>(actual->rights_inherit)};
                if(base != selected.expected_base || inheriting != selected.expected_inheriting)
                { out.result = status::changed_descriptor; return out; }
                if(!ws::rights_subset(selected.new_base, selected.new_inheriting, base, inheriting))
                { out.result = status::capability_increase; return out; }
                // Both subsets were checked BEFORE either scalar is changed.
                // No handle injection, dup/open/close/renumber or OS operation.
                actual->rights_base = static_cast<decltype(actual->rights_base)>(selected.new_base);
                actual->rights_inherit = static_cast<decltype(actual->rights_inherit)>(selected.new_inheriting);
                out.mutation_applied = true; out.result = status::ok; return out;
            }
            if(storage.opens.size() > ws::maximum_fd_scan || storage.renumber_map.size() > ws::maximum_fd_scan - storage.opens.size())
            { out.result = status::resource_limit; return out; }
            // renumber_map is not assumed ordered. Collect bounded actual guest
            // identifiers, sort BEFORE pagination and reject duplicates/inconsistent
            // vector/map membership; numeric next can never skip a lower FD.
            ::uwvm2::utils::container::vector<::std::uint64_t> numbers{};
            numbers.reserve(storage.opens.size() + storage.renumber_map.size());
            for(::std::size_t i{}; i != storage.opens.size(); ++i) { numbers.push_back(i); }
            for(auto const& [number, item] : storage.renumber_map)
            {
                (void)item;
                if(number < 0 || static_cast<::std::uint64_t>(number) < storage.opens.size())
                { out.result = status::unavailable_environment; return out; }
                numbers.push_back(static_cast<::std::uint64_t>(number));
            }
            if(numbers.size() > 1u)
            {
                // [actual owned guest ID buffer ... bounded <=65536 cells] end
                // [safe] nonempty container iterators bound all sorting accesses.
                ::std::sort(numbers.begin(), numbers.end());
            }
            for(::std::size_t i{1u}; i < numbers.size(); ++i)
            {
                // [sorted bounded IDs ... i-1,i<N] end
                // [safe] i>=1 and i<N BEFORE both reads.
                if(numbers.index_unchecked(i - 1u) == numbers.index_unchecked(i))
                { out.result = status::unavailable_environment; return out; }
            }
            out.descriptors.reserve(static_cast<::std::size_t>(selected.count));
            out.next = selected.first;
            ::std::size_t page_bytes{}; bool page_closed{};
            auto const copy_row{[&](fd* actual, ::std::uint64_t number) noexcept
            {
                if(actual == nullptr) { return; }
                ::uwvm2::utils::mutex::mutex_guard_t descriptor_guard{actual->fd_mutex};
                ws::descriptor_entry row{};
                if(!descriptor_row(*actual, number, row) || (selected.operation == action::preopens && !row.preopened)) { return; }
                ++out.total_entries; // Entire scan <=65536, no overflow.
                if(number < selected.first) { return; }
                if(page_closed || out.descriptors.size() == selected.count || row.guest_preopen_name.size() > ws::maximum_page_text_bytes - page_bytes)
                { page_closed = true; out.more = true; return; }
                out.next = number + 1u; // number<=INT32_MAX, checked BEFORE next advance.
                page_bytes += row.guest_preopen_name.size(); // Checked BEFORE addition.
                out.descriptors.push_back(::std::move(row));
            }};
            for(auto const number : numbers) { copy_row(find_descriptor(storage, number), number); }
            out.result = status::ok; return out;
        }
#endif
        [[nodiscard]] static view apply_current(request const& selected) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
            view out{}; out.module = selected.module; out.operation = selected.operation;
            if(!ws::valid(selected)) { out.result = status::invalid_request; return out; }
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
            namespace storage = ::uwvm2::uwvm::imported::wasi::wasip1::storage;
            if(selected.module >= g_runtime.modules.size() || g_runtime.modules.size() > ws::maximum_entries ||
               !is_wasip1_import_visible_for_runtime_module_id_slow(static_cast<::std::size_t>(selected.module)))
            { out.result = status::unavailable_environment; return out; }
            auto const id{static_cast<::std::size_t>(selected.module)};
            // [actual runtime module vector ... selected.module<size] end
            // [safe] actual generation lease+publication and canonical full engine.
            auto const& record{g_runtime.modules.index_unchecked(id)};
            auto const publication{record.llvm_jit_full_publication.get()};
            if(record.runtime_module == nullptr || !record.llvm_jit_ready || publication == nullptr || !publication->engine ||
               !publication->context || publication->plan || publication->debug_source_runtime_epoch != current_runtime_generation())
            { out.result = status::unavailable_environment; return out; }
            // An unused native provider can retain stable host API tables;
            // an empty counted gate/current exposure=false cannot revoke them.
            // This cold current source census is required BEFORE env/FD/text
            // reads or mutation, in addition to genuine outer ALL/host/N/pub.
            if(!::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(publication->source) ||
               !publication->source->actual_no_unadapted_native_memory_provider(id,current_runtime_generation(),record.runtime_module))
            { out.result=status::unavailable_foreign_host_state;return out; }
            auto& environment{resolve_wasip1_env_for_runtime_module_id(id)};
            auto* arguments{::std::addressof(storage::wasip1_argument_storage)};
            auto* variables{::std::addressof(storage::wasip1_environment_storage)};
            if(::std::addressof(environment) != ::std::addressof(storage::default_wasip1_env))
            {
                // No caller address is accepted. Resolve the actual configured
                // target again and require its native env to be the same object.
                auto const group{find_wasip1_override_for_runtime_module_id_slow(id)};
                if(group == nullptr || ::std::addressof(group->env) != ::std::addressof(environment))
                { out.result = status::unavailable_owned_text; return out; }
                arguments = ::std::addressof(group->argument_storage); variables = ::std::addressof(group->environment_storage);
            }
            ::std::size_t aliases{};
            for(::std::size_t i{}; i != g_runtime.modules.size(); ++i)
            {
                if(is_wasip1_import_visible_for_runtime_module_id_slow(i) &&
                   ::std::addressof(resolve_wasip1_env_for_runtime_module_id(i)) == ::std::addressof(environment)) { ++aliases; }
            }
            switch(selected.operation)
            {
                case action::arguments: case action::replace_argument: case action::insert_argument: case action::remove_argument:
                    out = text_operation(selected, *arguments, environment.argv); break;
                case action::environment: case action::set_environment: case action::remove_environment:
                    out = text_operation(selected, *variables, environment.envs); break;
                case action::trace_enable: case action::trace_disable: case action::trace_clear: case action::trace_read:
                    out.result = status::invalid_request; return out; // trace DATA bypasses no environment admission
                case action::portable_export: case action::portable_import:
                case action::portable_export_group: case action::portable_import_group:
                case action::checkpoint_save: case action::checkpoint_restore: case action::checkpoint_drop:
                    out.result = status::invalid_request; return out; // Native capsule API, never a DATA-only request.
                case action::create_file: case action::duplicate_descriptor: case action::close_descriptor:
                    if constexpr(::uwvm2::runtime::lib::wasip1_native_file::supported)
                    { out = manage_descriptor(selected, environment); break; }
                    else { out.result = status::unavailable_resource_rollback; return out; }
                case action::descriptors: case action::preopens: case action::reduce_rights:
                    out = descriptor_operation(selected, environment); break;
            }
            out.shared_environment = aliases > 1u; out.observed_runtime_epoch = current_runtime_generation();
            return out;
#else
            out.result = status::unavailable_environment; return out;
#endif
        }
    };
}
#endif
