        // This one-entry cache is owned by the genuine native sealed entry.
        // It retains a descriptor from this store's canonical chain, never a
        // pointer decoded from Wasm bits. The existing entry excludes every
        // collector and teardown through the last immediate native access.
        // Every detach revokes the JIT nonce and clears this pin before a poll,
        // callback, sweep, or admission release can retire the descriptor.
        // No public token, root, table, or carrier contains the cached pointer.
        mutable ::std::shared_ptr<descriptor> local_compact_lookup_pin_{};
        mutable ::std::uint64_t local_compact_lookup_epoch_{};

        [[nodiscard]] gc_object_store::aggregate_object_view
        checked_entry_local_compact_object(gc_reference reference) const noexcept
        {
            // Both private call sites have just proved live_authority(), native
            // owner, stop and pause. The real canonical store is still pinned;
            // no new shared admission is acquired under its held exclusive.
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr) { return {}; }
            auto const token{reinterpret_cast<::std::uintptr_t>(reference.storage.ptr)};
            auto const epoch{store_pin_->slab_epoch_};
            if(local_compact_lookup_pin_ && local_compact_lookup_epoch_ == epoch)
            {
                ::std::size_t slot{};
                auto const status{local_compact_lookup_pin_->locate_live_token_under_existing_admission(token, slot)};
                if(status == compact_numeric_status::ok)
                {
                    // [canonical descriptor pin][initialized live slot]
                    // [safe                                         ]
                    // ^^ native pointers come only from these genuine pins.
                    // The result cannot survive any poll or guest callback.
                    return {nullptr, local_compact_lookup_pin_.get(), store_pin_.get(), slot};
                }
                // IDs never overlap and are never recycled. A dead/unpublished
                // slot within the cached range cannot be live in another one.
                if(compact_numeric_range_geometry::slot(local_compact_lookup_pin_->token_begin_,
                    local_compact_lookup_pin_->capacity_, token, slot)) { return {}; }
            }

            // The current allocation descriptor already has its own genuine
            // entry pin. Looking it up must not evict the older-range cache:
            // programs commonly alternate an older read with a newborn write.
            if(descriptor_pin_)
            {
                ::std::size_t slot{};
                if(descriptor_pin_->locate_live_token_under_existing_admission(token, slot) ==
                   compact_numeric_status::ok)
                {
                    // [actual current descriptor pin][initialized live slot]
                    // [safe                                               ]
                    // ^^ borrow only this native pin and the canonical store.
                    return {nullptr, descriptor_pin_.get(), store_pin_.get(), slot};
                }
                if(compact_numeric_range_geometry::slot(descriptor_pin_->token_begin_,
                    descriptor_pin_->capacity_, token, slot)) { return {}; }
            }

            // [store-owned native range chain] the original locator proves
            // geometry, published phase, initialized frontier and live bitmap.
            // O(ranges) is paid only on a cache miss; a hit still proves each
            // opaque token's own slot instead of trusting the previous token.
            auto const object{store_pin_->checked_local_compact_object(reference)};
            if(!object) { return {}; }
            auto candidate{object.compact->canonical_self_.lock()};
            if(!candidate || candidate.get() != object.compact ||
               candidate->canonical_store_.owner_before(store_pin_) ||
               store_pin_.owner_before(candidate->canonical_store_)) { return {}; }

            // [real canonical descriptor][existing store and entry exclusion]
            // [safe                                                       ]
            // ^^ replace only a native strong pin; the store still owns the
            // old range, so dropping this cache cannot run its last deleter.
            // Nothing allocates, enters a safepoint, or calls guest code here.
            local_compact_lookup_pin_ = ::std::move(candidate);
            local_compact_lookup_epoch_ = epoch;
            return object;
        }

        void clear_entry_local_compact_lookup() noexcept
        {
            // [native cache pin][store-owned descriptor chain still live]
            // [safe                                                  ]
            // ^^ clear the remembered epoch and owning pointer after nonce
            // revocation, before descriptor unlink or collection is allowed.
            // This runs outside all slab/publication/registry/root locks.
            local_compact_lookup_epoch_ = 0u;
            local_compact_lookup_pin_.reset();
        }
