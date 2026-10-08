// Private size-class allocation: exact nonmoving objects and opaque tokens are unchanged.
        [[nodiscard]] inline bool general_slab_eligible(::std::uint_least32_t type_index,
            gc_type::composite_kind kind,::std::size_t length,::std::size_t bytes) const noexcept
        {
            if(type_index >= layout_count_ || bytes == 0uz || bytes > 512uz*sizeof(gc_object_value)) { return false; }
            auto const& layout{layouts_[type_index]};
            return layout.kind == kind &&
                ((kind == gc_type::composite_kind::struct_ && layout.field_count == length) ||
                 (kind == gc_type::composite_kind::array && layout.field_count == 1uz));
        }
        [[nodiscard]] inline gc_object_status allocate_general_slab(::std::uint_least32_t type_index,
            gc_type::composite_kind kind,::std::size_t length,::std::size_t bytes,bool raw,object*& result,
            ::std::byte const* complete_source = nullptr) noexcept
        {
            static_assert(::std::is_nothrow_default_constructible_v<object>);
            static_assert(::std::is_nothrow_default_constructible_v<gc_object_value>);
            auto const units{(bytes+sizeof(gc_object_value)-1uz)/sizeof(gc_object_value)};
            slot_reservation reservation{};
            object* fresh{};
            auto status{gc_object_status::ok};
            {
                slab_guard held{*this};
                status=reserve_numeric_slot_locked(units,reservation,held);
                if(status!=gc_object_status::ok) { return status; }
                // The locked reservation owns one entire, aligned original byte-array slot.
                // Placement starts its object and payload lifetimes; references are assigned
                // only after this guard returns, using the unchanged validation/root protocol.
                fresh=::new(static_cast<void*>(::std::addressof(reservation.envelope->payload.instance))) object{};
                auto* tail{reservation.original_array_slot+object_value_offset};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                fresh->numeric_array_=raw;
                if(raw) { fresh->values.reset_bytes(tail); }
                else
#else
                static_cast<void>(raw);
#endif
                {
                    auto* values{initialize_carrier_tail(tail, length, complete_source)};
                    if(values==nullptr) { status=gc_object_status::size_overflow; }
                    else { fresh->values.reset(values); }
                }
                if(status==gc_object_status::ok)
                {
                    fresh->owner=this;fresh->kind=kind;fresh->type_index=type_index;fresh->length=length;
                    commit_reserved_slot_locked(reservation.envelope->metadata,held);
                }
            }
            if(status!=gc_object_status::ok) { destroy_object(fresh);return status; }
            result=fresh;return gc_object_status::ok;
        }
