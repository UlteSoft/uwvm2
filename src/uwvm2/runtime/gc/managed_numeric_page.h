#pragma once
// PRIVATE SOURCE candidate, experimental compile-time opt-in only.
// Capability owns an ACTUAL execution-generation lease, canonical store pin,
// registered pause participant and exclusive(1) coupled to the original shared
// admission lease. No active count, token, marked bit or TLS address grants it.
// Unknown native readers remain excluded by the existing CLI/loader contract.
#if defined(UWVM_MODULE)
# error "Managed numeric page experiment has no module-mode ABI qualification"
#endif
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
#include "sealed_entry_compact_protocol.h"
#endif
#include <uwvm2/runtime/gc/instance_phase.h>
#include <cstring>
#include <utility>
#include <uwvm2/runtime/gc/collection_transaction.h>
#include <uwvm2/utils/thread/execution_domain.h>
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>

namespace uwvm2::runtime::gc { class managed_collection_state; }
namespace uwvm2::uwvm::runtime::storage
{
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    enum class numeric_page_status : unsigned char
    {
        ok, use_original_route, unowned, wrong_thread, busy, unsupported,
        capacity, out_of_memory, size_overflow, epoch_exhausted, metadata_budget,
        pause_required, invalid_reference, invalid_ticket, terminal
    };
    enum class numeric_page_drain_reason : unsigned char { pause, foreign_exit, retirement, capacity };

    class managed_numeric_entry_page
    {
        friend class ::uwvm2::runtime::gc::managed_collection_state;
        using store_type = gc_object_store;
        using module_type = wasm_module_storage_t;
        using generation_lease = ::uwvm2::utils::thread::execution_domain::lease;
        using object = store_type::object;
        using chunk_type = store_type::slab_chunk;
        using admission = ::uwvm2::runtime::gc::managed_entry_admission;
        using domain_type = ::uwvm2::utils::thread::collection_pause_domain;
        using root_context = ::uwvm2::runtime::gc::collection_root_context;
        enum class phase : unsigned char { empty, owned, drained, retired };
        inline static ::std::atomic<::std::uint64_t> next_owner_cookie_{1u};
        inline static thread_local ::std::uint64_t calling_owner_cookie_{};
        ::std::uint64_t owner_cookie_{};
        ::std::shared_ptr<store_type> store_pin_{};
        ::std::optional<admission::exclusive_lease> entry_exclusion_{};
        // Strong canonical CLOSED one-store cohort plus generation admission.
        // The actual entry owns shared_admission_ and outlives this member.
        generation_lease execution_generation_{};
        admission::shared_lease const* shared_admission_{};
        void const* entry_identity_{};
        void const* collection_identity_{};
        module_type const* module_{};
        ::std::uint_least64_t initializer_serial_{};
        ::std::shared_ptr<domain_type> domain_pin_{};
        domain_type* domain_{};
        domain_type::participant participant_owner_{};
        domain_type::participant const* participant_{};
        root_context context_owner_{};
        root_context* context_{};
        ::std::size_t backing_budget_{};
        bool retired_{};
        ::std::size_t refills_{}, drains_{}, page_allocations_{};
        chunk_type* chunk_{};
        ::std::uint64_t epoch_{}, generation_{};
        ::std::uintptr_t token_begin_{};
        ::std::uint_least32_t type_index_{};
        ::std::size_t issued_{};
        ::std::array<::std::atomic<object*>, 256uz> entries_{};
        phase phase_{phase::empty};

