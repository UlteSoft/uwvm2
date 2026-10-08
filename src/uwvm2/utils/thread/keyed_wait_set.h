/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io_dsal/array.h>
# include <atomic>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <memory>
# include <exception>
# include <span>
# include <vector>
# include <utility>
# include <uwvm2/utils/macro/push_macros.h>
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#  include <condition_variable>
#  include <mutex>
#  include <stop_token>
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::thread
{
    // A resource identity and a position are separate so moving backing storage
    // does not change wait identity. The owner must keep the resource alive until
    // all its waiters finish; reuse of an identity before that is not permitted.
    struct wait_key
    {
        void const* resource{};
        ::std::uint_least64_t position{};
        friend constexpr bool operator==(wait_key const&, wait_key const&) noexcept = default;
    };
    enum class keyed_wait_result { notified, not_equal, timed_out, cancelled, closed, too_many_waiters };

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    inline constexpr bool has_keyed_wait_set{true};

    // Owner-scoped wait registry, with no guest/runtime types or global registry.
    // An ordinary thread's wait node lives on its stack; opt-in cold batches
    // retain nonmoving heap nodes. All queue links and notifications
    // use the same shard mutex, so comparison + insertion cannot lose a wake.
    // This utility is ONLY on blocking wait/notify paths, never plain memory access.
    // close() is idempotent, prohibits new waits and cancels existing ones.
    // close_and_drain() also waits for nodes and stop callbacks to be released.
    // Destruction requires the owner to stop new callers and join other users;
    // it must not run on a thread currently blocked in this same wait set.
    class keyed_wait_set
    {
        struct waiter
        {
            wait_key key{};
            waiter* previous{};
            waiter* next{};
            bool notified{};
            ::std::condition_variable event{};
        };
        struct shard
        {
            ::std::mutex mutex{};
            ::std::condition_variable drained{};
            waiter* first{};
            waiter* last{};
            ::std::size_t active_calls{};
        };
        // Count the whole wait call, not just its linked node. A stop callback
        // may be running after dequeue; drain must also cover its unregistration.
        struct call_registration
        {
            shard& bucket;
            explicit call_registration(shard& value) : bucket{value}
            { ::std::lock_guard lock{bucket.mutex}; ++bucket.active_calls; }
            ~call_registration()
            {
                ::std::lock_guard lock{bucket.mutex};
                if(--bucket.active_calls == 0uz) { bucket.drained.notify_all(); }
            }
        };
        static constexpr ::std::size_t shard_count{64uz};
        ::fast_io::array<shard, shard_count> shards{};
        ::std::atomic_bool closing{};

        [[nodiscard]] shard& select(wait_key key) noexcept
        {
            auto const identity{reinterpret_cast<::std::uintptr_t>(key.resource)};
            auto const hash{(identity >> 4u) ^ (identity >> 13u) ^ key.position ^ (key.position >> 17u)};
            return shards[hash & (shard_count - 1uz)];
        }
        static void link(shard& bucket, waiter& node) noexcept
        {
            // [live shard] [nonmoving caller-owned node] [registered live nodes]
            // [safe     ] [safe                       ] [safe                 ]
            // All links borrow under bucket.mutex; stack waits cannot return,
            // and a prepared batch cannot destroy an active node, while linked.
            node.previous = bucket.last;
            // node.previous is null or the still-registered tail in this locked shard.
            if(bucket.last != nullptr) { bucket.last->next = ::std::addressof(node); }
            // Previous tail now points to the current retained nonmoving node.
            else { bucket.first = ::std::addressof(node); }
            // Empty-list head now points to the current retained nonmoving node.
            bucket.last = ::std::addressof(node);
            // [first ... node] | end
            // [safe         ] | unsafe
            //            ^^ last; node.next remains null.
        }
        static void unlink(shard& bucket, waiter& node) noexcept
        {
            // [previous?] [node] [next?]
            // [safe     ] [safe] [safe ] all protected by bucket.mutex
            if(node.previous != nullptr) { node.previous->next = node.next; }
            else { bucket.first = node.next; }
            // Predecessor/head now bypasses node, retaining a registered successor or null.
            if(node.next != nullptr) { node.next->previous = node.previous; }
            else { bucket.last = node.previous; }
            // Successor/tail now bypasses node, retaining a registered predecessor or null.
            // No queue link references node. Its caller still joins callback
            // cleanup before releasing stack/batch storage.
            if(bucket.first == nullptr) { bucket.drained.notify_all(); }
        }
        using clock = ::std::chrono::steady_clock;
        [[nodiscard]] static clock::time_point deadline(::std::int_least64_t nanoseconds) noexcept
        {
            auto const now{clock::now()};
            auto const requested{::std::chrono::nanoseconds{nanoseconds}};
            auto const available{clock::time_point::max() - now};
            // Compare in floating duration before integer conversion: an i64
            // nanosecond timeout may exceed the remaining monotonic clock range.
            using wide_ns = ::std::chrono::duration<long double, ::std::nano>;
            if(wide_ns{requested} >= wide_ns{available}) { return clock::time_point::max(); }
            return now + ::std::chrono::ceil<clock::duration>(requested);
        }

    public:
        // Generic HOST observation only, no resource pointer or native address
        // in the copied DATA. Resource identity is comparison-only below; VM
        // consumers must supply their genuine canonical owner separately.
        struct suspension_snapshot
        {
            ::std::uint_least64_t position{};
            ::std::int_least64_t remaining_nanoseconds{-1};
            ::std::size_t pending_predecessors{};
            bool notified{}, cancelled{}, closed{};
        };
        struct suspension_result
        {
            keyed_wait_result outcome{keyed_wait_result::cancelled};
            // HOST cleanup outcome only; NEVER a fourth guest wait result.
            bool aborted_by_suspension{};
        };
        class prepared_wait_batch;
        class suspension_borrow final
        {
            friend class keyed_wait_set;
            friend class prepared_wait_batch;
            keyed_wait_set& owner_;
            shard& bucket_;
            waiter& node_;
            clock::time_point until_;
            bool infinite_;
            ::std::stop_token cancellation_;
            suspension_borrow(keyed_wait_set& owner, shard& bucket, waiter& node,
                clock::time_point until, bool infinite, ::std::stop_token cancellation) noexcept
                : owner_{owner}, bucket_{bucket}, node_{node}, until_{until},
                  infinite_{infinite}, cancellation_{::std::move(cancellation)} {}
        public:
            suspension_borrow(suspension_borrow const&) = delete;
            suspension_borrow& operator=(suspension_borrow const&) = delete;
            suspension_borrow(suspension_borrow&&) = delete;
            suspension_borrow& operator=(suspension_borrow&&) = delete;
            // Synchronous only. The stack node remains linked through the real
            // suspension; this borrow cannot outlive suspend(). Visitor must
            // not re-enter this shard/domain or call a host/guest provider.
            template<typename Visitor>
            [[nodiscard]] bool inspect(wait_key actual_resource, Visitor&& visitor) const noexcept
            {
                ::std::lock_guard lock{bucket_.mutex};
                if(node_.key != actual_resource) { return false; }
                suspension_snapshot copy{}; copy.position=node_.key.position;
                copy.notified=node_.notified;copy.cancelled=cancellation_.stop_requested();copy.closed=owner_.is_closed();
                auto const* cursor{bucket_.first}; bool found{};
                // [actual locked live queue] | null; every node is stack-pinned
                // [safe] compare membership BEFORE accepting this node's rank.
                while(cursor != nullptr)
                {
                    if(cursor == ::std::addressof(node_)) { found=true;break; }
                    if(cursor->key == node_.key && !cursor->notified)
                    {
                        if(copy.pending_predecessors == SIZE_MAX) { return false; }
                        ++copy.pending_predecessors;
                    }
                    cursor=cursor->next;
                    // [visited][remaining live queue] | null
                    // [safe] loop tests successor before reading its fields.
                }
                if(!found) { return false; }
                if(!infinite_)
                {
                    auto const now{clock::now()};
                    if(until_ <= now) { copy.remaining_nanoseconds=0; }
                    else
                    {
                        auto const remaining{until_-now};
                        using wide_ns=::std::chrono::duration<long double,::std::nano>;
                        if(wide_ns{remaining}.count() >= static_cast<long double>(INT64_MAX))
                        { copy.remaining_nanoseconds=INT64_MAX; }
                        else { copy.remaining_nanoseconds=::std::chrono::ceil<::std::chrono::nanoseconds>(remaining).count(); }
                    }
                }
                static_assert(noexcept(::std::forward<Visitor>(visitor)(copy)),"suspension visitor must be nonthrowing");
                ::std::forward<Visitor>(visitor)(copy);return true;
            }
        };

        // Opt-in HOST-managed wait. The original wait() body stays byte-exact:
        // callers without a suspension policy allocate/register no extra state.
        // Policy register_waker(ctx, fn) returns a stack-lifetime RAII listener;
        // requested() is an atomic wake hint ONLY; suspend(borrow) performs its
        // actual participant/domain proof OUTSIDE the wait-shard mutex and
        // returns false only for a genuine HOST-owned cleanup decision.
        template<typename Matches,typename Suspension>
        [[nodiscard]] suspension_result wait_suspendable(wait_key key,::std::int_least64_t timeout_ns,
            Matches&& matches,Suspension& suspension,::std::stop_token cancellation={},
            ::std::size_t max_waiters_per_key=SIZE_MAX)
        {
            static_assert(noexcept(suspension.requested()),"wake hint must not wait or throw");
            auto& bucket{select(key)};call_registration active_call{bucket};waiter node{.key=key};
            struct wake_context { shard& bucket;waiter& node; } context{bucket,node};
            // Register BEFORE owning shard. Destruction is AFTER shard release,
            // so real controller request uses domain -> shard and never reverses
            // it. Node/context survive listener destruction and stop callbacks.
            auto listener{suspension.register_waker(::std::addressof(context),+[](void* address) noexcept
            {
                auto& actual{*static_cast<wake_context*>(address)};
                // [real synchronous listener context][live stack node]
                // [safe] only the source registration supplies this address;
                // it cannot outlive this wait and no guest/file pointer enters.
                ::std::lock_guard lock{actual.bucket.mutex};actual.node.event.notify_one();
            })};
            if(!listener) { return {keyed_wait_result::cancelled,true}; }
            ::std::stop_callback wake_on_stop{cancellation,[&]
            { ::std::lock_guard lock{bucket.mutex};node.event.notify_one(); }};
            ::std::unique_lock lock{bucket.mutex};
            if(is_closed()) { return {keyed_wait_result::closed,false}; }
            if(cancellation.stop_requested()) { return {keyed_wait_result::cancelled,false}; }
            if(!::std::forward<Matches>(matches)()) { return {keyed_wait_result::not_equal,false}; }
            if(timeout_ns == 0) { return {keyed_wait_result::timed_out,false}; }
            if(bucket.active_calls > max_waiters_per_key)
            {
                ::std::size_t matching{};auto const* cursor{bucket.first};
                // [live locked queue] | null
                // [safe] every node remains registered until its shard unlocks.
                while(cursor != nullptr)
                {
                    if(cursor->key == key && !cursor->notified) { ++matching; }
                    cursor=cursor->next; // loop checks successor before dereference.
                }
                if(matching >= max_waiters_per_key) { return {keyed_wait_result::too_many_waiters,false}; }
            }
            auto const until{timeout_ns < 0 ? clock::time_point::max() : deadline(timeout_ns)};
            link(bucket,node);
            struct registration
            {
                shard& bucket;waiter& node;::std::unique_lock<::std::mutex>& lock;
                ~registration() noexcept
                {
                    // suspension runs unlocked. A host lock failure must never
                    // splice the live queue without owning its actual mutex.
                    // Reacquisition failure in this noexcept cleanup terminates
                    // before any unsafe link access, as condition_variable does.
                    if(!lock.owns_lock()) { lock.lock(); }
                    unlink(bucket,node);
                }
            } cleanup{bucket,node,lock}; // original wait() cleanup remains unchanged
            suspension_borrow borrowed{*this,bucket,node,until,timeout_ns < 0,cancellation};
            static_assert(noexcept(suspension.suspend(borrowed)),"real suspension must clean up without throwing");
            auto const ready{[&]
            { return node.notified || is_closed() || cancellation.stop_requested() || suspension.requested(); }};
            for(;;)
            {
                // Original arbitration wins BEFORE a new suspension: notification
                // precedes cancellation/close/timeout, exactly as wait(). No
                // repeated matches() load or removal/reinsertion can steal wake.
                if(node.notified) { return {keyed_wait_result::notified,false}; }
                if(cancellation.stop_requested()) { return {keyed_wait_result::cancelled,false}; }
                if(is_closed()) { return {keyed_wait_result::closed,false}; }
                if(timeout_ns >= 0 && clock::now() >= until) { return {keyed_wait_result::timed_out,false}; }
                if(suspension.requested())
                {
                    lock.unlock(); // Actual linked node stays live; NO shard -> domain.
                    bool const keep_waiting{suspension.suspend(borrowed)};
                    lock.lock(); // Reacquire BEFORE any cleanup/arbitration/node read.
                    // A real notification/cancellation/close/deadline can win
                    // WHILE suspend() is unlocked. Preserve the original FIFO
                    // winner before considering a still-pending HOST abort.
                    if(node.notified) { return {keyed_wait_result::notified,false}; }
                    if(cancellation.stop_requested()) { return {keyed_wait_result::cancelled,false}; }
                    if(is_closed()) { return {keyed_wait_result::closed,false}; }
                    if(timeout_ns >= 0 && clock::now() >= until) { return {keyed_wait_result::timed_out,false}; }
                    if(!keep_waiting) { return {keyed_wait_result::cancelled,true}; }
                    continue;
                }
                if(timeout_ns < 0) { node.event.wait(lock,ready); }
                else { (void)node.event.wait_until(lock,until,ready); }
            }
        }
        enum class prepared_wait_completion : unsigned char { pending, notified, timed_out };
        struct prepared_wait_spec
        {
            wait_key key{}; // Actual HOST-owned resource, NEVER a wire pointer.
            ::std::int_least64_t remaining_nanoseconds{-1};
            ::std::uint_least64_t pending_ordinal{};
            prepared_wait_completion completion{prepared_wait_completion::pending};
        };
        // Cold, owner-held pre-registration for a genuinely closed new execution.
        // The utility supplies queue resources, NOT permission to restore a VM.
        // The caller retains each actual resource and this registry, prevents new
        // guest entry, and joins consuming native threads before destroying batch.
        class prepared_wait_batch final
        {
            friend class keyed_wait_set;
            struct prepared_node
            {
                waiter node;
                shard& bucket;
                prepared_wait_spec specification;
                clock::time_point until{clock::time_point::max()};
                bool linked{}, counted{}, claimed{}, consumed{}, abort_requested{}, armed{};
                prepared_node(shard& actual,prepared_wait_spec const& spec)
                    :node{.key=spec.key},bucket{actual},specification{spec} {}
            };
            keyed_wait_set& owner_;
            ::std::vector<::std::unique_ptr<prepared_node>> nodes_{};
            bool started_{}; // Every access holds ALL shards, or one held shard.
            explicit prepared_wait_batch(keyed_wait_set& owner) noexcept:owner_{owner} {}
            [[nodiscard]] static clock::time_point remaining_deadline(clock::time_point now,::std::int_least64_t ns) noexcept
            {
                if(ns < 0) { return clock::time_point::max(); }
                auto const available{clock::time_point::max()-now};
                auto const duration{::std::chrono::nanoseconds{ns}};
                using wide_ns=::std::chrono::duration<long double,::std::nano>;
                if(wide_ns{duration} >= wide_ns{available}) { return clock::time_point::max(); }
                return now+::std::chrono::ceil<clock::duration>(duration);
            }
        public:
            prepared_wait_batch(prepared_wait_batch const&)=delete;
            prepared_wait_batch& operator=(prepared_wait_batch const&)=delete;
            prepared_wait_batch(prepared_wait_batch&&)=delete;
            prepared_wait_batch& operator=(prepared_wait_batch&&)=delete;
            [[nodiscard]] ::std::size_t size() const noexcept { return nodes_.size(); }
            // The real startup publisher calls this once after its fallible
            // source/seed/worker/session checks and BEFORE releasing guest entry.
            // No allocation or recoverable failure belongs on that commit edge.
            // Calling twice, after close, or with active consumers is a HOST
            // invariant violation; it never silently grants a new execution.
            void start_deadlines() noexcept
            {
                ::fast_io::array<::std::unique_lock<::std::mutex>,shard_count> held{};
                for(::std::size_t i{};i != shard_count;++i)
                { held[i]=::std::unique_lock<::std::mutex>{owner_.shards[i].mutex}; }
                if(started_ || owner_.is_closed()) { ::std::terminate(); }
                for(auto const& entry:nodes_)
                { if(!entry->counted || entry->claimed || entry->consumed || entry->abort_requested) { ::std::terminate(); } }
                auto const now{clock::now()}; // ONE real clock origin for the batch.
                for(auto const& entry:nodes_)
                {
                    entry->until=remaining_deadline(now,entry->specification.remaining_nanoseconds);
                    entry->armed=true;
                }
                started_=true;
            }
            // Startup failure removes genuinely unclaimed pre-linked nodes.
            // Claimed consumers receive only a wake/abort request; their actual
            // caller must leave the pause domain, finish callbacks, and OS-join.
            // This method is NOT an acknowledgement that those threads retired.
            void abort_unconsumed() noexcept
            {
                for(auto const& entry:nodes_)
                {
                    ::std::lock_guard lock{entry->bucket.mutex};
                    if(!entry->counted) { continue; }
                    entry->abort_requested=true;
                    if(entry->claimed) { entry->node.event.notify_one();continue; }
                    if(entry->linked) { unlink(entry->bucket,entry->node);entry->linked=false; }
                    entry->consumed=true;entry->counted=false;
                    if(--entry->bucket.active_calls == 0u) { entry->bucket.drained.notify_all(); }
                }
            }
            ~prepared_wait_batch()
            {
                abort_unconsumed();
                // Never wait forever in a destructor or free a live CV/node.
                // The real owner has to cancel and join actual consuming workers.
                for(auto const& entry:nodes_)
                {
                    ::std::lock_guard lock{entry->bucket.mutex};
                    if(entry->claimed || entry->counted) { ::std::terminate(); }
                }
            }
            template<typename Suspension>
            [[nodiscard]] suspension_result consume(::std::size_t index,wait_key actual_resource,
                Suspension& suspension,::std::stop_token cancellation={})
            {
                if(index >= nodes_.size()) { return {keyed_wait_result::cancelled,true}; }
                // [nonmoving owned nodes0 ... index ... N] end
                // [safe] complete ordinal bound BEFORE taking the actual node;
                // a caller's scalar cannot select another resource or payload.
                auto& selected{*nodes_[index]};
                struct claim_registration
                {
                    prepared_node& selected;bool acquired{};
                    explicit claim_registration(prepared_node& current,wait_key expected):selected{current}
                    {
                        ::std::lock_guard lock{selected.bucket.mutex};
                        if(selected.node.key != expected || !selected.armed || !selected.counted ||
                           selected.claimed || selected.consumed || selected.abort_requested) { return; }
                        selected.claimed=true;acquired=true;
                    }
                    ~claim_registration() noexcept
                    {
                        if(!acquired) { return; }
                        // Declared BEFORE callbacks/listener/lock, so this final
                        // count release follows their actual unregistration.
                        ::std::lock_guard lock{selected.bucket.mutex};
                        if(selected.linked) { unlink(selected.bucket,selected.node);selected.linked=false; }
                        selected.consumed=true;selected.claimed=false;selected.counted=false;
                        if(--selected.bucket.active_calls == 0u) { selected.bucket.drained.notify_all(); }
                    }
                } claim{selected,actual_resource};
                if(!claim.acquired) { return {keyed_wait_result::cancelled,true}; }
                if(selected.specification.completion == prepared_wait_completion::notified)
                { return {keyed_wait_result::notified,false}; }
                if(selected.specification.completion == prepared_wait_completion::timed_out)
                { return {keyed_wait_result::timed_out,false}; }
                static_assert(noexcept(suspension.requested()),"wake hint must not wait or throw");
                struct wake_context { shard& bucket;waiter& node; } context{selected.bucket,selected.node};
                auto listener{suspension.register_waker(::std::addressof(context),+[](void* address) noexcept
                {
                    auto& actual{*static_cast<wake_context*>(address)};
                    // [real synchronous stack context][batch-owned nonmoving node]
                    // [safe] unregister joins this callback BEFORE context dies.
                    ::std::lock_guard lock{actual.bucket.mutex};actual.node.event.notify_one();
                })};
                if(!listener) { return {keyed_wait_result::cancelled,true}; }
                ::std::stop_callback wake_on_stop{cancellation,[&]
                { ::std::lock_guard lock{selected.bucket.mutex};selected.node.event.notify_one(); }};
                ::std::unique_lock lock{selected.bucket.mutex};
                struct queue_cleanup
                {
                    prepared_node& selected;::std::unique_lock<::std::mutex>& lock;
                    ~queue_cleanup() noexcept
                    {
                        if(!lock.owns_lock()) { lock.lock(); }
                        if(selected.linked) { unlink(selected.bucket,selected.node);selected.linked=false; }
                    }
                } cleanup{selected,lock};
                suspension_borrow borrowed{owner_,selected.bucket,selected.node,selected.until,
                    selected.specification.remaining_nanoseconds < 0,cancellation};
                static_assert(noexcept(suspension.suspend(borrowed)),"real suspension must clean up without throwing");
                auto const ready{[&]
                { return selected.node.notified || owner_.is_closed() || cancellation.stop_requested() ||
                    selected.abort_requested || suspension.requested(); }};
                auto const finite{selected.specification.remaining_nanoseconds >= 0};
                for(;;)
                {
                    if(selected.node.notified) { return {keyed_wait_result::notified,false}; }
                    if(cancellation.stop_requested()) { return {keyed_wait_result::cancelled,false}; }
                    if(owner_.is_closed()) { return {keyed_wait_result::closed,false}; }
                    if(finite && clock::now() >= selected.until) { return {keyed_wait_result::timed_out,false}; }
                    if(selected.abort_requested) { return {keyed_wait_result::cancelled,true}; }
                    if(suspension.requested())
                    {
                        lock.unlock(); // Actual queue node remains linked; no shard -> domain inversion.
                        bool const keep_waiting{suspension.suspend(borrowed)};
                        lock.lock(); // Actual mutex is owned BEFORE reading/splicing node state.
                        if(selected.node.notified) { return {keyed_wait_result::notified,false}; }
                        if(cancellation.stop_requested()) { return {keyed_wait_result::cancelled,false}; }
                        if(owner_.is_closed()) { return {keyed_wait_result::closed,false}; }
                        if(finite && clock::now() >= selected.until) { return {keyed_wait_result::timed_out,false}; }
                        if(!keep_waiting || selected.abort_requested) { return {keyed_wait_result::cancelled,true}; }
                        continue;
                    }
                    if(!finite) { selected.node.event.wait(lock,ready); }
                    else { (void)selected.node.event.wait_until(lock,selected.until,ready); }
                }
            }
        };
        [[nodiscard]] ::std::unique_ptr<prepared_wait_batch> prepare_ordered_waits(
            ::std::span<prepared_wait_spec const> specifications,::std::size_t quota=4096u)
        {
            if(quota > 4096u || specifications.size() > quota || is_closed()) { return {}; }
            // All allocations happen BEFORE any real queue publication. A new
            // actual memory/source adapter supplies keys; copied wire identities
            // or a caller bool never attest that a VM startup is closed.
            ::std::unique_ptr<prepared_wait_batch> batch{new prepared_wait_batch{*this}};
            batch->nodes_.reserve(specifications.size());
            ::fast_io::array<::std::size_t,shard_count> additions{};
            for(::std::size_t i{};i != specifications.size();++i)
            {
                auto const& spec{specifications[i]}; // i<N before every specification borrow.
                if(spec.key.resource == nullptr || spec.remaining_nanoseconds < -1) { return {}; }
                ::std::size_t ordinal{};
                if(spec.completion == prepared_wait_completion::pending)
                {
                    // Zero remaining is still a real pending queue node until
                    // arbitration selects timeout; it does NOT forge ready2.
                    for(::std::size_t j{};j != i;++j)
                    {
                        auto const& previous{specifications[j]}; // j<i<N before earlier borrow.
                        if(previous.completion == prepared_wait_completion::pending && previous.key == spec.key) { ++ordinal; }
                    }
                    if(spec.pending_ordinal != ordinal) { return {}; }
                }
                else if((spec.completion != prepared_wait_completion::notified && spec.completion != prepared_wait_completion::timed_out) ||
                        spec.pending_ordinal != UINT64_MAX) { return {}; }
                auto& bucket{select(spec.key)};
                batch->nodes_.push_back(::std::unique_ptr<prepared_wait_batch::prepared_node>{
                    new prepared_wait_batch::prepared_node{bucket,spec}});
                for(::std::size_t shard_index{};shard_index != shard_count;++shard_index)
                { if(::std::addressof(shards[shard_index]) == ::std::addressof(bucket)) { ++additions[shard_index];break; } }
            }
            ::fast_io::array<::std::unique_lock<::std::mutex>,shard_count> held{};
            for(::std::size_t i{};i != shard_count;++i)
            { held[i]=::std::unique_lock<::std::mutex>{shards[i].mutex}; }
            if(is_closed()) { return {}; }
            for(::std::size_t i{};i != shard_count;++i)
            { if(shards[i].active_calls > SIZE_MAX-additions[i]) { return {}; } }
            for(auto const& entry:batch->nodes_)
            {
                auto const* cursor{entry->bucket.first};
                // [actual locked real queue] | null
                // [safe] reject existing same-key nodes BEFORE publishing any;
                // a fresh batch cannot splice ahead of or adopt another caller.
                while(cursor != nullptr)
                {
                    if(cursor->key == entry->node.key) { return {}; }
                    cursor=cursor->next; // Check nonnull successor before its next field read.
                }
            }
            for(auto const& entry:batch->nodes_)
            {
                ++entry->bucket.active_calls;entry->counted=true;
                if(entry->specification.completion == prepared_wait_completion::pending)
                { link(entry->bucket,entry->node);entry->linked=true; }
            }
            return batch; // Actual nodes linked in saved FIFO source order, NEVER matches().
        }
        keyed_wait_set() = default;
        keyed_wait_set(keyed_wait_set const&) = delete;
        keyed_wait_set& operator=(keyed_wait_set const&) = delete;
        ~keyed_wait_set() { close_and_drain(); }

        [[nodiscard]] bool is_closed() const noexcept { return closing.load(::std::memory_order_acquire); }

        // matches() must perform the caller's synchronized value comparison and
        // must not reenter this registry or request cancellation from this thread. It runs once while holding the shard
        // mutex, before publishing the node. A negative timeout is unbounded.
        // Spurious host wakeups do not become successful keyed notifications.
        // Optional cancellation affects only this call, allowing independent
        // execution owners to share one resource's notification domain.
        template <typename Matches>
        [[nodiscard]] keyed_wait_result wait(wait_key key, ::std::int_least64_t timeout_ns, Matches&& matches,
                                             ::std::stop_token cancellation = {}, ::std::size_t max_waiters_per_key = SIZE_MAX)
        {
            auto& bucket{select(key)};
            call_registration active_call{bucket};
            waiter node{.key = key};
            // Declare the callback before the unique_lock: its destructor may
            // wait for an executing callback, so our shard lock must be released
            // first. active_call keeps the shard alive through that destruction.
            ::std::stop_callback wake_on_stop{cancellation, [&]
            {
                ::std::lock_guard lock{bucket.mutex};
                node.event.notify_one();
            }};
            ::std::unique_lock lock{bucket.mutex};
            if(is_closed()) { return keyed_wait_result::closed; }
            if(cancellation.stop_requested()) { return keyed_wait_result::cancelled; }
            if(!::std::forward<Matches>(matches)()) { return keyed_wait_result::not_equal; }
            if(timeout_ns == 0) { return keyed_wait_result::timed_out; }
            // Only scan on capacity pressure; the default remains unbounded.
            // active_calls includes this call and is an upper bound on matching nodes.
            if(bucket.active_calls > max_waiters_per_key)
            {
                ::std::size_t matching{};
                // [live locked queue] | null
                // ^^ cursor; every node is retained until this mutex is released.
                auto const* cursor{bucket.first};
                while(cursor != nullptr)
                {
                    if(cursor->key == key && !cursor->notified) { ++matching; }
                    cursor = cursor->next;
                    // [visited] [remaining live nodes] | null
                    //           ^^ cursor; loop checks before dereferencing.
                }
                if(matching >= max_waiters_per_key) { return keyed_wait_result::too_many_waiters; }
            }
            auto const until{timeout_ns < 0 ? clock::time_point::max() : deadline(timeout_ns)};
            link(bucket, node);
            // Unlink on every exit, including a host condition-variable failure.
            // Declared after lock: cleanup runs while the mutex is still owned.
            struct registration
            {
                shard& bucket;
                waiter& node;
                ~registration() { unlink(bucket, node); }
            } cleanup{bucket, node};
            auto const ready{[&] { return node.notified || is_closed() || cancellation.stop_requested(); }};
            if(timeout_ns < 0) { node.event.wait(lock, ready); }
            else { (void)node.event.wait_until(lock, until, ready); }
            if(node.notified) { return keyed_wait_result::notified; }
            if(cancellation.stop_requested()) { return keyed_wait_result::cancelled; }
            return is_closed() ? keyed_wait_result::closed : keyed_wait_result::timed_out;
        }

        [[nodiscard]] ::std::size_t notify(wait_key key, ::std::size_t limit)
        {
            if(limit == 0uz) { return 0uz; }
            auto& bucket{select(key)};
            ::std::lock_guard lock{bucket.mutex};
            if(is_closed()) { return 0uz; }
            ::std::size_t count{};
            // [registered nodes ...] | null
            // [safe                ] | no dereference
            // ^^ node borrows the locked list head.
            auto* node{bucket.first};
            while(node != nullptr && count != limit)
            {
                if(node->key == key && !node->notified)
                {
                    node->notified = true;
                    ++count;
                    // Notify before releasing the lock: node and its event cannot
                    // be unlinked/destroyed until the waiting thread reacquires it.
                    node->event.notify_one();
                }
                node = node->next;
                // [consumed nodes] [remaining ...] | null
                // [safe          ] [safe         ] | no dereference
                //                  ^^ node, or null; loop checks before accessing.
            }
            return count;
        }

        void close()
        {
            closing.store(true, ::std::memory_order_release);
            for(auto& bucket: shards)
            {
                ::std::lock_guard lock{bucket.mutex};
                // [registered nodes ...] | null
                // [safe                ] | no dereference
                // ^^ node borrows this locked shard's head.
                auto* node{bucket.first};
                while(node != nullptr)
                {
                    node->event.notify_one();
                    node = node->next;
                    // [consumed] [remaining nodes ...] | null
                    // [safe    ] [safe               ] | no dereference
                    //            ^^ node, or null; checked by loop condition.
                }
            }
        }
        void close_and_drain()
        {
            close();
            for(auto& bucket: shards)
            {
                ::std::unique_lock lock{bucket.mutex};
                bucket.drained.wait(lock, [&] { return bucket.active_calls == 0uz; });
            }
        }
    };
#else
    inline constexpr bool has_keyed_wait_set{false};
#endif
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
