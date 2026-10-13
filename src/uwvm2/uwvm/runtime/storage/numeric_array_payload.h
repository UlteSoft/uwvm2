// Included only in gc_object_store under the exact numeric-array experiment.
// Original token membership, cohort ownership, mutation locks and publication
// stay authoritative. These private helpers accept an already-checked native
// object and layout; they do not admit a Wasm address or an unowned byte span.

        [[nodiscard]] static constexpr ::std::size_t numeric_array_storage_width(
            gc_type::storage_type storage) noexcept
        {
            if(storage.packed == gc_type::packed_kind::i8)
            { return storage.value.kind == gc_type::value_kind::i32 ? 1uz : 0uz; }
            if(storage.packed == gc_type::packed_kind::i16)
            { return storage.value.kind == gc_type::value_kind::i32 ? 2uz : 0uz; }
            if(storage.packed != gc_type::packed_kind::none) { return 0uz; }
            switch(storage.value.kind)
            {
                case gc_type::value_kind::i32:
                case gc_type::value_kind::f32: return 4uz;
                case gc_type::value_kind::i64:
                case gc_type::value_kind::f64: return 8uz;
                case gc_type::value_kind::v128: return 16uz;
                default: return 0uz; // Every reference/unsupported form stays legacy.
            }
        }
        [[nodiscard]] static constexpr ::std::size_t numeric_array_width(type_layout const* layout) noexcept
        {
            if(layout == nullptr || layout->kind != gc_type::composite_kind::array ||
               layout->field_count != 1uz || !layout->fields) { return 0uz; }
            return numeric_array_storage_width(layout->fields[0uz].storage);
        }
        [[nodiscard]] static inline gc_object_value load_array_value(
            object const& obj, ::std::size_t index, gc_type::storage_type storage) noexcept
        {
            if(!obj.numeric_array_) { return obj.values[index]; }
            auto const width{numeric_array_storage_width(storage)};
            // [live original byte tail][length*width][one-past allocation]
            // [safe ] caller proved index<length, actual immutable array
            // layout and owner lifetime. Allocation checked the whole product
            // against PTRDIFF_MAX; no element offset or end can wrap.
            auto const* source{obj.values.byte_data() + index * width};
            if(width == 1uz)
            { return gc_object_value::i32(::std::to_integer<::std::uint8_t>(*source)); }
            if(width == 2uz)
            {
                ::std::uint16_t bits{};
                // [complete two-byte element][real aligned uint16 object]
                // [safe ] copy raw bytes without an unaligned typed access.
                ::fast_io::freestanding::my_memcpy(::std::addressof(bits), source, 2uz);
                return gc_object_value::i32(bits);
            }
            gc_object_value result{};
            // [complete width-byte element][complete 16-byte carrier]
            // [safe ] exact native-bit copies preserve signalling NaNs and
            // both endian orders. The remaining carrier bytes stay zero.
            switch(width)
            {
                case 4uz: ::fast_io::freestanding::my_memcpy(result.bits.data(), source, 4uz); break;
                case 8uz: ::fast_io::freestanding::my_memcpy(result.bits.data(), source, 8uz); break;
                case 16uz: ::fast_io::freestanding::my_memcpy(result.bits.data(), source, 16uz); break;
                default: ::std::terminate(); // Impossible for allocator-selected byte tails.
            }
            return result;
        }
        static inline void store_array_value(object& obj, ::std::size_t index,
            gc_object_value value, gc_type::storage_type storage) noexcept
        {
            if(!obj.numeric_array_) { obj.values[index] = value; return; }
            auto const width{numeric_array_storage_width(storage)};
            // [live original byte tail][length*width][one-past allocation]
            // [safe ] caller checked the complete destination element and
            // holds the original mutation lock, or owns an unpublished array.
            // Only the actual allocation's retained byte pointer is advanced.
            auto* destination{obj.values.byte_data() + index * width};
            if(width == 1uz)
            {
                *destination = static_cast<::std::byte>(static_cast<::std::uint8_t>(value.as<::std::uint32_t>()));
            }
            else if(width == 2uz)
            {
                // Truncate as an integer before copying its native bytes.
                // Copying the first two bytes of an i32 would be wrong on BE.
                auto const bits{static_cast<::std::uint16_t>(value.as<::std::uint32_t>())};
                // [real uint16 source][complete two-byte destination element]
                // [safe ] no floating conversion or unaligned typed store.
                ::fast_io::freestanding::my_memcpy(destination, ::std::addressof(bits), 2uz);
            }
            else
            {
                // [complete 16-byte carrier][complete width-byte element]
                // [safe ] each constant copy is bounded by the checked native
                // layout. Bits are never interpreted through floating ABI.
                switch(width)
                {
                    case 4uz: ::fast_io::freestanding::my_memcpy(destination, value.bits.data(), 4uz); break;
                    case 8uz: ::fast_io::freestanding::my_memcpy(destination, value.bits.data(), 8uz); break;
                    case 16uz: ::fast_io::freestanding::my_memcpy(destination, value.bits.data(), 16uz); break;
                    default: ::std::terminate();
                }
            }
        }
        template<typename Pattern>
        static inline void fill_numeric_pattern(::std::byte* destination,
            ::std::size_t count, Pattern pattern) noexcept
        {
            static_assert(::std::is_trivially_copyable_v<Pattern>);
            static_assert(sizeof(Pattern) == 2uz || sizeof(Pattern) == 4uz ||
                          sizeof(Pattern) == 8uz || sizeof(Pattern) == 16uz);
            // Caller validated the complete count*width extent and mutation
            // ownership. Each advancing byte window belongs to that same
            // allocation. Only the local pattern representation is copied.
            auto* const begin{destination};
            // The caller has checked count*width for this complete array tail.
            // Small fills use fixed-width stores. Large fills seed a bounded
            // 512-byte prefix, then let the platform libc copy full byte ranges.
            // No ISA-specific intrinsic, alignment promise or FP conversion.
            auto const bytes{count * sizeof(Pattern)};
            auto const seed_count{bytes >= 2048uz ? 512uz / sizeof(Pattern) : count};
            for(::std::size_t index{}; index != seed_count; ++index)
            {
                ::std::memcpy(destination, ::std::addressof(pattern), sizeof(Pattern));
                destination += sizeof(Pattern);
            }
            auto filled{seed_count * sizeof(Pattern)};
            while(filled != bytes)
            {
                auto const remaining{bytes - filled};
                auto const copy{filled < remaining ? filled : remaining};
                // [source 0..copy)[destination filled..filled+copy) within the
                // same live byte tail. copy<=filled proves no overlap; the
                // remaining subtraction bounds the full destination extent.
                ::std::memcpy(begin + filled, begin, copy);
                filled += copy;
            }
        }
        static inline void fill_array_values(object& obj, ::std::size_t offset,
            ::std::size_t count, gc_object_value value, gc_type::storage_type storage) noexcept
        {
            // Caller proved offset/count by subtraction, validated the value
            // and retained its one semantic foreign lease before entering.
            // An unpublished array or the original mutation lock owns writes.
            if(count == 0uz) { return; }
            static_assert(::std::is_trivially_copyable_v<gc_object_value>);
            if(obj.numeric_array_)
            {
                auto const width{numeric_array_storage_width(storage)};
                if(width == 0uz) { ::std::terminate(); }
                auto* destination{obj.values.byte_data() + offset * width};
                if(width == 1uz)
                {
                    ::std::memset(destination, static_cast<unsigned char>(value.as<::std::uint32_t>()), count);
                    return;
                }
                // Repeat a fixed native-width pattern through bounded stores and libc copies.
                // Pattern objects are real local objects; the destination is
                // the original bounded byte tail. Constant-size memcpy does
                // not form unaligned typed pointers or new semantic leases.
                // Packed i16 truncates before copying on big-endian hosts;
                // floating and v128 bits never enter floating arithmetic.
                switch(width)
                {
                    case 2uz:
                        fill_numeric_pattern(destination, count,
                            static_cast<::std::uint16_t>(value.as<::std::uint32_t>()));
                        break;
                    case 4uz:
                        fill_numeric_pattern(destination, count, value.as<::std::uint32_t>());
                        break;
                    case 8uz:
                        fill_numeric_pattern(destination, count, value.as<::std::uint64_t>());
                        break;
                    case 16uz:
                        fill_numeric_pattern(destination, count,
                            value.as<::std::array<::std::uint64_t, 2uz>>());
                        break;
                    default: ::std::terminate();
                }
            }
            else
            {
                // Every carrier lifetime was started by allocate(). References
                // remain opaque byte values; no new lease/root is inferred from
                // copying a representation, and no publication occurs here.
                auto* destination{obj.values.get()+offset};destination[0uz]=value;
                for(::std::size_t initialized{1uz};initialized<count;)
                {
                    auto const remaining{count - initialized};
                    auto const copy{initialized < remaining ? initialized : remaining};
                    ::std::memcpy(destination+initialized,destination,copy*sizeof(gc_object_value));
                    initialized+=copy;
                }
            }
        }
        static inline void copy_array_values(object& destination, ::std::size_t dst_offset,
            object const& source, ::std::size_t src_offset, ::std::size_t count,
            gc_type::storage_type storage) noexcept
        {
            // The caller proved BOTH complete element windows and holds BOTH
            // original object locks in their original order. It also validated
            // equal numeric storage or every reference/lease BEFORE this call.
            if(count == 0uz) { return; } // Never form a pointer from an empty tail.
            if(destination.numeric_array_ && source.numeric_array_)
            {
                auto const width{numeric_array_storage_width(storage)};
                // [actual destination byte tail][dst_offset,count checked]
                // [safe ] allocation product and subtracted window bounds
                // prove the offset and full count*width without overflow.
                auto* dst{destination.values.byte_data() + dst_offset * width};
                // [actual source byte tail][src_offset,count checked]
                // [safe ] only a retained original byte pointer is advanced.
                auto const* src{source.values.byte_data() + src_offset * width};
                ::fast_io::freestanding::my_memmove(dst, src, count * width); // Overlapping self-copy is defined.
            }
            else if(!destination.numeric_array_ && !source.numeric_array_)
            {
                // [complete carrier arrays][both checked nonempty windows]
                // [safe ] the original bounded memmove and alias semantics.
                ::fast_io::freestanding::my_memmove(destination.values.get() + dst_offset, source.values.get() + src_offset,
                    count * sizeof(gc_object_value));
            }
            else
            {
                // Distinct formats imply distinct objects; self-copy can
                // never take this edge. Keep complete carrier conversion for
                // a genuine legacy/compact native interoperability boundary.
                for(::std::size_t index{}; index != count; ++index)
                {
                    // [checked source and destination windows][index<count]
                    // [safe ] additions stay in the corresponding allocations.
                    store_array_value(destination, dst_offset + index,
                        load_array_value(source, src_offset + index, storage), storage);
                }
            }
        }
