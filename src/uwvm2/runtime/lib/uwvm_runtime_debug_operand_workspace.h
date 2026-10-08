// Private native storage ownership only; no debug/restore/PC capability.
// Real generated returns, tails and EH cleanup retire the exact LIFO buffer.
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <span>
# include <vector>
#endif
namespace uwvm2::runtime::lib::details
{
    class debug_operand_workspace_stack
    {
        ::std::vector<::std::vector<::std::byte>> buffers_{};
    public:
        [[nodiscard]] ::std::size_t size() const noexcept { return buffers_.size(); }
        [[nodiscard]] ::std::span<::std::byte> enter(::std::size_t bytes)
        {
            if(bytes == 0u || bytes > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())) { return {}; }
            buffers_.emplace_back(bytes); return buffers_.back();
        }
        [[nodiscard]] bool leave(::std::uintptr_t actual_buffer) noexcept
        {
            if(buffers_.empty() || actual_buffer != reinterpret_cast<::std::uintptr_t>(buffers_.back().data())) { return false; }
            buffers_.pop_back(); return true;
        }
    };
}
