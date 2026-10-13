// Calling-thread metadata indices only. No cached entry, function pointer or
// code owner is returned. The caller must check the indexed live registry row
// against the actual reference before using it, under its execution lease.
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <memory>
#endif
namespace uwvm2::runtime::lib::details
{
    class function_identity_cache
    {
    public:
        struct identity { ::std::size_t module{}, local{}; };
    private:
        struct row { ::std::uint_least64_t epoch{}; ::std::uintptr_t address{}; identity index{}; };
        ::std::array<row,32u> rows_{};
        [[nodiscard]] static constexpr ::std::size_t slot(::std::uintptr_t address) noexcept
        { return static_cast<::std::size_t>((address >> 4u) ^ (address >> 9u)) & 31u; }
    public:
        [[nodiscard]] identity const* find(::std::uint_least64_t epoch, ::std::uintptr_t address) const noexcept
        {
            if(epoch == 0u || address == 0u) { return nullptr; }
            auto const& selected{rows_[slot(address)]};
            return selected.epoch == epoch && selected.address == address ? ::std::addressof(selected.index) : nullptr;
        }
        // Insert only after the full address-range, stride and live-row check.
        void remember_validated(::std::uint_least64_t epoch, ::std::uintptr_t address, identity index) noexcept
        {
            if(epoch != 0u && address != 0u) { rows_[slot(address)] = {epoch,address,index}; }
        }
    };
}