        [[nodiscard]] static ::std::uint64_t cold_current_owner() noexcept
        {
            if(calling_owner_cookie_ != 0u) { return calling_owner_cookie_; }
            auto next{next_owner_cookie_.load(::std::memory_order_relaxed)};
            do
            {
                if(next == 0u || next == (::std::numeric_limits<::std::uint64_t>::max)()) { return 0u; }
            }
            while(!next_owner_cookie_.compare_exchange_weak(next, next + 1u,
                ::std::memory_order_relaxed, ::std::memory_order_relaxed));
            calling_owner_cookie_ = next;
            return next;
        }
        [[nodiscard]] bool same_native_owner() const noexcept
        { return owner_cookie_ != 0u && calling_owner_cookie_ == owner_cookie_; }
        [[nodiscard]] static bool eligible(store_type const& store, ::std::uint_least32_t index) noexcept
        {
            auto const* layout{store.checked_type(index, gc_type::composite_kind::struct_)};
            if(layout == nullptr || layout->field_count != 1uz || !layout->fields) { return false; }
            auto const& field{layout->fields[0uz]};
            return !field.mutable_ && field.storage.packed == gc_type::packed_kind::none &&
                (field.storage.value.kind == gc_type::value_kind::i32 ||
                 field.storage.value.kind == gc_type::value_kind::f32);
        }
        [[nodiscard]] static bool reserve_fresh_token_page(::std::uintptr_t& begin) noexcept
        {
            constexpr ::std::uintptr_t width{256u};
            constexpr auto last{(::std::numeric_limits<::std::uintptr_t>::max)()};
            auto first{store_type::next_object_token_id_.load(::std::memory_order_relaxed)};
            do
            {
                // Same issuer as all original 1024-token TLS blocks; disjoint
                // reserved ranges, no second token namespace or recycled ID.
                if(first < store_type::object_token_first || first % width != 0u || first > last - width)
                { return false; }
            }
            while(!store_type::next_object_token_id_.compare_exchange_weak(first, first + width,
                ::std::memory_order_relaxed, ::std::memory_order_relaxed));
            begin = first; return true;
        }
        [[nodiscard]] numeric_page_status cold_refill(::std::size_t store_backing_budget) noexcept
        {
            auto& store{*store_pin_};
            store_type::slab_guard guard{store};
            if(store.slab_closing_ || store.slab_reserved_slots_ != 0uz)
            { return numeric_page_status::busy; }
            if(store.slab_epoch_ == (::std::numeric_limits<::std::uint64_t>::max)() ||
               store.next_slab_generation_ >= store_type::slab_generation_limit)
            { return numeric_page_status::epoch_exhausted; }
            constexpr auto stride{(store_type::object_value_offset + sizeof(gc_object_value) +
                store_type::allocation_alignment - 1uz) / store_type::allocation_alignment * store_type::allocation_alignment};
            constexpr auto maximum{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
            static_assert(stride <= (maximum - store_type::chunk_slots_offset) / page_slots);
            constexpr auto bytes{store_type::chunk_slots_offset + page_slots * stride};
            if(store.slab_chunk_count_ == (::std::numeric_limits<::std::size_t>::max)() / page_slots ||
               store.slab_reserved_slots_ > (::std::numeric_limits<::std::size_t>::max)() - page_slots ||
               bytes > (::std::numeric_limits<::std::size_t>::max)() - store.slab_backing_bytes_)
            { return numeric_page_status::size_overflow; }
            if(bytes > store_backing_budget || store.slab_backing_bytes_ > store_backing_budget - bytes)
            { return numeric_page_status::metadata_budget; }
            ::std::uintptr_t tokens{};
            if(!reserve_fresh_token_page(tokens)) { return numeric_page_status::size_overflow; }
            // On OOM this range is burned, never authenticated or reused.
            auto* backing{new (::std::nothrow) ::std::byte[bytes]};
            if(backing == nullptr) { return numeric_page_status::out_of_memory; }
            auto* chunk{::new(static_cast<void*>(backing)) chunk_type{}};
            chunk->owner = ::std::addressof(store); chunk->backing = backing;
            chunk->bytes = bytes; chunk->stride = stride; chunk->class_index = 0uz;
            chunk->generation = store.next_slab_generation_++;
            // Real original byte array -> checked slot -> live envelope/union.
            // No object/header address is derived from the opaque guest token.
            for(::std::size_t slot{}; slot != page_slots; ++slot)
            {
                auto* address{store_type::slab_slot_bytes(*chunk, slot)};
                auto* envelope{::new(static_cast<void*>(address)) store_type::allocation_header{}};
                envelope->metadata.owner_or_storage = chunk;
                store_type::set_active_metadata(envelope->metadata, chunk->generation, slot,
                    store_type::slab_slot_state::reserved);
                ++chunk->reserved_slots;
            }
            chunk->all_next = store.slab_all_[0uz];
            if(chunk->all_next != nullptr) { chunk->all_next->all_previous = chunk; }
            store.slab_all_[0uz] = chunk;
            ++store.slab_chunk_count_; store.slab_backing_bytes_ += bytes;
            store.slab_reserved_slots_ += page_slots;
            // All 256 slots are reserved to this actual native owner. The
            // shared available list cannot expose/free them before drain.
            chunk_ = chunk; epoch_ = store.slab_epoch_; generation_ = chunk->generation;
            token_begin_ = tokens; issued_ = 0uz; phase_ = phase::owned;
            return numeric_page_status::ok;
        }
        [[nodiscard]] object* checked_page_object(gc_reference reference) const noexcept
        {
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr || chunk_ == nullptr) { return nullptr; }
            auto const token{reinterpret_cast<::std::uintptr_t>(reference.storage.ptr)};
            if(token < token_begin_ || token - token_begin_ >= page_slots) { return nullptr; }
            auto const index{static_cast<::std::size_t>(token - token_begin_)};
            // Only exact release-published entries authenticate tokens. A
            // reserved-but-unissued slot is null and its object does not exist.
            auto* header{entries_[index].load(::std::memory_order_acquire)};
            if(header == nullptr) { return nullptr; }
            // Actual admission/native exclusion and scope pin keep header,
            // envelope and whole carrier live; pin alone would not suffice.
            auto* envelope{store_type::allocation_header_for(header)};
            if(header->token != reference.storage.ptr || header->owner != store_pin_.get() ||
               header->type_index != type_index_ || header->kind != gc_type::composite_kind::struct_ ||
               header->length != 1uz || !header->values ||
               envelope->metadata.owner_or_storage != chunk_ ||
               store_type::metadata_generation(envelope->metadata) != generation_ ||
               store_type::metadata_slot_index(envelope->metadata) != index ||
               store_type::metadata_state(envelope->metadata) != store_type::slab_slot_state::reserved)
            { ::std::terminate(); }
            return header;
        }
        [[nodiscard]] numeric_page_status owner_gate() const noexcept
        {
            if(owner_cookie_ == 0u) { return numeric_page_status::unowned; }
            if(!same_native_owner()) { return numeric_page_status::wrong_thread; }
            if(retired_) { return numeric_page_status::terminal; }
            if(!store_pin_ || !execution_generation_ || !entry_exclusion_ || shared_admission_ == nullptr ||
               !::uwvm2::runtime::gc::runtime_gc_entry_admission.protects_shared(
                   *entry_exclusion_, *shared_admission_) ||
               domain_ == nullptr || participant_ == nullptr || !*participant_)
            { ::std::terminate(); }
            if(phase_ == phase::owned && (store_pin_->slab_epoch_ != epoch_ || chunk_->generation != generation_))
            { ::std::terminate(); } // Genuine exclusion/reservation makes this impossible.
            return numeric_page_status::ok;
        }
    public:
        inline static constexpr ::std::size_t page_slots{256uz};
        inline static constexpr bool production_fast_path_enabled{false};
        struct storage_bound
        {
            ::std::size_t slots, entry_metadata_bytes, scope_bytes, physical_chunk_bytes,
                physical_slot_stride, maximum_unused_slots;
        };
        [[nodiscard]] static constexpr storage_bound bounds() noexcept
        {
            constexpr auto stride{(store_type::object_value_offset + sizeof(gc_object_value) +
                store_type::allocation_alignment - 1uz) / store_type::allocation_alignment * store_type::allocation_alignment};
            return {page_slots, page_slots * sizeof(::std::atomic<object*>), sizeof(managed_numeric_entry_page),
                store_type::chunk_slots_offset + page_slots * stride, stride, page_slots - 1uz};
        }
        managed_numeric_entry_page() noexcept = default;
        managed_numeric_entry_page(managed_numeric_entry_page const&) = delete;
        managed_numeric_entry_page& operator=(managed_numeric_entry_page const&) = delete;
        managed_numeric_entry_page(managed_numeric_entry_page&&) = delete;
        managed_numeric_entry_page& operator=(managed_numeric_entry_page&&) = delete;
        ~managed_numeric_entry_page() noexcept
        {
            if(owner_cookie_ != 0u && !retired_) { retire(); }
            // Generation remains owned even after fallback/reentry revocation.
            // Actual outer scope clears runtime TLS before this member dies.
        }
    private:
        [[nodiscard]] bool open_actual_entry(void const* entry_identity,
            void const* collection_identity, module_type const& module,
            ::std::uint_least64_t serial, admission::shared_lease const& shared,
            generation_lease& execution, ::std::shared_ptr<domain_type> domain,
            ::std::size_t budget) noexcept
        {
            if(owner_cookie_ != 0u || !entry_identity || !collection_identity ||
               !execution || execution.stop_requested() || !shared || !domain || serial == 0u ||
               !module.gc_store || !module.gc_store->valid()) { return false; }
            auto pin{module.gc_store->weak_from_this().lock()};
            if(!pin || pin.get() != module.gc_store.get() || pin.owner_before(module.gc_store) ||
               module.gc_store.owner_before(pin)) { return false; }
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(1uz)};
            if(!exclusive || !::uwvm2::runtime::gc::runtime_gc_entry_admission.protects_shared(exclusive, shared))
            { return false; }
            auto participant{domain->enter()};
            if(!participant) { return false; }
            root_context probe{};
            ::uwvm2::runtime::gc::prepare_collection_root_context(probe, nullptr, {});
            auto ticket{domain->request_pause(participant, ::std::addressof(probe))};
            if(!ticket || domain->wait_until_paused(ticket, ::std::chrono::steady_clock::now() +
                ::std::chrono::seconds{1}) != ::uwvm2::utils::thread::collection_pause_result::paused)
            { return false; }
            bool exact{};
            auto proof{domain->while_stopped(ticket, [&](domain_type::stopped_view view) noexcept
            {
                exact = view.participant_count() == 1uz;
                view.for_each([&](domain_type::stopped_participant value) noexcept
                { exact = exact && value.collecting && !value.blocking && value.root_context == &probe; });
            })};
            ticket.reset();
            auto const cookie{cold_current_owner()};
            if(proof != ::uwvm2::utils::thread::collection_pause_result::paused || !exact || cookie == 0u)
            { return false; }
            // Only after all admission/canonical/participant checks succeeds is
            // the genuine generation lease MOVED out of actual entry storage.
            execution_generation_ = ::std::move(execution);
            store_pin_ = ::std::move(pin); entry_exclusion_.emplace(::std::move(exclusive));
            domain_pin_ = ::std::move(domain); domain_ = domain_pin_.get();
            participant_owner_ = ::std::move(participant); participant_ = &participant_owner_;
            context_ = &context_owner_; shared_admission_ = &shared;
            entry_identity_ = entry_identity; collection_identity_ = collection_identity;
            module_ = &module; initializer_serial_ = serial; owner_cookie_ = cookie;
            backing_budget_ = budget; phase_ = phase::drained; return true;
        }
    public:
        [[nodiscard]] generation_lease* generation_address() noexcept
        { return owner_cookie_ == 0u ? nullptr : &execution_generation_; }
        [[nodiscard]] bool matches_actual_entry(void const* entry, generation_lease const* generation,
            ::std::uintptr_t module_address) const noexcept
        {
            return !retired_ && same_native_owner() && entry == entry_identity_ &&
                generation == &execution_generation_ && execution_generation_ &&
                !execution_generation_.stop_requested() &&
                module_address == reinterpret_cast<::std::uintptr_t>(module_) &&
                module_->gc_collection_phase.ready_for(initializer_serial_) &&
                ::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == initializer_serial_ &&
                ::uwvm2::runtime::gc::runtime_gc_entry_admission.protects_shared(*entry_exclusion_, *shared_admission_);
        }
        [[nodiscard]] bool belongs_to_collection(void const* state, ::std::uintptr_t module,
            ::std::uint_least64_t serial) const noexcept
        {
            return !retired_ && same_native_owner() && state == collection_identity_ && serial == initializer_serial_ &&
                matches_actual_entry(entry_identity_, &execution_generation_, module);
        }
        [[nodiscard]] void const* entry_identity() const noexcept { return entry_identity_; }
        [[nodiscard]] module_type const* module_identity() const noexcept { return module_; }
        [[nodiscard]] domain_type& collection_domain() noexcept
        { if(owner_gate() != numeric_page_status::ok) { ::std::terminate(); } return *domain_; }
        [[nodiscard]] root_context& collection_context() noexcept { return context_owner_; }
        [[nodiscard]] domain_type::participant const& collection_participant() const noexcept { return participant_owner_; }
        void clear_collection_borrows() noexcept { context_owner_ = {}; }
        [[nodiscard]] bool is_pending(gc_reference value) const noexcept
        {
            return owner_gate() == numeric_page_status::ok && phase_ == phase::owned &&
                checked_page_object(value) != nullptr;
        }
        [[nodiscard]] bool is_committed_local(gc_reference value) const noexcept
        {
            if(owner_gate() != numeric_page_status::ok || is_pending(value)) { return false; }
            // Header comes ONLY from the store's exact local membership, never
            // from an opaque token cast. No returned header borrow escapes.
            return (store_pin_->checked_local_object(value, gc_type::composite_kind::struct_) != nullptr ||
                    store_pin_->checked_local_object(value, gc_type::composite_kind::array) != nullptr);
        }
        [[nodiscard]] ::std::uintptr_t cast_values(gc_reference value, ::std::uint_least32_t type) const noexcept
        {
            if(owner_gate() != numeric_page_status::ok || phase_ != phase::owned || type != type_index_ ||
               execution_generation_.stop_requested() || domain_->pause_requested()) { return 0u; }
            auto* header{checked_page_object(value)};
            // Only the ACTUAL compiler's strict adjacent cast/get witness may
            // borrow this array. No local/table/root/PHI stores this pointer.
            return header == nullptr ? 0u : reinterpret_cast<::std::uintptr_t>(header->values.get());
        }
        [[nodiscard]] bool local_defined_table(module_type const& module, local_defined_table_storage_t const* table,
            ::std::size_t table_index) const noexcept
        {
            return owner_gate() == numeric_page_status::ok && &module == module_ &&
                module.imported_table_vec_storage.empty() && table != nullptr && table->owner_module_rt_ptr == module_ &&
                table_index < module.local_defined_table_vec_storage.size() &&
                table == &module.local_defined_table_vec_storage.index_unchecked(table_index);
        }
        void retire() noexcept
        {
            if(owner_cookie_ == 0u || retired_) { return; }
            if(!same_native_owner()) { ::std::terminate(); }
            if(drain(numeric_page_drain_reason::retirement) != numeric_page_status::ok) { ::std::terminate(); }
            context_owner_ = {}; participant_owner_.reset(); participant_ = nullptr;
            // Reset strong pins OUTSIDE every slab/global lock and before
            // reopening admission. Keep real generation until outer cleanup.
            store_pin_.reset(); domain_pin_.reset(); domain_ = nullptr;
            entry_exclusion_.reset(); retired_ = true; phase_ = phase::retired;
        }
        struct counters { ::std::size_t allocations, refills, drains, remaining; };
        [[nodiscard]] counters metrics() const noexcept
        { return {page_allocations_, refills_, drains_, phase_ == phase::owned ? issued_ : 0uz}; }
        [[nodiscard]] numeric_page_status try_new32(::std::uint_least32_t exact_type,
            ::std::uint32_t bits, gc_reference& result) noexcept
        {
            auto const gate{owner_gate()}; if(gate != numeric_page_status::ok) { return gate; }
            if(execution_generation_.stop_requested() || domain_->pause_requested()) { return numeric_page_status::pause_required; }
            if(phase_ != phase::owned || exact_type != type_index_ || issued_ == page_slots)
            {
                if(phase_ == phase::owned && drain(numeric_page_drain_reason::capacity) != numeric_page_status::ok)
                { ::std::terminate(); }
                if(!eligible(*store_pin_, exact_type)) { return numeric_page_status::use_original_route; }
                type_index_ = exact_type;
                auto const refill{cold_refill(backing_budget_)};
                if(refill != numeric_page_status::ok) { return refill; }
                ++refills_;
            }
            auto const slot{issued_};
            auto* envelope{store_type::slab_slot_header(*chunk_, slot)};
            auto* original_slot{store_type::slab_slot_bytes(*chunk_, slot)};
            // Actual original array reaches checked envelope + full tail. The
            // union and envelope exist; placement construction starts header
            // and real ONE-ELEMENT array lifetimes, exactly as the original.
            auto* fresh{::new(static_cast<void*>(::std::addressof(envelope->payload.instance))) object{}};
            auto* values{::new(static_cast<void*>(original_slot + store_type::object_value_offset)) gc_object_value[1uz]{}};
            fresh->values.reset(values); values[0uz] = gc_object_value::i32(bits);
            fresh->owner = store_pin_.get(); fresh->kind = gc_type::composite_kind::struct_;
            fresh->type_index = type_index_; fresh->length = 1uz;
            fresh->token = reinterpret_cast<void*>(token_begin_ + slot);
            // No shared accounting/commit/CAS/global stripe in this native leaf.
            // Null -> initialized exact member publication, not token->pointer.
            entries_[slot].store(fresh, ::std::memory_order_release);
            ++issued_; ++page_allocations_;
            result.storage.ptr = fresh->token;
            result.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_struct;
            return numeric_page_status::ok;
        }
        [[nodiscard]] numeric_page_status read32(gc_reference reference, ::std::uint32_t& bits) const noexcept
        {
            auto const gate{owner_gate()}; if(gate != numeric_page_status::ok) { return gate; }
            if(execution_generation_.stop_requested() || domain_->pause_requested()) { return numeric_page_status::pause_required; }
            if(phase_ != phase::owned) { return numeric_page_status::invalid_reference; }
            auto* header{checked_page_object(reference)};
            if(header == nullptr) { return numeric_page_status::invalid_reference; }
            bits = header->values[0uz].template as<::std::uint32_t>();
            return numeric_page_status::ok; // No borrow escapes this leaf.
        }
        [[nodiscard]] numeric_page_status drain(numeric_page_drain_reason) noexcept
        {
            if(owner_cookie_ == 0u) { return numeric_page_status::unowned; }
            if(!same_native_owner()) { return numeric_page_status::wrong_thread; }
            if(phase_ == phase::drained) { return numeric_page_status::ok; }
            auto const gate{owner_gate()}; if(gate != numeric_page_status::ok) { return gate; }
            auto& store{*store_pin_}; auto* chunk{chunk_};
            // Batch-commit ONLY after real exclusion and full invariant checks.
            // Old collector refuses reserved slots; it never scans page-only
            // bodies. Returning every unused reservation is required pre-pause.
            {
                store_type::slab_guard guard{store};
                if(chunk->owner != ::std::addressof(store) || chunk->generation != generation_ ||
                   store.slab_epoch_ != epoch_ || chunk->reserved_slots != page_slots ||
                   store.slab_reserved_slots_ < page_slots || chunk->free_slots != 0uz ||
                   chunk->allocated_slots != 0uz || issued_ > page_slots ||
                   store.slab_allocated_slots_ > (::std::numeric_limits<::std::size_t>::max)() - issued_)
                { ::std::terminate(); }
                // Validate ALL envelopes before changing any accounting/index.
                for(::std::size_t slot{}; slot != page_slots; ++slot)
                {
                    auto* envelope{store_type::slab_slot_header(*chunk, slot)};
                    if(store_type::metadata_generation(envelope->metadata) != generation_ ||
                       store_type::metadata_slot_index(envelope->metadata) != slot ||
                       store_type::metadata_state(envelope->metadata) != store_type::slab_slot_state::reserved ||
                       envelope->metadata.owner_or_storage != chunk ||
                       (entries_[slot].load(::std::memory_order_acquire) != nullptr) != (slot < issued_))
                    { ::std::terminate(); }
                }
                for(::std::size_t slot{}; slot != issued_; ++slot)
                {
                    auto* envelope{store_type::slab_slot_header(*chunk, slot)};
                    store_type::set_active_metadata(envelope->metadata, generation_, slot, store_type::slab_slot_state::allocated);
                }
                chunk->reserved_slots = 0uz; chunk->allocated_slots = issued_;
                store.slab_reserved_slots_ -= page_slots; store.slab_allocated_slots_ += issued_;
                for(::std::size_t slot{issued_}; slot != page_slots; ++slot)
                {
                    auto* envelope{store_type::slab_slot_header(*chunk, slot)};
                    // UNUSED inactive union; never call an object destructor.
                    store_type::poison_free_slab_body(*chunk, slot);
                    store_type::set_free_metadata(envelope->metadata, chunk->free_head);
                    chunk->free_head = slot; ++chunk->free_slots;
                }
                if(chunk->free_slots != 0uz) { store_type::add_available_chunk(store, *chunk); }
            }
            // Cold compatibility drain keeps original membership semantics.
            // It intentionally STILL pays per-issued local/global publication,
            // deferred from the leaf. No claim of total publication elimination.
            object* chain{}; object* tail{};
            for(::std::size_t slot{}; slot != issued_; ++slot)
            {
                auto* fresh{entries_[slot].load(::std::memory_order_acquire)};
                if(fresh == nullptr) { ::std::terminate(); }
                auto& bucket{store.membership_buckets_[store_type::bucket_index(fresh->token)]};
                auto* previous{bucket.load(::std::memory_order_relaxed)};
                do { fresh->hash_next = previous; }
                while(!bucket.compare_exchange_weak(previous, fresh, ::std::memory_order_release, ::std::memory_order_relaxed));
                fresh->next = chain; chain = fresh; if(tail == nullptr) { tail = fresh; }
                {
                    auto const index{store_type::bucket_index(fresh->token)};
                    store_type::global_guard global{index};
                    fresh->global_next = store_type::global_buckets_[index];
                    store_type::global_buckets_[index] = fresh;
                }
            }
            if(chain != nullptr)
            {
                auto* previous{store.objects_.load(::std::memory_order_relaxed)};
                do { tail->next = previous; }
                while(!store.objects_.compare_exchange_weak(previous, chain,
                    ::std::memory_order_release, ::std::memory_order_relaxed));
            }
            // No collector/peer sees the intermediate indexes: real native
            // exclusion lasts throughout commit + all three publications.
            for(auto& entry : entries_) { entry.store(nullptr, ::std::memory_order_release); }
            if(issued_ == 0uz)
            {
                {
                    store_type::slab_guard guard{store};
                    store_type::remove_available_chunk(store, *chunk);
                    if(chunk->all_previous != nullptr) { chunk->all_previous->all_next = chunk->all_next; }
                    else { store.slab_all_[0uz] = chunk->all_next; }
                    if(chunk->all_next != nullptr) { chunk->all_next->all_previous = chunk->all_previous; }
                    --store.slab_chunk_count_; store.slab_backing_bytes_ -= chunk->bytes;
                }
                // All slots inactive/free; original backing pointer, no lock.
                store_type::release_empty_chunk(*chunk);
            }
            // Drop ALL header/chunk borrows before snapshot/park/foreign exit.
            chunk_ = nullptr; generation_ = 0u; epoch_ = 0u; phase_ = phase::drained; ++drains_;
            return numeric_page_status::ok;
        }
        [[nodiscard]] numeric_page_status flush_for_pause(::uwvm2::runtime::gc::root_frame const* frames,
            ::std::span<::uwvm2::runtime::gc::root_reference const> native_roots) noexcept
        {
            auto const status{drain(numeric_page_drain_reason::pause)};
            if(status != numeric_page_status::ok) { return status; }
            // REAL roots are provided by the privileged native owner. This is
            // not a compiler stack map or automatically discovered VM census.
            ::uwvm2::runtime::gc::prepare_collection_root_context(*context_, frames, native_roots);
            return numeric_page_status::ok;
        }
        [[nodiscard]] numeric_page_status drain_before_foreign_exit() noexcept
        { retire(); return numeric_page_status::ok; }
        [[nodiscard]] ::std::size_t issued_count() const noexcept
        { return same_native_owner() ? issued_ : 0uz; }
        [[nodiscard]] ::std::uintptr_t native_reserved_token_begin() const noexcept
        { return same_native_owner() && phase_ == phase::owned ? token_begin_ : 0u; }

    };
#endif
}

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
namespace uwvm2::runtime::gc
{
    // Transport borrow ONLY: actual runtime callback proves entry identity,
    // generation address, serial and held leases before giving it to a helper.
    inline thread_local ::uwvm2::uwvm::runtime::storage::managed_numeric_entry_page* actual_entry_page{};
    inline void flush_actual_entry_page(bool revoke) noexcept
    {
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        flush_actual_sealed_entry(revoke);
#endif
        if(auto* page{actual_entry_page})
        {
            if(revoke) { actual_entry_page = nullptr; page->retire(); }
            else if(page->drain(::uwvm2::uwvm::runtime::storage::numeric_page_drain_reason::pause) !=
                ::uwvm2::uwvm::runtime::storage::numeric_page_status::ok) { ::std::terminate(); }
        }
    }
}
#endif

#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
#include "sealed_entry_compact_cursor.h"
#endif
