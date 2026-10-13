// Private allocator member definitions. This file is included only for the
// exact1 experiment, inside gc_object_store; it grants no public capability.
        [[nodiscard]] inline gc_object_status allocate_numeric_slab_single_lock(
            ::std::uint_least32_t type_index, ::std::size_t length, object*& result,
            ::std::byte const* complete_source = nullptr) noexcept
        {
            // Caller has proved the real immutable layout: struct, 1..8 fields,
            // every storage kind numeric. Preserve full carrier initialization.
            static_assert(::std::is_nothrow_default_constructible_v<object>);
            static_assert(::std::is_nothrow_default_constructible_v<gc_object_value>);
            slot_reservation reservation{};
            object* fresh{};
            auto status{gc_object_status::ok};
            {
                slab_guard held{*this};
                status = reserve_numeric_slot_locked(length, reservation, held);
                if(status != gc_object_status::ok) { return status; }
                // [reserved live envelope][inactive named object union]
                // [safe ] exact slot belongs to THIS held guard. Placement only:
                // no lease promotion, root poll, observer, host callback or new
                // allocation is introduced between reservation and commit.
                fresh = ::new(static_cast<void*>(::std::addressof(
                    reservation.envelope->payload.instance))) object{};
                // [original byte-array element][complete aligned carrier tail]
                // [safe ] reserve checked this size class/slot/stride before
                // producing original_array_slot; length is bounded by eight.
                auto* tail{reservation.original_array_slot + object_value_offset};
                auto* initialized_values{initialize_carrier_tail(tail, length, complete_source)};
                if(initialized_values == nullptr)
                { status = gc_object_status::size_overflow; }
                else
                {
                    fresh->values.reset(initialized_values);
                    fresh->owner = this;
                    fresh->kind = gc_type::composite_kind::struct_;
                    fresh->type_index = type_index;
                    fresh->length = length;
                    commit_reserved_slot_locked(reservation.envelope->metadata, held);
                }
            }
            if(status != gc_object_status::ok)
            {
                // [guard released][reserved metadata][possibly constructed header]
                // [safe ] ORIGINAL destruction/retirement takes its own slab lock.
                // Never recursively destroy a slot while its allocator is held.
                destroy_object(fresh);
                return status;
            }
            // Still unpublished: original field assignments, token issuance,
            // lease validation and all membership publication follow unchanged.
            result = fresh;
            return gc_object_status::ok;
        }
