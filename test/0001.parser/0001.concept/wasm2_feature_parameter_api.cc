#include <cstddef>
#include <type_traits>

#ifndef UWVM_MODULE
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2 = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using parameter_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using wasm2_kind = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_feature_kind;

    [[nodiscard]] constexpr bool multi_value_disabled(parameter_t const& parameter) noexcept
    {
        return ::uwvm2::parser::wasm::standard::wasm1::features::
            get_feature_parameter_controllable_allow_multi_result_vector_from_paras(parameter);
    }

    [[nodiscard]] constexpr bool multiple_tables_disabled(parameter_t const& parameter) noexcept
    {
        return ::uwvm2::parser::wasm::standard::wasm1::features::
            get_feature_parameter_controllable_allow_multi_table_from_paras(parameter);
    }

    consteval bool canonical_legacy_matrix() noexcept
    {
        parameter_t parameter{};
        auto& policy{::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(parameter)};

        if(multi_value_disabled(parameter) || multiple_tables_disabled(parameter)) { return false; }

        policy.disable_multi_value = true;
        if(!multi_value_disabled(parameter) ||
           ::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(policy, wasm2_kind::multi_value))
        {
            return false;
        }
        policy.disable_multi_value = false;
        policy.controllable_allow_multi_result_vector = true;
        if(!multi_value_disabled(parameter) ||
           ::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(policy, wasm2_kind::multi_value))
        {
            return false;
        }

        policy.controllable_allow_multi_result_vector = false;
        policy.disable_multiple_tables = true;
        if(!multiple_tables_disabled(parameter) ||
           ::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(policy, wasm2_kind::multiple_tables))
        {
            return false;
        }
        policy.disable_multiple_tables = false;
        policy.controllable_allow_multi_table = true;
        return multiple_tables_disabled(parameter) &&
               !::uwvm2::parser::wasm::standard::wasm2::features::feature_enabled(policy, wasm2_kind::multiple_tables);
    }

    static_assert(canonical_legacy_matrix());
    static_assert(::std::same_as<::uwvm2::parser::wasm::binfmt::ver1::final_code_version_reserve_type_t<wasm1, wasm1p1, wasm2>,
                                 ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_code_version>);
}

int main() {}
