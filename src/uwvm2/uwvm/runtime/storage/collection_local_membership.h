// Private class fragment. This borrow dies in the stopped callback. The
// caller's REAL complete admission closure, pause and canonical cohort pins
// protect every original collector operation. This code mints no permission.
[[nodiscard]] static inline bool collection_local_membership_eligible(
    ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t store_count) noexcept
{
    if(store_count == 0uz) { return false; }
    for(::std::size_t index{}; index != store_count; ++index)
    {
        // [stores,stores+store_count) owns validated canonical pins.
        // [safe ] index<store_count borrows one real owner, without callback.
        // This tests TRACKED numeric reservations/closing/epoch exhaustion.
        // Byte-array publication still requires the original full caller
        // admission/stop contract; reserved_slots==0 is not that authority.
        if(!stores[index]->can_advance_slab_epoch_exclusive()) { return false; }
    }
    return true;
}
[[nodiscard]] static inline object* locate_local_collection_member(
    ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t store_count,
    gc_reference reference, gc_type::composite_kind kind) noexcept
{
    for(::std::size_t index{}; index != store_count; ++index)
    {
        // [canonical stopped cohort][release-published local bucket chains]
        // [safe ] borrow the actual pin's unchanged store identity. The token
        // remains an integer KEY; checked_local_object never converts it to an
        // object pointer. Its acquire authenticates the real native header.
        auto const* owner{stores[index].get()};
        auto* member{owner->checked_local_object(reference, kind)};
        if(member == nullptr) { continue; }
        // [actual local member][its unchanged canonical owner]
        // [safe ] membership proved this header live. Preserve kind/token
        // checks and exact owner; type/layout equality is not source authority.
        if(member->owner != owner) { return nullptr; }
#if defined(UWVM2TEST_GC_COLLECTION_LOCAL_MEMBERSHIP_PROBE) && UWVM2TEST_GC_COLLECTION_LOCAL_MEMBERSHIP_PROBE == 1
        gc_collection_local_membership_probe(owner, reinterpret_cast<::std::uintptr_t>(reference.storage.ptr));
#endif
        return member;
    }
    // A stale/unissued/non-cohort key fails; do not silently retain an omitted
    // foreign owner and enlarge the already stopped precise root snapshot.
    return nullptr;
}
