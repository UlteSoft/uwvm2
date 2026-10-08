// Private synchronous Wasm value observation. Include inside runtime::lib after
// canonical publications, the genuine before-park capture and the N GC gate.
// This class is neither a restore issuer nor a public pointer/ID lookup service.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++"
{
    class runtime_checkpoint_debug_state_reader;
    class runtime_checkpoint_gc_state_borrow final
    {
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_coherent_manager;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_debug_state_reader;
        using capture = runtime_checkpoint_thread_capture;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        using execution_lease = ::uwvm2::utils::thread::execution_domain::lease;
        using closed_admission = ::uwvm2::utils::thread::checkpoint_host_admission::closed_admission;
        using gc_exclusion = ::uwvm2::runtime::gc::managed_entry_admission::exclusive_lease;
        using profile_owner = ::uwvm2::runtime::checkpoint::compilation_profile::owner;
        using source_owner = ::uwvm2::uwvm::runtime::full::full_source_instance::owner;
        using module_type = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using store_type = ::uwvm2::uwvm::runtime::storage::gc_object_store;
        using reference = ::uwvm2::uwvm::runtime::storage::gc_reference;
        using reference_kind = ::uwvm2::object::global::wasm_ref_kind;
        using core_type = ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type;
        using core_kind = ::uwvm2::parser::wasm::standard::wasm3::type::value_kind;
        using heap_kind = ::uwvm2::parser::wasm::standard::wasm3::type::abstract_heap_type;
        using composite_kind = ::uwvm2::parser::wasm::standard::wasm3::type::composite_kind;
        using object_value = ::uwvm2::uwvm::runtime::storage::gc_object_value;
        using logical_frame = ::uwvm2::runtime::checkpoint::logical_frame;
        using view = ::uwvm2::uwvm::debugger::wasm_state::view;
        using selection = ::uwvm2::uwvm::debugger::wasm_state::request;
        using copied_value = ::uwvm2::uwvm::debugger::wasm_state::value;
        using copied_object = ::uwvm2::uwvm::debugger::wasm_state::object;
        using copy_status = ::uwvm2::uwvm::debugger::wasm_state::status;
        static constexpr ::std::size_t module_limit{4096u}, record_limit{1048576u};

        execution_lease const& execution_;
        closed_admission const& closed_;
        gc_exclusion const& exclusion_;
        runtime_state_publication_guard const& publication_;
        ::std::span<domain::stopped_participant const> const cohort_;
        ::std::span<capture::owner const> const captures_;
        profile_owner const& profile_;
        ::std::uint_least64_t const epoch_;

        // SOLE constructor: the real manager, AFTER canonical owner/control
        // comparisons, exact current ticket/participant/location, N owned lease
        // exclusion, generated-root preflight and ALL frame/publication checks.
        // No boolean, epoch, report or caller-created capture can construct it.
        runtime_checkpoint_gc_state_borrow(execution_lease const& execution,
            closed_admission const& closed, gc_exclusion const& exclusion,
            runtime_state_publication_guard const& publication,
            ::std::span<domain::stopped_participant const> cohort,
            ::std::span<capture::owner const> captures, profile_owner const& profile,
            ::std::uint_least64_t epoch) noexcept
            : execution_{execution}, closed_{closed}, exclusion_{exclusion}, publication_{publication},
              cohort_{cohort}, captures_{captures}, profile_{profile}, epoch_{epoch} {}
        runtime_checkpoint_gc_state_borrow(runtime_checkpoint_gc_state_borrow const&) = delete;
        runtime_checkpoint_gc_state_borrow& operator=(runtime_checkpoint_gc_state_borrow const&) = delete;
        runtime_checkpoint_gc_state_borrow(runtime_checkpoint_gc_state_borrow&&) = delete;
        runtime_checkpoint_gc_state_borrow& operator=(runtime_checkpoint_gc_state_borrow&&) = delete;
        ~runtime_checkpoint_gc_state_borrow() = default;

        struct module_pin
        {
            ::std::uint64_t id{};
            module_type const* module{};
            source_owner source{};
            ::std::shared_ptr<store_type> store{};
        };
        struct native_value
        {
            ::std::size_t owner{};
            core_type type{};
            object_value carrier{};
            bool initialized{};
        }; // Private lexical N-only carrier, NEVER returned as debugger DATA.
        struct pending_object
        {
            reference ref{}; // Native comparison key ONLY; never emitted.
            ::std::size_t owner{};
            ::uwvm2::runtime::exception::value_ref exception{};
        };
        struct work
        {
            view output{};
            ::std::vector<module_pin> modules{};
            ::std::vector<pending_object> pending{};
            ::std::size_t members{}, records{};
            copy_status copy_error{copy_status::unavailable_gc_roots};
        };
        template<typename A, typename B>
        [[nodiscard]] static bool same_owner(A const& a, B const& b) noexcept
        { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
        [[nodiscard]] bool current_scope() const noexcept
        {
            (void)publication_;
            // These are actual retained guards. This check does not issue a
            // guard: only the private constructor's lexical producer can do so.
            return execution_ && !execution_.stop_requested() && closed_ && exclusion_ &&
                epoch_ != 0u && current_runtime_generation() == epoch_ &&
                get_runtime_state_publication_depth() == 1u && !cohort_.empty() &&
                cohort_.size() == captures_.size() && cohort_.size() <= 256u &&
                same_owner(profile_, g_runtime.checkpoint_profile) &&
                g_runtime.compiled_all.load(::std::memory_order_acquire);
        }
        [[nodiscard]] static bool charge(::std::size_t amount, ::std::size_t& total) noexcept
        { if(amount > record_limit - total) { return false; } total += amount; return true; }
        [[nodiscard]] static bool complete_modules(work& state, profile_owner const& profile,
            ::std::uint_least64_t epoch)
        {
            auto const count{g_runtime.modules.size()};
            if(count == 0u || count > module_limit) { state.copy_error = copy_status::resource_limit; return false; }
            state.modules.reserve(count); // count bounded BEFORE allocation.
            for(::std::size_t id{}; id != count; ++id)
            {
                // [actual dense module array ... id<count<=4096] end
                // [safe] actual lease + ONE cohort + hostclose + N + publication.
                auto const& actual{g_runtime.modules.index_unchecked(id)};
                auto const* publication{actual.llvm_jit_full_publication.get()};
                if(publication == nullptr || actual.runtime_module == nullptr || !actual.llvm_jit_ready ||
                   publication->plan ||
                   publication->debug_source_runtime_epoch != epoch ||
                   actual.llvm_jit_debug_source_fused_epoch != epoch ||
                   !same_owner(publication->checkpoint_profile, profile)) { return false; }
                // A module with no definitions has no generated code. Its
                // actual source seal and store remain mandatory; only native
                // engine/context ownership is vacuous for this empty extent.
                if(actual.runtime_module->local_defined_function_vec_storage.empty())
                { if(publication->engine || publication->context) { return false; } }
                else if(!publication->engine || !publication->context) { return false; }
                auto const source{publication->source}; // Actual publication strong owner, not supplied alias.
                if(!::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) ||
                   !source->initialized_from_actual_state() ||
                   source->actual_validated_file(id, epoch, actual.runtime_module) == nullptr)
                { return false; }
                auto const declaration_owner{source->actual_validated_file(id, epoch, actual.runtime_module)};
                if(declaration_owner == nullptr || !declaration_owner->has_owned_source_image()) { return false; }
                auto const member{source->registry().find(actual.module_name)};
                if(member == source->registry().end() || ::std::addressof(member->second) != actual.runtime_module)
                { return false; }
                // Only now is the genuine runtime module pointee read.
                auto const& module{member->second};
                auto const store{module.gc_store};
                if(!store || !store->valid_) { return false; }
                for(auto const& earlier : state.modules)
                { if(earlier.module == ::std::addressof(module) || earlier.store.get() == store.get()) { return false; } }
                if(!charge(module.local_defined_function_vec_storage.size(), state.records) ||
                   !charge(module.imported_function_vec_storage.size(), state.records) ||
                   !charge(module.local_defined_table_vec_storage.size(), state.records) ||
                   !charge(module.imported_table_vec_storage.size(), state.records) ||
                   !charge(module.local_defined_global_vec_storage.size(), state.records) ||
                   !charge(module.imported_global_vec_storage.size(), state.records) ||
                   !charge(module.local_defined_tag_vec_storage.size(), state.records) ||
                   !charge(module.imported_tag_vec_storage.size(), state.records))
                { state.copy_error = copy_status::resource_limit; return false; }
                state.modules.push_back({static_cast<::std::uint64_t>(id), ::std::addressof(module), source, store});
            }
            // ALL stores, including empty stores/modules absent from frames.
            // This list lock proves membership/lifetime only. Actual N admission
            // and current root/cohort proofs above exclude mutation/reclaim.
            ::std::size_t stores{};
            for(auto* actual{store_type::cohort_head_}; actual != nullptr; actual = actual->cohort_next_)
            {
                if(stores == state.modules.size()) { return false; }
                bool found{};
                for(auto const& pin : state.modules)
                { if(pin.store.get() == actual) { found = true; break; } }
                if(!found || !actual->cohort_registered_) { return false; }
                ++stores;
            }
            return stores == state.modules.size();
        }
        template<::uwvm2::parser::wasm::concepts::wasm_feature... Features>
        [[nodiscard]] static bool declared_module(module_pin const& pin,
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Features...> const& parsed) noexcept
        {
            namespace f = ::uwvm2::parser::wasm::standard::wasm1::features;
            namespace op = ::uwvm2::parser::wasm::concepts::operation;
            auto const& types{op::get_first_type_in_tuple<f::type_section_storage_t<Features...>>(parsed.sections)};
            auto const& globals{op::get_first_type_in_tuple<f::global_section_storage_t<Features...>>(parsed.sections).local_globals};
            auto const& tables{op::get_first_type_in_tuple<f::table_section_storage_t<Features...>>(parsed.sections)};
            auto const& module{*pin.module};
            auto const& runtime_types{module.type_section_storage};
            // Complete parser vectors are canonical-owner borrowed. Bound their
            // element counts BEFORE forming either end pointer or indexed type
            // address, including the separately owned rich signature vector.
            if(types.types.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*types.types.cbegin()) ||
               types.owned_signatures.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*types.owned_signatures.cbegin()) ||
               (!types.owned_signatures.empty() && types.owned_signatures.size() != types.types.size())) { return false; }
            if(runtime_types.type_section_count != types.types.size() ||
               runtime_types.type_section_begin != (types.types.empty() ? nullptr : types.types.cbegin()) ||
               runtime_types.type_section_end != (types.types.empty() ? nullptr : types.types.cend()) ||
               runtime_types.owned_signature_begin != (types.owned_signatures.empty() ? nullptr : types.owned_signatures.cbegin()) ||
               runtime_types.owned_signature_end != (types.owned_signatures.empty() ? nullptr : types.owned_signatures.cend()) ||
               globals.size() != module.local_defined_global_vec_storage.size() ||
               tables.tables.size() != module.local_defined_table_vec_storage.size() ||
               (!tables.initializers.empty() && tables.initializers.size() != tables.tables.size())) { return false; }
            for(::std::size_t i{}; i != globals.size(); ++i)
            {
                auto const& declaration{globals.index_unchecked(i)};
                auto const& actual{module.local_defined_global_vec_storage.index_unchecked(i)};
                // [actual parser/runtime arrays with equal count] end
                // [safe] compare borrowed pointers BEFORE dereferencing them.
                if(actual.global_type_ptr != ::std::addressof(declaration.global) ||
                   actual.local_global_type_ptr != ::std::addressof(declaration) ||
                   actual.owner_module_rt_ptr != pin.module ||
                   actual.init_state != ::uwvm2::uwvm::runtime::storage::wasm_global_init_state::initialized)
                { return false; }
            }
            for(::std::size_t i{}; i != tables.tables.size(); ++i)
            {
                auto const& declaration{tables.tables.index_unchecked(i)};
                auto const& actual{module.local_defined_table_vec_storage.index_unchecked(i)};
                if(actual.table_type_ptr != ::std::addressof(declaration) || actual.owner_module_rt_ptr != pin.module ||
                   actual.initializer_expr != (tables.initializers.empty() ? nullptr : ::std::addressof(tables.initializers.index_unchecked(i))) ||
                   actual.elems.size() < declaration.limits.min ||
                   (declaration.limits.present_max && actual.elems.size() > declaration.limits.max)) { return false; }
            }
            return true;
        }
        [[nodiscard]] static bool declaration_origin(module_pin const& pin) noexcept
        {
            // Per-member ownership comes from the actual initializer seal,
            // real dense builder and actual finalized validation publisher.
            // No caller-supplied module name/global map lookup, mutable mapped
            // byte copy or hash supplies declaration ownership.
            auto const& source{pin.source};
            auto const* file{source->actual_validated_file(static_cast<::std::size_t>(pin.id),
                current_runtime_generation(), pin.module)};
            if(file == nullptr || !file->has_owned_source_image() || file->binfmt_ver != 1u) { return false; }
            return declared_module(pin, file->wasm_module_storage.wasm_binfmt_ver1_storage);
        }
        [[nodiscard]] static copied_value declared_value(core_type type) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            copied_value result{};
            switch(type.kind)
            {
                case core_kind::i32: result.type.kind = ws::value_kind::i32; break;
                case core_kind::i64: result.type.kind = ws::value_kind::i64; break;
                case core_kind::f32: result.type.kind = ws::value_kind::f32; break;
                case core_kind::f64: result.type.kind = ws::value_kind::f64; break;
                case core_kind::v128: result.type.kind = ws::value_kind::v128; break;
                case core_kind::reference:
                    if(type.heap.code < -23 || type.heap.code > 0xffffffffll ||
                       (type.heap.code < 0 && type.heap.code > -12)) { return result; }
                    result.type.kind = ws::value_kind::reference;
                    result.type.heap = type.heap.code; result.type.nullable = type.nullable; break;
                default: return result;
            }
            result.type.known = true; return result;
        }
        template<unsigned Bits>
        static void numeric(copied_value& out, ::std::byte const* actual) noexcept
        {
            static_assert(Bits == 32u || Bits == 64u);
            using integer = ::std::conditional_t<Bits == 32u, ::std::uint32_t, ::std::uint64_t>;
            integer value{};
            // [actual complete native carrier; width=4 or8 <=16] end
            // [safe] memcpy avoids float evaluation and unaligned aliasing.
            ::std::memcpy(::std::addressof(value), actual, sizeof(value));
            auto* first{reinterpret_cast<unsigned char*>(out.bits.data())};
            ::fast_io::basic_obuffer_view<unsigned char> buffer{first, first + sizeof(value)};
            ::fast_io::io::print(buffer, ::fast_io::mnp::le_put<Bits>(value));
            out.available = true;
        }
        [[nodiscard]] static bool abstract_matches(reference ref, core_type type) noexcept
        {
            if(type.kind != core_kind::reference) { return false; }
            if(ref.kind == reference_kind::wasm_null) { return type.nullable; }
            if(type.heap.is_defined())
            { return ref.kind == reference_kind::wasm_struct || ref.kind == reference_kind::wasm_array ||
                     ref.kind == reference_kind::wasm_func_imported || ref.kind == reference_kind::wasm_func_defined; }
            switch(static_cast<heap_kind>(type.heap.code))
            {
                case heap_kind::any: return ref.kind == reference_kind::wasm_struct || ref.kind == reference_kind::wasm_array ||
                    ref.kind == reference_kind::wasm_i31 || ref.kind == reference_kind::wasm_extern;
                case heap_kind::eq: return ref.kind == reference_kind::wasm_struct || ref.kind == reference_kind::wasm_array || ref.kind == reference_kind::wasm_i31;
                case heap_kind::i31: return ref.kind == reference_kind::wasm_i31;
                case heap_kind::struct_: return ref.kind == reference_kind::wasm_struct;
                case heap_kind::array: return ref.kind == reference_kind::wasm_array;
                case heap_kind::func: return ref.kind == reference_kind::wasm_func_imported || ref.kind == reference_kind::wasm_func_defined;
                case heap_kind::extern_: return ref.kind == reference_kind::wasm_extern;
                case heap_kind::exn: return ref.kind == reference_kind::wasm_exn;
                default: return false;
            }
        }
        [[nodiscard]] static bool find_function(work const& state, reference ref,
            ::std::size_t& owner, ::std::size_t& index) noexcept
        {
            for(::std::size_t id{}; id != state.modules.size(); ++id)
            {
                auto const& module{*state.modules[id].module};
                if(ref.kind == reference_kind::wasm_func_imported)
                {
                    for(::std::size_t i{}; i != module.imported_function_vec_storage.size(); ++i)
                    { if(ref.storage.ptr == ::std::addressof(module.imported_function_vec_storage.index_unchecked(i))) { owner = id; index = i; return true; } }
                }
                else if(ref.kind == reference_kind::wasm_func_defined)
                {
                    for(::std::size_t i{}; i != module.local_defined_function_vec_storage.size(); ++i)
                    { if(ref.storage.ptr == ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(i)))
                      { owner = id; index = module.imported_function_vec_storage.size() + i; return true; } }
                }
            }
            // Comparison to every actual owned record precedes ANY pointee read.
            return false;
        }
        [[nodiscard]] static bool aggregate_owner(work const& state, reference ref,
            ::std::size_t& owner, ::std::uint_least32_t& type_index, ::std::size_t& length) noexcept
        {
            auto const kind{ref.kind == reference_kind::wasm_struct ? composite_kind::struct_ : composite_kind::array};
            for(::std::size_t i{}; i != state.modules.size(); ++i)
            {
                auto const& store{*state.modules[i].store};
                // Local lookup compares tokens only to real initialized nodes.
                // Do NOT use checked_foreign_object: observation installs no
                // recipient leases or other heap mutation under this guard.
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
                if(ref.kind == reference_kind::wasm_struct)
                {
                    auto const compact{store.checked_local_compact_object(ref)};
                    if(compact)
                    { owner = i; type_index = compact.type_index(); length = 1u; return true; }
                }
#endif
                auto const* actual{store.checked_local_object(ref, kind)};
                if(actual != nullptr)
                { owner = i; type_index = actual->type_index; length = actual->length; return true; }
            }
            return false;
        }
        [[nodiscard]] static bool append_reference(work& state, ::std::size_t declared_owner,
            core_type type, reference ref, copied_value& out)
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            if(declared_owner >= state.modules.size() || !out.type.known || !abstract_matches(ref, type)) { return false; }
            if(type.heap.is_defined()) { out.type.type_module = state.modules[declared_owner].id; }
            if(ref.kind == reference_kind::wasm_null)
            { out.ref.kind = ws::reference_kind::null; out.available = true; return true; }
            if(ref.kind == reference_kind::wasm_i31)
            { out.ref.kind = ws::reference_kind::i31; out.ref.i31_bits = ref.storage.wasm_i31.get_u(); out.available = true; return true; }
            if(ref.storage.ptr == nullptr) { return false; }
            if(ref.kind == reference_kind::wasm_func_imported || ref.kind == reference_kind::wasm_func_defined)
            {
                ::std::size_t owner{}, index{};
                if(!find_function(state, ref, owner, index) ||
                   !state.modules[declared_owner].store->reference_type_matches(ref, type)) { return false; }
                out.ref.kind = ws::reference_kind::function; out.ref.function_identity_available = true;
                out.ref.function_module = state.modules[owner].id; out.ref.function_index = index; out.available = true; return true;
            }
            ::std::size_t owner{declared_owner}, length{}; ::std::uint_least32_t type_index{};
            ::uwvm2::runtime::exception::value_ref exception{};
            copied_object object{};
            if(ref.kind == reference_kind::wasm_struct || ref.kind == reference_kind::wasm_array)
            {
                if(!aggregate_owner(state, ref, owner, type_index, length)) { return false; }
                if(type.heap.is_defined() && !store_type::canonical_subtype(state.modules[owner].store.get(), type_index,
                    state.modules[declared_owner].store.get(), static_cast<::std::uint_least32_t>(type.heap.code))) { return false; }
                auto const* layout{state.modules[owner].store->checked_type(type_index,
                    ref.kind == reference_kind::wasm_struct ? composite_kind::struct_ : composite_kind::array)};
                if(layout == nullptr || (ref.kind == reference_kind::wasm_struct && length != layout->field_count) ||
                   (ref.kind == reference_kind::wasm_array && layout->field_count != 1u)) { return false; }
                object.kind = ref.kind == reference_kind::wasm_struct ? ws::object_kind::structure : ws::object_kind::array;
                object.module = state.modules[owner].id; object.type_index = type_index; object.total_members = length;
                out.ref.kind = ref.kind == reference_kind::wasm_struct ? ws::reference_kind::structure : ws::reference_kind::array;
            }
            else if(ref.kind == reference_kind::wasm_exn)
            {
                if(!store_type::exn_token_shape(ref.storage.ptr)) { return false; }
                ::std::shared_ptr<store_type const> issuer{};
                {
                    store_type::exn_guard guard{};
                    auto const* registered{store_type::find_exn_locked(ref.storage.ptr)};
                    if(registered == nullptr) { return false; }
                    issuer = registered->owner.lock();
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
                    exception = registered->value.native_untracked_owner();
#else
                    exception = registered->value;
#endif
                }
                bool canonical_issuer{};
                for(auto const& actual : state.modules)
                { if(same_owner(issuer, actual.store)) { canonical_issuer = true; break; } }
                if(!exception || !canonical_issuer) { return false; }
                object.kind = ws::object_kind::exception; object.total_members = exception->fields().size();
                out.ref.kind = ws::reference_kind::exception;
                bool found{};
                for(::std::size_t id{}; id != state.modules.size(); ++id)
                {
                    auto const& module{*state.modules[id].module};
                    for(::std::size_t i{}; i != module.local_defined_tag_vec_storage.size(); ++i)
                    {
                        auto const& tag{module.local_defined_tag_vec_storage.index_unchecked(i)};
                        if(tag.exception_identity.get() != exception->tag_identity()) { continue; }
                        if(!tag.exception_identity || !declaration_origin(state.modules[id]) ||
                           tag.type_index >= module.type_section_storage.type_section_count) { return false; }
                        auto const& types{module.type_section_storage};
                        auto const first{reinterpret_cast<::std::uintptr_t>(types.type_section_begin)};
                        if(types.type_section_begin == nullptr || types.type_section_count >
                           static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*types.type_section_begin) ||
                           types.type_section_count > ((::std::numeric_limits<::std::uintptr_t>::max)() - first) / sizeof(*types.type_section_begin) ||
                           tag.function_type_ptr != types.type_section_begin + tag.type_index) { return false; }
                        owner = id; object.module = state.modules[id].id; object.type_index = tag.type_index;
                        object.tag_module = state.modules[id].id; object.tag_index = module.imported_tag_vec_storage.size() + i;
                        object.tag_identity_available = true; found = true; break;
                    }
                    if(found) { break; }
                }
                // Same signature is NEVER a substitute for actual tag identity.
                if(!found) { return false; }
            }
            else if(ref.kind == reference_kind::wasm_extern)
            {
                if(store_type::bridge_token_shape(ref.storage.ptr))
                {
                    bool registered{}; store_type const* bridge_owner{};
                    { store_type::bridge_guard guard{}; auto const* bridge{store_type::find_bridge_token_locked(ref.storage.ptr)};
                      if(bridge != nullptr) { bridge_owner = bridge->owner; registered = true; } }
                    if(!registered) { return false; }
                    bool found{};
                    for(::std::size_t i{}; i != state.modules.size(); ++i)
                    { if(state.modules[i].store.get() == bridge_owner) { owner = i; found = true; break; } }
                    if(!found) { return false; }
                    object.kind = ws::object_kind::external_wrapper; object.total_members = 1u;
                    out.ref.kind = ws::reference_kind::external_wrapper;
                }
                else
                {
                    // Actual rooted typed extern carrier, compared only for
                    // query-local alias identity; its opaque host payload is
                    // NEVER read, called, exported or treated as a GC object.
                    object.kind = ws::object_kind::host_reference; out.ref.kind = ws::reference_kind::host_reference;
                }
                object.module = state.modules[owner].id;
            }
            else { return false; }
            for(::std::size_t i{}; i != state.pending.size(); ++i)
            {
                auto const& previous{state.pending[i]};
                if(previous.ref.kind == ref.kind && previous.ref.storage.ptr == ref.storage.ptr)
                { out.ref.object = i + 1u; out.available = true; return true; }
            }
            if(state.pending.size() == ws::maximum_objects) { state.copy_error = copy_status::resource_limit; return false; }
            object.identifier = state.pending.size() + 1u;
            out.ref.object = object.identifier; out.available = true;
            state.pending.push_back({ref, owner, ::std::move(exception)});
            state.output.objects.push_back(::std::move(object));
            return true;
        }
        [[nodiscard]] static bool copy_native(work& state, ::std::size_t owner, core_type type,
            ::std::byte const* bytes, bool initialized, copied_value& out, native_value* native = nullptr)
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            out = declared_value(type);
            if(!out.type.known) { return false; }
            if(type.kind == core_kind::reference && type.heap.is_defined())
            {
                if(owner >= state.modules.size()) { return false; }
                out.type.type_module = state.modules[owner].id;
            }
            if(!initialized) { out.unavailable = ws::unavailable_reason::not_initialized; return true; }
            if(native != nullptr)
            {
                // Private mutator source hook. Its output never escapes this
                // actual N/publication borrow and is consumed only on success.
                auto const width{type.kind == core_kind::reference ? sizeof(reference) : type.kind == core_kind::v128 ? 16u :
                    type.kind == core_kind::i32 || type.kind == core_kind::f32 ? 4u : 8u};
                // [actual complete typed native carrier][checked width<=16]
                // [safe] copy exactly the declared width; exception numeric
                // payloads may be only4/8 bytes, never blindly read16.
                *native = {}; native->owner = owner; native->type = type; native->initialized = true;
                ::std::memcpy(native->carrier.bits.data(), bytes, width);
            }
            switch(type.kind)
            {
                case core_kind::i32: case core_kind::f32: numeric<32u>(out, bytes); return true;
                case core_kind::i64: case core_kind::f64: numeric<64u>(out, bytes); return true;
                case core_kind::v128:
                    // Actual packet producer stores get_llvm_type(...v128),
                    // which is <16 x i8>, never an i128 scalar. Runtime globals
                    // use the matching char[16]/GNU char-vector storage and GC
                    // fields/payloads copy this same byte carrier. Each element
                    // is one byte: copying lane order needs no endian reversal.
                    // Numeric lanes are interpreted later through FastIO LE.
                    ::std::memcpy(out.bits.data(), bytes, out.bits.size()); out.available = true; return true;
                case core_kind::reference:
                {
                    reference ref{}; static_assert(sizeof(ref) <= 16u);
                    ::std::memcpy(::std::addressof(ref), bytes, sizeof(ref));
                    return append_reference(state, owner, type, ref, out);
                }
            }
            return false;
        }
        [[nodiscard]] static bool copy_member(work& state, ::std::size_t index,
            ::std::uint64_t supplied_member, ::uwvm2::uwvm::debugger::wasm_state::row& result, native_value* carrier = nullptr)
        {
            if(index >= state.pending.size() || index >= state.output.objects.size()) { return false; }
            auto const pending{state.pending[index]};
            auto const owner{pending.owner};
            if(owner >= state.modules.size() || supplied_member >= state.output.objects[index].total_members ||
               supplied_member > (::std::numeric_limits<::std::size_t>::max)())
            { state.copy_error = copy_status::out_of_range; return false; }
            // [real immutable object total][member < total and SIZE_MAX] end
            // [safe] bound BEFORE narrowing or any member/layout pointer access.
            auto const member{static_cast<::std::size_t>(supplied_member)};
            auto const& store{*state.modules[owner].store}; copied_value value{};
            if(pending.ref.kind == reference_kind::wasm_struct || pending.ref.kind == reference_kind::wasm_array)
            {
                auto const type_index{state.output.objects[index].type_index};
                auto const* layout{store.checked_type(static_cast<::std::uint_least32_t>(type_index),
                    pending.ref.kind == reference_kind::wasm_struct ? composite_kind::struct_ : composite_kind::array)};
                if(layout == nullptr || layout->fields == nullptr ||
                   layout->field_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(::uwvm2::parser::wasm::standard::wasm3::type::field_type)) { return false; }
                auto const field{pending.ref.kind == reference_kind::wasm_struct ? member : 0u};
                if(field >= layout->field_count) { return false; }
                // [copied immutable real type fields ... field<count] end
                // [safe] full extent checked BEFORE layout field access.
                auto const storage{layout->fields[field].storage};
                object_value native{};
                auto const got{pending.ref.kind == reference_kind::wasm_struct ?
                    store.struct_get(pending.ref, member, false, native) : store.array_get(pending.ref, member, false, native)};
                if(got != ::uwvm2::uwvm::runtime::storage::gc_object_status::ok ||
                   !copy_native(state, owner, storage.value, native.bits.data(), true, value, carrier)) { return false; }
                using packed = ::uwvm2::parser::wasm::standard::wasm3::type::packed_kind;
                value.packed_bits = storage.packed == packed::i8 ? 8u : storage.packed == packed::i16 ? 16u : 0u;
                result.mutable_storage = layout->fields[field].mutable_; result.mutability_known = true;
            }
            else if(pending.ref.kind == reference_kind::wasm_extern)
            {
                reference inner{};
                { store_type::bridge_guard guard{}; auto const* bridge{store_type::find_bridge_token_locked(pending.ref.storage.ptr)};
                  if(bridge == nullptr || bridge->owner != ::std::addressof(store)) { return false; }
                  inner = bridge->inner; }
                // Release registry lock BEFORE recursive logical enqueue.
                core_type type{}; type.kind = core_kind::reference; type.heap.code = static_cast<::std::int_least64_t>(heap_kind::any); type.nullable = true;
                object_value native{}; ::std::memcpy(native.bits.data(), ::std::addressof(inner), sizeof(inner));
                if(!copy_native(state, owner, type, native.bits.data(), true, value, carrier)) { return false; }
                result.mutability_known = true; // Immutable Wasm-created wrapper edge.
            }
            else if(pending.ref.kind == reference_kind::wasm_exn)
            {
                if(!pending.exception || member >= pending.exception->fields().size()) { return false; }
                auto const& module{*state.modules[owner].module};
                auto const& types{module.type_section_storage};
                auto const type_index{state.output.objects[index].type_index};
                auto const* signatures{types.owned_signature_begin};
                if(signatures == nullptr || types.owned_signature_end == nullptr || type_index >= types.type_section_count ||
                   types.type_section_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*signatures)) { return false; }
                auto const first{reinterpret_cast<::std::uintptr_t>(signatures)}, last{reinterpret_cast<::std::uintptr_t>(types.owned_signature_end)};
                if(last < first || types.type_section_count > ((::std::numeric_limits<::std::uintptr_t>::max)() - first) / sizeof(*signatures) ||
                   last - first != types.type_section_count * sizeof(*signatures)) { return false; }
                auto const& signature{signatures[static_cast<::std::size_t>(type_index)]};
                if(signature.type_index != type_index || !signature.results.empty() ||
                   signature.parameters.size() != pending.exception->fields().size() || member >= signature.parameters.size()) { return false; }
                auto const declared{signature.parameters.index_unchecked(member)};
                auto const& field{pending.exception->fields()[member]};
                using payload = ::uwvm2::runtime::exception::payload_kind;
                auto const actual{field.kind()};
                auto const bits{field.bits()};
                if(declared.kind == core_kind::reference)
                {
                    if(actual != payload::wasm_reference || bits.size() != sizeof(reference)) { return false; }
                }
                else if((declared.kind == core_kind::i32 && actual != payload::i32) ||
                        (declared.kind == core_kind::i64 && actual != payload::i64) ||
                        (declared.kind == core_kind::f32 && actual != payload::f32) ||
                        (declared.kind == core_kind::f64 && actual != payload::f64) ||
                        (declared.kind == core_kind::v128 && actual != payload::v128)) { return false; }
                if(!copy_native(state, owner, declared, bits.data(), true, value, carrier)) { return false; }
                result.mutability_known = true; // Exception payload is immutable.
            }
            else { return false; }
            result.index = supplied_member; result.data = ::std::move(value); return true;
        }
        [[nodiscard]] static bool copy_member_page(work& state, selection const& requested)
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            if(state.output.rows.size() != 1u) { state.copy_error = copy_status::out_of_range; return false; }
            auto value{state.output.rows.front().data};
            auto expandable = [](copied_value const& item) noexcept
            {
                return item.available && item.type.kind == ws::value_kind::reference && item.ref.object != 0u &&
                    (item.ref.kind == ws::reference_kind::structure || item.ref.kind == ws::reference_kind::array ||
                     item.ref.kind == ws::reference_kind::exception || item.ref.kind == ws::reference_kind::external_wrapper);
            };
            // The root was just copied from ONE genuine original index after
            // complete current capture/hostclose/N/publication proof. These
            // dense IDs are strictly INTERNAL scratch references, NEVER input.
            auto const depth_count{requested.compressed_path() ? requested.long_path.size() : requested.path_size};
            for(::std::size_t depth{}; depth != depth_count; ++depth)
            {
                if(!expandable(value) || value.ref.object > state.pending.size())
                { state.copy_error = copy_status::invalid_selection; return false; }
                ws::row edge{};
                auto const index{static_cast<::std::size_t>(value.ref.object - 1u)};
                // [actual query-local dense scratch id 1..pending.size()] end
                // [safe] checked BEFORE subtraction/narrowing/indexing.
                // [bounded complete original path0..<=4096 OR fixed path0..16] end
                // [safe] depth<checked count BEFORE either index selection.
                auto const original_index{requested.compressed_path() ? requested.long_path[depth] : requested.path[depth]};
                if(!copy_member(state, index, original_index, edge)) { return false; }
                value = ::std::move(edge.data);
                if(requested.compressed_path())
                {
                    if(!expandable(value) || value.ref.object>state.pending.size() || state.pending.size()!=state.output.objects.size())
                    { state.copy_error=copy_status::invalid_selection;return false; }
                    auto const next{static_cast<::std::size_t>(value.ref.object-1u)};
                    // [freshly authenticated dense scratch0..N][root is0] end
                    // [safe] nonzero id<=N BEFORE moving either native owner.
                    // Keep ONLY root+current metadata under this lexical N
                    // exclusion. No intermediate native owner escapes or is
                    // cached. Every later query restarts at the original root.
                    if(next==0u) { state.pending.resize(1u);state.output.objects.resize(1u);value.ref.object=1u; }
                    else
                    {
                        auto pending{::std::move(state.pending[next])};auto object{::std::move(state.output.objects[next])};
                        state.pending.resize(1u);state.output.objects.resize(1u);
                        object.identifier=2u;state.pending.push_back(::std::move(pending));state.output.objects.push_back(::std::move(object));value.ref.object=2u;
                    }
                }
            }
            if(!expandable(value) || value.ref.object > state.pending.size())
            { state.copy_error = copy_status::invalid_selection; return false; }
            auto const target{static_cast<::std::size_t>(value.ref.object - 1u)};
            auto const total{state.output.objects[target].total_members};
            if(requested.member_first > total)
            { state.copy_error = copy_status::out_of_range; return false; }
            auto const remaining{total - requested.member_first};
            auto const count{requested.member_count < remaining ? requested.member_count : remaining};
            state.output.selected_object = value.ref.object;
            state.output.objects[target].members.reserve(static_cast<::std::size_t>(count));
            for(::std::uint64_t offset{}; offset != count; ++offset)
            {
                // [first <= total][offset < count <= total-first] end
                // [safe] subtraction proof BEFORE first+offset or object read.
                ws::row member{};
                if(!copy_member(state, target, requested.member_first + offset, member)) { return false; }
                state.output.objects[target].members.push_back(::std::move(member)); ++state.members;
            }
            // SHALLOW: child/intermediate nodes retain identity/type/total only;
            // no recursive enqueue can consume another member page. At most
            // root1 + path16 + child64 objects, each bounded by quota128.
            for(auto& object : state.output.objects)
            {
                object.first_member = 0u; object.next_member = 0u;
                object.has_more_members = object.total_members != 0u;
                object.members_truncated = object.total_members != 0u;
            }
            auto& selected{state.output.objects[target]};
            selected.first_member = requested.member_first; selected.next_member = requested.member_first + count;
            selected.has_more_members = count != remaining;
            selected.members_truncated = requested.member_first != 0u || count != total;
            for(auto const& object : state.output.objects)
            { state.output.graph_truncated = state.output.graph_truncated || object.members_truncated; }
            return true;
        }
        [[nodiscard]] static bool copy_objects(work& state)
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            for(::std::size_t index{}; index != state.pending.size(); ++index)
            {
                // Both vectors reserved their full bounded128 population. No
                // native/store/object pointer survives this synchronous scope.
                auto const total{state.output.objects[index].total_members};
                auto const remaining{ws::maximum_members - state.members};
                auto const count{total < remaining ? total : remaining};
                state.output.objects[index].members.reserve(static_cast<::std::size_t>(count));
                for(::std::size_t member{}; member != count; ++member)
                {
                    ws::row value{};
                    if(!copy_member(state, index, member, value)) { return false; }
                    state.output.objects[index].members.push_back(::std::move(value)); ++state.members;
                }
                state.output.objects[index].next_member = count;
                state.output.objects[index].has_more_members = count != total;
                state.output.objects[index].members_truncated = count != total;
                state.output.graph_truncated = state.output.graph_truncated || count != total;
            }
            return true;
        }
        // Supplied record addresses are comparison keys. Each hop must match a
        // member of an ALL-dense actual module vector before reading its union.
        template<bool Imported, bool Table, typename Pointer>
        [[nodiscard]] static bool record_owner(work const& state, Pointer supplied, ::std::size_t& owner, ::std::size_t& index) noexcept
        {
            for(::std::size_t id{}; id != state.modules.size(); ++id)
            {
                auto const& module{*state.modules[id].module};
                auto const& entries{[&]() -> auto const& {
                    if constexpr(Table && Imported) { return module.imported_table_vec_storage; }
                    else if constexpr(Table) { return module.local_defined_table_vec_storage; }
                    else if constexpr(Imported) { return module.imported_global_vec_storage; }
                    else { return module.local_defined_global_vec_storage; }
                }()};
                for(::std::size_t i{}; i != entries.size(); ++i)
                { if(supplied == ::std::addressof(entries.index_unchecked(i))) { owner = id; index = i; return true; } }
            }
            return false;
        }
        template<bool Table>
        [[nodiscard]] static bool resolve_resource(work const& state, ::std::size_t& owner, ::std::size_t& index) noexcept
        {
            auto const& module{*state.modules[owner].module};
            auto const imports{[&]() { if constexpr(Table) { return module.imported_table_vec_storage.size(); }
                                     else { return module.imported_global_vec_storage.size(); } }()};
            if(index >= imports) { index -= imports; return true; }
            auto const* next{[&]() { if constexpr(Table) { return ::std::addressof(module.imported_table_vec_storage.index_unchecked(index)); }
                                    else { return ::std::addressof(module.imported_global_vec_storage.index_unchecked(index)); } }()};
            for(::std::size_t hop{}; hop != state.records; ++hop)
            {
                if(!record_owner<true, Table>(state, next, owner, index)) { return false; }
                using imported = ::std::remove_cvref_t<decltype(*next)>;
                if constexpr(Table)
                {
                    using kind = typename imported::imported_table_link_kind;
                    if(next->link_kind == kind::defined) { return record_owner<false, true>(state, next->target.defined_ptr, owner, index); }
                    if(next->link_kind != kind::imported) { return false; }
                }
                else
                {
                    using kind = typename imported::imported_global_link_kind;
                    if(next->link_kind == kind::defined) { return record_owner<false, false>(state, next->target.defined_ptr, owner, index); }
                    if(next->link_kind != kind::imported) { return false; } // Actual host backend requires its own real borrow producer.
                }
                next = next->target.imported_ptr;
            }
            return false; // Cyclic/imported chain cannot exceed ALL actual records.
        }
        [[nodiscard]] static bool copy_global(work& state, ::std::size_t owner, ::std::size_t index, copied_value& value, native_value* carrier = nullptr)
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            if(!resolve_resource<false>(state, owner, index) || !declaration_origin(state.modules[owner]))
            { state.copy_error = copy_status::unavailable_foreign_host_state; return false; }
            auto const& module{*state.modules[owner].module};
            if(index >= module.local_defined_global_vec_storage.size()) { return false; }
            auto const& global{module.local_defined_global_vec_storage.index_unchecked(index)};
            auto const type{::uwvm2::parser::wasm::standard::wasm1p1::features::core3_initializer_type_details::declared_value_type(*global.global_type_ptr)};
            object_value native{};
            using kind = ::uwvm2::object::global::global_type;
            switch(type.kind)
            {
                case core_kind::i32:
                    if(global.global.kind != kind::wasm_i32) { return false; }
                    ::std::memcpy(native.bits.data(), ::std::addressof(global.global.storage.i32), 4u); break;
                case core_kind::i64:
                    if(global.global.kind != kind::wasm_i64) { return false; }
                    ::std::memcpy(native.bits.data(), ::std::addressof(global.global.storage.i64), 8u); break;
                case core_kind::f32:
                    if(global.global.kind != kind::wasm_f32) { return false; }
                    ::std::memcpy(native.bits.data(), ::std::addressof(global.global.storage.f32), 4u); break;
                case core_kind::f64:
                    if(global.global.kind != kind::wasm_f64) { return false; }
                    ::std::memcpy(native.bits.data(), ::std::addressof(global.global.storage.f64), 8u); break;
                case core_kind::v128:
                    if(global.global.kind != kind::wasm_v128) { return false; }
                    ::std::memcpy(native.bits.data(), ::std::addressof(global.global.storage.v128), 16u); break;
                case core_kind::reference:
                    if(global.global.kind != kind::wasm_ref || global.global.ref_lease_store != module.gc_store.get()) { return false; }
                    ::std::memcpy(native.bits.data(), ::std::addressof(global.global.storage.ref), sizeof(reference)); break;
            }
            return copy_native(state, owner, type, native.bits.data(), true, value, carrier);
        }
        [[nodiscard]] static bool copy_table(work& state, ::std::size_t owner, ::std::size_t table,
            ::std::uint64_t first, ::std::uint64_t count, native_value* carrier = nullptr)
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            if(!resolve_resource<true>(state, owner, table) || !declaration_origin(state.modules[owner]))
            { state.copy_error = copy_status::unavailable_foreign_host_state; return false; }
            auto const& module{*state.modules[owner].module};
            if(table >= module.local_defined_table_vec_storage.size()) { return false; }
            auto const& actual{module.local_defined_table_vec_storage.index_unchecked(table)};
            auto const& declaration{*actual.table_type_ptr};
            core_type type{}; type.kind = core_kind::reference; type.nullable = true;
            if(declaration.has_core_type) { type = declaration.core_type; }
            else if(static_cast<unsigned>(declaration.reftype) == 0x70u) { type.heap.code = static_cast<::std::int_least64_t>(heap_kind::func); }
            else if(static_cast<unsigned>(declaration.reftype) == 0x6fu) { type.heap.code = static_cast<::std::int_least64_t>(heap_kind::extern_); }
            else { return false; }
            state.output.total_values = actual.elems.size();
            if(first > actual.elems.size()) { state.copy_error = copy_status::out_of_range; return false; }
            auto const available{actual.elems.size() - static_cast<::std::size_t>(first)};
            auto const shown{count < available ? count : available};
            for(::std::size_t i{}; i != shown; ++i)
            {
                auto const offset{static_cast<::std::size_t>(first) + i};
                // [live actual table slots ... first+i < size] end
                // [safe] page subtraction BEFORE addition/index, no native pointer input.
                auto const& slot{actual.elems.index_unchecked(offset)};
                using kind = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
                if(slot.type != kind::func_ref_imported && slot.type != kind::func_ref_defined && slot.type != kind::extern_ref &&
                   slot.type != kind::exn_ref && slot.type != kind::gc_i31_ref && slot.type != kind::gc_struct_ref && slot.type != kind::gc_array_ref) { return false; }
                auto const reference{::uwvm2::uwvm::runtime::storage::runtime_table_slot_to_gc_reference(slot)};
                copied_value value{}; object_value native{};
                ::std::memcpy(native.bits.data(), ::std::addressof(reference), sizeof(reference));
                if(!copy_native(state, owner, type, native.bits.data(), true, value, carrier)) { return false; }
                state.output.rows.push_back({offset, ::std::move(value)});
            }
            state.output.rows_truncated = shown != available; return true;
        }
        [[nodiscard]] view copy_selected(selection const& requested) const noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            view failure{}; failure.requested = ws::unavailable_request(requested);
            if(!ws::valid(requested)) { failure.result = copy_status::invalid_selection; return failure; }
            if(!current_scope()) { failure.result = copy_status::stale_stop_or_generation; return failure; }
            try
            {
                // Strong actual module/store/source pins are declared BEFORE
                // cohort guard: guard unlocks before these owners can destruct.
                work state{}; state.output.requested = requested; state.output.runtime_epoch = epoch_;
                store_type::cohort_guard stores{};
                if(!complete_modules(state, profile_, epoch_)) { failure.result = state.copy_error; return failure; }
                bool const frame_selection{ws::frame_selection(requested.selected)};
                if(!frame_selection && requested.module >= state.modules.size())
                { failure.result = copy_status::out_of_range; return failure; }
                capture const* actual{};
                for(auto const& owner : captures_) { if(owner->participant_ == requested.participant) { actual = owner.get(); break; } }
                if(actual == nullptr) { failure.result = copy_status::missing_participant; return failure; }
                state.output.module = requested.module; state.output.first = requested.first;
                state.output.rows.reserve(static_cast<::std::size_t>(requested.count));
                state.pending.reserve(ws::maximum_objects); state.output.objects.reserve(ws::maximum_objects);
                auto owner{static_cast<::std::size_t>(requested.module)};
                if(frame_selection)
                {
                    if(requested.frame >= actual->frames_.size()) { failure.result = copy_status::out_of_range; return failure; }
                    // Frame ordinal0 denotes physical top (same command meaning
                    // as current frames/activation routing), not bottom/caller0.
                    auto const ordinal{actual->frames_.size() - 1u - static_cast<::std::size_t>(requested.frame)};
                    auto const& saved{actual->frames_[ordinal]}; auto const& frame{saved.logical};
                    if(saved.activation.module >= state.modules.size() || capture::check_owned_values(frame,
                        capture::value_use::live_observation) != checkpoint_thread_capture_status::captured)
                    { failure.result = copy_status::unavailable_typed_site; return failure; }
                    // Language frame commands carry THREAD/FRAME, not a
                    // caller-supplied type-module authority. Resolve the real
                    // frame's previously authenticated canonical module and
                    // report that exact label separately from the request.
                    owner = static_cast<::std::size_t>(saved.activation.module);
                    state.output.module = saved.activation.module;
                    auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(frame.site - 1u)]};
                    if(site.local_count > frame.values.size() || site.operand_count > frame.values.size() - site.local_count ||
                       site.saved_parameter_count != frame.values.size() - site.local_count - site.operand_count)
                    { failure.result = copy_status::invalid_data; return failure; }
                    auto const start{requested.selected == ws::selection::locals ? 0u : requested.selected == ws::selection::saved_parameters ?
                        static_cast<::std::size_t>(site.local_count+site.operand_count) : static_cast<::std::size_t>(site.local_count)};
                    // The exact authenticated packet partition above proves
                    // local+operand BEFORE forming the saved suffix offset.
                    // Saved if parameters remain separate from current operands.
                    auto total{requested.selected == ws::selection::locals ? static_cast<::std::size_t>(site.local_count) :
                        requested.selected == ws::selection::saved_parameters ? site.saved_parameter_count : site.operand_count};
                    ::std::vector<core_type> const* tuple{};
                    if(requested.selected == ws::selection::controls) { total=site.controls.size(); }
                    else if(requested.selected == ws::selection::handlers) { total=site.handlers.size(); }
                    else if(ws::declaration_selection(requested.selected))
                    {
                        if(requested.selected == ws::selection::handler_parameters)
                        {
                            if(requested.index >= site.handlers.size()) { failure.result=copy_status::out_of_range; return failure; }
                            // [actual sealed handler vector0 ... index ... N] end
                            // [safe] complete actual ordinal BEFORE tuple borrow.
                            tuple=::std::addressof(site.handlers[static_cast<::std::size_t>(requested.index)].parameters);
                        }
                        else
                        {
                            if(requested.index >= site.controls.size()) { failure.result=copy_status::out_of_range; return failure; }
                            // [actual sealed control vector0 ... index ... N] end
                            // [safe] complete actual ordinal BEFORE tuple borrow.
                            auto const& control{site.controls[static_cast<::std::size_t>(requested.index)]};
                            tuple=::std::addressof(requested.selected == ws::selection::control_parameters ?
                                control.declared_parameters : control.declared_results);
                        }
                        total=tuple->size();
                    }
                    state.output.total_values = total;
                    if(requested.first > total) { failure.result = copy_status::out_of_range; return failure; }
                    auto const remaining{total - static_cast<::std::size_t>(requested.first)};
                    auto const count{requested.count < remaining ? requested.count : remaining};
                    for(::std::size_t i{}; i != count; ++i)
                    {
                        auto const index{static_cast<::std::size_t>(requested.first) + i};
                        // [authenticated sealed site/packet ... first+i<total] end
                        // [safe] requested.first<=total, i<count<=remaining
                        // BEFORE any complete cell read. No view retains a native
                        // plan/value address or executable continuation capability.
                        if(requested.selected == ws::selection::controls)
                        {
                            auto const& source{site.controls[index]};
                            ws::control copied{}; copied.index=index;
                            switch(source.kind)
                            {
                                case ::uwvm2::runtime::checkpoint::control_kind::function: copied.kind=ws::control_kind::function; break;
                                case ::uwvm2::runtime::checkpoint::control_kind::block: copied.kind=ws::control_kind::block; break;
                                case ::uwvm2::runtime::checkpoint::control_kind::loop: copied.kind=ws::control_kind::loop; break;
                                case ::uwvm2::runtime::checkpoint::control_kind::if_then: copied.kind=ws::control_kind::if_then; break;
                                case ::uwvm2::runtime::checkpoint::control_kind::if_else: copied.kind=ws::control_kind::if_else; break;
                            }
                            copied.entry_offset=source.entry_offset; copied.end_offset=source.end_offset;
                            copied.outer_operand_height=source.outer_operand_height; copied.first_saved_parameter=source.first_saved_parameter;
                            copied.saved_parameter_count=source.saved_parameter_count; copied.parameter_count=source.declared_parameters.size();
                            copied.result_count=source.declared_results.size(); state.output.controls.push_back(copied);
                        }
                        else if(requested.selected == ws::selection::handlers)
                        {
                            auto const& source{site.handlers[index]};
                            state.output.handlers.push_back({index,source.tag_index,source.target_control,source.target_offset,
                                source.parameters.size(),source.catch_all,source.with_reference});
                        }
                        else if(tuple != nullptr)
                        {
                            auto type{declared_value((*tuple)[index]).type};
                            if(type.kind == ws::value_kind::reference && type.heap >= 0) { type.type_module=state.modules[owner].id; }
                            if(!ws::details::valid_declaration(type)) { failure.result=copy_status::invalid_data; return failure; }
                            state.output.declarations.push_back({index,type});
                        }
                        else
                        {
                            // Exact full packet partition implies start+total
                            // fits frame.values BEFORE this bounded cell borrow.
                            auto const& native{frame.values[start + index]}; copied_value value{};
                            if(!copy_native(state, owner, native.declaration.type, native.bits.data(), native.declaration.initialized, value))
                            { failure.result = state.copy_error; return failure; }
                            state.output.rows.push_back({index, ::std::move(value)});
                        }
                    }
                    state.output.rows_truncated = count != remaining;
                }
                else if(requested.selected == ws::selection::globals)
                {
                    auto const& module{*state.modules[owner].module};
                    auto const imports{module.imported_global_vec_storage.size()}, locals{module.local_defined_global_vec_storage.size()};
                    if(locals > record_limit - imports) { failure.result = copy_status::resource_limit; return failure; }
                    state.output.total_values = imports + locals;
                    if(requested.first > state.output.total_values) { failure.result = copy_status::out_of_range; return failure; }
                    auto const remaining{state.output.total_values - requested.first};
                    auto const count{requested.count < remaining ? requested.count : remaining};
                    for(::std::size_t i{}; i != count; ++i)
                    {
                        auto const index{static_cast<::std::size_t>(requested.first) + i}; copied_value value{};
                        if(!copy_global(state, owner, index, value)) { failure.result = state.copy_error; return failure; }
                        state.output.rows.push_back({index, ::std::move(value)});
                    }
                    state.output.rows_truncated = count != remaining;
                }
                else
                {
                    auto const& module{*state.modules[owner].module};
                    auto const imports{module.imported_table_vec_storage.size()}, locals{module.local_defined_table_vec_storage.size()};
                    if(locals > record_limit - imports || requested.index >= imports + locals)
                    { failure.result = copy_status::out_of_range; return failure; }
                    if(!copy_table(state, owner, static_cast<::std::size_t>(requested.index), requested.first, requested.count))
                    { failure.result = state.copy_error; return failure; }
                }
                if(!ws::metadata_selection(requested.selected) &&
                   !(requested.member_page() ? copy_member_page(state, requested) : copy_objects(state)))
                { failure.result = state.copy_error; return failure; }
                if(!current_scope()) { failure.result = copy_status::stale_stop_or_generation; return failure; }
                state.output.result = copy_status::available;
                if(!ws::valid(state.output)) { failure.result = copy_status::invalid_data; return failure; }
                // All output is bounded detached typed DATA. Native pending
                // carriers/value owners die here before N/publication can end.
                return ::std::move(state.output);
            }
            catch(...) { failure.result = copy_status::allocation_failed; return failure; }
        }
        // Cold portable instance DATA builders. Include within this private
        // class only after canonical native/source/type resolvers are defined.
        // Common work precedes every use; later inline members participate in
        // complete-class lookup. No constructor/restore authority is exported.
    private:
# include "uwvm_runtime_checkpoint_instance_census.h"
# include "uwvm_runtime_checkpoint_instance_census_resources.h"
# include "uwvm_runtime_checkpoint_instance_census_references.h"
# include "uwvm_runtime_checkpoint_instance_census_frames.h"
#include "uwvm_runtime_debug_wasm_mutation_borrow.h"
    };
}
#endif
