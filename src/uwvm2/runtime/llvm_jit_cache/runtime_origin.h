// SOURCE ONLY: genuine Runtime-issued, short-lived cache-origin admission.
#pragma once
#include <memory>

namespace uwvm2::runtime::lib { class runtime_cache_origin_bridge; }
namespace uwvm2::runtime::llvm_jit_cache::runtime_origin
{
    class ticket
    {
        friend class ::uwvm2::runtime::lib::runtime_cache_origin_bridge;
        ticket() noexcept = default;
        [[nodiscard]] virtual bool actual_current() const noexcept = 0;
        [[nodiscard]] virtual bool actual_secret_phase() const noexcept = 0;
    public:
        ticket(ticket const&) = delete;
        ticket& operator=(ticket const&) = delete;
        virtual ~ticket() = default;
        // This dispatch belongs to the actual Runtime-minted native object.
        // No context boolean, registry label or supplied callback mints one.
        [[nodiscard]] bool authorized() const noexcept { return actual_current(); }
        [[nodiscard]] bool secret_reads_allowed() const noexcept { return actual_current() && actual_secret_phase(); }
    };
    using use = ::std::shared_ptr<ticket const>;
    using reference = ::std::weak_ptr<ticket const>;
}

namespace uwvm2::runtime::lib
{
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
    // Defined solely in the actual Runtime TU. A component-only cache library
    // or an unknown embedder has no issuer and must disable persistent I/O.
    [[gnu::weak, gnu::visibility("hidden")]]
    ::uwvm2::runtime::llvm_jit_cache::runtime_origin::reference runtime_cache_current_origin() noexcept;
#endif
}

namespace uwvm2::runtime::llvm_jit_cache::runtime_origin
{
    [[nodiscard]] inline reference capture() noexcept
    {
#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
        if(::uwvm2::runtime::lib::runtime_cache_current_origin != nullptr)
        { return ::uwvm2::runtime::lib::runtime_cache_current_origin(); }
#endif
        return {};
    }
    [[nodiscard]] inline use acquire(reference const& value) noexcept
    {
        auto actual{value.lock()};
        return actual && actual->authorized() ? actual : use{};
    }
    [[nodiscard]] inline bool authorized(use const& value) noexcept
    { return value && value->authorized(); }
    [[nodiscard]] inline bool secret_reads_allowed(use const& value) noexcept
    { return value && value->secret_reads_allowed(); }
}
