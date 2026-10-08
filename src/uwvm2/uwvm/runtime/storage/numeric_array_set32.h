// Included only in gc_object_store's public section for exact SET32=1.
// This is scalar VALUE marshalling, not a new ownership or collection proof.
// The original checked_object, foreign lease and mutable object lock remain.
        [[nodiscard]] inline gc_object_status array_set32(
            gc_reference reference, ::std::size_t index, ::std::uint32_t bits) noexcept
        {
            // [opaque token][actual membership/owner lookup]
            // [safe ] no guest key is dereferenced. A foreign lookup must
            // retain its actual canonical owner under the original lease.
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return reference_error(reference); }
            if(index >= obj->length) { return gc_object_status::out_of_bounds; }
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
            if(layout == nullptr || layout->field_count != 1uz || !layout->fields)
            { return gc_object_status::invalid_type; }
            // [owned immutable array layout][exactly one actual field]
            // [safe ] the canonical owner pins this field across the entire
            // access. Array extent/layout cannot be changed by array.set.
            auto const& field{layout->fields[0uz]};
            if(!field.mutable_) { return gc_object_status::immutable_field; }
            auto const packed{field.storage.packed};
            if(packed == gc_type::packed_kind::none)
            {
                if(field.storage.value.kind != gc_type::value_kind::i32 &&
                   field.storage.value.kind != gc_type::value_kind::f32)
                { return gc_object_status::invalid_type; }
            }
            else if((packed != gc_type::packed_kind::i8 && packed != gc_type::packed_kind::i16) ||
                    field.storage.value.kind != gc_type::value_kind::i32)
            { return gc_object_status::invalid_type; }
            // Rebuild a zeroed complete native carrier only inside the leaf.
            // i32/f32 use the SAME low 32 raw bits; no floating operation,
            // signalling-NaN evaluation or public GC pointer ABI is involved.
            auto const value{gc_object_value::i32(bits)};
            if(!value_matches(value, field.storage, obj->owner))
            { return gc_object_status::invalid_value; }
            auto const lease{retain_embedded_reference(*obj, value, field.storage)};
            if(lease != gc_object_status::ok) { return lease; }
            object_lock lock{*obj};
            // [actual array allocation][index < immutable length]
            // [safe ] only the original pack/store route changes one complete
            // initialized element under the original mutation lock. Integer
            // truncation before byte-copy preserves i8/i16 on big endian.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            store_array_value(*obj, index, pack(value, packed), field.storage);
#else
            obj->values[index] = pack(value, packed);
#endif
            return gc_object_status::ok;
        }
