/*************************************************************
 * Experimental actual immutable-record / public alias ownership split.
 *************************************************************/
#pragma once
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
#if !defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) || UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS != 1
# error "external exception handles require homogeneous native exception roots"
#endif
#ifndef UWVM_MODULE
# include <memory>
# include <mutex>
# include <utility>
# include <uwvm2/runtime/exception/immutable_value.h>
# include <uwvm2/runtime/exception/native_roots.h>
# include <uwvm2/utils/thread/deferred_owner.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception
{
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
    template<class Producer> class external_exception_native_leaf_access;
    // Type-erased only across the dependency split. It retains native DATA;
    // NEVER producer/cohort authority. The concrete typed publisher's private
    // fresh-value receipt proves origin BEFORE this low-level native factory.
    class external_exception_lifetime
    {
    protected:
        external_exception_lifetime() noexcept = default;
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        // Native DATA construction split only. Called by the concrete source
        // publisher's PRIVATE actual-source factory after cold code/root checks.
        // Protected access grants no public producer, origin/census certificate,
        // entered scope or collection authority. Actual bridge and every poll
        // independently require the genuine pinned producer/entry control blocks.
        // Keep the concrete descriptor in its own module: dependent construction
        // avoids first-declaring that descriptor or its publisher here.
        template<class NativeDescriptor, class... Args>
        [[nodiscard]] static NativeDescriptor construct_private_source_data(Args&&... args) noexcept
        { return NativeDescriptor{::std::forward<Args>(args)...}; }
#endif
    public:
        virtual ~external_exception_lifetime() noexcept = default;
        external_exception_lifetime(external_exception_lifetime const&)=delete;
        external_exception_lifetime& operator=(external_exception_lifetime const&)=delete;
    };
#endif

    // One external control block, one embedded root node. Public value_ref
    // copies and exception_ptr activations share this same registration.
    // Domain retains census lifetime ONLY and is final: it owns no source/store.
    // This privileged native factory authenticates internal value construction,
    // NOT the producer/source/cohort. Real VM bridge fresh/raw values therefore
    // still decline under the pinned R4 helper until a typed publisher exists.
    // Reset/destruction requires every outer pause/cohort/entry lock retired.
    class external_exception_handle final : public ::uwvm2::utils::thread::deferred_native_owner
    {
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
        template<class Producer> friend class external_exception_native_leaf_access;
        class native_leaf_key final
        {
            template<class Producer> friend class external_exception_native_leaf_access;
            native_leaf_key() noexcept = default;
        };
        // This immutable native key is minted only by a Producer-private
        // template instantiation. It is not a guest address, shape or boolean.
        native_leaf_key const* native_leaf_certificate_{};
#endif
        // Last release order: unlink external registry; detach actual census
        // node outside registry lock; release INTERNAL record outside both;
        // release the concrete typed DATA lifetime (when present), then actual
        // census domain LAST. The deferred queue retains this WHOLE block.
        ::std::shared_ptr<native_exception_root_domain> domain_pin_{};
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
        ::std::shared_ptr<external_exception_lifetime const> lifetime_pin_{};
#endif
        immutable_exception_root_lease lease_{};
        ::std::weak_ptr<value const> canonical_public_owner_{};
        external_exception_handle* previous_{};
        external_exception_handle* next_{};
        bool linked_{};
        static void reclaim_after_native_resume(
            ::uwvm2::utils::thread::deferred_native_owner* node) noexcept
        {
            // [actual embedded node of this complete factory-owned block]
            // [safe] only this immutable callback receives the detached node;
            // the whole block still owns its census lease and native pins.
            auto* owner{static_cast<external_exception_handle*>(node)};
            delete owner;
            // [ended complete object lifetime] node AND owner are now invalid.
            // [safe] return without reading either borrow after native callbacks.
        }
        struct final_deleter
        {
            void operator()(external_exception_handle* owner) const noexcept
            {
                // [actual allocation from the private native constructor]
                // [safe] shared_ptr calls once at actual last release, including
                // its own control-block allocation failure. No guest pointer.
                // [complete native block] transfer to the real queue or reclaim
                // immediately when no queue is bound. Both cases consume it.
                ::uwvm2::utils::thread::deferred_native_owner_queue::retire(*owner);
                // [possibly ended object lifetime] owner is not dereferenced
                // after retire: immediate reclaim may have deleted the block.
            }
        };
        inline static ::std::mutex registry_mutex_{};
        inline static external_exception_handle* registry_head_{};
        class construction_key
        {
            friend class external_exception_handle;
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
            template<class Producer> friend class external_exception_native_leaf_access;
#endif
            construction_key() noexcept = default;
        public:
            construction_key(construction_key const&) noexcept = default;
        };
        [[nodiscard]] static external_exception_handle const* find_locked(value_ref const& candidate) noexcept
        {
            // Caller retains candidate's actual strong owner until return.
            // Compare only trusted native list records BEFORE candidate access;
            // even a wrong nonempty control-block alias cannot claim a lease.
            // [protected actual external node chain] or null
            // [safe] initialize only from the registered native head.
            auto const* current{registry_head_};
            while(current != nullptr)
            {
                if(current->lease_.instance().get() == candidate.get() &&
                   !current->canonical_public_owner_.owner_before(candidate) &&
                   !candidate.owner_before(current->canonical_public_owner_)) { return current; }
                // [live registry node][registered successor/null]
                // [safe] current/next stay alive while registry_mutex_ is held.
                current = current->next_;
            }
            return nullptr;
        }
        void link_before_publication(value_ref const& alias) noexcept
        {
            // Weak identity retains no public self-owner. Alias retains this
            // genuine block while linking; root attachment already completed.
            canonical_public_owner_ = alias;
            ::std::lock_guard lock{registry_mutex_};
            if(linked_ || previous_ != nullptr || next_ != nullptr || !lease_) { ::std::terminate(); }
            // [private live block] -> [registered old head/null]
            // [safe] both actual nodes are protected until link completes.
            next_ = registry_head_;
            if(next_ != nullptr) { next_->previous_ = this; }
            registry_head_ = this;
            linked_ = true;
        }
    public:
        // Private key, not a caller-supplied boolean/generation/count.
        explicit external_exception_handle(construction_key,
            ::std::shared_ptr<native_exception_root_domain> domain, immutable_record_ref record
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
            , ::std::shared_ptr<external_exception_lifetime const> lifetime = {}
#endif
            ) noexcept
            : deferred_native_owner{reclaim_after_native_resume},
              domain_pin_{::std::move(domain)}
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
            , lifetime_pin_{::std::move(lifetime)}
#endif
            , lease_{*domain_pin_, ::std::move(record)} {}
        external_exception_handle(external_exception_handle const&) = delete;
        external_exception_handle& operator=(external_exception_handle const&) = delete;
        ~external_exception_handle() noexcept
        {
            {
                ::std::lock_guard lock{registry_mutex_};
                if(linked_)
                {
                    // [this exact registered node][actual adjacent nodes/null]
                    // [safe] detach BEFORE ending this native node's lifetime.
                    if(previous_ != nullptr) { previous_->next_ = next_; }
                    else
                    {
                        if(registry_head_ != this) { ::std::terminate(); }
                        registry_head_ = next_;
                    }
                    if(next_ != nullptr) { next_->previous_ = previous_; }
                    previous_ = nullptr; next_ = nullptr; linked_ = false;
                }
            }
            lease_.reset(); // May run tag/payload deleters; NO registry/root lock.
        }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
        // Privileged lifecycle primitive ONLY: a lifetime pin is not a source
        // certificate. Only the concrete typed factory publishes a fresh receipt.
        [[nodiscard]] static value_ref materialize_retained(
            ::std::shared_ptr<native_exception_root_domain> domain,
            value_ref const& candidate,
            ::std::shared_ptr<external_exception_lifetime const> lifetime)
        {
            if(!native_exception_root_domain::has_canonical_owner(domain) || !lifetime) { return {}; }
            auto record{value::record_from_native(candidate)};
            if(!record) { return {}; } // existing/foreign/raw external routes stay separate
            // Real complete block + ROOT custom final deleter: a failed
            // control-block allocation may queue this unpublished rooted block,
            // preserving source/record until the actual native safe boundary.
            ::std::shared_ptr<external_exception_handle> owner{
                new external_exception_handle{construction_key{},::std::move(domain),
                    ::std::move(record),::std::move(lifetime)},final_deleter{}};
            if(!owner->lease_) { return {}; }
            // [actual new block][complete internal record owned by its lease]
            // [safe] alias remains valid through the SAME owning control block.
            value_ref alias{owner,owner->lease_.instance().get()};
            owner->link_before_publication(alias);
            return alias;
        }
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        struct native_source_provenance
        {
            ::std::shared_ptr<external_exception_lifetime const> lifetime;
            void const* certificate{};
        };
        [[nodiscard]] static native_source_provenance source_native_provenance(value_ref const& candidate) noexcept
        {
            if(!candidate) { return {}; }
            ::std::lock_guard lock{registry_mutex_};
            // [exact genuine block/null] registry compares alias pointer AND real
            // control block before any certificate read; no guest address access.
            auto const* found{find_locked(candidate)};
            // Candidate itself keeps this exact block alive. Both immutable
            // observations are captured in ONE registry visit; no last native
            // lifetime release can occur under this lock.
            return found == nullptr ? native_source_provenance{} :
                native_source_provenance{found->lifetime_pin_,found->native_leaf_certificate_};
        }
#endif
        [[nodiscard]] static ::std::shared_ptr<external_exception_lifetime const>
            retained_lifetime(value_ref const& candidate) noexcept
        {
            if(!candidate) { return {}; }
            ::std::lock_guard lock{registry_mutex_};
            auto const* found{find_locked(candidate)};
            // Candidate strongly owns the found genuine block. Copying this
            // native pin cannot run its last deleter under the registry lock.
            return found==nullptr ? ::std::shared_ptr<external_exception_lifetime const>{} : found->lifetime_pin_;
        }
#endif

        [[nodiscard]] static bool is_registered(value_ref const& candidate) noexcept
        {
            if(!candidate || candidate.use_count() == 0) { return false; }
            ::std::lock_guard lock{registry_mutex_};
            return find_locked(candidate) != nullptr;
        }
        [[nodiscard]] static immutable_record_ref registered_record(value_ref const& candidate) noexcept
        {
            if(!candidate || candidate.use_count() == 0) { return {}; }
            ::std::lock_guard lock{registry_mutex_};
            auto const* found{find_locked(candidate)};
            // Copy INTERNAL owner while its real public alias remains retained.
            // No root-domain lock or owner destruction occurs in this registry.
            return found == nullptr ? immutable_record_ref{} : found->lease_.instance();
        }
        [[nodiscard]] static immutable_record_ref record_from_privileged_native(value_ref const& candidate) noexcept
        {
            // Actual factory-origin/control block is O(1), not a shape check.
            // Genuine internal records never need the external registry.
            auto record{value::record_from_native(candidate)};
            return record ? ::std::move(record) : registered_record(candidate);
        }
        [[nodiscard]] static value_ref materialize(
            ::std::shared_ptr<native_exception_root_domain> domain, value_ref const& candidate)
        {
            if(!native_exception_root_domain::has_canonical_owner(domain)) { return {}; }
            // Native immutable input is already factory-owned. Authenticate
            // that exact internal control block first; no O(external handles)
            // scan on ordinary fresh throw. NEVER treat use_count/shape as owner.
            auto record{value::record_from_native(candidate)};
            if(!record)
            {
                ::std::lock_guard lock{registry_mutex_};
                auto const* found{find_locked(candidate)};
                if(found == nullptr) { return {}; }
                if(found->domain_pin_.get() == domain.get() &&
                   !found->domain_pin_.owner_before(domain) && !domain.owner_before(found->domain_pin_))
                {
                    // A real EXISTING registration remains valid after close.
                    // Public copies share it, never attach a new census node.
                    return candidate;
                }
                // Different domain/source: internal record alone cannot pin
                // local GC payloads whose root is intentionally empty. This
                // lifetime-only factory has no actual recipient/cohort proof.
                // Decline without consuming input; native fallback can keep
                // the original external alias and its genuine source pin.
                return {};
            }
            // make_shared cannot defer the complete object's destructor. A real
            // native allocation plus custom control-block deleter transfers its
            // WHOLE block on last release, without allocating in end_catch.
            // If control-block allocation throws, that same deleter retains the
            // live block until the real safe boundary; candidate remains intact.
            ::std::shared_ptr<external_exception_handle> owner{
                new external_exception_handle{construction_key{}, ::std::move(domain), ::std::move(record)},
                final_deleter{}};
            if(!owner->lease_) { return {}; } // New attach still rejects closed.
            // [lease-owned complete immutable value][actual external block]
            // [safe] alias value pointer ONLY while this same control block owns
            // the INTERNAL record. No public alias is stored in lease_.
            value_ref alias{owner, owner->lease_.instance().get()};
            owner->link_before_publication(alias);
            return alias;
        }
    };

#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
    // The template belongs to this module, avoiding a forward declaration of a
    // foreign named-module class. Only the exact Producer class can call its
    // private instantiation. Other native instantiations have different keys
    // and cannot qualify blocks for that actual producer's selective drain.
    template<class Producer> class external_exception_native_leaf_access final
    {
        friend Producer;
        inline static external_exception_handle::native_leaf_key const key_{};

        [[nodiscard]] static value_ref materialize(
            ::std::shared_ptr<native_exception_root_domain> domain,
            value_ref const& candidate,
            ::std::shared_ptr<external_exception_lifetime const> lifetime)
        {
            if(!native_exception_root_domain::has_canonical_owner(domain) || !lifetime) { return {}; }
            auto record{value::record_from_native(candidate)};
            if(!record) { return {}; }
            // Constructor/control-block failure still uses the real complete
            // deferred block. No leaf key is installed on a failed publication.
            ::std::shared_ptr<external_exception_handle> owner{
                new external_exception_handle{typename external_exception_handle::construction_key{},
                    ::std::move(domain),::std::move(record),::std::move(lifetime)},
                typename external_exception_handle::final_deleter{}};
            if(!owner->lease_) { return {}; }
            // [complete private block][this Producer's stable native key]
            // [safe] issue only after successful root attachment, before any
            // public alias escapes. Caller has proved its private frozen data.
            owner->native_leaf_certificate_ = ::std::addressof(key_);
            value_ref alias{owner,owner->lease_.instance().get()};
            owner->link_before_publication(alias);
            return alias;
        }

#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        [[nodiscard]] static void const* captured_certificate() noexcept { return ::std::addressof(key_); }
        [[nodiscard]] static bool matches_captured_certificate(void const* actual) noexcept
        { return actual == ::std::addressof(key_); }
#endif
        [[nodiscard]] static bool matches(
            ::uwvm2::utils::thread::deferred_native_owner const* node,
            ::std::shared_ptr<external_exception_lifetime const> const& actual_lifetime) noexcept
        {
            if(node == nullptr || !actual_lifetime ||
               !node->has_native_reclaimer(external_exception_handle::reclaim_after_native_resume)) { return false; }
            // [real queued node with this class's immutable native reclaimer]
            // [safe] authenticate the complete native allocation type BEFORE
            // downcasting. No guest token or unregistered value is dereferenced.
            auto const* owner{static_cast<external_exception_handle const*>(node)};
            return owner->native_leaf_certificate_ == ::std::addressof(key_) && owner->lease_ &&
                owner->lifetime_pin_.get() == actual_lifetime.get() &&
                !owner->lifetime_pin_.owner_before(actual_lifetime) &&
                !actual_lifetime.owner_before(owner->lifetime_pin_);
        }
    };
#endif
}
#endif
