/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Private native world preparation DATA. Include inside runtime::lib ONLY after
// all global checkpoint/initializer definitions. The real coherent manager is
// the sole constructor; preparation is not execution/publication permission.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
namespace checkpoint_world_data
{
    namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
    namespace in = ::uwvm2::uwvm::runtime::initializer;
    namespace st = ::uwvm2::uwvm::runtime::storage;
    namespace rt = ::uwvm2::parser::wasm::standard::wasm3::type;
}
extern "C++"
{
    class runtime_checkpoint_world_transaction final
    {
        friend class runtime_checkpoint_coherent_manager;
        using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
        using source_owner = source_type::owner;
        using module_type = checkpoint_world_data::st::wasm_module_storage_t;
        using file_type = ::uwvm2::uwvm::wasm::type::wasm_file_t;
        using state_type = checkpoint_world_data::cp::state;
        using object_type = checkpoint_world_data::cp::object;
        using object_id = checkpoint_world_data::cp::object_id;
        using reference = checkpoint_world_data::st::gc_reference;
        using native_value = checkpoint_world_data::st::gc_object_value;
        using core_type = checkpoint_world_data::rt::core_value_type;
        using store_owner = ::std::shared_ptr<checkpoint_world_data::st::gc_object_store>;
        using context_type = checkpoint_world_data::in::restoration_context;
        using staging_type = checkpoint_world_data::st::checkpoint_gc_staging;
        enum class preparation_status : unsigned char
        { empty, source_prepared, modules_bound, resources_prepared, malformed, source_mismatch,
          feature_mismatch, type_mismatch, invalid_import, quota_exceeded, allocation_failed, unavailable_adapter };
        struct native_object
        {
            ::std::size_t owner{SIZE_MAX}, local{SIZE_MAX}, shell{SIZE_MAX};
            reference value{};
            bool reference_ready{}, imported_function{};
        };
        struct module_binding
        {
            ::std::uint64_t old_actual_id{};
            ::std::size_t new_actual_id{SIZE_MAX};
            module_type* actual{};
            file_type const* file{};
            object_id module{}, instance{};
        };
        // Graph is exclusively copied DATA: do not keep old store/module/native
        // frame owners across drain. New source outlives context and staging
        // rollback, so every private shell is destroyed while all owners live.
        state_type saved_{};
        // Actual process-local compilation DATA, never a persisted build/provider
        // manifest or file restore proof. Each original source is bound below.
        ::uwvm2::runtime::checkpoint::compilation_profile::cache_identity_type observed_profile_{};
        source_type::mutable_owner prepared_source_{};
        staging_type staged_gc_{};
        ::std::unique_ptr<context_type> context_{};
#include "uwvm_runtime_checkpoint_world_wasip1.h"
        ::std::vector<module_binding> modules_{};
        ::std::vector<native_object> objects_{};
        ::std::vector<::std::size_t> actual_dense_module_owners_{};
        // Fixed compressed index of genuine memory/table chunk records. Bound
        // target IDs BEFORE id-1. Prefix offsets are monotonic owned DATA,
        // and each flat record index is rechecked before reading saved state.
        ::std::vector<::std::size_t> chunk_offsets_{};
        ::std::vector<::std::size_t> chunk_records_{};
        using engine_type=runtime_checkpoint_staged_llvm_full_engine;
        ::std::vector<::std::unique_ptr<engine_type>> engines_{};
        ::uwvm2::runtime::checkpoint::compilation_profile::owner actual_prepared_profile_{};
        ::std::uint64_t native_bytes_{}, maximum_native_bytes_{};
        preparation_status phase_{preparation_status::empty};
        unsigned engine_diagnostic_{};
        ::std::size_t prepared_native_endpoints_{},prepared_native_helper_bodies_{},prepared_native_claims_{};
        bool prepared_native_endpoint_capture_{};
        ::std::size_t prepared_functions_{};
        runtime_checkpoint_world_transaction() noexcept = default;
        [[nodiscard]] bool fail(preparation_status error) noexcept
        { phase_ = error; return false; }
        [[nodiscard]] bool charge_native(::std::uint64_t count, ::std::uint64_t width) noexcept
        {
            if(width == 0u || native_bytes_ > maximum_native_bytes_ || count > (maximum_native_bytes_-native_bytes_)/width)
            { return fail(preparation_status::quota_exceeded); }
            native_bytes_ += count*width; return true; // division BEFORE multiply/sum.
        }
        [[nodiscard]] bool charge_exclusive_graph_copy(state_type const& graph) noexcept
        {
            // Charge the actual owned vector representations BEFORE deep copying.
            // This counts logical allocation payloads, not allocator headers,
            // parser/LLVM workspace or the separately retained original world.
            if(!charge_native(1u,sizeof(runtime_checkpoint_world_transaction)) ||
               !charge_native(graph.objects.size(),sizeof(object_type)) ||
               !charge_native(graph.root_instances.size(),sizeof(object_id)) ||
               !charge_native(graph.retained_roots.size(),sizeof(checkpoint_world_data::cp::value))) { return false; }
            for(auto const& object:graph.objects)
            {
                if(!charge_native(object.links.size(),sizeof(object_id)) ||
                   !charge_native(object.values.size(),sizeof(checkpoint_world_data::cp::value)) ||
                   !charge_native(object.bytes.size(),1u)) { return false; }
            }
            return true;
        }
        [[nodiscard]] object_type const* lookup(object_id id) const noexcept
        { return checkpoint_world_data::cp::state_details::lookup(saved_, id); }
        [[nodiscard]] native_object* native(object_id id) noexcept
        {
            if(id == 0u || id > objects_.size()) { return nullptr; }
            // [fixed native DATA relocation roster ... id-1<size] end
            // [safe] id bound BEFORE index. Wire IDs never become native pointers.
            return ::std::addressof(objects_[static_cast<::std::size_t>(id-1u)]);
        }
        [[nodiscard]] native_object const* native(object_id id) const noexcept
        {
            if(id == 0u || id > objects_.size()) { return nullptr; }
            return ::std::addressof(objects_[static_cast<::std::size_t>(id-1u)]);
        }
        [[nodiscard]] ::std::size_t module_owner(object_id module) const noexcept
        {
            auto const* record{native(module)};
            // [fixed graph-native correspondence, module ID checked by native]
            // [safe] only this world's exact module row can name its owner.
            // Per-value type lookup is O(1); no module-wide scan per GC field.
            return record!=nullptr && record->owner<modules_.size() && modules_[record->owner].module==module ? record->owner : SIZE_MAX;
        }
        [[nodiscard]] static bool owned_extent_matches(file_type const& file, object_type const& object) noexcept
        {
            if(file.binfmt_ver != 1u || !file.has_owned_source_image() || object.kind != checkpoint_world_data::cp::object_kind::module ||
               object.bytes.size() != file.source_size() || file.source_size() >
                static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
               file.source_size() > UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(file.source_cbegin())) { return false; }
            auto const* bytes{reinterpret_cast<::std::byte const*>(file.source_cbegin())};
            for(::std::size_t n{}; n != object.bytes.size(); ++n)
            {
                // [actual original exclusive source][actual owned saved bytes] end
                // [safe] matched complete extents BEFORE n reads. No persisted
                // native pointer is used, and no original file is reopened.
                if(bytes[n] != object.bytes[n]) { return false; }
            }
            return true;
        }
        [[nodiscard]] bool bind_actual_modules(source_owner const& original, ::std::uint_least64_t observed_epoch)
        {
            namespace cp = checkpoint_world_data::cp;
            namespace binding = cp::binding;
            if(phase_ != preparation_status::source_prepared || !source_type::has_canonical_owner(original) ||
               observed_epoch == 0u || !original->initialized_from_actual_state() ||
               original->preload_files_.size() != original->preload_bindings_.size() ||
               original->preload_files_.size() != prepared_source_->preload_files_.size() ||
               original->registry_.size() != prepared_source_->registry_.size() ||
               original->registry_.size() != original->preload_files_.size()+1u)
            { return fail(preparation_status::source_mismatch); }
            struct actual_member
            { ::std::uint64_t id{}; module_type const* module{}; file_type const* old_file{}; file_type const* new_file{}; bool main{}; };
            ::std::vector<actual_member> actual{};
            if(original->registry_.size() > actual.max_size() || original->registry_.size() >
               static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(actual_member))
            { return fail(preparation_status::quota_exceeded); }
            actual.reserve(original->registry_.size());
            if(original->actual_validated_file(original->main_module_id_, observed_epoch, original->main_module_) !=
               ::std::addressof(original->file_)) { return fail(preparation_status::source_mismatch); }
            actual.push_back({original->main_module_id_, original->main_module_, ::std::addressof(original->file_),
                ::std::addressof(prepared_source_->file_), true});
            for(::std::size_t n{}; n != original->preload_files_.size(); ++n)
            {
                // [actual FINAL bindings+old/new parser arrays, paired counts] end
                // [safe] n<all final extents BEFORE every indexed file/member read.
                auto const& record{original->preload_bindings_[n]};
                auto const& old_file{original->preload_files_.index_unchecked(n)};
                auto const& new_file{prepared_source_->preload_files_.index_unchecked(n)};
                if(original->actual_validated_file(record.id, observed_epoch, record.module) != ::std::addressof(old_file))
                { return fail(preparation_status::source_mismatch); }
                actual.push_back({record.id, record.module, ::std::addressof(old_file), ::std::addressof(new_file), false});
            }
            ::std::sort(actual.begin(), actual.end(), [](auto const& a, auto const& b) noexcept { return a.id < b.id; });
            if(saved_.root_instances.size() != actual.size())
            { return fail(preparation_status::source_mismatch); }
            ::std::vector<object_id> module_ids{}; module_ids.reserve(actual.size());
            for(::std::size_t n{}; n != saved_.objects.size(); ++n)
            {
                if(saved_.objects[n].kind == cp::object_kind::module)
                {
                    if(module_ids.size() == actual.size()) { return fail(preparation_status::source_mismatch); }
                    module_ids.push_back(static_cast<object_id>(n)+1u);
                }
            }
            if(module_ids.size() != actual.size()) { return fail(preparation_status::source_mismatch); }
            modules_.reserve(actual.size());
            for(::std::size_t n{}; n != actual.size(); ++n)
            {
                // [actual sorted source member roster][saved bound source order] end
                // [safe] exact paired sizes BEFORE all indexed DATA reads.
                auto const& member{actual[n]};
                auto const* module{lookup(module_ids[n])};
                if(module == nullptr || (n != 0u && actual[n-1u].id == member.id) ||
                   !owned_extent_matches(*member.old_file, *module) || !owned_extent_matches(*member.new_file, *module) ||
                   member.old_file->module_name != member.new_file->module_name)
                { return fail(preparation_status::source_mismatch); }
                // Exact old/new immutable source extents and actual member-name
                // equality above bind this freshly captured graph. Persisted file
                // binding/build validation is a separate future restore entry.
                auto const policy{::uwvm2::runtime::checkpoint::module_feature_policy::from_actual_parameter(
                    ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(member.new_file->wasm_parameter.binfmt1_para))};
                if(!policy.valid() || module->flags != 1u || module->words != ::std::array<::std::uint64_t,8u>{
                    policy.revision,policy.cli_mode,policy.disabled,policy.controlled,0u,0u,0u,0u})
                { return fail(preparation_status::feature_mismatch); }
                auto const found{prepared_source_->registry_.find(member.new_file->module_name)};
                if(found == prepared_source_->registry_.end() || context_->unpublished_file_for_module(::std::addressof(found->second)) != member.new_file)
                { return fail(preparation_status::source_mismatch); }
                object_id instance{};
                for(auto id : saved_.root_instances)
                {
                    auto const* candidate{lookup(id)};
                    if(candidate == nullptr || candidate->kind != cp::object_kind::instance || candidate->links.empty())
                    { return fail(preparation_status::source_mismatch); }
                    if(candidate->links[0u] == module_ids[n])
                    { if(instance != 0u) { return fail(preparation_status::source_mismatch); } instance = id; }
                }
                if(instance == 0u) { return fail(preparation_status::source_mismatch); }
                modules_.push_back({member.id, SIZE_MAX, ::std::addressof(found->second), member.new_file, module_ids[n], instance});
            }
            // Actual unpublished dense registry DATA is constructed in the SAME
            // source-map order as ordinary runtime modules. It is not an epoch
            // or permission and is published only with this exact world later.
            actual_dense_module_owners_.reserve(modules_.size());
            for(auto const& [name,module] : prepared_source_->registry_)
            {
                static_cast<void>(name);::std::size_t owner{SIZE_MAX};
                for(::std::size_t m{};m<modules_.size();++m)
                { if(modules_[m].actual==::std::addressof(module)) { owner=m;break; } }
                if(owner>=modules_.size() || modules_[owner].new_actual_id!=SIZE_MAX) { return fail(preparation_status::source_mismatch); }
                modules_[owner].new_actual_id=actual_dense_module_owners_.size();actual_dense_module_owners_.push_back(owner);
            }
            if(actual_dense_module_owners_.size()!=modules_.size()) { return fail(preparation_status::source_mismatch); }
            phase_ = preparation_status::modules_bound; return true;
        }
        [[nodiscard]] bool decode_type(checkpoint_world_data::cp::value_type const& value, ::std::size_t fallback_owner,
            core_type& type, ::std::size_t& owner) const noexcept
        {
            namespace cp = checkpoint_world_data::cp;
            namespace rt = checkpoint_world_data::rt;
            if(fallback_owner >= modules_.size()) { return false; }
            type = {}; owner = fallback_owner;
            switch(value.kind)
            {
                case cp::value_kind::i8: case cp::value_kind::i16: case cp::value_kind::i32: type.kind = rt::value_kind::i32; return true;
                case cp::value_kind::i64: type.kind = rt::value_kind::i64; return true;
                case cp::value_kind::f32: type.kind = rt::value_kind::f32; return true;
                case cp::value_kind::f64: type.kind = rt::value_kind::f64; return true;
                case cp::value_kind::v128: type.kind = rt::value_kind::v128; return true;
                case cp::value_kind::reference: break;
                default: return false;
            }
            type.kind = rt::value_kind::reference; type.nullable = value.nullable;
            using heap = rt::abstract_heap_type;
            switch(value.heap)
            {
                case cp::heap_kind::func: type.heap.code = static_cast<::std::int_least64_t>(heap::func); break;
                case cp::heap_kind::external: type.heap.code = static_cast<::std::int_least64_t>(heap::extern_); break;
                case cp::heap_kind::any: type.heap.code = static_cast<::std::int_least64_t>(heap::any); break;
                case cp::heap_kind::eq: type.heap.code = static_cast<::std::int_least64_t>(heap::eq); break;
                case cp::heap_kind::i31: type.heap.code = static_cast<::std::int_least64_t>(heap::i31); break;
                case cp::heap_kind::structure: type.heap.code = static_cast<::std::int_least64_t>(heap::struct_); break;
                case cp::heap_kind::array: type.heap.code = static_cast<::std::int_least64_t>(heap::array); break;
                case cp::heap_kind::exception: type.heap.code = static_cast<::std::int_least64_t>(heap::exn); break;
                case cp::heap_kind::nofunc: type.heap.code = static_cast<::std::int_least64_t>(heap::nofunc); break;
                case cp::heap_kind::noextern: type.heap.code = static_cast<::std::int_least64_t>(heap::noextern); break;
                case cp::heap_kind::bottom: type.heap.code = static_cast<::std::int_least64_t>(heap::none); break;
                case cp::heap_kind::noexception: type.heap.code = static_cast<::std::int_least64_t>(heap::noexn); break;
                case cp::heap_kind::defined:
                    owner = module_owner(value.type_module);
                    if(owner >= modules_.size() || value.type_index >= modules_[owner].actual->type_section_storage.type_section_count)
                    { return false; }
                    type.heap.code = value.type_index; break;
                default: return false;
            }
            return true;
        }
        [[nodiscard]] bool relocate_value(checkpoint_world_data::cp::value const& value, ::std::size_t owner,
            native_value& native_value_out, bool allow_uninitialized = false) const noexcept
        {
            namespace cp = checkpoint_world_data::cp;
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            native_value_out = {};
            if(!cp::state_details::valid(value,saved_,allow_uninitialized,false)) { return false; }
            if(!value.initialized) { return allow_uninitialized; } // complete zero carrier, not a null value.
            switch(value.type.kind)
            {
                case cp::value_kind::i8: case cp::value_kind::i16: case cp::value_kind::i32: case cp::value_kind::f32:
                {
                    auto const bits{static_cast<::std::uint32_t>(value.low_bits)};
                    // [fixed16 native bits][actual raw4 integer/IEEE object] end
                    // [safe] width4<=16. Native integer memcpy preserves raw FP
                    // bits without floating evaluation or host-endian wire casts.
                    ::fast_io::freestanding::my_memcpy(native_value_out.bits.data(),::std::addressof(bits),4u); return true;
                }
                case cp::value_kind::i64: case cp::value_kind::f64:
                    ::fast_io::freestanding::my_memcpy(native_value_out.bits.data(),::std::addressof(value.low_bits),8u); return true;
                case cp::value_kind::v128:
                {
                    // [complete fixed16 Wasm byte order destination] one-past
                    // [safe] whole fixed bound BEFORE +16; FastIO emits canonical
                    // lane byte order on either endian host, without FP operands.
                    auto* first{reinterpret_cast<unsigned char*>(native_value_out.bits.data())};
                    ::fast_io::basic_obuffer_view<unsigned char> out{first,first+16u};
                    ::fast_io::print(out,::fast_io::mnp::le_put<64u>(value.low_bits),::fast_io::mnp::le_put<64u>(value.high_bits));
                    return out.curr_ptr == first+16u;
                }
                case cp::value_kind::reference: break;
                default: return false;
            }
            reference relocated{};
            if(value.reference == cp::reference_kind::null) { relocated = {}; }
            else if(value.reference == cp::reference_kind::i31)
            { relocated.kind = kind::wasm_i31; relocated.storage.wasm_i31 = checkpoint_world_data::rt::wasm_i31::from_i32(static_cast<::std::int32_t>(value.low_bits)); }
            else
            {
                auto const* object{native(value.target)};
                if(object == nullptr || !object->reference_ready) { return false; }
                relocated = object->value;
            }
            core_type type{}; ::std::size_t declared_owner{};
            if(!decode_type(value.type,owner,type,declared_owner) || declared_owner >= modules_.size() ||
               !staged_gc_.pending_value_matches(native_value::reference(relocated),{type},modules_[declared_owner].actual->gc_store))
            { return false; }
            native_value_out = native_value::reference(relocated); return true;
        }
        [[nodiscard]] bool slot_type_matches(checkpoint_world_data::cp::value_type const& saved,
            ::std::size_t expected_owner, checkpoint_world_data::rt::storage_type expected) const noexcept
        {
            namespace cp = checkpoint_world_data::cp;
            namespace rt = checkpoint_world_data::rt;
            if(expected_owner >= modules_.size()) { return false; }
            if(expected.packed != rt::packed_kind::none)
            { return saved.kind == (expected.packed == rt::packed_kind::i8 ? cp::value_kind::i8 : cp::value_kind::i16); }
            if(saved.kind == cp::value_kind::i8 || saved.kind == cp::value_kind::i16) { return false; }
            core_type actual{}; ::std::size_t owner{};
            return decode_type(saved,expected_owner,actual,owner) && owner < modules_.size() &&
                checkpoint_world_data::st::gc_object_store::canonical_value_type_matches(modules_[owner].actual->gc_store.get(),actual,
                    modules_[expected_owner].actual->gc_store.get(),expected.value);
        }
        template<unsigned Resource,bool Imported>
        [[nodiscard]] static auto& records(module_type& module) noexcept
        {
            static_assert(Resource < 7u);
            if constexpr(Resource==0u && Imported) { return module.imported_function_vec_storage; }
            else if constexpr(Resource==0u) { return module.local_defined_function_vec_storage; }
            else if constexpr(Resource==1u && Imported) { return module.imported_table_vec_storage; }
            else if constexpr(Resource==1u) { return module.local_defined_table_vec_storage; }
            else if constexpr(Resource==2u && Imported) { return module.imported_memory_vec_storage; }
            else if constexpr(Resource==2u) { return module.local_defined_memory_vec_storage; }
            else if constexpr(Resource==3u && Imported) { return module.imported_global_vec_storage; }
            else if constexpr(Resource==3u) { return module.local_defined_global_vec_storage; }
            else if constexpr(Resource==4u && Imported) { return module.imported_tag_vec_storage; }
            else if constexpr(Resource==4u) { return module.local_defined_tag_vec_storage; }
            else if constexpr(Resource==5u) { static_assert(!Imported); return module.local_defined_data_vec_storage; }
            else { static_assert(!Imported); return module.local_defined_element_vec_storage; }
        }
        template<unsigned Resource>
        [[nodiscard]] bool wire_space(::std::size_t owner,::std::size_t& first,::std::size_t& imported,::std::size_t& local) const noexcept
        {
            static_assert(Resource<7u);
            if(owner>=modules_.size()) { return false; }
            auto const* instance{lookup(modules_[owner].instance)};
            if(instance==nullptr || instance->kind!=checkpoint_world_data::cp::object_kind::instance || instance->links.empty()) { return false; }
            first=1u;
            for(unsigned n{};n<Resource;++n)
            {
                if(first>instance->links.size() || instance->words[n]>instance->links.size()-first) { return false; }
                first+=static_cast<::std::size_t>(instance->words[n]); // actual links remaining bound BEFORE sum.
            }
            local=records<Resource,false>(*modules_[owner].actual).size();
            if constexpr(Resource<5u) { imported=records<Resource,true>(*modules_[owner].actual).size(); }
            else { imported=0u; }
            return first<=instance->links.size() && imported<=SIZE_MAX-local &&
                imported+local<=instance->links.size()-first && instance->words[Resource]==imported+local;
        }
        template<unsigned Resource,bool Imported,typename Record>
        [[nodiscard]] bool record_owner(Record const* candidate,::std::size_t& owner,::std::size_t& index) const noexcept
        {
            if(candidate==nullptr) { return false; }
            for(::std::size_t n{};n<modules_.size();++n)
            {
                auto const& actual{records<Resource,Imported>(*modules_[n].actual)};
                if(checkpoint_world_data::in::details::linked_function_record_index(candidate,actual,index)) { owner=n;return true; }
            }
            return false; // Exact owned vector membership; never read supplied pointee.
        }
        template<unsigned Resource>
        [[nodiscard]] bool bind_defined_space()
        {
            namespace cp=checkpoint_world_data::cp;namespace st=checkpoint_world_data::st;
            constexpr cp::object_kind kinds[]{cp::object_kind::function,cp::object_kind::table,cp::object_kind::memory,
                cp::object_kind::global,cp::object_kind::tag,cp::object_kind::data,cp::object_kind::element};
            for(::std::size_t owner{};owner<modules_.size();++owner)
            {
                ::std::size_t first{},imported{},locals{};
                if(!wire_space<Resource>(owner,first,imported,locals)) { return fail(preparation_status::source_mismatch); }
                auto const* instance{lookup(modules_[owner].instance)};auto& module{*modules_[owner].actual};
                for(::std::size_t local{};local<locals;++local)
                {
                    // [actual instance complete seven spaces][actual local vector] end
                    // [safe] first+imported+local<=last bounded by wire_space BEFORE index.
                    auto const id{instance->links[first+imported+local]};auto const* object{lookup(id)};auto* relocated{native(id)};
                    if(object==nullptr || object->kind!=kinds[Resource] || relocated==nullptr || relocated->owner!=SIZE_MAX)
                    { return fail(preparation_status::source_mismatch); }
                    relocated->owner=owner;relocated->local=local;
                    if constexpr(Resource==0u)
                    {
                        auto const& function{records<0u,false>(module).index_unchecked(local)};
                        auto const& types{module.type_section_storage};
                        if(object->flags!=0u || object->links.size()!=1u || object->links[0u]!=modules_[owner].instance ||
                           object->words[0u]!=imported+local || object->words[1u]==0u || object->words[2u]>=types.type_section_count ||
                           types.type_section_begin==nullptr || types.type_section_count>
                            static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(*types.type_section_begin) ||
                           object->words[2u]>(UINTPTR_MAX-reinterpret_cast<::std::uintptr_t>(types.type_section_begin))/sizeof(*types.type_section_begin) ||
                           function.function_type_ptr!=types.type_section_begin+static_cast<::std::size_t>(object->words[2u]) ||
                           (object->words[1u]==1u && !object->bytes.empty()) || (object->words[1u]>1u && object->bytes.empty()))
                        { return fail(preparation_status::source_mismatch); }
                        // [actual FINAL typed function vector, local<locals] end
                        // [safe] genuine source record only; effective generation
                        // body is compiled later by the SAME fused native walker.
                        relocated->value={};relocated->value.kind=::uwvm2::object::global::wasm_ref_kind::wasm_func_defined;
                        relocated->value.storage.ptr=::std::addressof(records<0u,false>(module).index_unchecked(local));
                        relocated->reference_ready=true;
                    }
                    else if constexpr(Resource==4u)
                    {
                        auto const& tag{records<4u,false>(module).index_unchecked(local)};
                        if(object->links.size()!=1u || object->links[0u]!=modules_[owner].module || object->words[0u]!=tag.type_index ||
                           !tag.exception_identity || tag.type_index>=module.type_section_storage.type_section_count)
                        { return fail(preparation_status::source_mismatch); }
                    }
                }
            }
            return true;
        }
        template<unsigned Resource>
        [[nodiscard]] bool resolve_import(::std::size_t declaring,::std::size_t imported,::std::size_t& owner,::std::size_t& local,
            ::std::size_t maximum_hops,bool& builtin) const noexcept
        {
            static_assert(Resource<5u);
            builtin=false;
            namespace st=checkpoint_world_data::st;
            if(declaring>=modules_.size() || imported>=records<Resource,true>(*modules_[declaring].actual).size()) { return false; }
            auto const* next{::std::addressof(records<Resource,true>(*modules_[declaring].actual).index_unchecked(imported))};
            for(::std::size_t hop{};hop<maximum_hops;++hop)
            {
                ::std::size_t index{};
                if(!record_owner<Resource,true>(next,owner,index)) { return false; }
                // [actual NEW source-owned import vector, owned index] end
                // [safe] supplied link compared before all active union reads.
                auto const& actual{records<Resource,true>(*modules_[owner].actual).index_unchecked(index)};
                if constexpr(Resource==4u)
                {
                    if(actual.defined_target!=nullptr)
                    { return actual.imported_target==nullptr && actual.resolved_tag==actual.defined_target &&
                        record_owner<4u,false>(actual.defined_target,owner,local); }
                    if(actual.imported_target==nullptr || actual.resolved_tag==nullptr) { return false; }
                    ::std::size_t next_owner{},next_local{};
                    if(!record_owner<4u,true>(actual.imported_target,next_owner,next_local) ||
                        records<4u,true>(*modules_[next_owner].actual).index_unchecked(next_local).resolved_tag!=actual.resolved_tag) { return false; }
                    next=actual.imported_target; // Next membership proof precedes all pointee reads.
                }
                else
                {
                    bool defined{},chained{};
                    using actual_type=::std::remove_cvref_t<decltype(actual)>;
                    if constexpr(Resource==0u) { defined=actual.link_kind==st::imported_function_link_kind::defined;
                        chained=actual.link_kind==st::imported_function_link_kind::imported; }
                    else if constexpr(Resource==1u) { defined=actual.link_kind==actual_type::imported_table_link_kind::defined;
                        chained=actual.link_kind==actual_type::imported_table_link_kind::imported; }
                    else if constexpr(Resource==2u) { defined=actual.link_kind==actual_type::imported_memory_link_kind::defined;
                        chained=actual.link_kind==actual_type::imported_memory_link_kind::imported; }
                    else { defined=actual.link_kind==actual_type::imported_global_link_kind::defined;
                        chained=actual.link_kind==actual_type::imported_global_link_kind::imported; }
                    if(defined) { return record_owner<Resource,false>(actual.target.defined_ptr,owner,local); }
                    if constexpr(Resource==0u)
                    {
                        if(actual.link_kind==st::imported_function_link_kind::local_imported)
                        { builtin=true;local=index;return true; } // Actual new terminal, separately authenticated below.
                    }
                    if(!chained) { return false; } // Actual independent native/environment adapter required, never guessed.
                    next=actual.target.imported_ptr; // Compare next final owned element BEFORE dereference.
                }
            }
            return false; // finite ALL-owned import records bound detects cycles.
        }
#include "uwvm_runtime_checkpoint_world_builtin_wasip1.h"
        template<unsigned Resource>
        [[nodiscard]] bool check_import_space(source_owner const& original,::std::uint_least64_t epoch)
        {
            static_assert(Resource<5u);
            ::std::size_t hops{};
            for(auto const& owner:modules_)
            { auto const count{records<Resource,true>(*owner.actual).size()};if(count>SIZE_MAX-hops) { return fail(preparation_status::quota_exceeded); }hops+=count; }
            for(::std::size_t declaring{};declaring<modules_.size();++declaring)
            {
                ::std::size_t first{},imported{},locals{};
                if(!wire_space<Resource>(declaring,first,imported,locals)) { return fail(preparation_status::source_mismatch); }
                auto const* instance{lookup(modules_[declaring].instance)};
                for(::std::size_t index{};index<imported;++index)
                {
                    ::std::size_t owner{},local{};bool builtin{};
                    if(!resolve_import<Resource>(declaring,index,owner,local,hops,builtin)) { return fail(preparation_status::invalid_import); }
                    // Original declaring index must alias precisely this actual
                    // target definition's logical object, not a type-only copy.
                    auto const id{instance->links[first+index]};auto const* relocated{native(id)};
                    if constexpr(Resource==0u)
                    {
                        if(builtin && !bind_builtin_wasip1_function(original,epoch,owner,local,id))
                        { return fail(preparation_status::unavailable_adapter); }
                    }
                    if(relocated==nullptr || relocated->owner!=owner || relocated->local!=local) { return fail(preparation_status::source_mismatch); }
                    if constexpr(Resource==0u)
                    { if(relocated->imported_function!=builtin) { return fail(preparation_status::source_mismatch); } }
                }
            }
            return true;
        }
        [[nodiscard]] bool bind_all_native_resources(source_owner const& original,::std::uint_least64_t epoch)
        {
            if(phase_!=preparation_status::modules_bound || !charge_native(saved_.objects.size(),sizeof(native_object)) ||
                saved_.objects.size()>objects_.max_size() || saved_.objects.size()>
                static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(native_object))
            { return fail(preparation_status::quota_exceeded); }
            objects_.resize(saved_.objects.size());
            for(::std::size_t owner{};owner<modules_.size();++owner)
            {
                auto const id{modules_[owner].module};auto const* logical{lookup(id)};auto* actual{native(id)};
                if(logical==nullptr || logical->kind!=checkpoint_world_data::cp::object_kind::module || actual==nullptr || actual->owner!=SIZE_MAX)
                { return fail(preparation_status::source_mismatch); }
                actual->owner=owner; // uniquely source-authenticated module row, not a raw wire address.
                auto const instance_id{modules_[owner].instance};auto const* instance{lookup(instance_id)};auto* instance_record{native(instance_id)};
                if(instance==nullptr || instance->kind!=checkpoint_world_data::cp::object_kind::instance || instance_record==nullptr || instance_record->owner!=SIZE_MAX)
                { return fail(preparation_status::source_mismatch); }
                instance_record->owner=owner; // the exact paired root-instance row, never a host receiver.
            }
            if(!bind_defined_space<0u>() || !bind_defined_space<1u>() || !bind_defined_space<2u>() || !bind_defined_space<3u>() ||
               !bind_defined_space<4u>() || !bind_defined_space<5u>() || !bind_defined_space<6u>() || !check_import_space<0u>(original,epoch) ||
               !check_import_space<1u>(original,epoch) || !check_import_space<2u>(original,epoch) || !check_import_space<3u>(original,epoch) || !check_import_space<4u>(original,epoch)) { return false; }
            for(::std::size_t n{};n<saved_.objects.size();++n)
            {
                auto const kind{saved_.objects[n].kind};
                using k=checkpoint_world_data::cp::object_kind;
                if((kind==k::module || kind==k::instance || kind==k::function || kind==k::table || kind==k::memory || kind==k::global || kind==k::tag || kind==k::data || kind==k::element) &&
                   objects_[n].owner>=modules_.size()) { return fail(preparation_status::source_mismatch); }
            }
            return true;
        }
        [[nodiscard]] bool reserve_all_gc_shells()
        {
            namespace cp=checkpoint_world_data::cp;namespace rt=checkpoint_world_data::rt;namespace st=checkpoint_world_data::st;
            // Every mutable aggregate shell is reserved BEFORE any field or
            // immutable exception is filled, preserving self/cross-store cycles.
            for(::std::size_t n{};n<saved_.objects.size();++n)
            {
                auto const& object{saved_.objects[n]};
                if(object.kind!=cp::object_kind::structure && object.kind!=cp::object_kind::array) { continue; }
                if(object.links.size()!=1u || object.words[0u]>UINT32_MAX) { return fail(preparation_status::type_mismatch); }
                auto const owner{module_owner(object.links[0u])};
                if(owner>=modules_.size() || !modules_[owner].actual->gc_store || objects_[n].owner!=SIZE_MAX)
                { return fail(preparation_status::source_mismatch); }
                auto const type{static_cast<::std::uint_least32_t>(object.words[0u])};
                auto const expected{object.kind==cp::object_kind::structure?rt::composite_kind::struct_:rt::composite_kind::array};
                auto const& store{modules_[owner].actual->gc_store};rt::composite_kind actual{};::std::size_t fields{};
                if(!store->type_kind(type,actual) || actual!=expected || !store->field_count(type,fields) ||
                   (expected==rt::composite_kind::struct_ ? fields!=object.values.size() : fields!=1u) ||
                   object.values.size()>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(native_value))
                { return fail(preparation_status::type_mismatch); }
                for(::std::size_t field{};field<object.values.size();++field)
                {
                    auto const* declaration{store->field_at(type,expected==rt::composite_kind::struct_?field:0u)};
                    if(declaration==nullptr || !slot_type_matches(object.values[field].type,owner,declaration->storage))
                    { return fail(preparation_status::type_mismatch); }
                }
                ::std::size_t upper{};
                if(!staged_gc_.shell_native_upper_bound(store,type,expected,object.values.size(),upper) ||
                   !charge_native(upper,1u) || !charge_native(object.values.size(),sizeof(native_value)))
                { return fail(preparation_status::quota_exceeded); }
                auto& relocated{objects_[n]};
                auto const result{staged_gc_.reserve_shell(store,type,expected,object.values.size(),relocated.shell,relocated.value)};
                if(result!=st::gc_object_status::ok) { return fail(preparation_status::allocation_failed); }
                relocated.owner=owner;relocated.reference_ready=true;
            }
            return true;
        }
        [[nodiscard]] static bool charge_staged_native_backing(void* actual_owner,
            ::std::size_t count, ::std::size_t width) noexcept
        {
            // Installed only by prepare_actual_resources on its own nonmoving
            // unique owner. This is internal quota accounting, not pointer DATA
            // or a constructor/restore permission accepted from a caller.
            return static_cast<runtime_checkpoint_world_transaction*>(actual_owner)->charge_native(count, width);
        }
        [[nodiscard]] bool build_pending_bridges()
        {
            namespace cp=checkpoint_world_data::cp;namespace st=checkpoint_world_data::st;
            for(::std::size_t n{};n<saved_.objects.size();++n)
            {
                auto const& object{saved_.objects[n]};if(object.kind!=cp::object_kind::external) { continue; }
                if(object.values.size()!=1u || modules_.empty()) { return fail(preparation_status::type_mismatch); }
                auto const& value{object.values[0u]};
                ::std::size_t owner{};
                if(value.reference!=cp::reference_kind::i31)
                {
                    auto const* inner{native(value.target)};
                    if(inner==nullptr || !inner->reference_ready || inner->owner>=modules_.size())
                    { return fail(preparation_status::unavailable_adapter); }
                    owner=inner->owner;
                }
                native_value actual{};
                if(!relocate_value(value,owner,actual) ||
                   staged_gc_.stage_external_bridge(modules_[owner].actual->gc_store,actual.template as<reference>(),objects_[n].value)!=st::gc_object_status::ok)
                { return fail(preparation_status::allocation_failed); }
                objects_[n].owner=owner;objects_[n].reference_ready=true;
            }
            return true;
        }
        [[nodiscard]] bool build_original_exception_diagnostic(object_type const& object,
            ::uwvm2::runtime::exception::diagnostic_trace_ref& result)
        {
            namespace cp=checkpoint_world_data::cp;namespace ex=::uwvm2::runtime::exception;
            result={};if(object.links.size()==1u) { return true; }
            if(object.links.size()!=2u) { return fail(preparation_status::malformed); }
            auto const* trace{lookup(object.links[1u])};
            if(trace==nullptr || trace->kind!=cp::object_kind::exception_trace || trace->words[0u]!=1u ||
               trace->links.size()!=trace->values.size() || !cp::state_details::valid_trace_payload(saved_,*trace) ||
               !charge_native(trace->links.size(),sizeof(ex::diagnostic_frame))) { return fail(preparation_status::malformed); }
            ::std::vector<ex::diagnostic_frame> frames{};
            if(trace->links.size()>frames.max_size()) { return fail(preparation_status::quota_exceeded); }
            frames.reserve(trace->links.size());::std::size_t offset{};
            for(::std::size_t n{};n<trace->links.size();++n)
            {
                if(trace->values[n].low_bits!=cp::unknown_diagnostic_instruction || offset>trace->bytes.size() ||
                   40u>trace->bytes.size()-offset) { return fail(preparation_status::type_mismatch); }
                // [owned trace record, whole header40+both names] end
                // [safe] validated payload + remaining40 BEFORE +offset/+40.
                auto const* first{reinterpret_cast<unsigned char const*>(trace->bytes.data())+offset};
                auto const* end{first+40u};::std::array<::std::uint64_t,5u> header{};
                for(auto& field:header)
                {
                    if(static_cast<::std::size_t>(end-first)<8u) { return fail(preparation_status::malformed); }
                    auto const* field_end{first+8u};auto const parsed{::fast_io::parse_by_scan(first,field_end,::fast_io::mnp::le_get<64u>(field))};
                    if(parsed.code!=::fast_io::parse_code::ok || parsed.iter!=field_end) { return fail(preparation_status::malformed); }
                    first=parsed.iter; // exact8 BEFORE pointer reassignment; never outside header.
                }
                auto const* instance{lookup(header[0u])};auto const* relocated{native(header[0u])};
                auto const owner{relocated==nullptr?SIZE_MAX:relocated->owner};
                if(instance==nullptr || owner>=modules_.size() || modules_[owner].instance!=header[0u] ||
                   header[1u]>=instance->words[0u] || header[2u]!=0u ||
                   header[3u]>trace->bytes.size()-offset-40u || header[4u]>trace->bytes.size()-offset-40u-header[3u] ||
                   modules_[owner].new_actual_id>=actual_dense_module_owners_.size() || header[1u]>SIZE_MAX ||
                   !charge_native(header[3u]+header[4u],1u)) { return fail(preparation_status::malformed); }
                ex::diagnostic_frame frame{};
                // The actual newly built unpublished dense roster supplies this
                // diagnostic ID; joint commit must publish that exact roster.
                // No old ID is assigned a future epoch or resume authority.
                frame.module_id=modules_[owner].new_actual_id;
                frame.function_index=static_cast<::std::size_t>(header[1u]);
                auto const* name_first{reinterpret_cast<char8_t const*>(trace->bytes.data())+offset+40u};
                frame.module_name.assign(name_first,static_cast<::std::size_t>(header[3u]));
                frame.function_name.assign(name_first+static_cast<::std::size_t>(header[3u]),static_cast<::std::size_t>(header[4u]));
                offset+=40u+static_cast<::std::size_t>(header[3u])+static_cast<::std::size_t>(header[4u]);
                frames.push_back(::std::move(frame));
            }
            if(offset!=trace->bytes.size()) { return fail(preparation_status::malformed); }
            if(!charge_native(1u, sizeof(ex::diagnostic_trace))) { return false; }
            result=ex::diagnostic_trace::make(::std::move(frames),(trace->flags&1u)!=0u);return static_cast<bool>(result);
        }
        [[nodiscard]] bool build_exception_instance(::std::size_t index)
        {
            namespace cp=checkpoint_world_data::cp;namespace ex=::uwvm2::runtime::exception;namespace st=checkpoint_world_data::st;
            if(index>=saved_.objects.size()) { return fail(preparation_status::malformed); }
            auto const& object{saved_.objects[index]};
            if(object.kind!=cp::object_kind::exception || object.links.empty()) { return fail(preparation_status::type_mismatch); }
            auto const* logical_tag{lookup(object.links[0u])};auto const* relocated_tag{native(object.links[0u])};
            if(logical_tag==nullptr || logical_tag->kind!=cp::object_kind::tag || relocated_tag==nullptr ||
               relocated_tag->owner>=modules_.size() || relocated_tag->local>=records<4u,false>(*modules_[relocated_tag->owner].actual).size())
            { return fail(preparation_status::type_mismatch); }
            auto const owner{relocated_tag->owner};auto const& module{*modules_[owner].actual};
            auto const& tag{module.local_defined_tag_vec_storage.index_unchecked(relocated_tag->local)};
            auto const& types{module.type_section_storage};auto const type{tag.type_index};
            if(types.owned_signature_begin==nullptr || types.owned_signature_end==nullptr || type>=types.type_section_count ||
               types.type_section_count>static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())/sizeof(*types.owned_signature_begin))
            { return fail(preparation_status::type_mismatch); }
            auto const first{reinterpret_cast<::std::uintptr_t>(types.owned_signature_begin)},last{reinterpret_cast<::std::uintptr_t>(types.owned_signature_end)};
            if(last<first || types.type_section_count>(UINTPTR_MAX-first)/sizeof(*types.owned_signature_begin) ||
               last-first!=types.type_section_count*sizeof(*types.owned_signature_begin)) { return fail(preparation_status::type_mismatch); }
            // [actual owned finalized rich signatures, type<count] end
            // [safe] exact byte extent/type bound BEFORE native signature index.
            auto const& signature{types.owned_signature_begin[type]};
            if(signature.type_index!=type || !signature.results.empty() || signature.parameters.size()!=object.values.size() ||
               !tag.exception_identity || !charge_native(object.values.size(),sizeof(ex::payload_field)))
            { return fail(preparation_status::type_mismatch); }
            ::std::vector<ex::payload_field> fields{};if(object.values.size()>fields.max_size()) { return fail(preparation_status::quota_exceeded); }
            fields.reserve(object.values.size());
            for(::std::size_t n{};n<object.values.size();++n)
            {
                auto const declared{signature.parameters.index_unchecked(n)};auto const& saved{object.values[n]};native_value native{};
                if(!slot_type_matches(saved.type,owner,{declared}) || !relocate_value(saved,owner,native)) { return fail(preparation_status::type_mismatch); }
                ::std::optional<ex::payload_field> field{};
                using k=checkpoint_world_data::rt::value_kind;
                if(declared.kind==k::reference)
                {
                    auto const ref{native.template as<reference>()};ex::instance_root root{};
                    if(!staged_gc_.pending_reference_root(module.gc_store,ref,root)) { return fail(preparation_status::type_mismatch); }
                    // [fixed native carrier <=16][new immutable typed payload]
                    // [safe] real reference membership/type/root precedes byte span.
                    field=ex::payload_field::wasm_reference({native.bits.data(),sizeof(reference)},::std::move(root));
                }
                else
                {
                    auto const kind{declared.kind==k::i32?ex::payload_kind::i32:declared.kind==k::i64?ex::payload_kind::i64:
                        declared.kind==k::f32?ex::payload_kind::f32:declared.kind==k::f64?ex::payload_kind::f64:ex::payload_kind::v128};
                    auto const width{ex::payload_width(kind)};if(width==0u || width>native.bits.size()) { return fail(preparation_status::type_mismatch); }
                    field=ex::payload_field::numeric(kind,{native.bits.data(),width});
                }
                if(!field) { return fail(preparation_status::type_mismatch); }fields.push_back(::std::move(*field));
            }
            ex::diagnostic_trace_ref diagnostic{};if(!build_original_exception_diagnostic(object,diagnostic)) { return false; }
            if(!charge_native(1u, sizeof(ex::value))) { return false; }
            auto value{ex::value::make_owned(::std::static_pointer_cast<void const>(tag.exception_identity),::std::move(fields),::std::move(diagnostic))};
            if(!value || staged_gc_.stage_exception(module.gc_store,value,objects_[index].value)!=st::gc_object_status::ok)
            { return fail(preparation_status::allocation_failed); }
            objects_[index].owner=owner;objects_[index].reference_ready=true;return true;
        }
        [[nodiscard]] bool build_all_exceptions()
        {
            namespace cp=checkpoint_world_data::cp;
            struct dependency_edge { ::std::size_t dependent{}, next{SIZE_MAX}; };
            auto const count{saved_.objects.size()};
            ::std::size_t exception_count{}, dependency_count{};
            // Count only direct immutable exception dependencies. Already minted
            // function/GC-shell/bridge references are ready; cycles through GC
            // shells remain legal. No native pointer comes from wire DATA.
            for(::std::size_t index{};index<count;++index)
            {
                auto const& object{saved_.objects[index]};
                if(object.kind!=cp::object_kind::exception) { continue; }
                if(exception_count==SIZE_MAX || objects_[index].reference_ready) { return fail(preparation_status::malformed); }
                ++exception_count;
                for(auto const& field:object.values)
                {
                    if(field.type.kind!=cp::value_kind::reference || field.reference==cp::reference_kind::null || field.reference==cp::reference_kind::i31) { continue; }
                    auto const* target{native(field.target)};auto const* logical{lookup(field.target)};
                    if(target==nullptr || logical==nullptr) { return fail(preparation_status::malformed); }
                    if(target->reference_ready) { continue; }
                    if(logical->kind!=cp::object_kind::exception || dependency_count==SIZE_MAX)
                    { return fail(preparation_status::type_mismatch); }
                    ++dependency_count;
                }
            }
            if(exception_count==0u) { return true; }
            ::std::vector<::std::size_t> pending{}, heads{}, ready{};
            ::std::vector<dependency_edge> edges{};
            if(count>pending.max_size() || count>heads.max_size() || exception_count>ready.max_size() ||
               dependency_count>edges.max_size() || !charge_native(count,sizeof(::std::size_t)*2u) ||
               !charge_native(exception_count,sizeof(::std::size_t)) || !charge_native(dependency_count,sizeof(dependency_edge)))
            { return fail(preparation_status::quota_exceeded); }
            pending.resize(count);heads.assign(count,SIZE_MAX);ready.reserve(exception_count);edges.resize(dependency_count);
            ::std::size_t cursor{};
            for(::std::size_t index{};index<count;++index)
            {
                auto const& object{saved_.objects[index]};if(object.kind!=cp::object_kind::exception) { continue; }
                for(auto const& field:object.values)
                {
                    if(field.type.kind!=cp::value_kind::reference || field.reference==cp::reference_kind::null || field.reference==cp::reference_kind::i31) { continue; }
                    auto const* target{native(field.target)};
                    if(target==nullptr) { return fail(preparation_status::malformed); }
                    if(target->reference_ready) { continue; }
                    if(field.target==0u || field.target>count || cursor>=edges.size() || pending[index]==SIZE_MAX)
                    { return fail(preparation_status::malformed); }
                    auto const target_index{static_cast<::std::size_t>(field.target-1u)};
                    // [fixed target heads][fixed edge roster] end
                    // [safe] target<=count and cursor<size BEFORE both indices.
                    // Each repeated reference owns a separate edge/count; aliases
                    // therefore release exactly the corresponding dependencies.
                    edges[cursor]={index,heads[target_index]};heads[target_index]=cursor;++cursor;++pending[index];
                }
                if(pending[index]==0u) { ready.push_back(index); }
            }
            if(cursor!=edges.size()) { return fail(preparation_status::malformed); }
            ::std::size_t built{}, visited{};
            for(::std::size_t position{};position<ready.size();++position)
            {
                auto const index{ready[position]};
                if(index>=count || saved_.objects[index].kind!=cp::object_kind::exception || objects_[index].reference_ready ||
                   !build_exception_instance(index)) { return false; }
                ++built;
                for(auto edge_index{heads[index]};edge_index!=SIZE_MAX;)
                {
                    if(edge_index>=edges.size() || visited==edges.size()) { return fail(preparation_status::malformed); }
                    auto const edge{edges[edge_index]};++visited;
                    if(edge.dependent>=count || saved_.objects[edge.dependent].kind!=cp::object_kind::exception || pending[edge.dependent]==0u)
                    { return fail(preparation_status::malformed); }
                    --pending[edge.dependent];
                    if(pending[edge.dependent]==0u)
                    {
                        if(ready.size()==exception_count) { return fail(preparation_status::malformed); }
                        ready.push_back(edge.dependent);
                    }
                    edge_index=edge.next; // edge was copied AFTER bounded index; no pointer jump.
                }
            }
            // Direct immutable cycles have no zero-dependency root. The finite
            // queue rejects those and unknown adapters without recursive calls.
            return (built==exception_count && visited==edges.size()) || fail(preparation_status::type_mismatch);
        }
        [[nodiscard]] bool build_chunk_index()
        {
            namespace cp=checkpoint_world_data::cp;
            auto const count{saved_.objects.size()};
            if(!chunk_offsets_.empty() || !chunk_records_.empty() || count==SIZE_MAX || count+1u>chunk_offsets_.max_size() ||
               !charge_native(count+1u,sizeof(::std::size_t))) { return fail(preparation_status::quota_exceeded); }
            chunk_offsets_.resize(count+1u);
            ::std::size_t chunk_count{};
            for(auto const& object:saved_.objects)
            {
                if(object.kind!=cp::object_kind::memory_chunk && object.kind!=cp::object_kind::table_chunk) { continue; }
                if(object.links.size()!=1u || object.links[0u]==0u || object.links[0u]>count || chunk_count==SIZE_MAX)
                { return fail(preparation_status::malformed); }
                auto const target_id{object.links[0u]};auto const* target{lookup(target_id)};
                if(target==nullptr || target->kind!=(object.kind==cp::object_kind::memory_chunk?cp::object_kind::memory:cp::object_kind::table) ||
                   chunk_offsets_[static_cast<::std::size_t>(target_id)]==SIZE_MAX) { return fail(preparation_status::malformed); }
                // [fixed counts, target_id<=count<count+1] end
                // [safe] actual target kind/extent BEFORE target-index update.
                ++chunk_offsets_[static_cast<::std::size_t>(target_id)];++chunk_count;
            }
            for(::std::size_t n{1u};n<chunk_offsets_.size();++n)
            {
                if(chunk_offsets_[n-1u]>chunk_count || chunk_offsets_[n]>chunk_count-chunk_offsets_[n-1u])
                { return fail(preparation_status::malformed); }
                chunk_offsets_[n]+=chunk_offsets_[n-1u]; // checked BEFORE sum.
            }
            if(chunk_offsets_.back()!=chunk_count || chunk_count>chunk_records_.max_size() ||
               !charge_native(chunk_count,sizeof(::std::size_t)) || !charge_native(count,sizeof(::std::size_t)))
            { return fail(preparation_status::quota_exceeded); }
            chunk_records_.resize(chunk_count);
            ::std::vector<::std::size_t> next{};
            if(count>next.max_size()) { return fail(preparation_status::quota_exceeded); }
            next.resize(count);
            for(::std::size_t n{};n<count;++n) { next[n]=chunk_offsets_[n]; }
            for(::std::size_t index{};index<count;++index)
            {
                auto const& object{saved_.objects[index]};
                if(object.kind!=cp::object_kind::memory_chunk && object.kind!=cp::object_kind::table_chunk) { continue; }
                auto const target_id{object.links[0u]};
                if(target_id==0u || target_id>count) { return fail(preparation_status::malformed); }
                auto const owner{static_cast<::std::size_t>(target_id-1u)};auto const position{next[owner]};
                if(position>=chunk_offsets_[owner+1u] || position>=chunk_records_.size()) { return fail(preparation_status::malformed); }
                // [fixed flat chunk roster, position<size] end
                // [safe] prefix/flat bounds BEFORE write and checked cursor advance.
                chunk_records_[position]=index;++next[owner];
            }
            for(::std::size_t n{};n<count;++n) { if(next[n]!=chunk_offsets_[n+1u]) { return fail(preparation_status::malformed); } }
            return true;
        }
        [[nodiscard]] bool fill_all_gc_shells()
        {
            namespace cp=checkpoint_world_data::cp;namespace rt=checkpoint_world_data::rt;namespace st=checkpoint_world_data::st;
            for(::std::size_t n{};n<saved_.objects.size();++n)
            {
                auto const& object{saved_.objects[n]};if(object.kind!=cp::object_kind::structure && object.kind!=cp::object_kind::array) { continue; }
                auto const& relocated{objects_[n]};
                if(relocated.owner>=modules_.size() || relocated.shell==SIZE_MAX) { return fail(preparation_status::type_mismatch); }
                auto const& store{modules_[relocated.owner].actual->gc_store};auto const type{static_cast<::std::uint_least32_t>(object.words[0u])};
                ::std::vector<native_value> values{};if(object.values.size()>values.max_size()) { return fail(preparation_status::quota_exceeded); }
                values.resize(object.values.size());
                for(::std::size_t field{};field<object.values.size();++field)
                {
                    auto const* declared{store->field_at(type,object.kind==cp::object_kind::structure?field:0u)};
                    if(declared==nullptr || !slot_type_matches(object.values[field].type,relocated.owner,declared->storage) ||
                       !relocate_value(object.values[field],relocated.owner,values[field])) { return fail(preparation_status::type_mismatch); }
                }
                if(staged_gc_.fill_shell(relocated.shell,{values.data(),values.size()})!=st::gc_object_status::ok)
                { return fail(preparation_status::type_mismatch); }
            }
            return true;
        }
        [[nodiscard]] bool fill_memory_object(object_id id)
        {
            namespace cp=checkpoint_world_data::cp;
            auto const* saved{lookup(id)};auto const* target{native(id)};
            if(saved==nullptr || saved->kind!=cp::object_kind::memory || target==nullptr || target->owner>=modules_.size() ||
               target->local>=records<2u,false>(*modules_[target->owner].actual).size()) { return fail(preparation_status::source_mismatch); }
            auto* actual{::std::addressof(records<2u,false>(*modules_[target->owner].actual).index_unchecked(target->local))};
            ::std::vector<context_type::saved_memory_chunk> chunks{};
            if(id==0u || id>saved_.objects.size() || chunk_offsets_.size()!=saved_.objects.size()+1u) { return fail(preparation_status::malformed); }
            auto const first{chunk_offsets_[static_cast<::std::size_t>(id-1u)]},last{chunk_offsets_[static_cast<::std::size_t>(id)]};
            if(last<first || last>chunk_records_.size() || last-first>chunks.max_size() ||
               !charge_native(last-first,sizeof(context_type::saved_memory_chunk))) { return fail(preparation_status::quota_exceeded); }
            chunks.reserve(last-first);
            for(auto position{first};position<last;++position)
            {
                auto const index{chunk_records_[position]};if(index>=saved_.objects.size()) { return fail(preparation_status::malformed); }
                auto const& object{saved_.objects[index]};
                if(object.kind!=cp::object_kind::memory_chunk || object.links.size()!=1u || object.links[0u]!=id) { return fail(preparation_status::malformed); }
                // [pre-grouped actual saved chunk bytes, owned complete extent]
                // [safe] original graph index/parent/kind BEFORE exposing span.
                chunks.push_back({object.words[0u],{object.bytes.data(),object.bytes.size()}});
            }
            auto const remaining{maximum_native_bytes_-native_bytes_};
            if(!charge_native(saved->words[1u],65536u) || remaining>SIZE_MAX ||
               !context_->stage_saved_memory_image(actual,static_cast<unsigned>(saved->words[0u]),saved->flags!=0u,
                   saved->words[1u],saved->words[2u],saved->words[3u],{chunks.data(),chunks.size()},static_cast<::std::size_t>(remaining)))
            { return fail(preparation_status::allocation_failed); }
            return true;
        }
        [[nodiscard]] bool fill_table_object(object_id id)
        {
            namespace cp=checkpoint_world_data::cp;
            auto const* saved{lookup(id)};auto const* target{native(id)};
            if(saved==nullptr || saved->kind!=cp::object_kind::table || saved->values.size()!=1u || target==nullptr ||
               target->owner>=modules_.size() || target->local>=records<1u,false>(*modules_[target->owner].actual).size())
            { return fail(preparation_status::source_mismatch); }
            auto const owner{target->owner};auto* actual{::std::addressof(records<1u,false>(*modules_[owner].actual).index_unchecked(target->local))};
            if(actual->table_type_ptr==nullptr) { return fail(preparation_status::type_mismatch); }
            core_type expected{};
            if(!context_type::saved_reference_declaration(*actual->table_type_ptr,expected) ||
               !slot_type_matches(saved->values[0u].type,owner,{expected}))
            { return fail(preparation_status::type_mismatch); }
            reference default_value{};
            if(saved->words[1u]!=0u)
            {
                native_value relocated{};if(!relocate_value(saved->values[0u],owner,relocated)) { return fail(preparation_status::type_mismatch); }
                default_value=relocated.template as<reference>();
            }
            ::std::vector<::std::vector<reference>> owned{};::std::vector<context_type::saved_table_chunk> chunks{};
            if(id==0u || id>saved_.objects.size() || chunk_offsets_.size()!=saved_.objects.size()+1u) { return fail(preparation_status::malformed); }
            auto const first{chunk_offsets_[static_cast<::std::size_t>(id-1u)]},last{chunk_offsets_[static_cast<::std::size_t>(id)]};
            if(last<first || last>chunk_records_.size()) { return fail(preparation_status::malformed); }
            auto const count{last-first};
            if(count>owned.max_size() || count>chunks.max_size() ||
               !charge_native(count,sizeof(::std::vector<reference>)+sizeof(context_type::saved_table_chunk))) { return fail(preparation_status::quota_exceeded); }
            owned.resize(count);chunks.reserve(count);::std::size_t cursor{};
            for(auto position{first};position<last;++position)
            {
                auto const index{chunk_records_[position]};if(index>=saved_.objects.size()) { return fail(preparation_status::malformed); }
                auto const& object{saved_.objects[index]};
                if(object.kind!=cp::object_kind::table_chunk || object.links.size()!=1u || object.links[0u]!=id) { return fail(preparation_status::malformed); }
                if(cursor>=owned.size() || object.values.size()>owned[cursor].max_size() ||
                   !charge_native(object.values.size(),sizeof(reference))) { return fail(preparation_status::quota_exceeded); }
                auto& values{owned[cursor]};values.resize(object.values.size());
                for(::std::size_t n{};n<object.values.size();++n)
                {
                    native_value relocated{};
                    if(!slot_type_matches(object.values[n].type,owner,{expected}) || !relocate_value(object.values[n],owner,relocated))
                    { return fail(preparation_status::type_mismatch); }
                    values[n]=relocated.template as<reference>();
                }
                chunks.push_back({object.words[0u],{values.data(),values.size()}});++cursor;
            }
            auto const remaining{maximum_native_bytes_-native_bytes_};
            if(cursor!=owned.size() || remaining>SIZE_MAX ||
               !charge_native(saved->words[1u],sizeof(checkpoint_world_data::st::local_defined_table_elem_storage_t)) ||
               !context_->stage_saved_table_image(actual,static_cast<unsigned>(saved->words[0u]),
                saved->words[1u],saved->words[2u],saved->words[3u],expected,default_value,{chunks.data(),chunks.size()},
                static_cast<::std::size_t>(remaining)))
            { return fail(preparation_status::allocation_failed); }
            return true;
        }
        [[nodiscard]] bool fill_global_object(object_id id)
        {
            namespace cp=checkpoint_world_data::cp;
            auto const* saved{lookup(id)};auto const* target{native(id)};
            if(saved==nullptr || saved->kind!=cp::object_kind::global || saved->values.size()!=1u || target==nullptr ||
               target->owner>=modules_.size() || target->local>=records<3u,false>(*modules_[target->owner].actual).size())
            { return fail(preparation_status::source_mismatch); }
            auto const owner{target->owner};auto* actual{::std::addressof(records<3u,false>(*modules_[owner].actual).index_unchecked(target->local))};
            if(actual->global_type_ptr==nullptr) { return fail(preparation_status::type_mismatch); }
            auto const expected{::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(*actual->global_type_ptr)};
            native_value value{};
            if(!slot_type_matches(saved->values[0u].type,owner,{expected}) || !relocate_value(saved->values[0u],owner,value))
            { return fail(preparation_status::type_mismatch); }
            auto const filled{expected.kind==checkpoint_world_data::rt::value_kind::reference ?
                context_->stage_saved_reference_global(actual,expected,saved->flags!=0u,value.template as<reference>()) :
                context_->stage_saved_numeric_global(actual,expected,saved->flags!=0u,value)};
            return filled || fail(preparation_status::type_mismatch);
        }
        [[nodiscard]] bool fill_data_object(object_id id)
        {
            namespace cp=checkpoint_world_data::cp;
            auto const* saved{lookup(id)};auto const* target{native(id)};
            if(saved==nullptr || saved->kind!=cp::object_kind::data || target==nullptr || target->owner>=modules_.size() ||
               target->local>=records<5u,false>(*modules_[target->owner].actual).size()) { return fail(preparation_status::source_mismatch); }
            auto* actual{::std::addressof(records<5u,false>(*modules_[target->owner].actual).index_unchecked(target->local))};
            return context_->stage_saved_data_segment(actual,saved->flags!=0u,{saved->bytes.data(),saved->bytes.size()}) || fail(preparation_status::source_mismatch);
        }
        [[nodiscard]] bool fill_element_object(object_id id)
        {
            namespace cp=checkpoint_world_data::cp;
            auto const* saved{lookup(id)};auto const* target{native(id)};
            if(saved==nullptr || saved->kind!=cp::object_kind::element || target==nullptr || target->owner>=modules_.size() ||
               target->local>=records<6u,false>(*modules_[target->owner].actual).size()) { return fail(preparation_status::source_mismatch); }
            auto const owner{target->owner};auto* actual{::std::addressof(records<6u,false>(*modules_[owner].actual).index_unchecked(target->local))};
            if(actual->element_type_ptr==nullptr) { return fail(preparation_status::type_mismatch); }
            core_type expected{};
            if(!context_type::saved_reference_declaration(actual->element_type_ptr->storage.segment,expected)) { return fail(preparation_status::type_mismatch); }
            ::std::vector<reference> values{};
            if(saved->values.size()>values.max_size() || !charge_native(saved->values.size(),sizeof(reference))) { return fail(preparation_status::quota_exceeded); }
            values.resize(saved->values.size());
            for(::std::size_t n{};n<saved->values.size();++n)
            {
                native_value relocated{};
                if(!slot_type_matches(saved->values[n].type,owner,{expected}) || !relocate_value(saved->values[n],owner,relocated))
                { return fail(preparation_status::type_mismatch); }
                values[n]=relocated.template as<reference>();
            }
            return context_->stage_saved_element_segment(actual,saved->flags!=0u,expected,{values.data(),values.size()}) || fail(preparation_status::source_mismatch);
        }
        [[nodiscard]] bool charge_context_resource_backing(::std::size_t& element_bytes)
        {
            namespace st=checkpoint_world_data::st;
            using family=st::runtime_table_reference_family;
            element_bytes=0u;
            if(!context_ || !prepared_source_ || prepared_source_->registry_.size()!=modules_.size())
            { return fail(preparation_status::source_mismatch); }
            // The context allocates from FINAL ORIGINAL expression declarations,
            // including dropped segments whose saved value vectors are empty.
            // Charge the exact same family widths BEFORE its reserve/resize.
            for(auto const& module:modules_)
            {
                if(module.actual==nullptr || module.file==nullptr ||
                   context_->unpublished_file_for_module(module.actual)!=module.file)
                { return fail(preparation_status::source_mismatch); }
                auto const& actual{*module.actual};
                for(auto count:{actual.local_defined_memory_vec_storage.size(),actual.local_defined_table_vec_storage.size(),
                    actual.local_defined_global_vec_storage.size(),actual.local_defined_data_vec_storage.size(),
                    actual.local_defined_element_vec_storage.size()})
                {
                    if(!charge_native(count,sizeof(context_type::completion_record))) { return false; }
                }
                if(!charge_native(1u,sizeof(context_type::element_allocation))) { return false; }
                for(auto const& element:actual.local_defined_element_vec_storage)
                {
                    // [genuine context-owned skeleton element][owned parser declaration]
                    // [safe] FINAL source/module/file identity proved before union read.
                    if(element.element_type_ptr==nullptr) { return fail(preparation_status::source_mismatch); }
                    auto const& segment{element.element_type_ptr->storage.segment};
                    auto const count{segment.vec_expr.size()};
                    if(count==0u) { continue; }
                    if(!segment.vec_funcidx.empty()) { return fail(preparation_status::type_mismatch); }
                    auto const kind{st::runtime_element_family(element,actual)};
                    auto const width{kind==family::function ? sizeof(st::local_defined_table_elem_storage_t) :
                        (kind==family::external || kind==family::exception ? sizeof(void*) : sizeof(reference))};
                    if(count>(SIZE_MAX-element_bytes)/width || !charge_native(count,width))
                    { return fail(preparation_status::quota_exceeded); }
                    element_bytes+=count*width; // quotient proved BEFORE product and sum.
                }
            }
            return true;
        }
        [[nodiscard]] bool fill_all_instance_resources()
        {
            namespace cp=checkpoint_world_data::cp;
            ::std::size_t element_bytes{};
            if(maximum_native_bytes_>SIZE_MAX || saved_.objects.size()>SIZE_MAX || !build_chunk_index() ||
               !charge_context_resource_backing(element_bytes) ||
               !context_->begin_resource_fixups(static_cast<::std::size_t>(saved_.objects.size()),element_bytes))
            { return fail(preparation_status::allocation_failed); }
            for(::std::size_t n{};n<saved_.objects.size();++n)
            {
                auto const id{static_cast<object_id>(n)+1u};bool ok{true};
                switch(saved_.objects[n].kind)
                {
                    case cp::object_kind::memory:ok=fill_memory_object(id);break;
                    case cp::object_kind::table:ok=fill_table_object(id);break;
                    case cp::object_kind::global:ok=fill_global_object(id);break;
                    case cp::object_kind::data:ok=fill_data_object(id);break;
                    case cp::object_kind::element:ok=fill_element_object(id);break;
                    default:break;
                }
                if(!ok) { return false; }
            }
            if(!context_->all_actual_resource_records_filled()) { return fail(preparation_status::type_mismatch); }
            phase_=preparation_status::resources_prepared;return true;
        }
        // This real constructor is intentionally PRIVATE. Its result is a
        // fresh native resource graph, not a complete/issued execution world.
        // The sole manager caller owns actual current source/profile and the
        // closed cohort/host/N/publication lifetime. Persisted build/provider
        // binding, root installation, old-reader drain, epoch/joint publication
        // are independent requirements of future complete-world restoration.
        [[nodiscard]] static ::std::unique_ptr<runtime_checkpoint_world_transaction> prepare_actual_resources(
            state_type const& saved,::uwvm2::runtime::checkpoint::compilation_profile::owner const& current_profile,
            source_owner const& original,::std::uint_least64_t actual_epoch,
            ::std::uint64_t maximum_native_bytes,::std::size_t maximum_original_source_bytes,
            checkpoint_world_data::cp::limits const& cap,llvm_jit_checkpoint_prepare_result& diagnostic) noexcept
        {
            namespace cp=checkpoint_world_data::cp;
            if(!source_type::has_canonical_owner(original) || actual_epoch==0u || maximum_native_bytes==0u || !current_profile) { return {}; }
            try
            {
                // Validation/semantic comparison allocate bounded cold scratch.
                // Keep them inside the real rollback exception boundary; a
                // malformed/oversized/OOM request cannot terminate noexcept.
                auto integrity{cp::state_identity::derive(saved,cap)};
                if(integrity.observation!=cp::state_identity::status::derived_data) { return {}; }
                auto result{::std::unique_ptr<runtime_checkpoint_world_transaction>{new runtime_checkpoint_world_transaction{}}};
                struct report_scope
                {
                    runtime_checkpoint_world_transaction& actual;llvm_jit_checkpoint_prepare_result& diagnostic;
                    ~report_scope() noexcept
                    { diagnostic.resource_diagnostic=static_cast<unsigned>(actual.phase_);diagnostic.native_payload_bytes=actual.native_bytes_; }
                } report{*result,diagnostic};
                result->maximum_native_bytes_=maximum_native_bytes;
                result->staged_gc_.native_budget_owner_=result.get();
                result->staged_gc_.native_budget_charge_=&charge_staged_native_backing;
                if(!result->charge_exclusive_graph_copy(saved)) { return {}; }
                result->saved_=saved;result->observed_profile_=current_profile->cache_identity();
                auto const remaining{maximum_native_bytes-result->native_bytes_};
                auto const source_cap{static_cast<::std::size_t>((::std::min)(remaining,static_cast<::std::uint64_t>(maximum_original_source_bytes)))};
                auto copied{context_type::prepare_owned_original_source(original,actual_epoch,source_cap)};
                if(copied.status!=context_type::source_copy_status::prepared_unpublished || !copied.source) { return {}; }
                result->prepared_source_=::std::move(copied.source);result->phase_=preparation_status::source_prepared;
                if(!result->charge_native(result->prepared_source_->file_.source_size(),1u)) { return {}; }
                for(auto const& file:result->prepared_source_->preload_files_)
                { if(!result->charge_native(file.source_size(),1u)) { return {}; } }
                result->context_.reset(new context_type{result->prepared_source_,result->staged_gc_});
                if(result->context_->build_resource_skeleton()!=context_type::skeleton_result::built ||
                   !result->bind_actual_modules(original,actual_epoch) || !result->bind_all_native_resources(original,actual_epoch) ||
                   !result->reserve_all_gc_shells() || !result->build_pending_bridges() || !result->build_all_exceptions() ||
                   !result->fill_all_gc_shells() || !result->fill_all_instance_resources()) { return {}; }
                return result; // DATA only; no old native frame/store owner escapes.
            }
            catch(...) { return {}; } // private RAII rollback, original world not mutated.
        }
        [[nodiscard]] bool prepare_all_native_engines(
            ::uwvm2::runtime::checkpoint::compilation_profile::owner const& actual_profile,
            bool unwind_stack,
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_debug_safe_point_granularity granularity,
            ::std::size_t maximum_endpoint_functions)
        {
            namespace checkpoint=::uwvm2::runtime::checkpoint;
            auto const stack_policy{unwind_stack?engine_type::call_stack_policy::unwind:engine_type::call_stack_policy::instruction};
            // A real privately issued compiler profile is DATA about genuine
            // emitter selection, never graph/epoch/root execution authority.
            if(phase_!=preparation_status::resources_prepared || !actual_profile ||
               actual_profile->purpose()!=checkpoint::compilation_purpose::resumable ||
               checkpoint::native_resume_abi_revision!=2u || !engines_.empty() || actual_dense_module_owners_.size()!=modules_.size()) { return false; }
            auto const new_profile{actual_profile->cache_identity()};
            if(new_profile[1u]!=17u || (observed_profile_[1u]!=13u && observed_profile_[1u]!=17u))
            { return fail(preparation_status::source_mismatch); }
            for(::std::size_t field{};field<new_profile.size();++field)
            {
                // Compilation DATA may upgrade the actual observation emitter
                // v13 to resumable v17 ONLY with the identical complete schema,
                // ABI2, native slot width and every budget/workspace field.
                // It is not a plan/frame permission: each saved Wasm site/slot/
                // control/EH shape must separately match the freshly sealed plan
                // before the sole issuer constructs any new executable frame.
                if(field!=1u && observed_profile_[field]!=new_profile[field])
                { return fail(preparation_status::source_mismatch); }
            }
            if(modules_.size()>engines_.max_size() || !charge_native(modules_.size(),sizeof(::std::unique_ptr<engine_type>)))
            { return fail(preparation_status::quota_exceeded); }
            engines_.resize(modules_.size());
            for(auto const owner:actual_dense_module_owners_)
            {
                if(owner>=modules_.size() || modules_[owner].new_actual_id>=engines_.size()) { return fail(preparation_status::source_mismatch); }
                auto actual_module{context_->prepare_compiler_module_owner(modules_[owner].actual)};
                if(!actual_module || !actual_module->matches_unpublished_file() || actual_module->file()!=modules_[owner].file)
                { return fail(preparation_status::source_mismatch); }
                ::std::size_t first{},imported{},locals{};
                if(!wire_space<0u>(owner,first,imported,locals)) { return fail(preparation_status::source_mismatch); }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
                if(prepared_native_endpoints_>maximum_endpoint_functions ||
                   locals>(maximum_endpoint_functions-prepared_native_endpoints_)/2u)
                { return fail(preparation_status::quota_exceeded); }
#else
                static_cast<void>(maximum_endpoint_functions);
#endif
                auto const* instance{lookup(modules_[owner].instance)};
                ::std::vector<engine_type::effective_function_body> bodies{};
                if(locals>bodies.max_size() || !charge_native(locals,sizeof(engine_type::effective_function_body)))
                { return fail(preparation_status::quota_exceeded); }
                bodies.resize(locals);
                for(::std::size_t local{};local<locals;++local)
                {
                    // [actual complete function index space][exact dense graph] end
                    // [safe] bounds from wire_space BEFORE imported/local+first
                    // reads; effective gen2 body comes from actual saved committed
                    // body DATA, never original parser body pretending generation2.
                    auto const id{instance->links[first+imported+local]};auto const* function{lookup(id)};auto const* native{this->native(id)};
                    if(function==nullptr || native==nullptr || native->owner!=owner || native->local!=local ||
                       function->flags!=0u || function->words[0u]!=imported+local || function->words[1u]==0u)
                    { return fail(preparation_status::source_mismatch); }
                    if(!charge_native(function->bytes.size(),1u)) { return false; }
                    bodies[local].public_function_index=imported+local;bodies[local].generation=function->words[1u];bodies[local].body=function->bytes;
                }
                auto compiled{engine_type::prepare(*actual_module,modules_[owner].new_actual_id,actual_profile,bodies,stack_policy,granularity,this,&charge_staged_native_backing)};
                engine_diagnostic_=static_cast<unsigned>(compiled.status);
                if(compiled.status!=engine_type::preparation_status::ok || !compiled.data)
                { if(phase_==preparation_status::resources_prepared) { (void)fail(preparation_status::type_mismatch); }return false; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
                if(!compiled.data->native_endpoint_capture_qualified_ ||
                   compiled.data->native_endpoint_expected_!=locals*2u ||
                   compiled.data->native_endpoint_helpers_>SIZE_MAX-prepared_native_helper_bodies_ ||
                   compiled.data->native_endpoint_claims_>SIZE_MAX-prepared_native_claims_)
                { return fail(preparation_status::type_mismatch); }
                prepared_native_endpoints_+=compiled.data->native_endpoint_expected_;
                prepared_native_helper_bodies_+=compiled.data->native_endpoint_helpers_;
                prepared_native_claims_+=compiled.data->native_endpoint_claims_;
#endif
                if(locals>SIZE_MAX-prepared_functions_) { return fail(preparation_status::quota_exceeded); }
                prepared_functions_+=locals;
                engines_[modules_[owner].new_actual_id]=::std::move(compiled.data);
            }
            actual_prepared_profile_=actual_profile;
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
            prepared_native_endpoint_capture_=true;
#endif
            return true; // Real engines/typedplans/aliases retained privately,
                         // native range/CFI permission and VM epoch NOT published.
        }
# include "uwvm_runtime_checkpoint_world_frames.h"
# include "uwvm_runtime_checkpoint_world_function_bindings.h"
# include "uwvm_runtime_checkpoint_world_indirect_bindings.h"
# include "uwvm_runtime_checkpoint_world_roots.h"
# include "uwvm_runtime_checkpoint_world_root_workers.h"
    public:
        runtime_checkpoint_world_transaction(runtime_checkpoint_world_transaction const&)=delete;
        runtime_checkpoint_world_transaction& operator=(runtime_checkpoint_world_transaction const&)=delete;
        ~runtime_checkpoint_world_transaction()=default;
    };
}
#endif
