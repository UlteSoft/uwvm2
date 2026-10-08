/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
#include <chrono>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <uwvm2/utils/thread/collection_pause_domain.h>
#include <uwvm2/runtime/gc/frame_roots.h>
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#if defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) && UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION == 1
#include <uwvm2/runtime/exception/native_roots.h>
#include <utility>
#endif

#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

// Private production candidate: managed_collection supplies the actual cold
// admission closure before entering this protocol. An enrolled mutator must
// supply a complete typed population; this helper alone proves neither omitted
// peers nor interpreter, host, initializer or exception roots stopped.
UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
#if defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) && UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION == 1
#if !defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) || UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS != 1 || !defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) || UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH != 1
# error Registered exception collection requires the actual native-owner domain and typed exception graph.
#endif
#endif
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    enum class root_population : unsigned char { unavailable, precise_native };
    struct collection_root_context
    {
        root_population population{root_population::unavailable};
        root_frame const* frames{};
        // Complete native carriers from the enclosing admitted activation.
        // Their owner must outlive the participant's park/collecting ticket.
        // Empty aliases and a store lease alone do not prove object liveness.
        ::std::span<root_reference const> native_roots{};
    };
    enum class collection_transaction_status : unsigned char
    {
        collected, concurrent_request, timeout, closed, invalid_ticket,
        unavailable_population, invalid_population, static_roots_rejected,
        size_overflow, out_of_memory, heap_rejected
    };
    struct collection_transaction_result
    {
        collection_transaction_status status{collection_transaction_status::invalid_ticket};
        ::uwvm2::uwvm::runtime::storage::gc_object_status heap_status{
            ::uwvm2::uwvm::runtime::storage::gc_object_status::invalid_store};
        ::std::size_t participants{}, roots{}, reclaimed{};
    };

    // This borrowed context is published only by the owning native thread after
    // copying its compiler-validated reference locations. Updating it during a
    // blocking scope or a collecting ticket is forbidden. No guest address can
    // supply a context, frame or span to this host-only API.
    inline void prepare_collection_root_context(collection_root_context& context,
        root_frame const* frames, ::std::span<root_reference const> native_roots) noexcept
    {
        // [calling-thread live native chain][complete owner-held native roots]
        // [safe                                                             ]
        // ^^ replace only unpublished borrows; the subsequent park retains both.
        context.frames = frames;
        context.native_roots = native_roots;
        context.population = root_population::precise_native;
    }

    // The caller holds its execution-generation lease AND every canonical store
    // pin throughout this function. visit_static must enumerate every admitted
    // module global/table/element root and native root registration while stopped,
    // or return false. It cannot reenter, mutate, retain a borrow or throw. All
    // guest/native mutators must be enrolled; this helper cannot infer admission
    // from an OS thread id, a raw store address, or an empty frame chain.
    // Only the stopped callback can call the collector. A partial failed root
    // census, an unavailable backend, or a timeout never authorizes reclamation.
