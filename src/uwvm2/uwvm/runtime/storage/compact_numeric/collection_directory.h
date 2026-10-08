// PRIVATE collector-local fragment; include only inside the real static
// gc_object_store::collect_exclusive_aggregate_domain member's COMPACT branch.
// It mints no admission, source, root, descriptor, token or guest authority.
// The original canonical cohort/pause/native-reader closure remains mandatory.
            struct compact_directory_record
            {
                ::std::uintptr_t first{};
                ::std::uintptr_t limit{}; // Exclusive reserved-token limit, never a native pointer.
                compact_numeric_descriptor* descriptor{};
                gc_object_store const* owner{};
            };
            static_assert(::std::is_trivially_copyable_v<compact_directory_record>);
            static_assert(::std::is_trivially_destructible_v<compact_directory_record>);
            static_assert(::std::is_nothrow_copy_constructible_v<compact_directory_record>);
            static_assert(::std::is_nothrow_copy_assignable_v<compact_directory_record>);
            constexpr auto max_compact_directory_count{
                static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                sizeof(compact_directory_record)};
            // Declared BEFORE cohort_guard: the trivial native array is released
            // AFTER that lock on every return. It holds no shared/weak owners.
            // Caller-owned stores and genuine entry/pause admission pin the
            // raw descriptors; this directory cannot authorize collection.
            ::std::unique_ptr<compact_directory_record[]> compact_directory_records{};
            ::std::size_t compact_directory_count{};
            auto build_compact_directory{[&]() noexcept -> gc_object_status
            {
                if(compact_directory_count == 0uz) { return gc_object_status::ok; }
                if(compact_directory_count > max_compact_directory_count ||
                   compact_directory_records) { return gc_object_status::invalid_store; }
                // [0,count<=PTRDIFF_MAX/sizeof(record)) checked allocation size.
                // The original descriptor preflight has already completed;
                // OOM may clear marks but cannot change any token/live/frontier,
                // object, epoch, lease or registry, and never reaches sweep.
                compact_directory_records.reset(
                    new(::std::nothrow) compact_directory_record[compact_directory_count]);
                if(!compact_directory_records) { return gc_object_status::out_of_memory; }
                ::std::size_t filled{};
                for(::std::size_t index{}; index != store_count; ++index)
                {
                    // [0,store_count) names the caller's authentic canonical
                    // strong pins, proved by the original complete cohort check.
                    auto const* store{stores[index].get()};
                    if(store->compact_numeric_head_.load(::std::memory_order_acquire) !=
                       store->compact_numeric_segments_.get())
                    { return gc_object_status::invalid_store; }
                    for(auto* current{store->compact_numeric_segments_.get()}; current != nullptr;)
                    {
                        if(filled == compact_directory_count ||
                           !compact_numeric_range_geometry::valid(current->token_begin_, current->capacity_) ||
                           current->phase_.load(::std::memory_order_acquire) != compact_numeric_descriptor::phase::published ||
                           current->canonical_store_.owner_before(stores[index]) ||
                           stores[index].owner_before(current->canonical_store_))
                        { return gc_object_status::invalid_store; }
                        // valid() proves first <= UINTPTR_MAX-reserved_width:
                        // the exclusive INTEGER limit is representable. This
                        // addition creates no payload/native object pointer.
                        auto const limit{current->token_begin_ +
                            compact_numeric_range_geometry::reserved_width};
                        // [0,count) filled<count before writing a real record.
                        compact_directory_records[filled++] = {
                            current->token_begin_, limit, current, store};
                        // [authentic store-owned strong descriptor chain]
                        // ^^ follow its initialized native next link while the
                        // actual quiescent caller excludes mutation/retirement.
                        current = current->owner_next_.get();
                    }
                }
                if(filled != compact_directory_count) { return gc_object_status::invalid_store; }
                if(compact_directory_count > 1uz)
                {
                    auto* first{compact_directory_records.get()};
                    // [first,first+count) is one complete array of initialized
                    // trivial records; count<=PTRDIFF_MAX/sizeof(record) proves
                    // this ONE-PAST end. No Wasm token is used for this address.
                    ::std::sort(first, first + compact_directory_count,
                        [](compact_directory_record const& a, compact_directory_record const& b) noexcept
                        { return a.first < b.first; });
                }
                for(::std::size_t index{1uz}; index < compact_directory_count; ++index)
                {
                    // [0,count) index>=1 and index<count bound both records.
                    // Prove reserved ranges disjoint, including unused slots.
                    if(compact_directory_records[index - 1uz].limit >
                       compact_directory_records[index].first)
                    { return gc_object_status::invalid_store; }
                }
                return gc_object_status::ok;
            }};
            auto locate_compact_directory{[&](gc_reference reference) noexcept -> aggregate_object_view
            {
                using kind = ::uwvm2::object::global::wasm_ref_kind;
                if(reference.kind != kind::wasm_struct || reference.storage.ptr == nullptr ||
                   compact_directory_count == 0uz || !compact_directory_records) { return {}; }
                auto const token{reinterpret_cast<::std::uintptr_t>(reference.storage.ptr)};
                // Binary search uses ONLY opaque integer keys. The lookup
                // remains inside this actual transaction, never a live-entry
                // cache or source/cohort/epoch permission granted by a bool.
                ::std::size_t low{}, high{compact_directory_count};
                while(low != high)
                {
                    auto const middle{low + (high - low) / 2uz};
                    // [0,count) low<high implies low<=middle<high<=count.
                    if(token < compact_directory_records[middle].first) { high = middle; }
                    else { low = middle + 1uz; } // integer index only, no pointer movement.
                }
                if(low == 0uz) { return {}; }
                // [0,count) 1<=low<=count proves the predecessor index low-1.
                auto const& member{compact_directory_records[low - 1uz]};
                if(token >= member.limit) { return {}; }
                auto* descriptor{member.descriptor};
                // The complete preflight and actual strong store-owned chain
                // authenticated this native descriptor/owner. Recheck immutable
                // range identity and PRIVATE stopped locator's reader count,
                // phase, actual capacity, frontier and live bit before return.
                if(descriptor == nullptr || member.owner == nullptr ||
                   descriptor->token_begin_ != member.first) { return {}; }
                ::std::size_t slot{};
                if(descriptor->locate_live_token_while_stopped(token, slot) != compact_numeric_status::ok)
                { return {}; }
                // [authenticated actual descriptor][initialized live slot]
                // No guest key is dereferenced; the existing typed collector
                // marks this numeric leaf and never traces nonexistent fields.
                return {nullptr, descriptor, member.owner, slot};
            }};
