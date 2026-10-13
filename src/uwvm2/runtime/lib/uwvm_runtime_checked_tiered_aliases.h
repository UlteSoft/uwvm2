#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>

namespace uwvm2::runtime::lib::details
{
    // Cold symbol-resolution scratch only: this owns INTEGER diagnostic DATA,
    // not an engine/FDE/native-PC, source, execution or debugger capability.
    // The real materializer resolves every entry in the same private engine,
    // installs the engine/actual listener ranges, then records these aliases
    // before publishing T2 READY. Default full/instruction-policy T2 create no allocation or rows.
    class checked_tiered_native_aliases final
    {
    public:
        struct row
        {
            ::std::size_t function_index{};
            ::std::uintptr_t address{};
            bool raw_entry{}, wrapper_entry{};
        };
        static constexpr ::std::size_t maximum_entries{65536uz};
    private:
        ::std::unique_ptr<row[]> rows_{};
        ::std::size_t extent_{}, written_{};
    public:
        checked_tiered_native_aliases() = default;
        checked_tiered_native_aliases(checked_tiered_native_aliases const&) = delete;
        checked_tiered_native_aliases& operator=(checked_tiered_native_aliases const&) = delete;
        [[nodiscard]] bool prepare(::std::size_t count) noexcept
        {
            if(rows_ != nullptr || extent_ != 0uz || written_ != 0uz || count > maximum_entries ||
               count > (::std::numeric_limits<::std::size_t>::max)() / sizeof(row)) { return false; }
            if(count == 0uz) { return true; }
            // Bounded cold best-effort allocation, no throwing allocation inside
            // the runtime's noexcept materializer. Failure means unavailable T2,
            // never invalid guest code or permission to rewalk original bytes.
            ::std::unique_ptr<row[]> pending{new (::std::nothrow) row[count]};
            if(pending == nullptr) { return false; }
            rows_ = ::std::move(pending); extent_ = count;
            return true;
        }
        [[nodiscard]] bool append(::std::size_t function_index, ::std::uintptr_t address,
            bool raw_entry, bool wrapper_entry) noexcept
        {
            if(address == 0u || rows_ == nullptr || written_ >= extent_) { return false; }
            // [one owned cold row0 ... written_ ... extent_] one-past
            // [safe] written_<extent_ BEFORE exact indexed store; no source or
            // native-code pointer is dereferenced/advanced by these integer DATA.
            rows_[written_] = {function_index, address, raw_entry, wrapper_entry};
            ++written_; // Count cannot overflow: the previous check proved <extent_<=65536.
            return true;
        }
        [[nodiscard]] bool complete() const noexcept { return written_ == extent_; }
        [[nodiscard]] ::std::span<row const> entries() const noexcept
        {
            if(!complete() || extent_ == 0uz) { return {}; }
            // [exact owned complete rows0 ... extent_] one-past
            // [safe] span retains its declared extent; iterator advancement is
            // performed only by bounded range iteration while this owner lives.
            return {rows_.get(), extent_};
        }
    };
}
