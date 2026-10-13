// Native owner retirement and actual shared-entry admission component.
// This does not authenticate a Wasm source, cohort, or complete root census.
#include <uwvm2/utils/thread/deferred_owner.h>
#include <uwvm2/runtime/gc/entry_admission.h>
#include <fast_io.h>
#include <atomic>
#include <exception>
#include <memory>
#include <new>
#include <thread>
#include <utility>
#ifndef __cpp_exceptions
# error This native lifecycle component requires real C++ exception support.
#endif

namespace t = ::uwvm2::utils::thread;
namespace g = ::uwvm2::runtime::gc;
namespace
{
    ::std::atomic_size_t checks{};
    void check(bool condition, unsigned line) noexcept
    {
        checks.fetch_add(1uz, ::std::memory_order_relaxed);
        if(!condition)
        {
            ::fast_io::io::perrln("deferred_native_owners FAIL line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), __LINE__)
    struct observations
    {
        ::std::atomic_size_t reclaimed{};
        bool require_exclusive{};
    };
    inline thread_local unsigned reclaim_depth{};
    inline thread_local unsigned maximum_reclaim_depth{};

    class owner final : public t::deferred_native_owner
    {
        observations& observations_;
        ::std::shared_ptr<owner> child_{};
        static void reclaim(t::deferred_native_owner* node) noexcept
        {
            // [actual embedded base of this complete owned native allocation]
            // [safe] this immutable callback is installed only by owner ctor;
            // the queue detached its node before calling us. No guest address.
            auto* actual{static_cast<owner*>(node)};
            if(actual->observations_.require_exclusive)
            {
                auto exclusive{g::runtime_gc_entry_admission.try_exclusive(0uz)};
                CHECK(exclusive); // Real shared admission MUST already be gone.
            } // Retire exclusive before delete can release arbitrary native owners.
            ++reclaim_depth;
            if(maximum_reclaim_depth < reclaim_depth) { maximum_reclaim_depth = reclaim_depth; }
            actual->observations_.reclaimed.fetch_add(1uz, ::std::memory_order_relaxed);
            delete actual; // May retire child into the same queue; no node borrow afterward.
            --reclaim_depth;
        }
    public:
        owner(observations& state, ::std::shared_ptr<owner> child) noexcept
            : deferred_native_owner{reclaim}, observations_{state}, child_{::std::move(child)} {}
        struct final_deleter
        {
            void operator()(owner* actual) const noexcept
            {
                // [complete allocation returned by the real native factory]
                // [safe] shared_ptr invokes this once, at actual last release.
                t::deferred_native_owner_queue::retire(*actual);
            }
        };
    };
    ::std::shared_ptr<owner> make_owner(observations& state, ::std::shared_ptr<owner> child = {})
    {
        // The shared_ptr constructor also invokes this deleter if its actual
        // control-block allocation throws, retaining the complete object safely.
        return {new owner{state, ::std::move(child)}, owner::final_deleter{}};
    }
    struct activation { ::std::shared_ptr<owner> instance; };

    struct expected_constructor_failure {};
    struct constructor_observations
    {
        ::std::size_t entered{}, complete_reclaims{};
    };
    class throwing_constructor_owner final : public t::deferred_native_owner
    {
        constructor_observations& observations_;
        ::std::shared_ptr<owner> child_;
        static void reclaim(t::deferred_native_owner* node) noexcept
        {
            // [actual embedded base of a complete throwing_constructor_owner]
            // [safe] this callback is never used for a partially constructed object.
            auto* actual{static_cast<throwing_constructor_owner*>(node)};
            ++actual->observations_.complete_reclaims;
            delete actual; // No node access after whole-object destruction.
        }
    public:
        throwing_constructor_owner(constructor_observations& observations,
            ::std::shared_ptr<owner> child)
            : deferred_native_owner{reclaim}, observations_{observations}, child_{::std::move(child)}
        {
            ++observations_.entered;
            // Real C++ partial construction unwinds child_ and the native base.
            // Neither a failed allocation nor this incomplete parent may enqueue.
            throw expected_constructor_failure{};
        }
    };

    struct control_block_failure_state
    {
        ::std::size_t allocation_calls{}, failures{};
        bool fail_next{true}; // Native allocator fault selection, not GC authority.
    };
    template<typename T>
    class control_block_failure_allocator
    {
        template<typename> friend class control_block_failure_allocator;
        control_block_failure_state* state_;
    public:
        using value_type = T;
        explicit control_block_failure_allocator(control_block_failure_state& state) noexcept
            // [live test state] remains alive through control-block construction.
            // [safe] native reference supplies this address, never a guest token.
            : state_{::std::addressof(state)} {}
        template<typename U>
        control_block_failure_allocator(control_block_failure_allocator<U> const& other) noexcept
            // [same live test state] preserve identity through actual allocator rebind.
            : state_{other.state_} {}
        [[nodiscard]] T* allocate(::std::size_t count)
        {
            ++state_->allocation_calls;
            if(state_->fail_next)
            {
                state_->fail_next = false;
                ++state_->failures;
                // Only this genuine shared_ptr control-block allocator fails.
                // No object-size guess, global new override or fake native owner.
                throw ::std::bad_alloc{};
            }
            return ::std::allocator<T>{}.allocate(count);
        }
        void deallocate(T* pointer, ::std::size_t count) noexcept
        {
            // [actual rebound allocation returned by allocator<T>][same count]
            // [safe] shared_ptr supplies the matching allocated base and extent.
            ::std::allocator<T>{}.deallocate(pointer, count);
        }
        template<typename U>
        [[nodiscard]] bool operator==(control_block_failure_allocator<U> const& other) const noexcept
        { return state_ == other.state_; }
    };

    void actual_exception_ptr_last_release()
    {
        observations state{};
        state.require_exclusive = true;
        t::deferred_native_owner_queue queue{};
        auto admission{g::runtime_gc_entry_admission.enter()};
        auto object{make_owner(state)};
        ::std::exception_ptr retained{};
        try { throw activation{object}; }
        catch(activation const& caught)
        {
            CHECK(caught.instance.get() == object.get());
            retained = ::std::current_exception();
        }
        object.reset();
        CHECK(retained && queue.pending_count() == 0uz && state.reclaimed.load() == 0uz);
        try { ::std::rethrow_exception(retained); }
        catch(activation const& caught) { CHECK(caught.instance); }
        retained = nullptr; // Real C++ ABI last activation release under shared admission.
        CHECK(queue.pending_count() == 1uz && state.reclaimed.load() == 0uz);
        CHECK(g::runtime_gc_entry_admission.active_count() == 1uz);
        admission.reset();
        queue.finish_after_native_resume();
        CHECK(queue.pending_count() == 0uz && state.reclaimed.load() == 1uz);
        CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
    }
    void iterative_dependent_destruction()
    {
        observations state{};
        state.require_exclusive = true;
        ::std::shared_ptr<owner> chain{};
        for(::std::size_t i{}; i != 4096uz; ++i) { chain = make_owner(state, ::std::move(chain)); }
        maximum_reclaim_depth = 0u;
        t::deferred_native_owner_queue queue{};
        auto admission{g::runtime_gc_entry_admission.enter()};
        chain.reset();
        CHECK(queue.pending_count() == 1uz && state.reclaimed.load() == 0uz);
        admission.reset();
        queue.finish_after_native_resume();
        CHECK(state.reclaimed.load() == 4096uz && maximum_reclaim_depth == 1u);
        CHECK(reclaim_depth == 0u && queue.pending_count() == 0uz);
    }
    void nested_native_queues_without_admission()
    {
        observations state{};
        t::deferred_native_owner_queue outer{};
        auto first{make_owner(state)};
        first.reset();
        CHECK(outer.pending_count() == 1uz);
        {
            t::deferred_native_owner_queue inner{};
            auto second{make_owner(state)};
            second.reset();
            CHECK(inner.pending_count() == 1uz && outer.pending_count() == 1uz);
            inner.finish_after_native_resume();
            CHECK(state.reclaimed.load() == 1uz && t::deferred_native_owner_queue::active_on_current_thread());
        }
        outer.finish_after_native_resume();
        CHECK(state.reclaimed.load() == 2uz && !t::deferred_native_owner_queue::active_on_current_thread());
    }
    void actual_thread_local_separation()
    {
        observations state{};
        t::deferred_native_owner_queue caller{};
        ::std::thread worker{[&]
        {
            CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
            t::deferred_native_owner_queue other{};
            auto object{make_owner(state)};
            object.reset();
            CHECK(other.pending_count() == 1uz && state.reclaimed.load() == 0uz);
            other.finish_after_native_resume();
            CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
        }};
        worker.join();
        CHECK(caller.pending_count() == 0uz && state.reclaimed.load() == 1uz);
        caller.finish_after_native_resume();
    }

    void real_throwing_constructor_cleanup()
    {
        observations state{};
        state.require_exclusive = true;
        constructor_observations partial{};
        t::deferred_native_owner_queue queue{};
        auto admission{g::runtime_gc_entry_admission.enter()};
        try
        {
            auto child{make_owner(state)};
            bool caught{};
            try
            {
                // A real new-expression releases its raw allocation on ctor throw.
                // No complete parent pointer is ever returned or retired.
                (void)new throwing_constructor_owner{partial, ::std::move(child)};
            }
            catch(expected_constructor_failure const&) { caught = true; }
            CHECK(caught && !child && partial.entered == 1uz);
            CHECK(partial.complete_reclaims == 0uz);
            CHECK(queue.pending_count() == 1uz && state.reclaimed.load() == 0uz);
        }
        catch(...)
        {
            // Unexpected real allocation/exception failure must still obey the
            // explicit native cleanup boundary; it does not become test success.
            admission.reset();
            queue.finish_after_native_resume();
            throw;
        }
        admission.reset();
        queue.finish_after_native_resume();
        CHECK(queue.pending_count() == 0uz && state.reclaimed.load() == 1uz);
        CHECK(partial.complete_reclaims == 0uz);
        CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
    }

    void real_shared_control_block_oom()
    {
        observations state{};
        state.require_exclusive = true;
        control_block_failure_state fault{};
        t::deferred_native_owner_queue queue{};
        auto admission{g::runtime_gc_entry_admission.enter()};
        maximum_reclaim_depth = 0u;
        try
        {
            auto child{make_owner(state)};
            // [complete genuinely allocated owner] has a real last-owned child.
            // The allocator below never participates in either object's new.
            auto* complete{new owner{state, ::std::move(child)}};
            bool caught{};
            try
            {
                // Hand this complete allocation to the real standard constructor.
                // Its supplied final_deleter owns cleanup even if allocate throws.
                ::std::shared_ptr<owner> rejected{complete, owner::final_deleter{},
                    control_block_failure_allocator<owner>{fault}};
                CHECK(false); // Fault injection must actually fail its allocation.
            }
            catch(::std::bad_alloc const&) { caught = true; }
            // [ownership transferred to shared_ptr's actual exception cleanup]
            // [safe] erase our raw stack borrow without reading the queued owner.
            complete = nullptr;
            CHECK(caught && !child && complete == nullptr);
            CHECK(fault.allocation_calls >= 1uz && fault.failures == 1uz && !fault.fail_next);
            CHECK(queue.pending_count() == 1uz && state.reclaimed.load() == 0uz);
        }
        catch(...)
        {
            admission.reset();
            queue.finish_after_native_resume();
            throw; // An unrelated OOM is failure, never the requested control.
        }
        admission.reset();
        queue.finish_after_native_resume();
        CHECK(queue.pending_count() == 0uz && state.reclaimed.load() == 2uz);
        CHECK(maximum_reclaim_depth == 1u && reclaim_depth == 0u);
        CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
    }

    void retire_in_nested_native_body(observations& state)
    {
        // This ordinary native call intentionally binds no second FIFO/admission.
        // It models the registered RT inner scope's reuse, not a VM entry proof.
        auto object{make_owner(state)};
        object.reset();
    }

    void nested_native_body_reuses_outer_fifo()
    {
        observations state{};
        state.require_exclusive = true;
        t::deferred_native_owner_queue outer{};
        auto admission{g::runtime_gc_entry_admission.enter()};
        try
        {
            auto first{make_owner(state)};
            first.reset();
            CHECK(outer.pending_count() == 1uz && state.reclaimed.load() == 0uz);
            retire_in_nested_native_body(state);
            CHECK(outer.pending_count() == 2uz && state.reclaimed.load() == 0uz);
            CHECK(t::deferred_native_owner_queue::active_on_current_thread());
            CHECK(admission); // Actual held lease; no TLS/count reclaim permission.
        }
        catch(...)
        {
            admission.reset();
            outer.finish_after_native_resume();
            throw;
        }
        admission.reset();
        outer.finish_after_native_resume();
        CHECK(outer.pending_count() == 0uz && state.reclaimed.load() == 2uz);
        CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
    }

    struct expected_wrapper_failure {};
    void throwing_outer_native_wrapper(observations& state)
    {
        t::deferred_native_owner_queue queue{};
        auto admission{g::runtime_gc_entry_admission.enter()};
        try
        {
            auto object{make_owner(state)};
            throw expected_wrapper_failure{}; // Real object dtor runs during unwinding.
        }
        catch(...)
        {
            auto const pending_before{queue.pending_count()};
            auto const reclaimed_before{state.reclaimed.load()};
            // Do not let the queue's destructor guess that arbitrary reclamation
            // is safe. Genuine admission reset precedes explicit finish/rethrow,
            // including an unexpected allocation error which will remain FAIL.
            admission.reset();
            queue.finish_after_native_resume();
            CHECK(pending_before == 1uz && reclaimed_before == 0uz);
            CHECK(state.reclaimed.load() == 1uz && queue.pending_count() == 0uz);
            throw;
        }
    }

    void exceptional_outer_wrapper_cleanup()
    {
        observations state{};
        state.require_exclusive = true;
        bool caught{};
        try { throwing_outer_native_wrapper(state); }
        catch(expected_wrapper_failure const&) { caught = true; }
        CHECK(caught && state.reclaimed.load() == 1uz);
        CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
        auto exclusive{g::runtime_gc_entry_admission.try_exclusive(0uz)};
        CHECK(exclusive); // No stale shared admission survives the real rethrow.
    }

    void immediate_native_release()
    {
        observations state{};
        state.require_exclusive = true;
        CHECK(!t::deferred_native_owner_queue::active_on_current_thread());
        auto object{make_owner(state)};
        object.reset();
        CHECK(state.reclaimed.load() == 1uz);
    }
}
int main()
{
    actual_exception_ptr_last_release();
    iterative_dependent_destruction();
    nested_native_queues_without_admission();
    actual_thread_local_separation();
    immediate_native_release();
    real_throwing_constructor_cleanup();
    real_shared_control_block_oom();
    nested_native_body_reuses_outer_fifo();
    exceptional_outer_wrapper_cleanup();
    ::fast_io::io::println("deferred_native_owners checks=", checks.load(),
        " actual_cpp_activation=true actual_entry_admission=true full_VM=false full_GC=false performance=false");
}
