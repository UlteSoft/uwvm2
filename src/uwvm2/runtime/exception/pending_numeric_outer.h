// PRIVATE validated-LLVM experiment. Production compilation does not import it.
// These native objects are deliberately outside the Wasm linear-memory ABI.
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/runtime/exception/pending_numeric_bridge.h>
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#include <uwvm2/uwvm/runtime/storage/full.h>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <limits>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception::pending_experiment::numeric_entry
{
    using word = ::std::uintptr_t;

    // A PRIVILEGED native generation type, not a guest or public VM handle.
    // The canonical check must compare the generation object's own private
    // weak publication owner to this exact typed shared owner. The accessor
    // must return ONLY the actual parser/initializer module retained by that
    // generation (or null before initialization), with its real native module
    // ID. Neither a marker, this requires-expression, nor shared_ptr<void>
    // authenticates an arbitrary host implementation of these operations.
    //
    // HOST CONTRACT: after publication there are no mutable generation/module/
    // source aliases or concurrent initializer/reset operations. The generation
    // owns the initialized module/source lifetime, and tears it down only after
    // engines, calls, plans, registries and any native entry borrowers drain.
    // Copying a pin alone cannot enforce global module-storage immutability.
    template<typename Generation>
    concept initialized_native_generation = requires(::std::shared_ptr<Generation const> const& owner,
        Generation const& generation)
    {
        { Generation::pending_numeric_has_canonical_owner(owner) } noexcept -> ::std::same_as<bool>;
        { generation.pending_numeric_initialized_module() } noexcept ->
            ::std::same_as<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*>;
        { generation.pending_numeric_module_id() } noexcept -> ::std::same_as<::std::size_t>;
    };

    template<initialized_native_generation Generation> class native_cold_publisher;

    class admitted_plan;

    // Compiler-only immutable declaration shape. This owns the genuine source
    // without any full-validation proof, registry or executable authority.
    // Private IR may use it during fused validation/emission; engine binding
    // still accepts only the separately published admitted_plan.
    class compile_plan
    {
        instance_root const compile_generation_pin_;
        ::uwvm2::uwvm::runtime::full::full_source_instance::owner const compile_source_pin_;
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> const compile_store_pin_;
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> const compile_lease_pin_;
        diagnostic_symbols_ref const compile_symbols_pin_;
        friend class admitted_plan;
        template<initialized_native_generation Generation> friend class native_cold_publisher;

        compile_plan(instance_root generation,
            ::uwvm2::uwvm::runtime::full::full_source_instance::owner source,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> store,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> leases,
            diagnostic_symbols_ref symbols,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module,
            ::std::size_t actual_module_id, ::std::size_t actual_function_count) noexcept
            : compile_generation_pin_{::std::move(generation)}, compile_source_pin_{::std::move(source)},
              compile_store_pin_{::std::move(store)}, compile_lease_pin_{::std::move(leases)},
              compile_symbols_pin_{::std::move(symbols)}, module{actual_module}, module_id{actual_module_id},
              function_count{actual_function_count} {}
    public:
        using owner = ::std::shared_ptr<compile_plan const>;
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* const module;
        ::std::size_t const module_id;
        ::std::size_t const function_count;
        compile_plan(compile_plan const&) = delete;
        compile_plan& operator=(compile_plan const&) = delete;
        compile_plan(compile_plan&&) = delete;
        compile_plan& operator=(compile_plan&&) = delete;
        ~compile_plan() = default;

        // Lifetime and declaration consistency only: this never proves that
        // bodies were validated and cannot admit an owner or executable entry.
        [[nodiscard]] bool shape_valid_for_compilation() const noexcept
        {
            return module != nullptr && compile_generation_pin_ &&
                compile_store_pin_ && compile_store_pin_.get() == module->gc_store.get() &&
                !compile_store_pin_.owner_before(module->gc_store) && !module->gc_store.owner_before(compile_store_pin_) &&
                compile_lease_pin_.get() == module->gc_lease_roots.get() &&
                !compile_lease_pin_.owner_before(module->gc_lease_roots) && !module->gc_lease_roots.owner_before(compile_lease_pin_) &&
                diagnostic_symbols::has_canonical_owner(compile_symbols_pin_) &&
                function_count != 0uz && function_count == module->local_defined_function_vec_storage.size() &&
                compile_symbols_pin_->contains({module_id, function_count - 1uz}) &&
                !compile_symbols_pin_->contains({module_id, function_count}) &&
                module->imported_function_vec_storage.empty() && module->imported_tag_vec_storage.empty() &&
                !module->local_defined_tag_vec_storage.empty() &&
                (!compile_source_pin_ ||
                 (::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(compile_source_pin_) &&
                  compile_source_pin_->initialized_main_module() == module &&
                  compile_source_pin_->bound_initialized_main_module_id() == module_id));
        }
    };

    // No aggregate, public constructor or mutable registry/schema input. ONLY
    // the cold publisher below can build this plan from the genuine module.
    // These const read fields preserve the private compiler's existing API;
    // compiler admission must still validate actual bodies and matching IDs.
    class admitted_plan final : public compile_plan, public ::uwvm2::uwvm::runtime::full::numeric_plan_lifetime
    {
        // Declared first, released last: module/source generation teardown must
        // not occur while registry/tag/symbol/actual-store fields are retiring.
        instance_root const generation_pin_;
        // Product publication has a concrete, canonical owning source pin in
        // addition to generic native-fixture generation erasure. No raw module
        // or shared_ptr<void> becomes its authentication/lifetime authority.
        ::uwvm2::uwvm::runtime::full::full_source_instance::owner const product_source_pin_;
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> const store_pin_;
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> const lease_pin_;
        ::std::weak_ptr<admitted_plan const> canonical_owner_{};
        template<initialized_native_generation Generation> friend class native_cold_publisher;

        admitted_plan(instance_root generation,
            ::uwvm2::uwvm::runtime::full::full_source_instance::owner product_source,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> store,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_lease_owner> leases,
            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* actual_module,
            prepared_registry::owner prepared, diagnostic_symbols_ref owned_symbols,
            ::std::size_t actual_module_id, ::std::size_t actual_function_count) noexcept
            : compile_plan{generation, product_source, store, leases, owned_symbols,
                           actual_module, actual_module_id, actual_function_count},
              generation_pin_{::std::move(generation)}, product_source_pin_{::std::move(product_source)}, store_pin_{::std::move(store)},
              lease_pin_{::std::move(leases)}, registry{::std::move(prepared)}, symbols{::std::move(owned_symbols)} {}
    public:
        using owner = ::std::shared_ptr<admitted_plan const>;
        prepared_registry::owner const registry;
        diagnostic_symbols_ref const symbols;
        admitted_plan(admitted_plan const&) = delete;
        admitted_plan& operator=(admitted_plan const&) = delete;
        admitted_plan(admitted_plan&&) = delete;
        admitted_plan& operator=(admitted_plan&&) = delete;
        ~admitted_plan() = default;

        [[nodiscard]] ::uwvm2::uwvm::runtime::full::full_source_instance::owner const&
            product_full_source_owner_pin() const noexcept { return product_source_pin_; }

        [[nodiscard]] static bool canonical(owner const& candidate) noexcept
        {
            return candidate && !candidate->canonical_owner_.owner_before(candidate) &&
                !candidate.owner_before(candidate->canonical_owner_);
        }

        // Lightweight immutable shape/lifetime-consistency check. This never
        // authenticates caller-supplied schemas, source bytes, native addresses,
        // the generation's privileged implementation, or a complete VM cohort.
        [[nodiscard]] bool valid() const noexcept
        {
            return module != nullptr && generation_pin_ &&
                store_pin_ && store_pin_.get() == module->gc_store.get() &&
                !store_pin_.owner_before(module->gc_store) && !module->gc_store.owner_before(store_pin_) &&
                lease_pin_.get() == module->gc_lease_roots.get() &&
                !lease_pin_.owner_before(module->gc_lease_roots) && !module->gc_lease_roots.owner_before(lease_pin_) &&
                prepared_registry::canonical(registry) &&
                diagnostic_symbols::has_canonical_owner(symbols) &&
                function_count != 0uz && function_count == module->local_defined_function_vec_storage.size() &&
                symbols->contains({module_id, function_count - 1uz}) &&
                !symbols->contains({module_id, function_count}) &&
                module->imported_function_vec_storage.empty() &&
                module->imported_tag_vec_storage.empty() &&
                !module->local_defined_tag_vec_storage.empty();
        }
    };

    template<initialized_native_generation Generation>
    class native_cold_publisher final
    {
    public:
        native_cold_publisher() = delete;

        // Compiler-only preparation from real initialized/bound source metadata.
        // No code body is read and no actual-full-validation state is changed.
        // The returned type cannot enter entry_owner or engine admission APIs.
        [[nodiscard]] static compile_plan::owner prepare_compile_plan(
            ::std::shared_ptr<Generation const> const& generation, diagnostic_symbols_ref symbols)
            requires(::std::same_as<Generation, ::uwvm2::uwvm::runtime::full::full_source_instance>)
        {
            if(!generation || !Generation::has_canonical_owner(generation) ||
               !diagnostic_symbols::has_canonical_owner(symbols)) { return {}; }
            // [canonical source owns its actual initialized main module]
            // [safe                                                     ] null
            // is checked before dereference; the borrow never moves a cursor.
            auto const* module{generation->initialized_main_module()};
            auto const module_id{generation->bound_initialized_main_module_id()};
            if(module == nullptr || module_id == (::std::numeric_limits<::std::size_t>::max)() ||
               !module->imported_function_vec_storage.empty() || !module->imported_table_vec_storage.empty() ||
               !module->imported_memory_vec_storage.empty() || !module->imported_global_vec_storage.empty() ||
               !module->imported_tag_vec_storage.empty() || !module->gc_store || !module->gc_store->valid())
            { return {}; }
            auto const function_count{module->local_defined_function_vec_storage.size()};
            auto const tag_count{module->local_defined_tag_vec_storage.size()};
            if(function_count == 0uz || tag_count == 0uz || tag_count > max_tags ||
               !symbols->contains({module_id, function_count - 1uz}) || symbols->contains({module_id, function_count}))
            { return {}; }
            auto const canonical_store{module->gc_store->weak_from_this()};
            if(canonical_store.owner_before(module->gc_store) || module->gc_store.owner_before(canonical_store) ||
               (!module->gc_lease_roots && module->gc_lease_roots.use_count() != 0)) { return {}; }
            for(auto const& global: module->local_defined_global_vec_storage)
            {
                // [actual global record][parser-retained descriptor]
                // [safe                                           ] check
                // the descriptor borrow before reading; no pointer advances.
                if(global.global_type_ptr == nullptr) { return {}; }
                switch(static_cast<unsigned>(global.global_type_ptr->type))
                {
                    case 0x7fu: case 0x7eu: case 0x7du: case 0x7cu: case 0x7bu: break;
                    default: return {};
                }
            }
            for(::std::size_t index{}; index != tag_count; ++index)
            {
                // [actual local tags, tag_count records][index < tag_count]
                // [safe                                                  ]
                // index_unchecked borrows a live record; no cursor is moved.
                auto const& tag{module->local_defined_tag_vec_storage.index_unchecked(index)};
                if(!tag.exception_identity || tag.exception_identity.use_count() == 0 ||
                   tag.function_type_ptr == nullptr ||
                   tag.function_type_ptr->result.begin != tag.function_type_ptr->result.end) { return {}; }
                auto const& parameters{tag.function_type_ptr->parameter};
                if(parameters.begin != parameters.end && (parameters.begin == nullptr || parameters.end == nullptr)) { return {}; }
                // [begin ... end) belongs to one retained signature allocation.
                // [safe         ] empty endpoints are never subtracted.
                // ^^ parameters.begin is borrowed, never advanced or modified.
                auto const count{parameters.begin == parameters.end ? 0uz :
                    static_cast<::std::size_t>(parameters.end - parameters.begin)};
                if(count > max_payload_fields) { return {}; }
                for(::std::size_t field{}; field != count; ++field)
                {
                    // [parameter records, count][field < count]
                    // [safe                                   ] no cursor moves.
                    // ^^ parameters.begin[field] reads one live metadata value.
                    switch(static_cast<unsigned>(parameters.begin[field]))
                    {
                        case 0x7fu: case 0x7eu: case 0x7du: case 0x7cu: case 0x7bu: break;
                        default: return {};
                    }
                }
                for(::std::size_t prior{}; prior != index; ++prior)
                {
                    // [actual local tags][prior < index < tag_count]
                    // [safe                                       ] metadata
                    // borrow only, with no pointer arithmetic or advancement.
                    if(module->local_defined_tag_vec_storage.index_unchecked(prior).exception_identity.get() ==
                       tag.exception_identity.get()) { return {}; }
                }
            }
            auto candidate{::std::shared_ptr<compile_plan>{new compile_plan{
                generation, generation, module->gc_store, module->gc_lease_roots,
                ::std::move(symbols), module, module_id, function_count}}};
            if(!candidate->shape_valid_for_compilation()) { return {}; }
            return compile_plan::owner{::std::move(candidate)};
        }

        // COLD, MAY ALLOCATE/THROW. No externally prepared registry, parameter
        // span, tag identity, raw module pointer, void pin, module ID or function
        // count can enter this factory. All are derived from one canonical typed
        // generation. Invalid inputs return null before any publication; native
        // allocation failures propagate normally with RAII retirement.
        //
        // Cold work: two bounded vectors copy up to max_tags * max_payload_fields
        // numeric kinds and actual tag owners; prepare allocates registry table/
        // control block and copies schema/pins; publication allocates plan + its
        // shared control block. shared copies/weak publication occur ONLY here,
        // not as new normal-call/throw work in the six numeric leaf helpers.
        [[nodiscard]] static admitted_plan::owner publish_from_initialized(
            ::std::shared_ptr<Generation const> const& generation, diagnostic_symbols_ref symbols)
        {
            if(!generation || generation.use_count() == 0 ||
                !Generation::pending_numeric_has_canonical_owner(generation) ||
                !diagnostic_symbols::has_canonical_owner(symbols)) { return {}; }
            auto retained_generation{generation};
            // [canonical typed native generation][its actual initialized module]
            // [safe                                                            ]
            // The accessor may return null; it never consumes a caller-selected
            // module pointer. Its genuine lifetime is the HOST contract above.
            auto const* module{retained_generation->pending_numeric_initialized_module()};
            auto const module_id{retained_generation->pending_numeric_module_id()};
            if(module == nullptr || !module->imported_function_vec_storage.empty() ||
                !module->imported_tag_vec_storage.empty() || !module->gc_store ||
                module->gc_store.use_count() == 0 || !module->gc_store->valid()) { return {}; }
            auto const function_count{module->local_defined_function_vec_storage.size()};
            auto const tag_count{module->local_defined_tag_vec_storage.size()};
            if(function_count == 0uz || tag_count == 0uz || tag_count > max_tags ||
                !symbols->contains({module_id, function_count - 1uz}) ||
                symbols->contains({module_id, function_count})) { return {}; }
            // A numeric global uses the ordinary validated global lowering;
            // it does not introduce a reference payload/root or an imported
            // callback into this closed private execution island. Decline ALL
            // imported globals and reference local globals before code
            // generation. The same genuine generation retains their storage.
            if(!module->imported_global_vec_storage.empty()) { return {}; }
            for(auto const& global: module->local_defined_global_vec_storage)
            {
                // [genuine initialized local-global array][live type descriptor]
                // [safe                                                    ]
                // The immutable native generation owns this parser metadata;
                // no guest or caller-selected descriptor pointer is consumed.
                if(global.global_type_ptr == nullptr) { return {}; }
                switch(static_cast<unsigned>(global.global_type_ptr->type))
                {
                    case 0x7fu: case 0x7eu: case 0x7du: case 0x7cu: case 0x7bu: break;
                    default: return {}; // No GC/reference global in this cohort.
                }
            }
            // Genuine initializer ownership is still a native generation
            // contract. This cold control-block comparison additionally rejects
            // an alien/nonowning store alias; it does not promote a weak owner.
            auto const canonical_store{module->gc_store->weak_from_this()};
            if(canonical_store.owner_before(module->gc_store) ||
                module->gc_store.owner_before(canonical_store) ||
                (!module->gc_lease_roots && module->gc_lease_roots.use_count() != 0)) { return {}; }

            ::std::vector<::std::array<payload_kind, max_payload_fields>> signatures(tag_count);
            ::std::vector<admitted_tag> input(tag_count);
            for(::std::size_t index{}; index != tag_count; ++index)
            {
                // [actual initialized tag array: tag_count][index < count]
                // [safe                                                   ]
                // The canonical generation keeps this immutable module live.
                auto const& actual{module->local_defined_tag_vec_storage.index_unchecked(index)};
                if(!actual.exception_identity || actual.exception_identity.use_count() == 0 ||
                    actual.function_type_ptr == nullptr ||
                    actual.function_type_ptr->result.begin != actual.function_type_ptr->result.end) { return {}; }
                auto const& parameters{actual.function_type_ptr->parameter};
                if(parameters.begin != parameters.end &&
                    (parameters.begin == nullptr || parameters.end == nullptr)) { return {}; }
                // [genuine parser/initializer same-array signature endpoints]
                // [safe ] No forged signature span enters this API; nonempty
                // subtraction relies on that native metadata ownership contract.
                // Empty signatures avoid subtraction of null or one-past pointers.
                auto const count{parameters.begin == parameters.end ? 0uz :
                    static_cast<::std::size_t>(parameters.end - parameters.begin)};
                if(count > max_payload_fields) { return {}; }
                for(::std::size_t field{}; field != count; ++field)
                {
                    // [actual same-array parameters: count][field < count]
                    // [safe                                               ]
                    // Copy kinds without keeping a parser span in the registry.
                    switch(static_cast<unsigned>(parameters.begin[field]))
                    {
                        case 0x7fu: signatures[index][field] = payload_kind::i32; break;
                        case 0x7eu: signatures[index][field] = payload_kind::i64; break;
                        case 0x7du: signatures[index][field] = payload_kind::f32; break;
                        case 0x7cu: signatures[index][field] = payload_kind::f64; break;
                        case 0x7bu: signatures[index][field] = payload_kind::v128; break;
                        default: return {}; // No reference or unknown schema in this cohort.
                    }
                }
                // Distinct actual local tags are distinct instances even when
                // all parameter kinds match. Duplicate identities fail closed.
                for(::std::size_t prior{}; prior != index; ++prior)
                { if(input[prior].identity.get() == actual.exception_identity.get()) { return {}; } }
                input[index] = {actual.exception_identity, {signatures[index].data(), count}};
            }

            // Strong pins come from this SAME typed generation and actual module.
            // Erasure happens only after canonical typed admission; an arbitrary
            // shared_ptr<void> supplied by a caller is never a generation proof.
            ::std::array<instance_root, 3uz> pins{retained_generation, module->gc_store, module->gc_lease_roots};
            auto const pin_count{module->gc_lease_roots ? 3uz : 2uz};
            auto registry{prepared_registry::prepare(input, {pins.data(), pin_count})};
            if(!prepared_registry::canonical(registry)) { return {}; }
            ::uwvm2::uwvm::runtime::full::full_source_instance::owner product_source;
            if constexpr(::std::is_same_v<Generation, ::uwvm2::uwvm::runtime::full::full_source_instance>)
            {
                product_source = generation;
                if(!::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(product_source) ||
                   product_source->pending_numeric_initialized_module() != module ||
                   product_source->pending_numeric_module_id() != module_id)
                { return {}; }
            }
            auto result{::std::shared_ptr<admitted_plan>{new admitted_plan{retained_generation, ::std::move(product_source),
                module->gc_store, module->gc_lease_roots, module, ::std::move(registry),
                ::std::move(symbols), module_id, function_count}}};
            // The factory owns the ONLY mutable native plan alias. Bind its
            // control block BEFORE publishing a const owner, then retire it.
            result->canonical_owner_ = result;
            if(!result->valid()) { return {}; }
            return admitted_plan::owner{::std::move(result)};
        }
    };
    static_assert(!::std::is_aggregate_v<admitted_plan>);
    static_assert(!::std::is_default_constructible_v<admitted_plan>);
    static_assert(!::std::is_copy_constructible_v<admitted_plan>);
    static_assert(!::std::is_move_constructible_v<admitted_plan>);

    // One genuinely constructed native owner in the PUBLIC caller's stack.
    // Internal direct calls borrow only the owned R2 header BEFORE unchanged Wasm args.
    // This isolated first experiment has numeric payloads/locals/parameters only
    // and declines host/indirect/ref/grow/bulk/atomic/GC guest instructions. It does NOT
    // register arbitrary VM reentry or prove an automatic-GC root population.
    // Future ref-bearing/reentrant entries need a caller-owned shared chain or
    // an independently complete live-chain registrar. No TLS pending singleton.
    class entry_owner final
    {
        admitted_plan const& plan_;
        native_island_chain chain_{};
        pending_context context_;
        execution_island island_;
        numeric_guest_bridge::numeric_header header_;
    public:
        explicit entry_owner(admitted_plan const& plan) noexcept
            : plan_{plan}, context_{plan.registry}, island_{chain_, context_}, header_{context_} {}
        entry_owner(entry_owner const&) = delete;
        entry_owner& operator=(entry_owner const&) = delete;
        [[nodiscard]] bool admitted() const noexcept { return island_.admission() == status::ok && header_.activated_on_owner(); }
        [[nodiscard]] word hidden_header_address() const noexcept
        {
            // [live owner member numeric_header][native hidden argument]
            // [safe                                                   ] no
            // mutable slow-context accessor/retired borrow is published.
            return reinterpret_cast<word>(::std::addressof(header_));
        }

        // No work or allocation on normal completion. Only an exception escaping
        // the complete numeric island materializes the canonical value/trace and
        // enters ordinary C++ propagation at this public boundary. The wrapper
        // invokes this bridge and destroys this owner on BOTH native edges.
        void finish()
        {
            if(!admitted()) { ::std::terminate(); }
            if(!header_.pending_on_owner()) { return; }
            caught_payload caught{plan_.registry};
            caught_root_scope registered{island_, caught};
            if(registered.admission() != status::ok) { ::std::terminate(); }
            auto const materialized{header_.materialize_in_registered(caught,
                [&](::std::span<retired_frame const> retired, bool truncated)
                {
                    ::std::vector<diagnostic_frame_index> indices{};
                    indices.reserve(retired.size());
                    for(auto const frame: retired)
                    {
                        if(frame.module_id != plan_.module_id || frame.function_index >= plan_.function_count)
                        { ::std::terminate(); }
                        indices.push_back({frame.module_id, frame.function_index});
                    }
                    auto trace{diagnostic_trace::make_compact(plan_.symbols, ::std::move(indices), truncated)};
                    if(!trace) { ::std::terminate(); }
                    return trace;
                })};
            if(materialized != status::ok || !caught.observable()) { ::std::terminate(); }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
            // The thrown activation copies the REAL immutable value owner before
            // registered retires. No native caught/pending/context address escapes.
            throw guest_exception{caught.observable()};
#else
            ::std::terminate();
#endif
        }
    };

    inline constexpr char8_t construct_semantic[] = u8"uwvm2_pending_numeric_owner_construct_r2";
    inline constexpr char8_t destroy_semantic[] = u8"uwvm2_pending_numeric_owner_destroy_r2";
    inline constexpr char8_t finish_semantic[] = u8"uwvm2_pending_numeric_owner_finish_r2";
    extern "C"
    {
        [[nodiscard]] inline word uwvm2_pending_numeric_owner_construct_r2(word owner_address,
            word plan_address) noexcept
        {
            if(owner_address == 0u || owner_address % alignof(entry_owner) != 0u ||
               plan_address == 0u || plan_address % alignof(admitted_plan) != 0u) { return 0u; }
            // [HOST-owned immutable plan][complete pinned native plan extent]
            // [safe                                                        ]
            // The compiler binds this address to the plan's genuine owner;
            // neither argument is computed from a Wasm operand or guest memory.
            auto const* plan{reinterpret_cast<admitted_plan const*>(plan_address)};
            if(!plan->valid()) { return 0u; }
            // [public wrapper's complete aligned byte allocation][extent]
            // [safe                                                    ]
            // Start a real entry_owner lifetime before borrowing any members.
            auto* owner{::new(reinterpret_cast<void*>(owner_address)) entry_owner{*plan}};
            if(!owner->admitted()) { owner->~entry_owner(); return 0u; }
            // [live entry_owner][real activated numeric_header] within owner extent
            // [safe                                                              ]
            // Only internal typed cores receive this pointer-sized hidden value.
            return owner->hidden_header_address();
        }

        inline void uwvm2_pending_numeric_owner_destroy_r2(word owner_address) noexcept
        {
            // [previously constructed caller-owned entry_owner] complete extent
            // [safe                                                         ]
            // Exactly one normal/exceptional wrapper edge ends its lifetime.
            auto* owner{reinterpret_cast<entry_owner*>(owner_address)};
            owner->~entry_owner();
        }

        inline void uwvm2_pending_numeric_owner_finish_r2(word owner_address)
        {
            // [live genuine native entry_owner] complete caller stack extent
            // [safe                                                       ]
            // invoke cleanup owns retirement if materialization/native throw fails.
            auto* owner{reinterpret_cast<entry_owner*>(owner_address)};
            owner->finish();
        }
    }
    static_assert(::std::is_nothrow_constructible_v<entry_owner, admitted_plan const&>);
    static_assert(::std::is_nothrow_destructible_v<entry_owner>);
}
