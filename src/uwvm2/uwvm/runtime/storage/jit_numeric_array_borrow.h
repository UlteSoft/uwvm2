// Authenticated native borrow for ONE generated numeric read or write only.
// No pointer may cross a callback, debugger boundary or GC safepoint.
// The external stopped-reader collection contract is unchanged from array_get.
        static_assert(::std::is_standard_layout_v<object>);
        static_assert(sizeof(::std::atomic_flag) == 1uz && alignof(::std::atomic_flag) == 1uz);
        inline static constexpr ::std::size_t jit_array_length_offset{offsetof(object, length)};
        inline static constexpr ::std::size_t jit_array_values_offset{offsetof(object, values) + offsetof(object_values_view, pointer)};
        inline static constexpr ::std::size_t jit_array_numeric_offset{offsetof(object, numeric_array_)};
        inline static constexpr ::std::size_t jit_array_lock_offset{offsetof(object, mutation_lock)};
#if defined(UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH) && UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH == 1
        inline static constexpr ::std::size_t jit_array_token_offset{offsetof(object, token)};
        inline static constexpr ::std::size_t jit_array_hash_next_offset{offsetof(object, hash_next)};
        inline static constexpr ::std::size_t jit_array_owner_offset{offsetof(object, owner)};
        inline static constexpr ::std::size_t jit_array_type_offset{offsetof(object, type_index)};
        inline static constexpr ::std::size_t jit_array_kind_offset{offsetof(object, kind)};
        inline static constexpr ::std::size_t jit_array_bucket_mask{bucket_count - 1uz};
        static_assert(sizeof(::std::atomic<object*>) == sizeof(object*));
        static_assert(alignof(::std::atomic<object*>) == alignof(object*));
        static_assert(sizeof(gc_type::composite_kind) == 4uz);
        // This is an index-domain descriptor, not an object borrow. Its table
        // allocation is immutable until store teardown after activations drain.
        // Collection changes bucket heads while stopped, never the table base.
        [[nodiscard]] inline unsigned __int128 jit_numeric_array_index_domain() const noexcept
        {
            if(!valid_ || !membership_buckets_) { return 0u; }
            return static_cast<unsigned __int128>(reinterpret_cast<::std::uintptr_t>(membership_buckets_.get())) << 64u |
                reinterpret_cast<::std::uintptr_t>(this);
        }
#endif
        template<::std::size_t Width, bool Mutable, bool Float>
        [[nodiscard]] inline unsigned __int128 jit_borrow_numeric_array32(
            gc_reference reference, ::std::uint32_t expected_type) const noexcept
        {
            static_assert(Width == 1uz || Width == 2uz || Width == 4uz);
            static_assert(!Float || Width == 4uz);
            auto const failure{[](gc_object_status status) noexcept
            { return static_cast<unsigned __int128>(status) << 64u; }};
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return failure(reference_error(reference)); }
            // Local exact declaration: validated compiler supplies the layout.
            // Foreign or subtype declarations retain actual-owner schema checks.
            if(obj->owner != this || obj->type_index != expected_type)
            {
                auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
                if(layout == nullptr || layout->field_count != 1uz || !layout->fields)
                { return failure(gc_object_status::invalid_type); }
                auto const& field{layout->fields[0uz]};
                constexpr auto packed{Width == 1uz ? gc_type::packed_kind::i8 :
                    Width == 2uz ? gc_type::packed_kind::i16 : gc_type::packed_kind::none};
                constexpr auto value_kind{Float ? gc_type::value_kind::f32 : gc_type::value_kind::i32};
                if(field.mutable_ != Mutable || field.storage.packed != packed ||
                   field.storage.value.kind != value_kind)
                { return failure(gc_object_status::invalid_type); }
            }
            // Canonical member address, never a dereferenced opaque Wasm token.
            // Bounds/address/access/lock are generated immediately with no calls.
            return reinterpret_cast<::std::uintptr_t>(obj);
        }
