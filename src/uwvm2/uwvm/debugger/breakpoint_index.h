#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger
{
    // Serialized by the controller mutex. This is only a candidate index:
    // the caller must still compare the complete key and breakpoint policy.
    // Zero means no candidate; stored values are physical slot index + 1.
    struct breakpoint_index
    {
        static constexpr ::std::size_t capacity{256u}, buckets{1024u};
        ::std::array<::std::uint16_t, buckets> heads{};
        ::std::array<::std::uint16_t, capacity> links{};
        [[nodiscard]] static constexpr ::std::size_t bucket(::std::uint64_t module,
            ::std::uint64_t function, ::std::uint64_t offset) noexcept
        {
            auto mix{[](::std::uint64_t n) constexpr noexcept
            { n ^= n >> 30u; n *= 0xbf58476d1ce4e5b9ull; n ^= n >> 27u;
              n *= 0x94d049bb133111ebull; return n ^ (n >> 31u); }};
            return (mix(module) ^ mix(function + 0x9e3779b97f4a7c15ull) ^ mix(offset)) & (buckets - 1u);
        }
        constexpr void clear() noexcept { heads.fill(0u); links.fill(0u); }
        // Insert in descending slot order to preserve the original scan order,
        // including an unconditional launch point preceding a conditional one.
        constexpr void insert(::std::size_t slot, ::std::uint64_t module,
            ::std::uint64_t function, ::std::uint64_t offset) noexcept
        {
            if(slot >= capacity) { return; }
            auto& head{heads[bucket(module,function,offset)]};
            links[slot] = head; head = static_cast<::std::uint16_t>(slot + 1u);
        }
        [[nodiscard]] constexpr ::std::uint16_t first(::std::uint64_t module,
            ::std::uint64_t function, ::std::uint64_t offset) const noexcept
        { return heads[bucket(module,function,offset)]; }
        [[nodiscard]] constexpr ::std::uint16_t next(::std::uint16_t candidate) const noexcept
        { return candidate != 0u && candidate <= capacity ? links[candidate - 1u] : 0u; }
    };
}
