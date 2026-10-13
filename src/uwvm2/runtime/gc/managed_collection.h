/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# include <atomic>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <mutex>
# include <new>
# include <optional>
# include <utility>
# include <span>
# include "entry_admission.h"
# include "instance_phase.h"
# include "allocation_policy.h"
# include "collection_transaction.h"
# include "managed_exception_cohort.h"
# include <uwvm2/uwvm/runtime/storage/gc_static_roots.h>
# if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
#  include "managed_numeric_page.h"
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
#include "sealed_entry_compact_cursor.h"
#endif
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
    enum class managed_gc_rejection : unsigned char
    {
        none, not_cli_owned, phase_pending, unsupported_mode, native_roots_unknown,
        imports_or_preloads, debug_reader, rootless_generation, incomplete_cohort,
        exception_population_unqualified, multiple_executions, out_of_memory,
        malformed_population, heap_rejected
    };
    struct managed_gc_metrics
    {
        ::std::uint_least64_t accounted_allocations{}, attempts{}, eligible{}, pauses{}, collections{}, reclaimed{};
        ::std::uint_least64_t rejected_multiple{}, rejected_policy{}, rejected_heap{}, rejected_population{};
        managed_gc_rejection rejection{};
        bool precise_roots_requested{}, disabled{};
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        ::std::uint_least64_t registered_exception_collections{}, retired_exception_tokens{}, retired_exception_batches{}, unqualified_exception_paths{};
#endif
    };
    enum class managed_gc_configure_result : unsigned char
    { requested, no_aggregate_cohort, unsupported_mode, busy, already_published, unowned_or_uninitialized };
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Runtime-managed FIRST slice: real cold entry exclusion proves there is
    // exactly one guest owner during each transaction. The temporary pause
    // participant belongs to that owner; it is NOT evidence that omitted peers
    // are stopped. active>1 skips GC and preserves multi-host execution.
    class managed_collection_state
    {
        using module_type = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using store_type = ::uwvm2::uwvm::runtime::storage::gc_object_store;
        ::std::atomic_bool requested_{}, disabled_{};
        ::std::atomic_uint_least64_t forbidden_initializer_serial_{};
        ::std::atomic<managed_gc_rejection> rejection_{managed_gc_rejection::not_cli_owned};
        ::std::uint_least64_t initializer_serial_{};
        ::std::atomic_bool cohort_ready_{};
        ::std::mutex prepare_mutex_;
        ::std::unique_ptr<::std::shared_ptr<store_type>[]> stores_;
        ::std::unique_ptr<module_type const*[]> modules_;
        ::std::size_t store_count_{};
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        ::std::optional<managed_exception_cohort> exception_cohort_;
        ::std::atomic_uint_least64_t registered_exception_collections_{}, retired_exception_tokens_{}, retired_exception_batches_{}, unqualified_exception_paths_{};
#endif
        ::std::shared_ptr<::uwvm2::utils::thread::collection_pause_domain> pause_;
        ::std::atomic_uint_least64_t allocations_{}, attempts_{}, eligible_{}, pauses_{}, collections_{}, reclaimed_{};
        ::std::atomic_uint_least64_t rejected_multiple_{}, rejected_policy_{}, rejected_heap_{}, rejected_population_{};
        static constexpr ::std::size_t allocation_interval{4096uz};
        struct native_pressure_counter
        { managed_collection_state* owner; ::std::uint_least64_t serial; ::std::size_t allocations;
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
          ::std::size_t large_array_charge;
#endif
          // Statistics flushing and the collection interval are independent.
          // Keep scheduling progress across short owned guest entries; this
          // prefix was already added to allocations_ and must not be added twice.
          ::std::size_t accounted_in_interval;
        };
        inline static thread_local native_pressure_counter pressure_{};
    public:
        [[nodiscard]] bool roots_requested() const noexcept { return requested_.load(::std::memory_order_acquire); }
        [[nodiscard]] bool disabled() const noexcept { return disabled_.load(::std::memory_order_acquire); }
        // Called only by actual CLI storage admission while the cold entry word
        // is exclusive at zero, before runtime registry/code publication.
        [[nodiscard]] bool request_cli(::std::uint_least64_t serial) noexcept
        {
            if(serial == 0u) { return false; }
            initializer_serial_ = serial;
            requested_.store(true, ::std::memory_order_release);
            if(forbidden_initializer_serial_.load(::std::memory_order_acquire) == serial)
            { disable(managed_gc_rejection::native_roots_unknown); return true; }
#ifdef UWVM_CPP_EXCEPTIONS
            try { pause_ = ::std::make_shared<::uwvm2::utils::thread::collection_pause_domain>(1uz); }
            catch(...) { disable(managed_gc_rejection::out_of_memory); return true; }
#else
            disable(managed_gc_rejection::unsupported_mode); return true;
#endif
            disabled_.store(false, ::std::memory_order_release);
            rejection_.store(managed_gc_rejection::phase_pending, ::std::memory_order_relaxed);
            return true;
        }
        // Unknown native/raw entry permanently disqualifies this actual owned
        // instance even across backend reset. Reset is not release of a host
        // reference handle; only a fresh initializer serial can become eligible.
        void note_native_escape() noexcept
        {
            auto const serial{published_initializer_serial.load(::std::memory_order_acquire)};
            if(serial != 0u) { forbidden_initializer_serial_.store(serial, ::std::memory_order_release); }
            disable(managed_gc_rejection::native_roots_unknown);
        }
        void disable(managed_gc_rejection reason) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
            // Same-thread unknown escape/debug/EH disqualification retires all
            // reservations before admission reopens. No original allocator API.
            flush_actual_entry_page(true);
#endif
            rejection_.store(reason, ::std::memory_order_relaxed);
            disabled_.store(true, ::std::memory_order_release);
        }
        void flush_current_thread_pressure() noexcept
        {
            if(pressure_.owner == this)
            {
                // A host boundary flush reports pending statistics, not a
                // heap-pressure reset. Root authority is still freshly checked
                // by allocation_poll in the next actual owned entry.
                allocations_.fetch_add(pressure_.allocations - pressure_.accounted_in_interval,
                                       ::std::memory_order_relaxed);
                pressure_.accounted_in_interval = pressure_.allocations;
            }
        }
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        // DENY only. Record a true fallback/catch/reentry barrier before any
        // unregistered guest owner can escape; never issue roots from counts.
        void note_unqualified_exception() noexcept
        {
            if(roots_requested()) { unqualified_exception_paths_.fetch_add(1u,::std::memory_order_relaxed); }
            note_native_escape();
        }
        [[nodiscard]] bool bind_local_exception_cohort(managed_exception_cohort const& actual) noexcept
        {
            if(!roots_requested() || disabled()) { return false; }
            ::std::lock_guard lock{prepare_mutex_};
            if(exception_cohort_)
            {
                if(exception_cohort_->same_authority(actual)) { return true; }
                // Never replace a live origin under admission/publication locks.
                note_native_escape(); return false;
            }
            if(cohort_ready_.load(::std::memory_order_acquire))
            { note_native_escape(); return false; }
            exception_cohort_.emplace(actual); // Strong, typed, allocation-free copy.
            return true;
        }
