// Private class fragment. Only the existing stopped collector creates this
// batch after complete shape/root validation and slab epoch commit. It grants
// no admission or reference authority and never retains an ended object*.
struct sweep_retirement_batch
{
    gc_object_store& store;
    ::std::array<allocation_header*, 32uz> envelopes;
    ::std::size_t count{};
    explicit sweep_retirement_batch(gc_object_store& owner) noexcept : store{owner} {}
    sweep_retirement_batch(sweep_retirement_batch const&) = delete;
    sweep_retirement_batch& operator=(sweep_retirement_batch const&) = delete;
    ~sweep_retirement_batch() { flush(); }
    void flush() noexcept
    {
        if(count == 0uz) { return; }
        slab_chunk* retired{};
        {
            slab_guard held{store};
            for(::std::size_t index{}; index != count; ++index)
            {
                // Each entry is a LIVE original allocator envelope, obtained
                // before its union-member body destructor. No old object is read.
                auto* chunk{retire_slab_allocation_locked(envelopes[index], held)};
                if(chunk != nullptr)
                {
                    // Detached all-free native chunk: all_next is its own live
                    // metadata, reusable after both allocator lists unlink it.
                    chunk->all_next = retired;
                    retired = chunk;
                }
            }
            count = 0uz;
        }
        while(retired != nullptr)
        {
            auto* next{retired->all_next};
            // Matching array deletion and envelope lifetime endings occur
            // outside every allocator/index lock, as before.
            release_empty_chunk(*retired);
            retired = next;
        }
    }
    void destroy_unlinked(object* header) noexcept
    {
        // Caller unlinked BOTH token indexes under their real protection and
        // retains every canonical cohort pin through this complete pass.
        auto* envelope{allocation_header_for(header)};
        auto* chunk{active_metadata_chunk(envelope->metadata)};
        if(chunk != nullptr && chunk->owner != ::std::addressof(store)) { ::std::terminate(); }
        header->~object(); // actual carriers/foreign owners end outside slab_lock_
        if(chunk == nullptr) { retire_object_allocation(envelope); return; }
        envelopes[count++] = envelope;
        if(count == envelopes.size()) { flush(); }
    }
};
