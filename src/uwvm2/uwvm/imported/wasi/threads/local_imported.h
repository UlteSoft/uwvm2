#pragma once
#include <cstdint>
#include <uwvm2/uwvm/wasm/type/impl.h>

namespace uwvm2::uwvm::imported::wasi::threads
{
    struct spawn_function
    {
        using features = ::uwvm2::uwvm::wasm::type::binfmt_ver1_feature_list_t;
        using value = ::uwvm2::uwvm::wasm::type::feature_list_final_value_type_t<features>;
        using result_tuple = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<features, value::i32>;
        using parameter_tuple = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<features, value::i32>;
        using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<result_tuple, parameter_tuple>;
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"thread-spawn"};
        static void call(local_imported_function_type& call) noexcept
        {
            // Ordinary execution has no genuine Group/ChildLease. Actual
            // threaded dispatch intercepts this concrete factory member and
            // invokes Runtime's real reservation/world/compiler/native launch.
            ::uwvm2::utils::container::get<0>(call.res) = static_cast<::std::int32_t>(-1);
        }
    };
    struct local_imported_module_t
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view module_name{u8"wasi"};
        using local_function_tuple = ::uwvm2::utils::container::tuple<spawn_function>;
    };
    inline constexpr local_imported_module_t local_imported_module{};
    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_module<local_imported_module_t>);
}
