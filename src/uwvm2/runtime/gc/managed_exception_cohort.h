// Narrow actual LLVM-full exception/root-domain composition. Default OFF.
#pragma once
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
#ifndef UWVM_MODULE
// Bootstrap this standalone gated header before testing its prerequisites.
// Imported modules do not carry preprocessor definitions to this TU.
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
#endif
#if !defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) || UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION != 1 || \
    !defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) || UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES != 1 || \
    !defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) || UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS != 1 || \
    !defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) || UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH != 1 || \
    !defined(UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION) || UWVM_EXPERIMENTAL_REGISTERED_EXCEPTION_COLLECTION != 1
# error "local full GC exceptions require homogeneous complete exception root closure"
#endif
// Existing numeric-page runtime uses defined(PAGE), even PAGE=0, to move
// actual entry leases. Omit PAGE entirely here; do not pretend0 disables it.
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE) || (defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1)
# error "local full GC exceptions do not yet qualify moved numeric-page entry leases"
#endif
#if !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) || !defined(UWVM_CPP_EXCEPTIONS)
# error "local full GC exceptions require actual LLVM/native-thread/C++ exception support"
#endif
#if (defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1) || (defined(UWVM_EXPERIMENTAL_INT_FULL_EXCEPTION_SOURCE) && UWVM_EXPERIMENTAL_INT_FULL_EXCEPTION_SOURCE == 1)
# error "local LLVM GC exception cohort does not yet qualify private-EH ABI or INT exception producers"
#endif
#ifndef UWVM_MODULE
# include <cstdint>
# include <memory>
# include <utility>
# include <uwvm2/runtime/exception/external_handle.h>
# include <uwvm2/utils/thread/deferred_owner.h>
# include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
    // DATA descriptor. It is never entry authority by itself. The actual native
    // runtime accepts it only from its private live source-bound outer scope.
    // Copies retain exact canonical control blocks, never a bool/TLS/count.
    class managed_exception_cohort final
    {
        // This type has ALREADY been declared by its imported original module.
        // Friend the native DATA base, never first-declare a foreign publisher.
        friend class ::uwvm2::runtime::exception::external_exception_lifetime;
        using storage = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
        using store = ::uwvm2::uwvm::runtime::storage::gc_object_store;
        using lifetime = ::uwvm2::runtime::exception::external_exception_lifetime;
        using domain = ::uwvm2::runtime::exception::native_exception_root_domain;
        // Release census before store/source pins; origin keeps code/tag/store alive.
        ::std::shared_ptr<lifetime const> origin_;
        ::std::shared_ptr<store> store_;
        ::std::shared_ptr<domain> domain_;
        storage const* module_;
        ::std::uint_least64_t serial_, epoch_;
        // PRIVATE DATA constructor; the imported native-lifetime base exposes
        // only a PROTECTED construction helper, used inside the publisher's
        // PRIVATE actual cold factory. No public issuer/ctor/certificate/setter.
        // DATA pins alone are never authority: actual runtime entry and every
        // poll still check genuine source/code/root/generation identity.
        managed_exception_cohort(::std::shared_ptr<lifetime const> origin,
            ::std::shared_ptr<store> actual_store, ::std::shared_ptr<domain> actual_domain,
            storage const* module, ::std::uint_least64_t serial, ::std::uint_least64_t epoch) noexcept
            : origin_{::std::move(origin)}, store_{::std::move(actual_store)},
              domain_{::std::move(actual_domain)}, module_{module}, serial_{serial}, epoch_{epoch} {}
    public:
        managed_exception_cohort(managed_exception_cohort const&) noexcept = default;
        managed_exception_cohort(managed_exception_cohort&&) noexcept = default;
        managed_exception_cohort& operator=(managed_exception_cohort const&) = delete;
        managed_exception_cohort& operator=(managed_exception_cohort&&) = delete;
        [[nodiscard]] bool matches(storage const& module, ::std::uint_least64_t serial) const noexcept
        {
            // [source-owned module and canonical store] pins live in this object;
            // no source-derived or guest-derived address is advanced here.
            return module_ == ::std::addressof(module) && serial_ == serial && serial != 0u && epoch_ != 0u &&
                origin_ && domain_ && !domain_->closed() && store_ && store_.get() == module.gc_store.get() &&
                !store_.owner_before(module.gc_store) && !module.gc_store.owner_before(store_) &&
                module.gc_collection_phase.ready_for(serial);
        }
        [[nodiscard]] bool same_authority(managed_exception_cohort const& actual) const noexcept
        {
            return module_ == actual.module_ && serial_ == actual.serial_ && epoch_ == actual.epoch_ &&
                origin_.get() == actual.origin_.get() && !origin_.owner_before(actual.origin_) && !actual.origin_.owner_before(origin_) &&
                domain_.get() == actual.domain_.get() && !domain_.owner_before(actual.domain_) && !actual.domain_.owner_before(domain_) &&
                store_.get() == actual.store_.get() && !store_.owner_before(actual.store_) && !actual.store_.owner_before(store_);
        }
        [[nodiscard]] domain const& census() const noexcept { return *domain_; }
        [[nodiscard]] bool origin_matches(::std::shared_ptr<lifetime const> const& actual) const noexcept
        { return origin_.get() == actual.get() && !origin_.owner_before(actual) && !actual.owner_before(origin_); }
    };
    // Generic detached batches retain the full outer-entry release boundary.
    // Only the actual Producer's authenticated callback + exact origin + EVERY
    // frozen-record certificate can qualify this owner for earlier leaf cleanup,
    // after real collector locks retire while source/code/tag pins stay alive.
    class managed_exception_retirement final : public ::uwvm2::utils::thread::deferred_native_owner
    {
        static void reclaim(::uwvm2::utils::thread::deferred_native_owner* node) noexcept
        {
            // [complete native queue-owned object] immutable callback authenticates
            // this exact type; queue has unlinked it before dispatching us.
            auto* owner{static_cast<managed_exception_retirement*>(node)};
            // Generic dispatch occurs at the full outer boundary. Early dispatch
            // requires matches_native_leaf's bounded all-node proof: payload roots
            // are genuinely EMPTY, values are private/frozen, tag and canonical
            // diagnostic owners are actual-source/native data pinned by pins_.
            // No foreign callback or last source/store deleter can run here.
            owner->batch_.reset_after_native_resume();
            delete owner; // Empty batch dies before its genuine source/domain pins.
        }
        managed_exception_cohort pins_;
        ::uwvm2::uwvm::runtime::storage::gc_object_store::retired_exception_batch batch_;
    public:
        explicit managed_exception_retirement(managed_exception_cohort const& pins) noexcept
            : deferred_native_owner{&reclaim}, pins_{pins} {}
        [[nodiscard]] auto& batch() noexcept { return batch_; }
        [[nodiscard]] static bool matches_native_leaf(
            ::uwvm2::utils::thread::deferred_native_owner const* node,
            ::std::shared_ptr<::uwvm2::runtime::exception::external_exception_lifetime const> const& actual,
            void const* certificate) noexcept
        {
            if(node == nullptr || !actual || certificate == nullptr || !node->has_native_reclaimer(&reclaim)) { return false; }
            // [queued object authenticated by immutable exact-type callback]
            // only now downcast; no guest pointer or integer is dereferenced.
            auto const* owner{static_cast<managed_exception_retirement const*>(node)};
            return owner->pins_.origin_matches(actual) && owner->batch_.matches_source_native_leaf(actual,certificate);
        }
    };
}
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
#endif
