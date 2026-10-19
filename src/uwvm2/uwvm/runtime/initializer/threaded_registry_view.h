// A genuine one-entry native view, not a map copied from a parent instance.
#pragma once
#include <optional>
#include <memory>
#include <utility>
#include <uwvm2/utils/container/impl.h>

namespace uwvm2::uwvm::runtime::initializer
{
    template<typename Module> class threaded_single_entry_view final
    {
    public:
        struct entry
        {
            ::uwvm2::utils::container::u8string_view first;
            Module& second;
        };
    private:
        ::std::optional<entry> entry_{};
    public:
        void bind(::uwvm2::utils::container::u8string_view name, Module& storage) noexcept
        { entry_.emplace(name, storage); }
        [[nodiscard]] ::std::size_t size() const noexcept { return entry_ ? 1uz : 0uz; }
        [[nodiscard]] bool empty() const noexcept { return !entry_; }
        [[nodiscard]] entry* begin() noexcept { return entry_ ? ::std::addressof(*entry_) : nullptr; }
        [[nodiscard]] entry const* begin() const noexcept { return entry_ ? ::std::addressof(*entry_) : nullptr; }
        [[nodiscard]] entry* end() noexcept { return entry_ ? ::std::addressof(*entry_) + 1 : nullptr; }
        [[nodiscard]] entry const* end() const noexcept { return entry_ ? ::std::addressof(*entry_) + 1 : nullptr; }
        [[nodiscard]] auto find(::uwvm2::utils::container::u8string_view name) noexcept
        { return entry_ && entry_->first == name ? begin() : end(); }
        [[nodiscard]] auto find(::uwvm2::utils::container::u8string_view name) const noexcept
        { return entry_ && entry_->first == name ? begin() : end(); }
        [[nodiscard]] auto cend() const noexcept { return end(); }
    };
}
