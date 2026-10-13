/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Private disabled persistent numeric descriptor foundation.
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <atomic>
# include <bit>
# include <cstddef>
# include <cstdint>
# include <exception>
# include <limits>
# include <memory>
# include <new>
# include <utility>
# include <uwvm2/object/global/ref.h>
# include <uwvm2/runtime/gc/entry_admission.h>
# include "payload.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    // In module mode this forward declaration belongs to the SAME primary
    // storage module as :wasm_module. There is no import of :wasm_module here.
    class gc_object_store;
    class compact_numeric_reader;

    class compact_numeric_descriptor
    {
        friend class gc_object_store; // The only native authority/mint publisher.
        friend class compact_numeric_reader;
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        friend class sealed_compact_entry;
#endif
        static constexpr ::std::size_t width{compact_numeric_range_geometry::reserved_width};
        static constexpr ::std::size_t bitmap_words{width / 64uz};
        static_assert(width % 64uz == 0uz);
        enum class phase : unsigned char { prepared, published, closing, retired };

        struct registry_node
        {
            registry_node* next{};
            ::std::shared_ptr<compact_numeric_descriptor> descriptor{};
        };
        inline static ::std::atomic_flag registry_lock_ = ATOMIC_FLAG_INIT;
        inline static registry_node* registry_head_{};
        struct registry_guard
        {
            registry_guard() noexcept
            {
                while(registry_lock_.test_and_set(::std::memory_order_acquire))
                { registry_lock_.wait(true, ::std::memory_order_relaxed); }
            }
            registry_guard(registry_guard const&) = delete;
            registry_guard& operator=(registry_guard const&) = delete;
            ~registry_guard() { registry_lock_.clear(::std::memory_order_release); registry_lock_.notify_one(); }
        };
        struct publication_guard
        {
            compact_numeric_descriptor& descriptor;
            explicit publication_guard(compact_numeric_descriptor& current) noexcept : descriptor{current}
            {
                while(descriptor.publish_lock_.test_and_set(::std::memory_order_acquire))
                { descriptor.publish_lock_.wait(true, ::std::memory_order_relaxed); }
            }
            ~publication_guard()
            { descriptor.publish_lock_.clear(::std::memory_order_release); descriptor.publish_lock_.notify_one(); }
        };

        // These weak canonical control blocks are set before private publication.
        // Registry -> descriptor -> weak store is not a store ownership cycle.
        ::std::weak_ptr<gc_object_store const> canonical_store_{};
        ::std::weak_ptr<compact_numeric_descriptor> canonical_self_{};
        ::std::uint_least32_t type_index_{};
        ::std::size_t canonical_type_id_{};
        compact_numeric_kind kind_{};
        ::std::size_t capacity_{};
        compact_numeric_payload payload_{};
        ::std::uintptr_t token_begin_{};
        ::std::atomic<phase> phase_{phase::prepared};
        ::std::atomic_size_t initialized_frontier_{};
        ::std::array<::std::atomic<::std::uint64_t>, bitmap_words> live_{};
        ::std::array<::std::uint64_t, bitmap_words> marks_{};
        ::std::atomic_size_t readers_{};
        ::std::atomic_flag publish_lock_ = ATOMIC_FLAG_INIT;
        // pending_node_ contains no descriptor pin until it is released into
        // the global registry. registered_node_ is a lock-protected native borrow.
        ::std::unique_ptr<registry_node> pending_node_{};
        registry_node* registered_node_{};
        // Store-owned segment chain, not a per-object list. The registry pins
        // descriptors, while every descriptor holds only a weak store owner.
        ::std::shared_ptr<compact_numeric_descriptor> owner_next_{};

        compact_numeric_descriptor(::std::weak_ptr<gc_object_store const> canonical,
            ::std::uint_least32_t type_index, ::std::size_t canonical_type_id,
            compact_numeric_kind kind, ::std::size_t capacity) noexcept
            : canonical_store_{::std::move(canonical)}, type_index_{type_index},
              canonical_type_id_{canonical_type_id}, kind_{kind}, capacity_{capacity} {}

        // Called only by a real store member after checked_type() has proved:
        // struct, field_count==1, immutable, packed-none, i32/f32; canonical ID
        // and local index are taken from this store's immutable layout, never
        // from a guest-provided kind/bool. canonical must be weak_from_this().
        [[nodiscard]] static compact_numeric_status prepare_for_store(
            ::std::shared_ptr<gc_object_store const> const& owner,
            ::std::weak_ptr<gc_object_store const> const& canonical,
            ::std::uint_least32_t type_index, ::std::size_t canonical_type_id,
            compact_numeric_kind kind, ::std::size_t capacity,
            ::std::shared_ptr<compact_numeric_descriptor>& result) noexcept
        {
            if(!owner || owner.use_count() == 0 || canonical.expired() ||
               canonical.owner_before(owner) || owner.owner_before(canonical))
            { return compact_numeric_status::invalid_store; }
            auto actual{canonical.lock()};
            if(actual.get() != owner.get()) { return compact_numeric_status::invalid_store; }
            if(canonical_type_id == (::std::numeric_limits<::std::size_t>::max)() ||
               (kind != compact_numeric_kind::i32 && kind != compact_numeric_kind::f32))
            { return compact_numeric_status::invalid_type; }
            if(capacity == 0uz || capacity > width) { return compact_numeric_status::size_overflow; }
            if(!compact_numeric_payload::has_raw_four_byte_layout)
            { return compact_numeric_status::unsupported_layout; }
#if defined(__cpp_exceptions)
            auto* raw{new(::std::nothrow) compact_numeric_descriptor{
                canonical, type_index, canonical_type_id, kind, capacity}};
            if(raw == nullptr) { return compact_numeric_status::out_of_memory; }
            ::std::shared_ptr<compact_numeric_descriptor> fresh{};
            try
            {
                // If control-block allocation throws, this exact constructor
                // deletes raw; do not retain another unique_ptr and double-delete.
                fresh = ::std::shared_ptr<compact_numeric_descriptor>{raw};
            }
            catch(...) { return compact_numeric_status::out_of_memory; }
            auto const storage{fresh->payload_.replace(capacity)};
            if(storage != compact_numeric_status::ok) { return storage; }
            // [one owned empty registration node] allocate before reserving IDs.
            fresh->pending_node_.reset(new(::std::nothrow) registry_node{});
            if(!fresh->pending_node_) { return compact_numeric_status::out_of_memory; }
            fresh->canonical_self_ = fresh;
            // [complete unpublished descriptor] only success replaces result.
            result = ::std::move(fresh);
            return compact_numeric_status::ok;
#else
            // Do not make a potentially throwing shared_ptr control-block
            // allocation in a no-C++-exceptions product configuration.
            (void)result;
            return compact_numeric_status::unsupported_exceptions;
#endif
        }

        // The store member MUST pass its existing next_object_token_id_.
        // This candidate declares NO independent token counter/TLS namespace.
        // Private access is intentionally not callable before that member exists.
        [[nodiscard]] compact_numeric_status publish_range_from_store(
            ::std::shared_ptr<compact_numeric_descriptor> const& self,
            ::std::atomic<::std::uintptr_t>& existing_store_issuer) noexcept
        {
            auto owner{canonical_store_.lock()};
            if(!owner) { return compact_numeric_status::invalid_store; }
            // Global shared admission precedes the page lock. A collector that
            // already owns exclusive admission may need this same page lock.
            auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            // Serialize cold publish/retire against the native append foundation.
            publication_guard publication{*this};
            if(self.get() != this || canonical_self_.owner_before(self) ||
               self.owner_before(canonical_self_) || !pending_node_)
            { return compact_numeric_status::invalid_store; }
            if(phase_.load(::std::memory_order_acquire) != phase::prepared)
            { return compact_numeric_status::invariant_error; }
            auto first{existing_store_issuer.load(::std::memory_order_relaxed)};
            constexpr auto last{(::std::numeric_limits<::std::uintptr_t>::max)()};
            for(;;)
            {
                if(first < compact_numeric_range_geometry::first_object_token ||
                   first > last - width) { return compact_numeric_status::size_overflow; }
                // [first,first+1024) is reserved from the REAL existing issuer.
                // End is exclusive/representable; even unused/dead IDs never return.
                if(existing_store_issuer.compare_exchange_weak(first, first + width,
                    ::std::memory_order_relaxed, ::std::memory_order_relaxed)) { break; }
            }
            registry_guard guard{};
            for(auto* current{registry_head_}; current != nullptr;)
            {
                auto const& other{*current->descriptor};
                auto const end{other.token_begin_ + width};
                if(first < end && other.token_begin_ < first + width)
                { return compact_numeric_status::invariant_error; } // burn unpublished range
                // [registered native chain] guard excludes unlink/destruction.
                // ^^ current follows only an initialized next link or null.
                current = current->next;
            }
            token_begin_ = first;
            // [fully allocated pending node] initialize its strong pin before
            // releasing its unique owner into the guarded registry chain.
            auto* node{pending_node_.get()};
            node->descriptor = self;
            node->next = registry_head_;
            registered_node_ = pending_node_.release();
            registry_head_ = registered_node_;
            phase_.store(phase::published, ::std::memory_order_release);
            return compact_numeric_status::ok;
        }

        // Native foundation only: a page-local lock serializes concurrent
        // appenders. It is NOT an optimized LLVM bump ABI and has no VM callsite.
        // No per-object local membership CAS/global registry publication occurs.
        [[nodiscard]] compact_numeric_status append_raw32(::std::uint32_t bits,
            ::uwvm2::object::global::wasm_global_ref_t& result) noexcept
        {
            auto owner{canonical_store_.lock()};
            if(!owner) { return compact_numeric_status::invalid_store; }
            auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            publication_guard lock{*this};
            if(phase_.load(::std::memory_order_acquire) != phase::published)
            { return compact_numeric_status::not_published; }
            auto const index{initialized_frontier_.load(::std::memory_order_relaxed)};
            if(index >= capacity_) { return compact_numeric_status::out_of_bounds; }
            auto const status{payload_.store(index, bits)};
            if(status != compact_numeric_status::ok) { return status; }
            auto const word{index / 64uz};
            auto const bit{::std::uint64_t{1u} << (index % 64uz)};
            // [0,capacity_<=1024) bounds both payload and 16 bitmap words.
            // A release frontier makes these exact initialized bits visible.
            live_[word].store(live_[word].load(::std::memory_order_relaxed) | bit,
                ::std::memory_order_relaxed);
            initialized_frontier_.store(index + 1uz, ::std::memory_order_release);
            // [token_begin_,token_begin_+capacity_) never overlaps another range.
            // Integer token conversion is identity only; NEVER dereference it.
            result.storage.ptr = reinterpret_cast<void*>(token_begin_ + index);
            result.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_struct;
            return compact_numeric_status::ok;
        }

        // Requires the REAL closed cohort/root snapshot/all actors stopped,
        // with runtime_gc_entry_admission exclusive held by the actual collector.
        // Private friend boundary, not an externally constructible bool ticket.
        // readers==0 is an additional check, NEVER the admission/root proof.
        [[nodiscard]] compact_numeric_status begin_marks_while_stopped() noexcept
        {
            if(phase_.load(::std::memory_order_acquire) != phase::published)
            { return compact_numeric_status::not_published; }
            if(readers_.load(::std::memory_order_acquire) != 0uz)
            { return compact_numeric_status::busy; }
            for(auto& word : marks_) { word = 0u; }
            return compact_numeric_status::ok;
        }
        // Collector lookup cannot call the PUBLIC reader: its shared admission
        // would wait for the collector's own exclusive admission and deadlock.
        // The actual closed-cohort collector instead holds canonical strong store
        // pins plus exclusive/pause/root closure and calls this private locator.
        // HOT local integration: no new admission, global lookup, lock or
        // shared_ptr promotion. The actual store caller already pins this
        // store-owned chain and excludes collection/reset exactly as its old
        // object* consumers do. The result is an index, never a guest pointer.
        [[nodiscard]] compact_numeric_status locate_live_token_under_existing_admission(
            ::std::uintptr_t token, ::std::size_t& result) const noexcept
        {
            if(phase_.load(::std::memory_order_acquire) != phase::published)
            { return compact_numeric_status::not_published; }
            ::std::size_t slot{};
            if(!compact_numeric_range_geometry::slot(token_begin_, capacity_, token, slot) ||
               slot >= initialized_frontier_.load(::std::memory_order_acquire))
            { return compact_numeric_status::invalid_reference; }
            auto const bit{::std::uint64_t{1u} << (slot % 64uz)};
            // [0,initialized_frontier_<=capacity_<=1024) names a live bit only.
            if((live_[slot / 64uz].load(::std::memory_order_acquire) & bit) == 0u)
            { return compact_numeric_status::invalid_reference; }
            result = slot;
            return compact_numeric_status::ok;
        }
        [[nodiscard]] compact_numeric_status locate_live_token_while_stopped(
            ::std::uintptr_t token, ::std::size_t& result) const noexcept
        {
            if(readers_.load(::std::memory_order_acquire) != 0uz)
            { return compact_numeric_status::busy; }
            return locate_live_token_under_existing_admission(token, result);
        }
        [[nodiscard]] ::std::uint32_t const* borrow_raw32_under_existing_admission(
            ::std::size_t authenticated_slot) const noexcept
        {
            // Caller must first authenticate this exact token/live slot through
            // the locator and keep its real owner/admission until the immediate
            // byte load. No callback/poll, no gc_object_value[1] reinterpretation.
            return payload_.address_of_cell(authenticated_slot);
        }
        // PRIVATE friend integration, never a guest cursor or admission API.
        // The actual closed collector must have JUST located this descriptor's
        // exact initialized/live slot while all mutators/readers are stopped.
        // Its canonical store pin and exclusive admission remain held, with no
        // callback, poll, unlock, reset or retirement before this bitmap write.
        // Slot bounds defend trusted-native misuse; they do not mint authority.
        [[nodiscard]] compact_numeric_status mark_authenticated_slot_while_stopped(::std::size_t slot) noexcept
        {
            if(slot >= capacity_ || slot >= width || slot / 64uz >= marks_.size())
            { return compact_numeric_status::invalid_reference; }
            // [0,capacity_<=1024) owns this just-authenticated live slot.
            // [safe ] slot/64<marks_.size() names a complete native bitmap word;
            // slot%64 is a bounded shift, never a guest-derived native address.
            marks_[slot / 64uz] |= ::std::uint64_t{1u} << (slot % 64uz);
            return compact_numeric_status::ok;
        }
        [[nodiscard]] compact_numeric_status mark_token_while_stopped(::std::uintptr_t token) noexcept
        {
            ::std::size_t slot{};
            auto const status{locate_live_token_while_stopped(token, slot)};
            if(status != compact_numeric_status::ok) { return status; }
            // [this authentic descriptor][its exact stopped live slot]
            // The existing token API keeps its complete authentication before
            // immediately borrowing the private bounded bitmap operation.
            return mark_authenticated_slot_while_stopped(slot);
        }
        [[nodiscard]] compact_numeric_status sweep_unmarked_while_stopped(::std::size_t& reclaimed) noexcept
        {
            if(phase_.load(::std::memory_order_acquire) != phase::published)
            { return compact_numeric_status::not_published; }
            if(readers_.load(::std::memory_order_acquire) != 0uz)
            { return compact_numeric_status::busy; }
            ::std::size_t count{};
            for(::std::size_t word{}; word != bitmap_words; ++word)
            {
                // [0,16) both complete bitmap arrays contain this word.
                auto old{live_[word].load(::std::memory_order_relaxed)};
                auto const dead{old & ~marks_[word]};
                count += static_cast<::std::size_t>(::std::popcount(dead));
                live_[word].store(old & marks_[word], ::std::memory_order_release);
            }
            // Frontier NEVER decreases and swept slots/IDs are NEVER reissued.
            reclaimed = count;
            return compact_numeric_status::ok;
        }

        // Two-step retirement, no blocking wait while holding a reader/registry
        // lock. Actual store teardown must close admission/drain before this.
        [[nodiscard]] compact_numeric_status begin_retire() noexcept
        {
            // An admitted append must finish before the phase closes/unlinks.
            // No registry lock is held while waiting for that native page lock.
            auto keep_alive{canonical_self_.lock()};
            if(!keep_alive || keep_alive.get() != this) { return compact_numeric_status::invalid_store; }
            publication_guard publication{*this};
            ::std::unique_ptr<registry_node> retired{};
            {
                registry_guard guard{};
                auto current_phase{phase_.load(::std::memory_order_acquire)};
                if(current_phase == phase::retired || current_phase == phase::closing)
                { return compact_numeric_status::ok; }
                if(current_phase == phase::prepared)
                {
                    phase_.store(phase::closing, ::std::memory_order_release);
                    return compact_numeric_status::ok;
                }
                // [registry_head_ or a registered node's next] guarded pointer-
                // to-link traversal never derives an address from a Wasm token.
                auto** link{::std::addressof(registry_head_)};
                while(*link != nullptr && *link != registered_node_)
                {
                    // [live registered node] next member remains live under guard.
                    // ^^ link advances to that exact native link field.
                    link = ::std::addressof((*link)->next);
                }
                if(*link == nullptr) { return compact_numeric_status::invariant_error; }
                // [link]->[owned node]->[next live node] unlink before retirement.
                auto* node{*link};
                *link = node->next;
                registered_node_ = nullptr;
                phase_.store(phase::closing, ::std::memory_order_release);
                retired.reset(node);
            }
            // Drop the registry's descriptor pin OUTSIDE its lock. A last pin
            // may run native destruction; it must never reenter a held guard.
            retired.reset();
            return compact_numeric_status::ok;
        }
        [[nodiscard]] compact_numeric_status finish_retire() noexcept
        {
            // Serializes simultaneous finish calls and any final admitted append.
            auto keep_alive{canonical_self_.lock()};
            if(!keep_alive || keep_alive.get() != this) { return compact_numeric_status::invalid_store; }
            publication_guard publication{*this};
            auto const current_phase{phase_.load(::std::memory_order_acquire)};
            if(current_phase == phase::retired) { return compact_numeric_status::ok; }
            if(current_phase != phase::closing) { return compact_numeric_status::invariant_error; }
            if(readers_.load(::std::memory_order_acquire) != 0uz)
            { return compact_numeric_status::busy; }
            // No new acquisition after unlink; every old reader is retired.
            payload_.release();
            phase_.store(phase::retired, ::std::memory_order_release);
            return compact_numeric_status::ok;
        }

    public:
        compact_numeric_descriptor(compact_numeric_descriptor const&) = delete;
        compact_numeric_descriptor& operator=(compact_numeric_descriptor const&) = delete;
        ~compact_numeric_descriptor()
        {
            // A registered node owns a strong descriptor pin; destruction can
            // occur only after begin_retire detached it or before publication.
            if(registered_node_ != nullptr) { ::std::terminate(); }
        }
    };

    // Cold typed reader. It owns the actual store/control block and a real
    // runtime shared admission. It exposes copied bits/immutable type IDs only.
    // It MUST retire before a VM safepoint/poll, guest callback or reset: retaining
    // it across those operations could block the collector/drain it waits for.
    // This has no raw-pointer borrowing API. Future immediate LLVM borrow must
    // be tied to this exact lease and end before any poll.
    class compact_numeric_reader
    {
        friend class gc_object_store;
        ::std::shared_ptr<compact_numeric_descriptor> descriptor_{};
        ::std::shared_ptr<gc_object_store const> store_{};
        ::uwvm2::runtime::gc::managed_entry_admission::shared_lease admission_{};
        ::std::size_t slot_{};
        // PRIVATE store integration only. Caller keeps this exact reader alive
        // through the immediate memcpy/LLVM byte load, with no poll/collection,
        // callback or cached derived pointer. Public APIs expose copied bits.
        [[nodiscard]] ::std::uint32_t const* borrow_raw32_while_admitted() const noexcept
        {
            return descriptor_ ? descriptor_->payload_.address_of_cell(slot_) : nullptr;
        }
        static compact_numeric_status acquire(
            ::uwvm2::object::global::wasm_global_ref_t reference,
            ::std::shared_ptr<gc_object_store const> const* expected,
            compact_numeric_reader& result) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(result.descriptor_) { return compact_numeric_status::busy; }
            if(expected != nullptr && (!*expected || expected->use_count() == 0))
            { return compact_numeric_status::invalid_store; }
            if(reference.kind != kind::wasm_struct || reference.storage.ptr == nullptr)
            { return compact_numeric_status::invalid_reference; }
            auto const token{reinterpret_cast<::std::uintptr_t>(reference.storage.ptr)};
            // Declare native owners BEFORE admission: every failure return
            // releases admission before dropping a possible last-owner pin.
            ::std::shared_ptr<compact_numeric_descriptor> descriptor{};
            ::std::shared_ptr<gc_object_store const> store{};
            auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            {
                compact_numeric_descriptor::registry_guard guard{};
                for(auto* node{compact_numeric_descriptor::registry_head_}; node != nullptr;)
                {
                    auto const& candidate{node->descriptor};
                    ::std::size_t index{};
                    if(compact_numeric_range_geometry::slot(candidate->token_begin_, candidate->capacity_, token, index))
                    { descriptor = candidate; break; }
                    // [registered native chain] a registry node strongly pins its descriptor.
                    // ^^ node follows only the initialized guarded next pointer.
                    node = node->next;
                }
            }
            if(!descriptor) { return compact_numeric_status::invalid_reference; }
            // Promote outside the global lock: never run a store's last-owner
            // destruction/canonical shared_ptr cleanup while holding that lock.
            store = descriptor->canonical_store_.lock();
            if(!store) { return compact_numeric_status::invalid_store; }
            if(expected != nullptr &&
               (store.get() != expected->get() ||
                descriptor->canonical_store_.owner_before(*expected) ||
                expected->owner_before(descriptor->canonical_store_)))
            { return compact_numeric_status::foreign_reference; }
            ::std::size_t slot{};
            {
                compact_numeric_descriptor::registry_guard guard{};
                if(descriptor->registered_node_ == nullptr ||
                   descriptor->phase_.load(::std::memory_order_acquire) !=
                        compact_numeric_descriptor::phase::published)
                { return compact_numeric_status::retiring; }
                if(!compact_numeric_range_geometry::slot(descriptor->token_begin_, descriptor->capacity_, token, slot) ||
                   slot >= descriptor->initialized_frontier_.load(::std::memory_order_acquire))
                { return compact_numeric_status::invalid_reference; }
                auto const bit{::std::uint64_t{1u} << (slot % 64uz)};
                // [0,initialized_frontier_<=capacity_<=1024) initialized live slot.
                if((descriptor->live_[slot / 64uz].load(::std::memory_order_acquire) & bit) == 0u)
                { return compact_numeric_status::invalid_reference; }
                if(descriptor->readers_.load(::std::memory_order_relaxed) ==
                        (::std::numeric_limits<::std::size_t>::max)())
                { return compact_numeric_status::size_overflow; }
                descriptor->readers_.fetch_add(1uz, ::std::memory_order_relaxed);
            }
            // [actual strongly pinned native owners] moves preserve object address.
            result.descriptor_ = ::std::move(descriptor);
            result.store_ = ::std::move(store);
            result.admission_ = ::std::move(admission);
            result.slot_ = slot;
            return compact_numeric_status::ok;
        }
    public:
        compact_numeric_reader() noexcept = default;
        compact_numeric_reader(compact_numeric_reader const&) = delete;
        compact_numeric_reader& operator=(compact_numeric_reader const&) = delete;
        compact_numeric_reader(compact_numeric_reader&& other) noexcept
            // [borrow owners] transfer the one reader-count obligation, never copy it.
            : descriptor_{::std::move(other.descriptor_)}, store_{::std::move(other.store_)},
              admission_{::std::move(other.admission_)}, slot_{::std::exchange(other.slot_, 0uz)} {}
        compact_numeric_reader& operator=(compact_numeric_reader&&) = delete;
        ~compact_numeric_reader() { reset(); }
        void reset() noexcept
        {
            // [owned descriptor/store pins] detach first so nested destruction
            // cannot observe an apparently still-active reader.
            auto descriptor{::std::exchange(descriptor_, {})};
            auto store{::std::exchange(store_, {})};
            slot_ = 0uz;
            if(descriptor)
            {
                auto const previous{descriptor->readers_.fetch_sub(1uz, ::std::memory_order_acq_rel)};
                if(previous == 0uz) { ::std::terminate(); }
            }
            // No payload accesses after decrement. Release global admission
            // before last native owners can run actual store teardown.
            admission_.reset();
            descriptor.reset();
            store.reset();
        }
        [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(descriptor_); }
        [[nodiscard]] compact_numeric_status raw_bits32(::std::uint32_t& bits) const noexcept
        {
            if(!descriptor_) { return compact_numeric_status::invalid_reference; }
            // [slot_,slot_+1) was membership/live checked before admission.
            // Existing readers remain valid during closing; payload is retained
            // until every reader retires. Mark/sweep cannot overlap this lease.
            return descriptor_->payload_.load(slot_, bits);
        }
        [[nodiscard]] ::std::uint_least32_t type_index() const noexcept
        { return descriptor_ ? descriptor_->type_index_ : (::std::numeric_limits<::std::uint_least32_t>::max)(); }
        [[nodiscard]] ::std::size_t canonical_type_id() const noexcept
        { return descriptor_ ? descriptor_->canonical_type_id_ : (::std::numeric_limits<::std::size_t>::max)(); }
        [[nodiscard]] compact_numeric_kind value_kind() const noexcept
        { return descriptor_ ? descriptor_->kind_ : compact_numeric_kind::invalid; }

        [[nodiscard]] static compact_numeric_status try_local(
            ::std::shared_ptr<gc_object_store const> const& store,
            ::uwvm2::object::global::wasm_global_ref_t reference,
            compact_numeric_reader& result) noexcept
        { return acquire(reference, ::std::addressof(store), result); }
        [[nodiscard]] static compact_numeric_status try_foreign(
            ::uwvm2::object::global::wasm_global_ref_t reference,
            compact_numeric_reader& result) noexcept
        { return acquire(reference, nullptr, result); }
    };
}