#endif
        template<class Resolve>
        [[nodiscard]] bool prepare_cohort(::std::size_t count, Resolve&& resolve) noexcept
        {
            if(!roots_requested() || disabled()) { return false; }
            ::std::lock_guard lock{prepare_mutex_};
            if(cohort_ready_.load(::std::memory_order_acquire)) { return true; }
            constexpr auto max_count{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
            if(count == 0uz || count > max_count / sizeof(::std::shared_ptr<store_type>) ||
               count > max_count / sizeof(module_type const*))
            { disable(managed_gc_rejection::incomplete_cohort); return false; }
            ::std::unique_ptr<::std::shared_ptr<store_type>[]> stores{new(::std::nothrow) ::std::shared_ptr<store_type>[count]};
            ::std::unique_ptr<module_type const*[]> modules{new(::std::nothrow) module_type const*[count]};
            if(!stores || !modules) { disable(managed_gc_rejection::out_of_memory); return false; }
            for(::std::size_t index{}; index != count; ++index)
            {
                // [actual compiled/native registry resolver] pointer authority
                // comes from runtime module-map identity, not guest bits.
                auto const* module{resolve(index)};
                if(module == nullptr || !module->gc_store || !module->gc_store->valid())
                {
                    if(!disabled()) { disable(managed_gc_rejection::incomplete_cohort); }
                    return false;
                }
                for(::std::size_t previous{}; previous != index; ++previous)
                {
                    if(modules[previous] == module || stores[previous].get() == module->gc_store.get())
                    { disable(managed_gc_rejection::incomplete_cohort); return false; }
                }
                if(!module->imported_function_vec_storage.empty() || !module->imported_table_vec_storage.empty() ||
                   !module->imported_global_vec_storage.empty() || !module->imported_memory_vec_storage.empty() ||
                   !module->imported_tag_vec_storage.empty())
                { disable(managed_gc_rejection::imports_or_preloads); return false; }
                // Genuine native exception search/cleanup is unregistered here.
                // Keep standard EH execution, but never claim automatic GC of
                // its old C++ owners until that population is admitted.
                if(!module->local_defined_tag_vec_storage.empty()
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
                   && (count != 1uz || !exception_cohort_ || !exception_cohort_->matches(*module,initializer_serial_))
#endif
                  )
                { disable(managed_gc_rejection::exception_population_unqualified); return false; }
                // The first slice permits DEFINED tables (including anyref
                // allocation rings), passive segments and typed globals. Active
                // element initialization keeps its separate alias/mutator gate.
                // Completed active data below carries no managed references.
                for(auto const& element: module->local_defined_element_vec_storage)
                {
                    if(element.element.kind == ::uwvm2::uwvm::runtime::storage::wasm_element_segment_kind::active)
                    { disable(managed_gc_rejection::phase_pending); return false; }
                }
                for(auto const& data: module->local_defined_data_vec_storage)
                {
                    if(data.data.kind == ::uwvm2::uwvm::runtime::storage::wasm_data_segment_kind::active &&
                       (!module->gc_collection_phase.ready_for(initializer_serial_) ||
                        !::uwvm2::uwvm::runtime::storage::wasm_data_segment_is_dropped(data.data)))
                    { disable(managed_gc_rejection::phase_pending); return false; }
                    // A completed, dropped active DATA segment retains bytes, not
                    // Wasm GC references. Its installed owned linear-memory bytes
                    // cannot become GC roots. Imports remain rejected above;
                    // active ELEMENT segments retain their separate original gate.
                }
                modules[index] = module;
                stores[index] = module->gc_store;
            }
            // [new owned fixed arrays] publish only complete canonical candidates.
            // The actual collector re-proves control blocks and entire global
            // registry, including outside EMPTY stores, before any reclamation.
            stores_ = ::std::move(stores); modules_ = ::std::move(modules); store_count_ = count;
            cohort_ready_.store(true, ::std::memory_order_release);
            return true;
        }
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        [[nodiscard]] bool authorize_numeric_page(
            ::uwvm2::uwvm::runtime::storage::managed_numeric_entry_page& page, void const* actual_entry,
            managed_entry_admission::shared_lease const& shared,
            ::uwvm2::utils::thread::execution_domain::lease& generation, ::std::uintptr_t module_address) noexcept
        {
            // Extra FIRST experimental slice, after ORIGINAL cohort gates.
            // No imported/foreign/EH/debug/thread/global/segment populations.
            if(!is_cli_gc_execution() || !roots_requested() || disabled() || !cohort_ready_.load(::std::memory_order_acquire) ||
               store_count_ != 1uz || !pause_ || !generation || generation.stop_requested() ||
               published_initializer_serial.load(::std::memory_order_acquire) != initializer_serial_ || actual_entry_page != nullptr)
            { return false; }
            auto const* module{modules_[0uz]};
            if(reinterpret_cast<::std::uintptr_t>(module) != module_address ||
               !module->gc_collection_phase.ready_for(initializer_serial_) ||
               !module->local_defined_memory_vec_storage.empty() || !module->local_defined_global_vec_storage.empty() ||
               !module->local_defined_tag_vec_storage.empty() || !module->local_defined_element_vec_storage.empty() ||
               !module->local_defined_data_vec_storage.empty()) { return false; }
            if(!page.open_actual_entry(actual_entry, this, *module, initializer_serial_, shared, generation, pause_, 256uz*1024uz*1024uz))
            { return false; }
            actual_entry_page = &page; return true;
        }
#endif
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        // Called only after ORIGINAL authorize_numeric_page has minted the real
        // shared+exclusive lease, generation, pause participant and full cohort.
        [[nodiscard]] bool authorize_sealed_cursor(
            ::uwvm2::uwvm::runtime::storage::sealed_compact_entry& cursor,
            ::uwvm2::uwvm::runtime::storage::managed_numeric_entry_page& page,
            void const* actual_entry, ::std::uintptr_t address) noexcept
        {
            if(!roots_requested() || disabled() || !cohort_ready_.load(::std::memory_order_acquire) ||
               store_count_ != 1uz || !modules_ || !stores_ || !pause_ ||
               published_initializer_serial.load(::std::memory_order_acquire) != initializer_serial_)
            { return false; }
            auto const* module{modules_[0uz]}; // native cohort, never guest address dereference
            if(address != reinterpret_cast<::std::uintptr_t>(module) ||
               !page.belongs_to_collection(this, address, initializer_serial_)) { return false; }
            return cursor.attach_actual_entry(page, actual_entry, this, *module, initializer_serial_, &disabled_);
        }
        [[nodiscard]] sealed_attempt_account reconcile_sealed_attempts(
            ::uwvm2::uwvm::runtime::storage::sealed_compact_entry& cursor,
            ::std::uintptr_t ctx, ::std::uintptr_t module, ::std::size_t committed,
            bool prepare_next) noexcept
        {
            // Resolve OWN scope/cursor in the runtime callback before calling
            // this. Stop is allowed for cleanup, never for issuing another slot.
            if(!cursor.exact_cleanup_identity(ctx, module)) { return {}; }
            if(committed > 1024uz || (committed != 0uz &&
               (pressure_.owner != this || pressure_.serial != initializer_serial_ ||
                pressure_.allocations >= allocation_interval ||
                committed >= allocation_interval - pressure_.allocations)))
            { ::std::terminate(); }
            // Each committed slot was already initialized and published under
            // this actual entry. A subsequent disable forbids NEW attempts;
            // it cannot erase successful earlier attempts during retirement.
            // The strict bound above proves addition stays below the original
            // 4096th poll, cannot overflow, and cannot collect using stale roots.
            // Settle this native thread's scheduling pressure once in O(1).
            pressure_.allocations += committed;
            if(!prepare_next || !roots_requested() || disabled()) { return {}; }
            if(!cohort_ready_.load(::std::memory_order_acquire) || store_count_ != 1uz ||
               !modules_ || module != reinterpret_cast<::std::uintptr_t>(modules_[0uz]) ||
               published_initializer_serial.load(::std::memory_order_acquire) != initializer_serial_)
            { return {}; }
            // ORIGINAL body records THIS attempt before allocation, even if
            // prepare subsequently fails/OOM. It performs the 4096th poll now.
            allocation_poll(module);
            bool const usable{!disabled() && pressure_.owner == this &&
                pressure_.serial == initializer_serial_ && pressure_.allocations < allocation_interval};
            return {usable ? allocation_interval - 1uz - pressure_.allocations : 0uz, true, usable};
        }
#endif
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
        void allocation_poll_large_array(::std::uintptr_t module_address, ::std::size_t length
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            , managed_exception_cohort const* actual_exception_entry = nullptr,
              bool* native_retirement_queued = nullptr
#endif
            ) noexcept
        {
            if(!roots_requested() || disabled()) { return; }
            if(pressure_.owner != this || pressure_.serial != initializer_serial_)
            { flush_current_thread_pressure(); pressure_ = native_pressure_counter{
                this, initializer_serial_, 0uz,
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
                {},
#endif
                {}
            }; }
            // Conservative carrier charge: 1 MiB / sizeof(gc_object_value).
            // Packed arrays may use fewer bytes. This is a collection scheduling
            // budget, never a claimed RSS, allocation extent, or liveness proof.
            constexpr ::std::size_t limit{1uz * 1024uz * 1024uz /
                sizeof(::uwvm2::uwvm::runtime::storage::gc_object_value)};
            auto const charge{length < limit ? length : limit};
            bool const force{charge >= limit - pressure_.large_array_charge};
            pressure_.large_array_charge = force ? 0uz : pressure_.large_array_charge + charge;
            allocation_poll(module_address
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
                , actual_exception_entry, native_retirement_queued
#endif
                , force);
        }
#endif
        void allocation_poll(::std::uintptr_t module_address
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            , managed_exception_cohort const* actual_exception_entry = nullptr,
              bool* native_retirement_queued = nullptr
#endif
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
            , bool force_large_array = false
#endif
            ) noexcept
        {
            if(!roots_requested() || disabled()) { return; }
            if(pressure_.owner != this || pressure_.serial != initializer_serial_)
            { flush_current_thread_pressure(); pressure_ = native_pressure_counter{
                this, initializer_serial_, 0uz,
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
                {},
#endif
                {}
            }; }
            if(++pressure_.allocations != allocation_interval
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
                && !force_large_array
#endif
                ) { return; }
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
            pressure_.large_array_charge = 0uz;
#endif
            allocations_.fetch_add(pressure_.allocations - pressure_.accounted_in_interval,
                                   ::std::memory_order_relaxed);
            pressure_.allocations = 0uz;
            pressure_.accounted_in_interval = 0uz;
            attempts_.fetch_add(1u, ::std::memory_order_relaxed);
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            ::std::unique_ptr<managed_exception_retirement> retirement;
            if(exception_cohort_)
            {
                // Authority comes from the real outer scope's live generation,
                // source and admission members; a current TLS queue is necessary
                // for delayed release, but is never sufficient for admission.
                if(actual_exception_entry == nullptr ||
                   !exception_cohort_->same_authority(*actual_exception_entry) ||
                   !::uwvm2::utils::thread::deferred_native_owner_queue::active_on_current_thread())
                { note_native_escape(); rejected_policy_.fetch_add(1u,::std::memory_order_relaxed); return; }
                retirement.reset(new(::std::nothrow) managed_exception_retirement{*exception_cohort_});
                if(!retirement)
                { disable(managed_gc_rejection::out_of_memory); rejected_policy_.fetch_add(1u,::std::memory_order_relaxed); return; }
            }
#endif
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
            auto* page{actual_entry_page};
            bool const borrowed{page != nullptr && page->belongs_to_collection(this, module_address, initializer_serial_)};
            // Never recursively acquire exclusive(1). A borrow is accepted only
            // from this actual entry, native thread, cohort and held lease.
            if(page != nullptr && !borrowed) { flush_actual_entry_page(true); page = nullptr; }
            ::std::optional<managed_entry_admission::exclusive_lease> exclusive{};
            if(!borrowed) { exclusive.emplace(runtime_gc_entry_admission.try_exclusive(1uz)); }
            if(!borrowed && !static_cast<bool>(*exclusive))
#else
            auto exclusive{runtime_gc_entry_admission.try_exclusive(1uz)};
            if(!exclusive)
#endif
            { rejected_multiple_.fetch_add(1u, ::std::memory_order_relaxed); return; }
            // This CAS excludes every outer execution and counted initializer
            // through the ENTIRE census/sweep. It is held until pause ticket,
            // participant and all native root borrows have retired.
            if(disabled() || !cohort_ready_.load(::std::memory_order_acquire) ||
               published_initializer_serial.load(::std::memory_order_acquire) != initializer_serial_)
            { rejected_policy_.fetch_add(1u, ::std::memory_order_relaxed); return; }
            bool module_in_cohort{};
            for(::std::size_t index{}; index != store_count_; ++index)
            {
                auto const* module{modules_[index]};
                if(!module->gc_collection_phase.ready_for(initializer_serial_))
                { rejected_policy_.fetch_add(1u, ::std::memory_order_relaxed); return; }
                module_in_cohort = module_in_cohort || reinterpret_cast<::std::uintptr_t>(module) == module_address;
            }
            if(!module_in_cohort)
            { disable(managed_gc_rejection::incomplete_cohort); rejected_policy_.fetch_add(1u, ::std::memory_order_relaxed); return; }
            eligible_.fetch_add(1u, ::std::memory_order_relaxed);
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
            if(borrowed && page->drain(::uwvm2::uwvm::runtime::storage::numeric_page_drain_reason::pause) !=
                ::uwvm2::uwvm::runtime::storage::numeric_page_status::ok) { ::std::terminate(); }
            auto participant{borrowed ? ::uwvm2::utils::thread::collection_pause_domain::participant{} : pause_->enter()};
            auto const* actual_participant{borrowed ? &page->collection_participant() : &participant};
            if(!*actual_participant)
#else
            auto participant{pause_->enter()};
            if(!participant)
#endif
            { rejected_population_.fetch_add(1u, ::std::memory_order_relaxed); return; }
            collection_root_context context{};
            prepare_collection_root_context(context, current_root_frames(), {});
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            auto visit_static{[&](auto& visitor) noexcept
            {
                auto const roots{::uwvm2::uwvm::runtime::storage::visit_quiescent_cohort_static_roots(
                    {modules_.get(), store_count_}, visitor)};
                return roots.status == ::uwvm2::uwvm::runtime::storage::gc_static_root_status::ok;
            }};
            auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{100}};
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
            auto const& collector_participant{*actual_participant};
#else
            auto const& collector_participant{participant};
#endif
            auto const outcome{
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
                retirement ? collect_managed_registered_exception_transaction(*pause_,collector_participant,context,
                    {stores_.get(),store_count_},exception_cohort_->census(),retirement->batch(),deadline,visit_static) :
#endif
                collect_managed_aggregate_transaction(*pause_,collector_participant,context,
                    {stores_.get(),store_count_},deadline,visit_static)};
#else
            auto const outcome{collect_managed_aggregate_transaction(*pause_,
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
                *actual_participant,
#else
                participant,
#endif
                context,
                {stores_.get(), store_count_}, ::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{100},
                [&](auto& visitor) noexcept
                {
                    auto const roots{::uwvm2::uwvm::runtime::storage::visit_quiescent_cohort_static_roots(
                        {modules_.get(), store_count_}, visitor)};
                    return roots.status == ::uwvm2::uwvm::runtime::storage::gc_static_root_status::ok;
                })};
#endif
            // Transaction resets its ticket before returning. Reset participant
            // before exclusive releases, so no new guest can observe old borrows.
            participant.reset();
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            if(retirement && outcome.status==collection_transaction_status::collected)
            { registered_exception_collections_.fetch_add(1u,::std::memory_order_relaxed); }
            if(retirement && !retirement->batch().empty())
            {
                // Actual detached token nodes, not payload bytes/RSS. No owner
                // is released here: locks/pause retire first, queue only links.
                retired_exception_tokens_.fetch_add(retirement->batch().size(),::std::memory_order_relaxed);
                retired_exception_batches_.fetch_add(1u,::std::memory_order_relaxed);
                // [heap-owned detached batch] ownership moves to the REAL outer
                // queue. retire only links; it never invokes the callback here.
                auto* queued{retirement.release()};
                ::uwvm2::utils::thread::deferred_native_owner_queue::retire(*queued);
                // [actual native caller's optional complete bool slot] report only;
                // this observation is never admission or root-census authority.
                if(native_retirement_queued != nullptr) { *native_retirement_queued = true; }
            }
#endif
            if(outcome.participants != 0uz) { pauses_.fetch_add(1u, ::std::memory_order_relaxed); }
            if(outcome.status == collection_transaction_status::collected)
            {
                collections_.fetch_add(1u, ::std::memory_order_relaxed);
                reclaimed_.fetch_add(outcome.reclaimed, ::std::memory_order_relaxed);
                rejection_.store(managed_gc_rejection::none, ::std::memory_order_relaxed);
            }
            else if(outcome.status == collection_transaction_status::heap_rejected)
            {
                rejected_heap_.fetch_add(1u, ::std::memory_order_relaxed);
                rejection_.store(managed_gc_rejection::heap_rejected, ::std::memory_order_relaxed);
            }
            else
            {
                rejected_population_.fetch_add(1u, ::std::memory_order_relaxed);
                rejection_.store(managed_gc_rejection::malformed_population, ::std::memory_order_relaxed);
            }
        }
        [[nodiscard]] managed_gc_metrics metrics() const noexcept
        {
            return {allocations_.load(::std::memory_order_relaxed), attempts_.load(::std::memory_order_relaxed),
                eligible_.load(::std::memory_order_relaxed), pauses_.load(::std::memory_order_relaxed),
                collections_.load(::std::memory_order_relaxed), reclaimed_.load(::std::memory_order_relaxed),
                rejected_multiple_.load(::std::memory_order_relaxed), rejected_policy_.load(::std::memory_order_relaxed),
                rejected_heap_.load(::std::memory_order_relaxed), rejected_population_.load(::std::memory_order_relaxed),
                rejection_.load(::std::memory_order_relaxed), roots_requested(), disabled()
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
                , registered_exception_collections_.load(::std::memory_order_relaxed), retired_exception_tokens_.load(::std::memory_order_relaxed),
                  retired_exception_batches_.load(::std::memory_order_relaxed), unqualified_exception_paths_.load(::std::memory_order_relaxed)
#endif
                };
        }
        void close() noexcept
        { disabled_.store(true, ::std::memory_order_release); if(pause_) { pause_->close(); } }
        // Call only AFTER actual execution drain. No own participant/ticket or
        // borrowed module can remain; native loader synchronization still holds.
        void reset_after_execution_drain() noexcept
        {
            close(); flush_current_thread_pressure();
            // Actual instance retirement ends this thread's scheduling epoch.
            // Other drained threads have flushed their statistics; a new
            // initializer serial rejects their old progress at the next poll.
            if(pressure_.owner == this) { pressure_ = {}; }
            if(pause_) { pause_->drain(); }
            cohort_ready_.store(false, ::std::memory_order_release);
            modules_.reset(); stores_.reset(); store_count_ = 0uz; pause_.reset();
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            exception_cohort_.reset(); // Actual native drain already completed.
            registered_exception_collections_=0u; retired_exception_tokens_=0u; retired_exception_batches_=0u; unqualified_exception_paths_=0u;
#endif
            requested_.store(false, ::std::memory_order_release); initializer_serial_ = 0u;
            rejection_.store(managed_gc_rejection::not_cli_owned, ::std::memory_order_relaxed);
            allocations_ = 0u; attempts_ = 0u; eligible_ = 0u; pauses_ = 0u; collections_ = 0u; reclaimed_ = 0u;
            rejected_multiple_ = 0u; rejected_policy_ = 0u; rejected_heap_ = 0u; rejected_population_ = 0u;
        }
    };
#endif
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
