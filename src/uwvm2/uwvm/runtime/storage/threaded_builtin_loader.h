#pragma once
// Cold concrete factory provenance. Names and copied export signatures are not
// authority. The actual loader constructs the typed implementation below and
// records its unique native identity before any Guest is admitted.
#include <array>
#include <cstddef>
#include <concepts>
#include <type_traits>
#include <uwvm2/uwvm/wasm/storage/impl.h>
#include <uwvm2/uwvm/wasm/type/impl.h>

namespace uwvm2::uwvm::imported::wasi::threads { struct local_imported_module_t; }
namespace uwvm2::uwvm::imported::ros
{
    struct ros1_local_imported_module_t;
    struct ros1_client_local_imported_module_t;
    struct ros2_local_imported_module_t;
    struct ros2_client_local_imported_module_t;
    struct ros2_rmw_local_imported_module_t;
    struct ros2_rmw_subscription_local_imported_module_t;
    struct ros2_rmw_service_local_imported_module_t;
}

namespace uwvm2::uwvm::runtime::full
{
    enum class threaded_builtin_kind : unsigned { threads, ros };
    class threaded_builtin_loader final
    {
        using module = ::uwvm2::uwvm::wasm::type::local_imported_t;
        using implementation = decltype(module::ptr);
        struct entry
        {
            implementation native;
            ::std::size_t ordinal;
            threaded_builtin_kind kind;
        };
        inline static ::std::array<entry, 16u> entries_{};
        inline static ::std::size_t count_{};
        template<typename T>
        static constexpr bool is_threads = ::std::same_as<::std::remove_cvref_t<T>,
            ::uwvm2::uwvm::imported::wasi::threads::local_imported_module_t>;
        template<typename T>
        static constexpr bool is_ros =
            ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::uwvm::imported::ros::ros1_local_imported_module_t> ||
            ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::uwvm::imported::ros::ros1_client_local_imported_module_t> ||
            ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::uwvm::imported::ros::ros2_local_imported_module_t> ||
            ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::uwvm::imported::ros::ros2_client_local_imported_module_t> ||
            ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::uwvm::imported::ros::ros2_rmw_local_imported_module_t> ||
            ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::uwvm::imported::ros::ros2_rmw_subscription_local_imported_module_t> ||
            ::std::same_as<::std::remove_cvref_t<T>, ::uwvm2::uwvm::imported::ros::ros2_rmw_service_local_imported_module_t>;
    public:
        // This factory, rather than an API accepting an existing opaque module
        // pointer, performs the real constrained C++ construction itself.
        template<typename Concrete>
            requires (is_threads<Concrete> || is_ros<Concrete>)
        [[nodiscard]] static bool append(Concrete const& concrete)
        {
            constexpr auto kind{is_threads<Concrete> ? threaded_builtin_kind::threads : threaded_builtin_kind::ros};
            auto& providers{::uwvm2::uwvm::wasm::storage::preload_local_imported};
            if(count_ == entries_.size()) { return false; }
            auto const ordinal{providers.size()};
            providers.emplace_back(concrete);
            auto const& actual{providers.index_unchecked(ordinal)};
            if(actual.ptr == nullptr) { return false; }
            entries_[count_++] = {actual.ptr, ordinal, kind};
            return true;
        }
        [[nodiscard]] static bool actual_member(module const* candidate, threaded_builtin_kind& kind) noexcept
        {
            if(candidate == nullptr) { return false; }
            auto const& providers{::uwvm2::uwvm::wasm::storage::preload_local_imported};
            for(::std::size_t i{}; i != count_; ++i)
            {
                auto const& identity{entries_[i]};
                if(identity.ordinal >= providers.size()) { continue; }
                auto const& member{providers.index_unchecked(identity.ordinal)};
                if(::std::addressof(member) != candidate || member.ptr == nullptr || member.ptr != identity.native) { continue; }
                ::std::size_t memberships{};
                for(auto const& declaration : ::uwvm2::uwvm::wasm::storage::all_module)
                {
                    if(declaration.second.type == ::uwvm2::uwvm::wasm::type::module_type_t::local_import &&
                       declaration.second.module_storage_ptr.li == candidate) { ++memberships; }
                }
                if(memberships != 1u) { return false; }
                kind = identity.kind; return true;
            }
            return false;
        }
        // Used only after genuine full-source/provider drain and before loading
        // another graph. It destroys provenance, never a live native provider.
        [[nodiscard]] static bool retire_actual_after_provider_drain() noexcept
        {
            if(!::uwvm2::uwvm::wasm::storage::all_module.empty()) { return false; }
            auto& providers{::uwvm2::uwvm::wasm::storage::preload_local_imported};
            if(count_ == 0u) { return true; }
            auto remaining{providers.size()};
            for(auto index{count_}; index != 0u; --index)
            {
                auto const& actual{entries_[index - 1u]};
                if(remaining == 0u || actual.ordinal != remaining - 1u ||
                   providers.index_unchecked(remaining - 1u).ptr != actual.native) { return false; }
                --remaining;
            }
            // No imported reader survives the actual Runtime drain. A retained
            // prepared old Source cannot execute or republish in a new epoch.
            while(providers.size() != remaining) { providers.pop_back(); }
            entries_ = {}; count_ = 0u;
            return true;
        }
    };
}
