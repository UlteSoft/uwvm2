// Included in the public gc_object_store section only for the exact numeric
// array experiment. Token lookup, owner leases, array extent and mutation
// exclusion remain the same as the complete-carrier array_get operation.
// https://webassembly.github.io/spec/core/exec/instructions.html#exec-array-get
        template<bool SignExtend>
        [[nodiscard]] inline ::std::uint64_t array_get32(
            gc_reference reference, ::std::size_t index) const noexcept
        {
            auto const failure{[](gc_object_status status) noexcept
            { return static_cast<::std::uint64_t>(status) << 32u; }};
            // [opaque reference token][actual canonical store membership]
            // [safe ] this lookup never dereferences a guest-provided token.
            // A foreign result retains its actual owner under the original
            // lease contract; readers use the original entry exclusion.
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return failure(reference_error(reference)); }
            if(index >= obj->length) { return failure(gc_object_status::out_of_bounds); }
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
            if(layout == nullptr || layout->field_count != 1uz || !layout->fields)
            { return failure(gc_object_status::invalid_type); }
            auto const& field{layout->fields[0uz]};
            auto const width{numeric_array_storage_width(field.storage)};
            // Only the validator-selected i8/i16/i32/f32 result shapes use
            // this scalar ABI. Other genuine array layouts remain supported
            // by array_get; a native misuse cannot read their first four bytes.
            if(width != 1uz && width != 2uz && width != 4uz)
            { return failure(gc_object_status::invalid_type); }
            auto const read_bits{[&]() noexcept -> ::std::uint32_t
            {
                if(!obj->numeric_array_)
                {
                    // [complete legacy carrier array][index < length]
                    // [safe ] the actual gc_object_value is alive; its raw
                    // integer view copies bytes without an FP operation.
                    return obj->values[index].template as<::std::uint32_t>();
                }
                // [retained original byte tail][length*width allocation]
                // [safe ] allocate checked the complete product against
                // PTRDIFF_MAX. The actual immutable layout supplies width and
                // index<length selects one whole initialized element.
                auto const* source{obj->values.byte_data() + index * width};
                if(width == 1uz)
                { return ::std::to_integer<::std::uint8_t>(*source); }
                if(width == 2uz)
                {
                    ::std::uint16_t bits{};
                    // [complete two-byte element][live native uint16 object]
                    // [safe ] a literal byte copy handles unaligned storage
                    // and both native byte orders without pointer type punning.
                    ::fast_io::freestanding::my_memcpy(::std::addressof(bits), source, 2uz);
                    return bits;
                }
                ::std::uint32_t bits{};
                // [complete four-byte element][live native uint32 object]
                // [safe ] the raw f32/i32 representation is preserved,
                // including signalling NaN payloads and signed zero.
                ::fast_io::freestanding::my_memcpy(::std::addressof(bits), source, 4uz);
                return bits;
            }};
            ::std::uint32_t bits{};
            if(field.mutable_)
            {
                // Exactly the original array_get acquire/release lock. It
                // covers the entire load; no new guard or owner is introduced.
                object_lock lock{*obj};
                bits = read_bits();
            }
            else { bits = read_bits(); }
            if(width == 1uz)
            {
                bits &= 0xffu;
                if constexpr(SignExtend) { bits = (bits ^ 0x80u) - 0x80u; }
            }
            else if(width == 2uz)
            {
                bits &= 0xffffu;
                if constexpr(SignExtend) { bits = (bits ^ 0x8000u) - 0x8000u; }
            }
            // Unsigned extension arithmetic is modulo 2^32; success status
            // occupies zero upper bits. No output pointer/buffer is exposed.
            return static_cast<::std::uint64_t>(bits);
        }
