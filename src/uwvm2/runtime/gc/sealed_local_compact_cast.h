        // Return a private four-byte borrow only for a real, local, live compact
        // object. Zero declines without changing roots, allocation credit or
        // admission; the caller then retires and uses the original cast.
        // Existing entry exclusion, never a token or TLS depth alone, pins the
        // immutable cell until the adjacent original witness getter consumes it.
        [[nodiscard]] ::std::uintptr_t local_compact_immutable_cast_values(
            gc_reference reference, ::std::uint_least32_t expected_index) const noexcept
        {
            if(!live_authority() || !descriptor_pin_ ||
               view_.armed_nonce.load(::std::memory_order_acquire) != nonce_ ||
               view_.epoch != store_pin_->slab_epoch_ ||
               view_.interrupts.load(::std::memory_order_acquire) != 0u ||
               authority_->collection_domain().pause_requested()) { return 0u; }

            // [real canonical store][native owned range chain] end
            // [safe                                               ]
            // No guest pointer is formed: lookup verifies integer token range,
            // published phase, initialized frontier and the live bitmap under
            // this entry's existing collection exclusion. Older ranges belong
            // to the same pinned store, even when the current window differs.
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_LOOKUP_CACHE == 1
            auto const object{checked_entry_local_compact_object(reference)};
#else
            auto const object{store_pin_->checked_local_compact_object(reference)};
#endif
            if(!object) { return 0u; }
            auto const actual_index{object.type_index()};
            auto const* actual{store_pin_->checked_type(actual_index, gc_type::composite_kind::struct_)};
            auto const* expected{store_pin_->checked_type(expected_index, gc_type::composite_kind::struct_)};
            if(!gc_object_store::compact_layout(actual) || !gc_object_store::compact_layout(expected) ||
               actual->fields[0uz].storage != expected->fields[0uz].storage) { return 0u; }

            // Prove only the immutable local parent chain or exact canonical
            // equality. Other canonical subtype relations decline to the real
            // original predicate; this leaf never enters its registry lock.
            if(!store_pin_->is_defined_subtype(actual_index, expected_index) &&
               !gc_object_store::canonical_defined_type_equal(
                   store_pin_.get(), actual_index, store_pin_.get(), expected_index)) { return 0u; }

            // [actual owned cell array][live initialized object.compact_slot]
            // [safe                                                       ]
            // address_of_cell bounds-checks and returns exactly one native
            // uint32 member. No carrier-array stride, allocation, safepoint,
            // callback or mutation is allowed before the immediate byte load.
            auto const* cell{object.compact->payload_.address_of_cell(object.compact_slot)};
            auto const address{reinterpret_cast<::std::uintptr_t>(cell)};
            return address > 1u ? address : 0u;
        }