#if defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) && UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION == 1
    namespace collection_transaction_details
    {
    template<class VisitStatic, class HeapCollect>
    [[nodiscard]] inline collection_transaction_result collect_managed_transaction_impl(
        ::uwvm2::utils::thread::collection_pause_domain& domain,
        ::uwvm2::utils::thread::collection_pause_domain::participant const& initiator,
        collection_root_context const& own_context,
        ::std::span<::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> const> stores,
        ::std::chrono::steady_clock::time_point deadline, VisitStatic&& visit_static, HeapCollect&& collect_heap) noexcept
#else
    template<class VisitStatic>
    [[nodiscard]] inline collection_transaction_result collect_managed_aggregate_transaction(
        ::uwvm2::utils::thread::collection_pause_domain& domain,
        ::uwvm2::utils::thread::collection_pause_domain::participant const& initiator,
        collection_root_context const& own_context,
        ::std::span<::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> const> stores,
        ::std::chrono::steady_clock::time_point deadline, VisitStatic&& visit_static) noexcept
#endif
    {
        namespace control = ::uwvm2::utils::thread;
        namespace storage = ::uwvm2::uwvm::runtime::storage;
        static_assert(::std::is_same_v<root_reference, storage::gc_reference>);
        collection_transaction_result result{};
        if(!initiator) { return result; }
        // [initiator-owned complete native context] park/request holds this
        // borrow only until the ticket is reset, before caller state resumes.
        auto ticket{domain.request_pause(initiator, ::std::addressof(own_context))};
        if(!ticket)
        {
            // Another collecting mutator needs this one's roots. Poll rather
            // than busy retrying or waiting for a pause that we did not own.
            if(domain.pause_requested())
            {
                initiator.poll(::std::addressof(own_context));
                result.status = collection_transaction_status::concurrent_request;
            }
            return result;
        }
        auto const stopped{domain.wait_until_paused(ticket, deadline)};
        if(stopped != control::collection_pause_result::paused)
        {
            result.status = stopped == control::collection_pause_result::timeout ?
                collection_transaction_status::timeout :
                stopped == control::collection_pause_result::closed ?
                    collection_transaction_status::closed : collection_transaction_status::invalid_ticket;
            // [live collecting context borrow] cancel before returning control
            // to the caller, which may alter roots or retry an allocation.
            ticket.reset();
            return result;
        }
        auto const committed{domain.while_stopped(ticket,
            [&](control::collection_pause_domain::stopped_view participants) noexcept
        {
            result.participants = participants.participant_count();
            constexpr auto max_roots{static_cast<::std::size_t>(
                (::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(root_reference)};
            auto visit_population{[&](auto& visitor) noexcept
            {
                bool complete{true};
                participants.for_each([&](control::collection_pause_domain::stopped_participant stopped_participant) noexcept
                {
                    if(!complete) { return; }
                    if(stopped_participant.root_context == nullptr)
                    {
                        result.status = collection_transaction_status::unavailable_population;
                        complete = false;
                        return;
                    }
                    // [actual owner-published context][domain-held native park]
                    // [safe                                                     ]
                    // ^^ the fixed enrolled slot retains this initialized object;
                    // no native owner can resume or mutate it during the callback.
                    auto const& context{*static_cast<collection_root_context const*>(stopped_participant.root_context)};
                    if(context.population != root_population::precise_native)
                    {
                        result.status = collection_transaction_status::unavailable_population;
                        complete = false;
                        return;
                    }
                    if(context.native_roots.size() > max_roots ||
                       (!context.native_roots.empty() && context.native_roots.data() == nullptr))
                    {
                        result.status = collection_transaction_status::invalid_population;
                        complete = false;
                        return;
                    }
                    auto const frames{visit_quiescent_frame_roots(context.frames, visitor)};
                    if(frames.status != frame_root_status::ok)
                    {
                        if(result.status != collection_transaction_status::size_overflow)
                        { result.status = collection_transaction_status::invalid_population; }
                        complete = false;
                        return;
                    }
                    for(auto const reference : context.native_roots)
                    {
                        if(!frame_root_details::runtime_kind(reference) || !visitor(reference))
                        {
                            if(result.status != collection_transaction_status::size_overflow)
                            { result.status = collection_transaction_status::invalid_population; }
                            complete = false;
                            return;
                        }
                    }
                });
                return complete;
            }};
            auto count{[&](root_reference) noexcept
            {
                if(result.roots == max_roots)
                { result.status = collection_transaction_status::size_overflow; return false; }
                ++result.roots;
                return true;
            }};
            static_assert(noexcept(visit_static(count)));
            if(!visit_population(count)) { return; }
            if(!visit_static(count))
            {
                if(result.status != collection_transaction_status::size_overflow)
                { result.status = collection_transaction_status::static_roots_rejected; }
                return;
            }
            ::std::unique_ptr<root_reference[]> roots{};
            if(result.roots != 0uz)
            {
                // [checked count * sizeof(complete carrier)] native allocation
                // [safe                                    ] each array element
                // is a real initialized C++ reference; no stack-bit inference.
                roots.reset(new (::std::nothrow) root_reference[result.roots]);
                if(!roots)
                { result.status = collection_transaction_status::out_of_memory; return; }
            }
            ::std::size_t copied{};
            auto copy{[&](root_reference reference) noexcept
            {
                if(copied == result.roots) { return false; }
                // [roots, roots + result.roots) owns complete live carriers.
                // [safe                       ] copied names an actual element.
                roots[copied++] = reference;
                return true;
            }};
            static_assert(noexcept(visit_static(copy)));
            if(!visit_population(copy)) { return; }
            if(!visit_static(copy) || copied != result.roots)
            {
                // Both passes ran under the same complete stop. A disagreeing
                // static visitor or failed copy cannot reach the heap collector.
                if(result.status != collection_transaction_status::size_overflow)
                { result.status = collection_transaction_status::static_roots_rejected; }
                return;
            }
#if defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) && UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION == 1
            static_assert(noexcept(collect_heap(stores.data(), stores.size(), roots.get(), result.roots, result.reclaimed)));
            result.heap_status = collect_heap(stores.data(), stores.size(), roots.get(), result.roots, result.reclaimed);
#else
            result.heap_status = storage::gc_object_store::collect_exclusive_aggregate_domain(
                stores.data(), stores.size(), roots.get(), result.roots, result.reclaimed);
#endif
            result.status = result.heap_status == storage::gc_object_status::ok ?
                collection_transaction_status::collected : collection_transaction_status::heap_rejected;
        })};
        if(committed != control::collection_pause_result::paused)
        {
            result.status = committed == control::collection_pause_result::closed ?
                collection_transaction_status::closed : collection_transaction_status::invalid_ticket;
        }
        // [all stopped root borrows] end the transaction before roots/native
        // frames can change. No borrowed context or carrier escapes the callback.
        ticket.reset();
        return result;
    }
#if defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) && UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION == 1
    }

    // Preserve the old aggregate-only front door: even in this experimental
    // composition it never omits unregistered exception roots to reclaim them.
    template<class VisitStatic>
    [[nodiscard]] inline collection_transaction_result collect_managed_aggregate_transaction(
        ::uwvm2::utils::thread::collection_pause_domain& domain,
        ::uwvm2::utils::thread::collection_pause_domain::participant const& initiator,
        collection_root_context const& own_context,
        ::std::span<::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> const> stores,
        ::std::chrono::steady_clock::time_point deadline, VisitStatic&& visit_static) noexcept
    {
        auto collect_heap{[](::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> const* cohort,
                             ::std::size_t count, root_reference const* roots, ::std::size_t root_count,
                             ::std::size_t& reclaimed) noexcept
        {
            return ::uwvm2::uwvm::runtime::storage::gc_object_store::collect_exclusive_aggregate_domain(
                cohort, count, roots, root_count, reclaimed);
        }};
        return collection_transaction_details::collect_managed_transaction_impl(
            domain, initiator, own_context, stores, deadline, ::std::forward<VisitStatic>(visit_static), collect_heap);
    }

    // The actual caller must already close every native entry and supply a
    // COMPLETE registered immutable owner domain for this canonical generation.
    // A census, TLS selection or registration count is not that authority.
    // retired MUST be declared outside the caller's exclusive admission scope
    // and stopped callback, with canonical generation/cohort pins still live
    // through its eventual reset/destruction AFTER native execution resumes.
    template<class VisitStatic>
    [[nodiscard]] inline collection_transaction_result collect_managed_registered_exception_transaction(
        ::uwvm2::utils::thread::collection_pause_domain& domain,
        ::uwvm2::utils::thread::collection_pause_domain::participant const& initiator,
        collection_root_context const& own_context,
        ::std::span<::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store> const> stores,
        ::uwvm2::runtime::exception::native_exception_root_domain const& native_domain,
        ::uwvm2::uwvm::runtime::storage::gc_object_store::retired_exception_batch& retired,
        ::std::chrono::steady_clock::time_point deadline, VisitStatic&& visit_static) noexcept
    {
        using storage = ::uwvm2::uwvm::runtime::storage::gc_object_store;
        using status = ::uwvm2::uwvm::runtime::storage::gc_object_status;
        if(!retired.empty())
        {
            collection_transaction_result invalid{};
            invalid.status = collection_transaction_status::heap_rejected;
            invalid.heap_status = status::invalid_value;
            return invalid;
        }
        auto collect_heap{[&](::std::shared_ptr<storage> const* cohort, ::std::size_t count,
                             root_reference const* roots, ::std::size_t root_count,
                             ::std::size_t& reclaimed) noexcept
        {
            auto result{status::invalid_value};
            // Lock order: real stopped callback -> native owner census ->
            // canonical cohort -> exception registry -> recipient index.
            // Borrow immutable owners directly. No copied shared owner or
            // arbitrary native destructor is released inside these locks.
            bool const visited{native_domain.with_registered_roots(
                [&](::uwvm2::runtime::exception::native_exception_root_domain::registered_view const& view) noexcept
            {
                result = storage::collect_exclusive_registered_exception_domain(
                    cohort, count, roots, root_count, view, reclaimed, retired);
                return true;
            })};
            return visited ? result : status::invalid_value;
        }};
        return collection_transaction_details::collect_managed_transaction_impl(
            domain, initiator, own_context, stores, deadline, ::std::forward<VisitStatic>(visit_static), collect_heap);
    }
#endif
#endif
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
