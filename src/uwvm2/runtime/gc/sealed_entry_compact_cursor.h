// Independently authored private SOURCE-ONLY candidate; no GPL code copied.
#pragma once
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE == 1
# if !defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST) || UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST != 1
#  error "Local compact lookup cache requires the genuine older-object cast path"
# endif
#endif
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST == 1
# if !defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) || UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR != 1
#  error "Local compact cast requires the genuine sealed actual-entry cursor"
# endif
#endif
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
# if defined(UWVM_MODULE)
#  error "Sealed compact cursor requires separately qualified NONMODULE integration"
# endif
# if !defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) || UWVM_EXPERIMENTAL_COMPACT_NUMERIC != 1
#  error "Exact compact dual-store and dual collector are required"
# endif
# if !defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE) || UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE != 1
#  error "The first cursor borrows genuine actual-entry page authority"
# endif
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <exception>
# include <memory>
# include <optional>
# include <stop_token>
# include <thread>
# include <type_traits>
# include <utility>
# include "sealed_entry_compact_protocol.h"
# include <uwvm2/runtime/gc/managed_numeric_page.h>
namespace uwvm2::uwvm::runtime::storage
{
    // Actual descriptor frontiers, sampled only on native refill/retirement.
    // These counters are evidence of committed JIT slots, not guessed from
    // module syntax, policy flags, or a generic GC allocation counter.
    struct sealed_compact_entry_counters
    {
        ::std::uint64_t committed_slots{}, deferred_allocations{}, refills{},
            windows{}, poll_attempts{}, retirements{};
        ::std::size_t remaining{};
    };
    class sealed_compact_entry
    {
        friend class ::uwvm2::runtime::gc::managed_collection_state;
        using status = ::uwvm2::runtime::gc::sealed_cursor_status;
        using view_type = ::uwvm2::runtime::gc::sealed_compact_cursor_view;
        using descriptor = compact_numeric_descriptor;
        view_type view_{};
        managed_numeric_entry_page* authority_{};
        void const* actual_entry_{};
        void const* actual_collection_{};
        wasm_module_storage_t const* module_{};
        ::std::uint_least64_t serial_{};
        ::std::uintptr_t nonce_{};
        ::std::thread::id native_owner_{};
        ::std::shared_ptr<gc_object_store> store_pin_{};
        ::std::shared_ptr<descriptor> descriptor_pin_{};
        ::std::size_t window_frontier_begin_{};
        sealed_compact_entry_counters counters_{};
        static void add_cold_counter(::std::uint64_t& counter, ::std::uint64_t amount) noexcept
        {
            if(amount > UINT64_MAX - counter) { ::std::terminate(); }
            counter += amount;
        }
        inline static ::std::atomic<::std::uintptr_t> next_nonce_{1u};
        struct on_stop
        {
            ::std::atomic<::std::uintptr_t>* notification;
            void operator()() const noexcept
            {
                // Foreign stop thread only signals. It never frees cells or
                // releases admission/participant/root/native window borrows.
                notification->fetch_or(1u, ::std::memory_order_release);
            }
        };
        ::std::optional<::std::stop_callback<on_stop>> stop_callback_{};
        bool retired_{}, reconciling_{};
        [[nodiscard]] bool live_authority() const noexcept
        {
            return !retired_ && authority_ && module_ && actual_entry_ && view_.policy_disabled &&
                !view_.policy_disabled->load(::std::memory_order_acquire) &&
                ::std::this_thread::get_id() == native_owner_ &&
                authority_->matches_actual_entry(actual_entry_, authority_->generation_address(),
                    reinterpret_cast<::std::uintptr_t>(module_)) &&
                authority_->belongs_to_collection(actual_collection_, reinterpret_cast<::std::uintptr_t>(module_), serial_) &&
                store_pin_ && store_pin_.get() == module_->gc_store.get() &&
                !store_pin_.owner_before(module_->gc_store) && !module_->gc_store.owner_before(store_pin_);
        }
        // Only the actual managed_collection_state may mint after ORIGINAL
        // CLI/full-source/phase/closed-cohort gates. Public construction is empty.
        [[nodiscard]] bool attach_actual_entry(managed_numeric_entry_page& page,
            void const* entry, void const* collection, wasm_module_storage_t const& module,
            ::std::uint_least64_t serial, ::std::atomic_bool const* actual_policy_disabled) noexcept
        {
            if(authority_ || retired_ || !entry || !collection || !actual_policy_disabled || serial == 0u ||
               !page.matches_actual_entry(entry, page.generation_address(), reinterpret_cast<::std::uintptr_t>(&module)) ||
               !page.belongs_to_collection(collection, reinterpret_cast<::std::uintptr_t>(&module), serial) ||
               !::uwvm2::runtime::gc::sealed_account.load(::std::memory_order_acquire)) { return false; }
            auto pin{module.gc_store ? module.gc_store->weak_from_this().lock() : ::std::shared_ptr<gc_object_store>{}};
            if(!pin || pin.get() != module.gc_store.get() || pin.owner_before(module.gc_store) ||
               module.gc_store.owner_before(pin)) { return false; }
            auto nonce{next_nonce_.load(::std::memory_order_relaxed)};
            for(;;)
            {
                if(nonce == 0u || nonce == UINTPTR_MAX) { return false; }
                if(next_nonce_.compare_exchange_weak(nonce, nonce+1u, ::std::memory_order_relaxed)) { break; }
            }
            authority_ = &page; actual_entry_ = entry; actual_collection_ = collection;
            module_ = &module; serial_ = serial; native_owner_ = ::std::this_thread::get_id();
            nonce_ = nonce; store_pin_ = ::std::move(pin);
            view_.policy_disabled = actual_policy_disabled; // gstate's real owned atomic
            // Real pause notification, pinned by page's real participant/domain.
            // Requires the precise friend declaration in required-friends.patch.
            view_.pause_requested = ::std::addressof(page.collection_domain().requested_);
            view_.epoch_address = ::std::addressof(store_pin_->slab_epoch_);
            stop_callback_.emplace(page.generation_address()->cancellation_token(), on_stop{&view_.interrupts});
            return live_authority();
        }
        [[nodiscard]] ::std::size_t detach_window() noexcept
        {
            // Invalidate BEFORE dropping any pin or entering collection/native.
            view_.armed_nonce.store(0u, ::std::memory_order_release);
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE == 1
            clear_entry_local_compact_lookup();
#endif
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
            clear_local_table_capture();
#endif
            // [actual pinned descriptor | initialized cell[0..capacity)]
            // [safe                                                   ]
            //         ^^ descriptor_pin_ still owns the actual frontier/payload.
            // Invalidation above precedes every pointer clear or pin release.
            auto const count{::std::exchange(view_.unaccounted, 0uz)};
            if(descriptor_pin_)
            {
                auto const frontier{descriptor_pin_->initialized_frontier_.load(::std::memory_order_acquire)};
                if(window_frontier_begin_ > frontier || frontier > descriptor_pin_->capacity_ ||
                   view_.first_credit > 1uz) { ::std::terminate(); }
                auto const issued{frontier - window_frontier_begin_};
                auto const credited{view_.first_credit == 0uz ? 1uz : 0uz};
                if(credited > issued || count != issued - credited) { ::std::terminate(); }
                add_cold_counter(counters_.committed_slots, issued);
                add_cold_counter(counters_.deferred_allocations, count);
                add_cold_counter(counters_.windows, 1u);
            }
            else if(count != 0uz) { ::std::terminate(); }
            window_frontier_begin_ = 0uz;
            // [actual-entry native view] armed=0; generated code cannot use
            // these members. Each pointer is cleared before its owner is reset.
            // [safe                        ]
            // ^^ view_.cells/frontier/live become null, never a guest address.
            view_.cells = nullptr; view_.frontier = nullptr; view_.live = nullptr;
            view_.token_begin = 0u; view_.capacity = 0uz; view_.budget = 0uz; view_.first_credit = 0uz; view_.epoch = 0u;
            descriptor_pin_.reset(); // OUTSIDE all slab/registry locks
            return count;
        }
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
#include "native_table_capture.h"
#endif
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE == 1
#include "sealed_local_compact_lookup_cache.h"
#endif
    public:
        using counters = sealed_compact_entry_counters;
        [[nodiscard]] counters metrics() const noexcept
        {
            auto result{counters_}; result.remaining = view_.unaccounted;
            return result;
        }
        sealed_compact_entry() noexcept = default;
        sealed_compact_entry(sealed_compact_entry const&) = delete;
        sealed_compact_entry& operator=(sealed_compact_entry const&) = delete;
        ~sealed_compact_entry() noexcept { retire(); }
        [[nodiscard]] ::std::uintptr_t generated_view_address() const noexcept
        { return live_authority() ? reinterpret_cast<::std::uintptr_t>(&view_) : 0u; }
        [[nodiscard]] bool exact_view_identity(::std::uintptr_t address) const noexcept
        { return address != 0u && address == reinterpret_cast<::std::uintptr_t>(&view_) && live_authority(); }
        // Cleanup remains necessary after stop_requested or policy disable.
        // The runtime callback FIRST selects its OWN pinned native scope; this
        // identity comparison then accesses only that scope's member. It does
        // not dereference supplied address/module and cannot mint authority.
        [[nodiscard]] bool exact_cleanup_identity(::std::uintptr_t address, ::std::uintptr_t module) const noexcept
        { return nonce_ != 0u && address == reinterpret_cast<::std::uintptr_t>(&view_) &&
                 module == reinterpret_cast<::std::uintptr_t>(module_) &&
                 ::std::this_thread::get_id() == native_owner_; }
        [[nodiscard]] bool exact_native_address(::std::uintptr_t address) const noexcept
        { return nonce_ != 0u && address == reinterpret_cast<::std::uintptr_t>(&view_) &&
                 ::std::this_thread::get_id() == native_owner_; }
        [[nodiscard]] bool local_defined_table(wasm_module_storage_t const& module,
            local_defined_table_storage_t const* table, ::std::size_t index) const noexcept
        { return live_authority() && authority_->local_defined_table(module, table, index); }
        [[nodiscard]] bool authenticates_current(gc_reference reference) const noexcept
        {
            if(!live_authority() || !descriptor_pin_ || view_.armed_nonce.load(::std::memory_order_acquire) != nonce_ ||
               view_.epoch != store_pin_->slab_epoch_ ||
               reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct) { return false; }
            ::std::size_t slot{};
            return descriptor_pin_->locate_live_token_under_existing_admission(
                reinterpret_cast<::std::uintptr_t>(reference.storage.ptr), slot) == compact_numeric_status::ok;
        }
        [[nodiscard]] bool authenticates_local_compact(gc_reference reference) const noexcept
        {
            if(!live_authority() || view_.interrupts.load(::std::memory_order_acquire) ||
               authority_->collection_domain().pause_requested()) { return false; }
            // REAL strong canonical store and existing exclusive entry exclude
            // sweep/teardown through the last immediate table carrier copy.
            // This is the original cb424 local dual-view check, not a reader
            // shared-enter under our held exclusive, and not a token->pointer.
            // Its ephemeral descriptor comes ONLY from the store-owned release
            // chain; range/frontier/live are proved even for older cold ranges.
            // No native pointer/borrow is returned to the table or guest.
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE == 1
            return static_cast<bool>(checked_entry_local_compact_object(reference));
#else
            return static_cast<bool>(store_pin_->checked_local_compact_object(reference));
#endif
        }
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST == 1
#include "sealed_local_compact_cast.h"
#endif
        [[nodiscard]] status refill_after_actual_root_snapshot(::std::uint32_t index) noexcept
        {
            if(!live_authority()) { retire(); return status::original_route; }
            auto& store{*store_pin_};
            auto const* layout{store.checked_type(index, gc_type::composite_kind::struct_)};
            if(!gc_object_store::compact_layout(layout)) { retire(); return status::original_route; }
            auto const count{detach_window()};
            auto account{::uwvm2::runtime::gc::sealed_account.load(::std::memory_order_acquire)};
            if(!account) { retire(); return status::original_route; }
            // Preserve ORIGINAL attempted-allocation semantics: settle prior
            // infallible committed hot slots, then original allocation_poll
            // charges THIS cold attempt before preparation (including OOM).
            // The first raw slot gets one non-authorizing scheduling credit;
            // later hot slots are bounded before the NEXT original threshold.
            // Actual SSA roots are published BEFORE this leaf (gc_emit).
            auto const accounting{account(reinterpret_cast<::std::uintptr_t>(&view_),
                reinterpret_cast<::std::uintptr_t>(module_), count, true)};
            if(accounting.charged) { add_cold_counter(counters_.poll_attempts, 1u); }
            if(!accounting.eligible || !live_authority())
            { retire(); return accounting.charged ? status::original_route_already_charged : status::original_route; }
            if(!accounting.charged) { retire(); return status::invariant_error; }
            auto const budget{accounting.budget};
            if(budget > 4095uz) { retire(); return status::invariant_error; }
            if(view_.interrupts.load(::std::memory_order_acquire) || authority_->collection_domain().pause_requested())
            { retire(); return status::interrupted; }
            ::std::shared_ptr<descriptor> chosen{};
            {
                gc_object_store::slab_guard slab{store};
                // Cold reuse can resume a PARTIALLY initialized range after a
                // real poll. Never reuse swept slots or tokens: only frontier++.
                // Other appenders/readers are excluded by genuine admission,
                // not active_count/TLS/marked. Native links are initialized.
                for(auto current{store.compact_numeric_segments_}; current; current = current->owner_next_)
                {
                    if(current->type_index_ == index && current->phase_.load(::std::memory_order_acquire) == descriptor::phase::published &&
                       current->initialized_frontier_.load(::std::memory_order_acquire) < current->capacity_ &&
                       current->readers_.load(::std::memory_order_acquire) == 0uz)
                    { chosen = current; break; }
                }
            }
            if(!chosen)
            {
                ::std::shared_ptr<gc_object_store const> canonical{store_pin_};
                auto const kind{layout->fields[0uz].storage.value.kind == gc_type::value_kind::i32 ?
                    compact_numeric_kind::i32 : compact_numeric_kind::f32};
                auto const prepared{descriptor::prepare_for_store(canonical, ::std::weak_ptr<gc_object_store const>{canonical},
                    index, store.canonical_type_id(index), kind, compact_numeric_range_geometry::reserved_width, chosen)};
                if(prepared != compact_numeric_status::ok)
                { return prepared == compact_numeric_status::out_of_memory ? status::out_of_memory :
                    prepared == compact_numeric_status::size_overflow ? status::size_overflow : status::original_route_already_charged; }
                // Allocator code can reenter/stop. Re-prove before publication.
                if(!live_authority() || view_.interrupts.load(::std::memory_order_acquire))
                { retire(); return status::interrupted; }
                auto first{gc_object_store::next_object_token_id_.load(::std::memory_order_relaxed)};
                constexpr auto width{compact_numeric_range_geometry::reserved_width};
                for(;;)
                {
                    if(first < compact_numeric_range_geometry::first_object_token || first > UINTPTR_MAX-width)
                    { return status::size_overflow; }
                    if(gc_object_store::next_object_token_id_.compare_exchange_weak(first, first+width,
                        ::std::memory_order_relaxed)) { break; }
                }
                {
                    // Existing cold order: slab -> publication -> registry.
                    // NO recursive shared enter() under this genuine exclusive.
                    gc_object_store::slab_guard slab{store};
                    descriptor::publication_guard publication{*chosen};
                    descriptor::registry_guard registry{};
                    if(store.slab_closing_ || store.slab_epoch_ == UINT64_MAX || !chosen->pending_node_ ||
                       chosen->phase_.load(::std::memory_order_acquire) != descriptor::phase::prepared)
                    { return status::invariant_error; } // unpublished IDs burn
                    for(auto* node{descriptor::registry_head_}; node; node = node->next)
                    {
                        auto const& other{*node->descriptor};
                        if(first < other.token_begin_+width && other.token_begin_ < first+width)
                        { return status::invariant_error; }
                    }
                    chosen->token_begin_ = first; chosen->owner_next_ = store.compact_numeric_segments_;
                    auto* node{chosen->pending_node_.get()}; node->descriptor = chosen; node->next = descriptor::registry_head_;
                    chosen->registered_node_ = chosen->pending_node_.release(); descriptor::registry_head_ = chosen->registered_node_;
                    chosen->phase_.store(descriptor::phase::published, ::std::memory_order_release);
                    store.compact_numeric_segments_ = chosen;
                    store.compact_numeric_head_.store(chosen.get(), ::std::memory_order_release);
                }
            }
            descriptor_pin_ = ::std::move(chosen);
            auto& d{*descriptor_pin_};
            window_frontier_begin_ = d.initialized_frontier_.load(::std::memory_order_acquire);
            if(window_frontier_begin_ >= d.capacity_) { ::std::terminate(); }
            // Genuine cell[] BASE. LLVM byte GEP stays within that allocation;
            // C++ never performs uint32 member-pointer arithmetic across cells.
            // FULL carrier reconstruction is separate and stays opaque/i128.
            view_.cells = d.payload_.cells_.get(); view_.frontier = &d.initialized_frontier_;
            view_.live = d.live_.data(); view_.token_begin = d.token_begin_; view_.capacity = d.capacity_;
            view_.type_index = d.type_index_; view_.raw_kind = static_cast<::std::uint32_t>(d.kind_);
            view_.epoch = store.slab_epoch_; view_.budget = budget; view_.first_credit = 1uz;
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
            capture_current_local_table(index);
#endif
            add_cold_counter(counters_.refills, 1u);
            view_.armed_nonce.store(nonce_, ::std::memory_order_release);
            return status::ready;
        }
        void before_collection() noexcept
        {
            if(authority_ && ::std::this_thread::get_id() != native_owner_) { ::std::terminate(); }
            if(reconciling_) { return; } // original poll's disable can recurse
            reconciling_ = true;
            auto const count{detach_window()};
            if(count != 0uz)
            {
                auto account{::uwvm2::runtime::gc::sealed_account.load(::std::memory_order_acquire)};
                // Callback registration outlives genuine entries. Missing
                // accounting is an invariant failure, never silently lost stats.
                if(!account) { ::std::terminate(); }
                (void)account(reinterpret_cast<::std::uintptr_t>(&view_), reinterpret_cast<::std::uintptr_t>(module_), count, false);
            }
            reconciling_ = false;
            // Genuine page participant/exclusive remains held for ORIGINAL
            // root/census/pause/graph transaction. Never substitute readers==0.
        }
        void retire() noexcept
        {
            if(retired_ || !authority_) { return; }
            if(::std::this_thread::get_id() != native_owner_) { ::std::terminate(); }
            before_collection();
            add_cold_counter(counters_.retirements, 1u);
            stop_callback_.reset(); // synchronize foreign stop
            store_pin_.reset(); // outside all allocator/index locks
            // Memory safety: revoke only this native owner's TLS transport
            // borrow after settling credit and synchronizing the stop callback.
            // The live entry still owns page, counters and generation; a later
            // metrics flush must not borrow and drain an already retired page.
            if(::uwvm2::runtime::gc::actual_entry_page == authority_)
            { ::uwvm2::runtime::gc::actual_entry_page = nullptr; }
            authority_->retire(); retired_ = true; // borrows die before admission
        }
    };
}
#endif
