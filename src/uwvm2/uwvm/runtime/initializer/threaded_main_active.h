// Finite, nonallocating actual-parent active-table initialization. SOURCE ONLY.
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <mutex>
# include "threaded_bootstrap.h"
# include <uwvm2/runtime/wasm_threads/thread_group.h>
#endif
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && defined(__linux__) && !defined(UWVM_MODULE)
namespace uwvm2::uwvm::runtime::initializer
{
    enum class threaded_main_active_status : unsigned char
    { applied, busy, invalid_identity, invalid_reservation, already_effectful, unsupported_shape, bounds, stopped };

    class ThreadedMainActiveSegments final
    {
        friend class ::uwvm2::runtime::lib::runtime_threaded_profile_bridge;
        using admission = ::uwvm2::runtime::wasm_threads::ThreadedSpawnAdmission;
        using group = ::uwvm2::runtime::wasm_threads::ThreadedGroup;
        using world = ThreadedChildWorld;
        using source = ::uwvm2::uwvm::runtime::full::full_source_instance;
        ThreadedMainActiveSegments() = delete;

        // Only the actual Runtime bridge, retaining its admitted startup state,
        // calls this after all backend construction and before native launch.
        // The real Group reservation is checked under its own native mutex.
        [[nodiscard]] static threaded_main_active_status apply(admission const& actual,
            world::owner const& parent_world, group& native, group::SpawnReservation const& reservation) noexcept
        {
            using namespace ::uwvm2::uwvm::runtime::storage;
            auto const retained{actual.retain()};
            auto const native_owner{native.canonical_owner_.lock()};
            if(!retained || retained.get()!=&actual || !actual.owns_actual_generation() || !native_owner ||
               native_owner.get()!=&native || native.admission_.get()!=&actual || !parent_world ||
               &parent_world->admission()!=&actual || parent_world->module(0)!=&actual.source_module() ||
               parent_world->source_owner()!=actual.source_owner()) { return threaded_main_active_status::invalid_identity; }
            auto const code{parent_world->validated_code()};
            if(!code || !code->covers(*parent_world)) { return threaded_main_active_status::invalid_identity; }
            ::std::unique_lock lock{native.mutex_,::std::try_to_lock};
            if(!lock.owns_lock()) { return threaded_main_active_status::busy; }
            if(reservation.group_!=&native || reservation.retained_group_.get()!=&native || reservation.position_!=0uz ||
               reservation.tid_!=0u || native.initialization_error_!=0 || native.stop_fd_<0 || !native.accepting_ ||
               !native.slots_ || native.slots_[0].group!=&native || native.occupied_!=1uz || native.occupied_children_!=0uz ||
               !native.initial_main_reserved_ || native.slots_[0].phase!=group::slot_phase::preparing ||
               native.slots_[0].tid!=0u || !native.slots_[0].initial_main || native.slots_[0].work ||
               native.slots_[0].launch_published) { return threaded_main_active_status::invalid_reservation; }
            if(native.ever_launched_.load(::std::memory_order_acquire) ||
               native.shared_effects_started_.load(::std::memory_order_acquire)) { return threaded_main_active_status::already_effectful; }
            auto stopped{[&]() noexcept { return native.stop_requested_.load(::std::memory_order_acquire); }};
            if(stopped()) { return threaded_main_active_status::stopped; }
            auto const& owner{actual.source_owner()};
            auto const selected{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
            if(!source::has_canonical_owner(owner) || selected.get()!=owner.get() ||
               selected.owner_before(owner) || owner.owner_before(selected) ||
               owner->actual_validated_file(actual.source_module_id(),actual.source_validation_epoch(),&actual.source_module())!=parent_world->file(0))
            { return threaded_main_active_status::invalid_identity; }
            auto& registry{::uwvm2::uwvm::runtime::storage::active_runtime_registry()};
            if(&registry!=&owner->registry()) { return threaded_main_active_status::invalid_identity; }
            auto const main_member{registry.find(owner->file().module_name)};
            if(main_member==registry.end() || &main_member->second!=&actual.source_module()) { return threaded_main_active_status::invalid_identity; }
            auto& main{main_member->second};
            if(::std::atomic_ref<bool>{main.gc_collection_phase.active_segments_ready}.load(::std::memory_order_acquire))
            { return threaded_main_active_status::already_effectful; }

            // All preloaded Wasm instances must be real, source-bound pure
            // memory providers. The first profile has exactly one shared leaf.
            ::std::optional<ThreadedMemoryProvider> provider;
            for(auto const& [name,module] : owner->registry())
            {
                static_cast<void>(name);
                if(&module==&main) { continue; }
                if(provider) { return threaded_main_active_status::unsupported_shape; }
                threaded_provider_error error{};
                provider=ThreadedMemoryProvider::inspect(owner,&module,actual.max_shared_memory_bytes(),error);
                if(!provider || error!=threaded_provider_error::ok) { return threaded_main_active_status::unsupported_shape; }
            }
            if(!provider || main.imported_memory_vec_storage.size()!=1uz || !main.local_defined_memory_vec_storage.empty() ||
               !main.imported_table_vec_storage.empty()) { return threaded_main_active_status::unsupported_shape; }
            auto const& imported{main.imported_memory_vec_storage.index_unchecked(0uz)};
            if(imported.link_kind!=imported_memory_storage_t::imported_memory_link_kind::defined ||
               imported.target.defined_ptr!=&provider->memory() || parent_world->shared_memory(0)!=&provider->memory())
            { return threaded_main_active_status::unsupported_shape; }
            auto const provider_member{registry.find(provider->file().module_name)};
            if(provider_member==registry.end() || &provider_member->second!=&provider->module() ||
               ::std::atomic_ref<bool>{provider_member->second.gc_collection_phase.active_segments_ready}.load(::std::memory_order_acquire))
            { return threaded_main_active_status::already_effectful; }
            // No expression-payload materialization, aggregate roots or active
            // shared-data replay is admitted. All rejection happens pre-effect.
            for(auto const& data:main.local_defined_data_vec_storage)
            {
                if(data.data_type_ptr==nullptr || data.data_type_ptr->storage.segment.active ||
                   data.data.kind!=wasm_data_segment_kind::passive) { return threaded_main_active_status::unsupported_shape; }
            }
            for(auto const& table:main.local_defined_table_vec_storage)
            {
                if(table.table_type_ptr==nullptr || table.owner_module_rt_ptr!=&main ||
                   table.elems.size()>actual.max_table_elements() ||
                   runtime_table_family(table)!=runtime_table_reference_family::function ||
                   static_cast<unsigned>(table.table_type_ptr->reftype)!=0x70u)
                { return threaded_main_active_status::unsupported_shape; }
            }
            ::std::size_t visited{};
            for(auto const& segment:main.local_defined_element_vec_storage)
            {
                if(stopped()) { return threaded_main_active_status::stopped; }
                if(segment.element_type_ptr==nullptr || runtime_element_family(segment,main)!=runtime_table_reference_family::function)
                { return threaded_main_active_status::unsupported_shape; }
                auto const& declaration{segment.element_type_ptr->storage.segment};
                auto const& element{segment.element};
                if(!declaration.vec_expr.empty() || element.funcref_begin || element.funcref_end ||
                   element.externref_begin || element.externref_end || element.gc_ref_begin || element.gc_ref_end)
                { return threaded_main_active_status::unsupported_shape; }
                auto const count{declaration.vec_funcidx.size()};
                auto const* expected_begin{count==0uz?nullptr:declaration.vec_funcidx.data()};
                auto const* expected_end{count==0uz?nullptr:expected_begin+count};
                if(element.funcidx_begin!=expected_begin || element.funcidx_end!=expected_end)
                { return threaded_main_active_status::invalid_identity; }
                for(auto function:declaration.vec_funcidx)
                {
                    if((visited++&1023uz)==0uz && stopped()) { return threaded_main_active_status::stopped; }
                    auto const imports{main.imported_function_vec_storage.size()};
                    if(function<imports || function-imports>=main.local_defined_function_vec_storage.size())
                    { return threaded_main_active_status::unsupported_shape; }
                }
                if(!declaration.active)
                {
                    if(element.kind!=wasm_element_segment_kind::passive) { return threaded_main_active_status::invalid_identity; }
                    continue;
                }
                if(element.kind!=wasm_element_segment_kind::active || wasm_element_segment_is_dropped(element) ||
                   declaration.expr.opcodes.size()!=1uz ||
                   declaration.expr.opcodes.front_unchecked().opcode!=::uwvm2::parser::wasm::standard::wasm1::opcode::op_basic::i32_const ||
                   declaration.table_idx!=element.table_idx || element.table_idx>=main.local_defined_table_vec_storage.size())
                { return threaded_main_active_status::unsupported_shape; }
                auto const offset{static_cast<::std::uint_least32_t>(declaration.expr.opcodes.front_unchecked().storage.i32)};
                auto const& table{main.local_defined_table_vec_storage.index_unchecked(element.table_idx)};
                if(element.offset!=offset || offset>table.elems.size() || count>table.elems.size()-offset)
                { return threaded_main_active_status::bounds; }
            }
            if(stopped()) { return threaded_main_active_status::stopped; }
            // Sticky BEFORE the first actual source-instance write. It is never
            // cleared here, even on cancellation/failure or before native launch.
            native.shared_effects_started_.store(true,::std::memory_order_release);
            visited=0uz;
            for(auto& segment:main.local_defined_element_vec_storage)
            {
                auto& element{segment.element};
                if(element.kind!=wasm_element_segment_kind::active) { continue; }
                auto& table{main.local_defined_table_vec_storage.index_unchecked(element.table_idx)};
                auto const& functions{segment.element_type_ptr->storage.segment.vec_funcidx};
                for(::std::size_t i{};i!=functions.size();++i)
                {
                    if((visited++&1023uz)==0uz && stopped()) { return threaded_main_active_status::stopped; }
                    auto const local{functions.index_unchecked(i)-main.imported_function_vec_storage.size()};
                    auto& slot{table.elems.index_unchecked(static_cast<::std::size_t>(element.offset)+i)};
                    slot.storage.defined_ptr=&main.local_defined_function_vec_storage.index_unchecked(local);
                    slot.type=local_defined_table_elem_storage_type_t::func_ref_defined;
                }
                drop_wasm_element_segment_payload(element);
            }
            // Pure preload has no data/table/start effects to execute. Publish
            // actual instance phases only after all main writes/drop complete.
            // The same source registry remained exclusive under Root's retained
            // admission; these pointers were authenticated before the effect.
            provider_member->second.gc_collection_phase.publish_active_segments(true);
            main.gc_collection_phase.publish_active_segments(true);
            return stopped()?threaded_main_active_status::stopped:threaded_main_active_status::applied;
        }
    };
}
#endif
