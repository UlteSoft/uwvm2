// Internal sealed-numeric ABI only. The header/context/output are actual live
// native objects retained by the owning entry; no guest pointer is admitted.
UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge
{
    inline constexpr char8_t take_semantic[] = u8"uwvm2_pending_numeric_take_r3";

    extern "C" [[nodiscard]] inline word uwvm2_pending_numeric_take_r3(
        word header_address, word tag_index, word destination_address, word destination_bytes) noexcept
    {
        // [empty native borrow slots][no referent or lifetime extension]
        // [safe ] null initialization precedes the one genuine borrow below.
        numeric_header* header{};
        pending_context* owner{};
        numeric_leaf_state borrowed{};
        // [retained complete native header/context][owner-thread borrow]
        // [safe ] borrow resets these output pointers, then derives only the
        // live header's initialized slow pointer. No guest/stale address is
        // authenticated; caller-owned lifetimes cannot race this leaf.
        auto const admitted{native_details::borrow(header_address, header, owner, borrowed)};
        if(admitted != status::ok) { return result(admitted); }
        auto const matched{owner->matches(static_cast<::std::size_t>(tag_index))};
        if(matched != status::ok) { return result(matched); }
        // The actual borrow either used the private successful-publication
        // proof or completed the unchanged generic numeric preflight. A wrong
        // tag never bypasses malformed schema/root rejection; matching remains
        // the real tag INSTANCE comparison above, not cached shape authority.
        auto const required{borrowed.exact_numeric_bytes};
        if(destination_bytes != required) { return result(status::invalid_signature); }
        // [borrowed live context][native integer address only]
        // [safe ] this conversion names the actual retained object solely for
        // disjoint-extent checks; it does not mint authority or dereference it.
        auto const context_address{reinterpret_cast<word>(owner)};
        auto const bounds{native_details::extent(destination_address, destination_bytes,
                                                 header_address, context_address)};
        if(bounds != status::ok) { return result(bounds); }

        // The borrow proves a complete packed numeric prefix from sealed
        // publication, or checks every legacy field kind/width/count and empty
        // owner against the immutable schema. Fresh has no existing value.
        // All failures
        // above precede output writes, slot moves, trace clear or phase stores.
        // After preflight there is no callback, GC/pause, native reentry or
        // C++ throw. leaf_access cannot change on this same native owner thread;
        // clear invalidates its private proof and only resets logical state;
        // numeric EMPTY-owner slots remain constructed and cannot call out.
        if(required != 0uz)
        {
            // [caller-owned COMPLETE byte destination][preflighted end]
            // [safe ] recover only its real native address AFTER all checks.
            // Empty payload never forms or advances a possibly-null pointer.
            auto* output{reinterpret_cast<::std::byte*>(destination_address)};
            if(borrowed.packed)
            {
                // The WHOLE output was authenticated and excluded the actual
                // context/header. Source is the complete committed raw prefix.
                ::std::memcpy(output,borrowed.packed_tuple.data(),required);
            }
            else for(auto const& field : borrowed.fields)
            {
                auto const bits{field.bits()};
                // [complete private field bytes][proved complete output field]
                // [safe ] width/count/root preflight and summed exact bytes
                // cover this entire copy; context/header overlap was rejected.
                // Genuine immutable borrow preflight limits this width to
                // 4 (i32/f32), 8 (i64/f64), or 16 (v128). Selecting the copy
                // width adds no admission proof or late failure after a write;
                // the final else is exactly the already-validated v128 case.
#if defined(__GNUC__) || defined(__clang__)
                if(bits.size() == 4uz) { __builtin_memcpy(output, bits.data(), 4uz); }
                else if(bits.size() == 8uz) { __builtin_memcpy(output, bits.data(), 8uz); }
                else { __builtin_memcpy(output, bits.data(), 16uz); }
#else
                // No new C/OS call on the non-builtin Windows fallback.
                // [complete private field][proved complete output prefix]
                // [safe ] each integer index stays below the selected literal
                // width; subscripting does not advance either native pointer.
                if(bits.size() == 4uz)
                {
                    for(::std::size_t index{}; index != 4uz; ++index)
                    { output[index] = bits[index]; }
                }
                else if(bits.size() == 8uz)
                {
                    for(::std::size_t index{}; index != 8uz; ++index)
                    { output[index] = bits[index]; }
                }
                else
                {
                    for(::std::size_t index{}; index != 16uz; ++index)
                    { output[index] = bits[index]; }
                }
#endif
                // [written field][next field or one-past owned destination]
                // [safe ] advance by its proved width; total is exact_bytes.
                output += bits.size();
            }
        }
        // Numeric-only fusion may clear before LLVM loads numeric SSA bits.
        // It must NEVER replace old copy-before-SSA-root handling for a
        // reference/existing/native-host payload. The original copy leaf and
        // its precise-root comment remain unchanged.
        auto const cleared{owner->clear()};
        if(cleared == status::ok) { native_details::header_access::publish_empty(*header); }
        return result(cleared);
    }
    static_assert(::std::is_same_v<decltype(&uwvm2_pending_numeric_take_r3), quaternary_abi>);
}
