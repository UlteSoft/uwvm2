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
    using parse_error = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
    using wasm1p1_feature_kind = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
    using wasm2_feature_kind = ::uwvm2::parser::wasm::base::wasm2_feature_kind;

    enum feature_mask : ::std::uint_least8_t
    {
        no_feature = 0u,
        bulk_memory = 1u << 0u,
        reference_types = 1u << 1u,
        multiple_tables = 1u << 2u
    };

    // Each module contains exactly one valid element segment. Active forms also contain one MVP funcref table.
    inline constexpr ::std::uint8_t flag0_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
                                                   0x09u, 0x06u, 0x01u, 0x00u, 0x41u, 0x00u, 0x0bu, 0x00u};
    inline constexpr ::std::uint8_t flag1_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x09u, 0x04u, 0x01u, 0x01u, 0x00u, 0x00u};
    inline constexpr ::std::uint8_t flag2_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
                                                   0x09u, 0x08u, 0x01u, 0x02u, 0x00u, 0x41u, 0x00u, 0x0bu, 0x00u, 0x00u};
    inline constexpr ::std::uint8_t flag3_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x09u, 0x04u, 0x01u, 0x03u, 0x00u, 0x00u};
    inline constexpr ::std::uint8_t flag4_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
                                                   0x09u, 0x06u, 0x01u, 0x04u, 0x41u, 0x00u, 0x0bu, 0x00u};
    inline constexpr ::std::uint8_t flag5_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x09u, 0x04u, 0x01u, 0x05u, 0x70u, 0x00u};
    inline constexpr ::std::uint8_t flag6_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
                                                   0x09u, 0x08u, 0x01u, 0x06u, 0x00u, 0x41u, 0x00u, 0x0bu, 0x70u, 0x00u};
    inline constexpr ::std::uint8_t flag7_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                   0x09u, 0x04u, 0x01u, 0x07u, 0x70u, 0x00u};

    struct element_case
    {
        ::std::uint8_t const* bytes{};
        ::std::size_t size{};
        ::std::uint_least8_t required_features{};
    };

    inline constexpr element_case cases[]{
        {flag0_module, sizeof(flag0_module), no_feature},
        {flag1_module, sizeof(flag1_module), bulk_memory},
        {flag2_module, sizeof(flag2_module), multiple_tables},
        {flag3_module, sizeof(flag3_module), bulk_memory},
        {flag4_module, sizeof(flag4_module), reference_types},
        {flag5_module, sizeof(flag5_module), bulk_memory | reference_types},
        {flag6_module, sizeof(flag6_module), reference_types | multiple_tables},
        {flag7_module, sizeof(flag7_module), bulk_memory | reference_types},
    };

    struct parse_attempt
    {
        ::uwvm2::parser::wasm::base::error_impl error{};
        bool accepted{};
    };

    [[nodiscard]] parse_attempt parse(element_case const& test_case, feature_mask const disabled) noexcept
    {
        fs_para_t fs_para{};
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        para.disable_bulk_memory = disabled == bulk_memory;
        para.disable_reference_types = disabled == reference_types;
        para.disable_multiple_tables = disabled == multiple_tables;

        parse_attempt result{};
        auto const* begin{reinterpret_cast<::std::byte const*>(test_case.bytes)};
        try
        {
            auto storage{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature>(
                begin, begin + test_case.size, result.error, fs_para)};
            static_cast<void>(storage);
        }
        catch(::fast_io::error const&)
        {
            return result;
        }
        result.accepted = result.error.err_code == parse_error::ok;
        return result;
    }

    [[nodiscard]] bool error_matches(parse_attempt const& attempt, feature_mask const disabled) noexcept
    {
        if(disabled == bulk_memory)
        {
            return attempt.error.err_code == parse_error::wasm1p1_feature_required &&
                   attempt.error.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature_kind::bulk_memory;
        }
        if(disabled == reference_types)
        {
            return attempt.error.err_code == parse_error::wasm1p1_feature_required &&
                   attempt.error.err_selectable.wasm1p1_feature_required.feature == wasm1p1_feature_kind::reference_types;
        }
        return attempt.error.err_code == parse_error::wasm2_feature_required &&
               attempt.error.err_selectable.wasm2_feature_required.feature == wasm2_feature_kind::multiple_tables;
    }

    [[noreturn]] void fail(::std::size_t const flag, feature_mask const disabled, char const* const message) noexcept
    {
        ::fast_io::io::perrln("element_declarative_feature_split: flag=", flag, " disabled=", static_cast<unsigned>(disabled), " ",
                              ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}

int main()
{
    constexpr feature_mask independently_disabled[]{bulk_memory, reference_types, multiple_tables};
    for(::std::size_t flag{}; flag != sizeof(cases) / sizeof(*cases); ++flag)
    {
        if(!parse(cases[flag], no_feature).accepted) { fail(flag, no_feature, "default policy rejected a valid element form"); }
        for(auto const disabled: independently_disabled)
        {
            auto const attempt{parse(cases[flag], disabled)};
            auto const required{(cases[flag].required_features & disabled) != 0u};
            if(attempt.accepted == required)
            {
                fail(flag, disabled, required ? "required feature disable was accepted" : "unrelated feature disable was rejected");
            }
            if(required && !error_matches(attempt, disabled)) { fail(flag, disabled, "feature rejection used the wrong diagnostic"); }
        }
    }
}
