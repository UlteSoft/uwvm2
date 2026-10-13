// Private native storage ownership only; no debug/restore/PC capability.
// Real generated returns, tails and EH cleanup retire the exact LIFO buffer.
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <span>
# include <utility>
# include <vector>
#endif
#include <uwvm2/utils/macro/push_macros.h>
namespace uwvm2::runtime::lib::details
{
    class debug_operand_workspace_stack
    {
        struct buffer_record
        {
            ::std::vector<::std::byte> bytes{};
            ::std::size_t prefix{};
            // Physical constructed byte storage may outlive a frame. Only
            // this exact active extent is visible to any current borrow.
            ::std::size_t extent{};
        };
        ::std::vector<buffer_record> buffers_{};
        ::std::size_t active_{};
        // Retain storage, never a live frame or initialized value. A fixed
        // 32 * 16 KiB ceiling prevents a deep/large guest call from retaining
        // its peak memory indefinitely. Oversized/excess buffers retire normally.
        inline static constexpr ::std::size_t recycled_limit{16384u};
        inline static constexpr ::std::size_t retained_records{32u};
        [[nodiscard]] UWVM_NOINLINE ::std::span<::std::byte> enter_cold(::std::size_t bytes, ::std::size_t prefix)
        {
            auto const added{active_ == buffers_.size()};
            if(added) { buffers_.emplace_back(); }
            auto& record{buffers_[active_]};
#if defined(__cpp_exceptions) && __cpp_exceptions >= 199711L
            try { record.bytes.resize(bytes+prefix); }
            catch(...) { if(added) { buffers_.pop_back(); } throw; }
#else
            record.bytes.resize(bytes+prefix);
#endif
            // A grown vector may retain its old prefix. Clear the ENTIRE new
            // active allocation, including site, flags, carriers and padding.
            ::std::memset(record.bytes.data(),0,bytes+prefix);
            record.prefix=prefix; record.extent=bytes+prefix; ++active_;
            return {record.bytes.data()+prefix,bytes};
        }
    public:
        debug_operand_workspace_stack() = default;
        debug_operand_workspace_stack(debug_operand_workspace_stack const&) = default;
        debug_operand_workspace_stack& operator=(debug_operand_workspace_stack const&) = default;
        debug_operand_workspace_stack(debug_operand_workspace_stack&& other) noexcept:
            buffers_(::std::move(other.buffers_)), active_(::std::exchange(other.active_,0u))
        { other.buffers_.clear(); }
        debug_operand_workspace_stack& operator=(debug_operand_workspace_stack&& other) noexcept
        {
            if(this != &other)
            {
                buffers_ = ::std::move(other.buffers_);
                active_ = ::std::exchange(other.active_,0u);
                other.buffers_.clear();
            }
            return *this;
        }
        struct local_snapshot
        {
            ::std::span<::std::byte const> values{};
            ::std::span<::std::uint8_t const> flags{};
            bool valid{};
        };
        [[nodiscard]] ::std::size_t size() const noexcept { return active_; }
        [[nodiscard]] UWVM_ALWAYS_INLINE ::std::span<::std::byte> enter(::std::size_t bytes, ::std::size_t prefix = 0u)
        {
            auto const limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
            if(bytes == 0u || prefix > limit || bytes > limit-prefix) { return {}; }
            if(active_ == buffers_.size() || buffers_[active_].bytes.size() < bytes+prefix)
            { return enter_cold(bytes,prefix); }
            auto& record{buffers_[active_]};
            // No resize or allocation in a reused depth. Every active byte is
            // freshly cleared; no previous initialized state is reused.
            ::std::memset(record.bytes.data(),0,bytes+prefix);
            record.prefix=prefix; record.extent=bytes+prefix; ++active_;
            return {record.bytes.data()+prefix,bytes};
        }
        // A bounded physical reserve allows constant-size native clearing.
        // Publish only the requested active extent, never the padded suffix.
        // All site/flags/carrier/padding bytes are zero BEFORE returning.
        [[nodiscard]] UWVM_ALWAYS_INLINE ::std::span<::std::byte> enter_small(::std::size_t bytes)
        {
            constexpr ::std::size_t capacity{64u}, prefix{sizeof(::std::uint64_t)};
            if(bytes == 0u || bytes > capacity) { return {}; }
            if(active_ == buffers_.size() || buffers_[active_].bytes.size() < capacity+prefix)
            {
                auto const allocation{enter_cold(capacity,prefix)};
                buffers_[active_-1u].extent=bytes+prefix;
                return {allocation.data(),bytes};
            }
            auto& record{buffers_[active_]};
            ::std::memset(record.bytes.data(),0,capacity+prefix);
            record.prefix=prefix;record.extent=bytes+prefix;++active_;
            return {record.bytes.data()+prefix,bytes};
        }
        // Only the actual owning runtime uses this full allocation to bind its
        // private site cell. Matching a label still grants no execution rights.
        [[nodiscard]] ::std::span<::std::byte> current_storage() noexcept
        {
            if(active_ == 0u) { return {}; }
            auto& record{buffers_[active_-1u]};
            return {record.bytes.data(),record.extent};
        }
        [[nodiscard]] UWVM_ALWAYS_INLINE bool leave(::std::uintptr_t actual_buffer) noexcept
        {
            if(active_ == 0u) { return false; }
            auto& record{buffers_[active_-1u]}; auto& buffer{record.bytes};
            if(actual_buffer != reinterpret_cast<::std::uintptr_t>(buffer.data()+record.prefix)) { return false; }
            // Reuse this depth directly; no vector swap or record destruction
            // on a repeated leaf call. Inactive records carry no live extent.
            if(buffer.capacity() > recycled_limit)
            { ::std::vector<::std::byte>{}.swap(buffer); }
            record.prefix = record.extent = 0u;
            --active_;
            if(buffers_.size()-active_ > retained_records) { buffers_.pop_back(); }
            return true;
        }
        // Cold owning-thread borrow only. Match the supplied payload label
        // with the actual LIFO buffer BEFORE forming any local/flag pointer.
        // This storage check supplies neither a pause nor activation authority.
        [[nodiscard]] local_snapshot current_local_snapshot(::std::uintptr_t actual_payload,
            ::std::size_t locals, ::std::size_t metadata_bytes, ::std::size_t slot_bytes,
            ::std::size_t packet_slots = 0u) const noexcept
        {
            if(active_ == 0u || metadata_bytes < 2u || slot_bytes == 0u) { return {}; }
            auto const& record{buffers_[active_-1u]};
            auto const buffer{::std::span<::std::byte const>{record.bytes.data(),record.extent}.subspan(record.prefix)};
            if(locals > buffer.size()/metadata_bytes) { return {}; }
            auto const prefix{locals*metadata_bytes};
            if(packet_slots == 0u) { packet_slots = locals; }
            if(packet_slots < locals || packet_slots > (buffer.size()-prefix)/slot_bytes) { return {}; }
            auto const base{reinterpret_cast<::std::uintptr_t>(buffer.data())};
            if(base > UINTPTR_MAX-prefix || actual_payload != base+prefix) { return {}; }
            return {{buffer.data()+prefix,packet_slots*slot_bytes},
                {reinterpret_cast<::std::uint8_t const*>(buffer.data()+locals),locals},true};
        }
    };
}
#include <uwvm2/utils/macro/pop_macros.h>
