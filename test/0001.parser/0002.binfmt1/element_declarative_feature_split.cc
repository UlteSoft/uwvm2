#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1_feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1_feature = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1_feature, wasm1p1_feature>;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1_feature, wasm1p1_feature>;

    constexpr ::std::uint8_t declarative_funcidx_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                          0x09u, 0x04u, 0x01u, 0x03u, 0x00u, 0x00u};
    constexpr ::std::uint8_t declarative_expr_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                       0x09u, 0x04u, 0x01u, 0x07u, 0x70u, 0x00u};
    constexpr ::std::uint8_t active_explicit_funcidx_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                              0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
                                                              0x09u, 0x08u, 0x01u, 0x02u, 0x00u, 0x41u, 0x00u, 0x0bu, 0x00u, 0x00u};

    template <::std::size_t N>
    [[nodiscard]] int parse_case(::std::uint8_t const (&bytes)[N],
                                 bool disable_reference_types,
                                 bool disable_bulk_memory,
                                 bool expect_rejection,
                                 ::uwvm2::parser::wasm::base::wasm1p1_feature_kind expected_feature) noexcept
    {
        fs_para_t fs_para{};
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        para.disable_reference_types = disable_reference_types;
        para.disable_bulk_memory = disable_bulk_memory;

        auto const* begin{reinterpret_cast<::std::byte const*>(bytes)};
        auto const* end{begin + N};
        ::uwvm2::parser::wasm::base::error_impl err{};
        bool rejected{};
        try
        {
            auto storage{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature>(begin, end, err, fs_para)};
            static_cast<void>(storage);
        }
        catch(::fast_io::error const&)
        {
            rejected = true;
        }

        if(!expect_rejection) { return !rejected && err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok ? 0 : 1; }
        return rejected && err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required &&
                       err.err_selectable.wasm1p1_feature_required.feature ==
                           expected_feature
                   ? 0
                   : 2;
    }

    template <::std::size_t N>
    [[nodiscard]] int parse_active_explicit_funcidx_case(::std::uint8_t const (&bytes)[N], bool canonical_disable, bool legacy_disable) noexcept
    {
        fs_para_t fs_para{};
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        para.disable_reference_types = true;
        para.disable_multiple_tables = canonical_disable;
        para.controllable_allow_multi_table = legacy_disable;

        auto const* begin{reinterpret_cast<::std::byte const*>(bytes)};
        auto const* end{begin + N};
        ::uwvm2::parser::wasm::base::error_impl err{};
        bool rejected{};
        try
        {
            auto storage{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature>(begin, end, err, fs_para)};
            static_cast<void>(storage);
        }
        catch(::fast_io::error const&)
        {
            rejected = true;
        }

        if(!canonical_disable && !legacy_disable) { return !rejected && err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok ? 0 : 1; }
        return rejected && err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm2_feature_required &&
                       err.err_selectable.wasm2_feature_required.feature == ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables
                   ? 0
                   : 2;
    }
}

int main()
{
    using feature = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
    // Flag 3 belongs to bulk-memory, not reference-types.
    if(parse_case(declarative_funcidx_module, true, false, false, feature::reference_types) != 0) { return 1; }
    if(parse_case(declarative_funcidx_module, false, true, true, feature::bulk_memory) != 0) { return 2; }
    // Flag 7 needs both bulk-memory and reference-types.
    if(parse_case(declarative_expr_module, false, false, false, feature::bulk_memory) != 0) { return 3; }
    if(parse_case(declarative_expr_module, false, true, true, feature::bulk_memory) != 0) { return 4; }
    if(parse_case(declarative_expr_module, true, false, true, feature::reference_types) != 0) { return 5; }
    // Canonical and legacy multiple-table controls are both fail-closed.
    if(parse_active_explicit_funcidx_case(active_explicit_funcidx_module, false, false) != 0) { return 6; }
    if(parse_active_explicit_funcidx_case(active_explicit_funcidx_module, true, false) != 0) { return 7; }
    if(parse_active_explicit_funcidx_case(active_explicit_funcidx_module, false, true) != 0) { return 8; }
    return 0;
}
