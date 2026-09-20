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
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1>;

    constexpr ::std::uint8_t funcref_table_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                    0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u};
    constexpr ::std::uint8_t externref_table_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
                                                      0x04u, 0x04u, 0x01u, 0x6fu, 0x00u, 0x01u};

    enum class expected_result : unsigned
    {
        accepted,
        reference_types_required,
        table_instructions_required
    };

    template <::std::size_t N>
    [[nodiscard]] int parse_case(::std::uint8_t const (&bytes)[N],
                                 bool disable_reference_types,
                                 bool disable_table_instructions,
                                 expected_result expected) noexcept
    {
        fs_para_t fs_para{};
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        para.disable_reference_types = disable_reference_types;
        para.disable_table_instructions = disable_table_instructions;

        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        bool rejected{};
        try
        {
            static_cast<void>(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1>(
                begin, begin + N, err, fs_para));
        }
        catch(::fast_io::error const&)
        {
            rejected = true;
        }

        switch(expected)
        {
            case expected_result::accepted:
                return !rejected && err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok ? 0 : 1;
            case expected_result::reference_types_required:
                return rejected && err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_feature_required &&
                               err.err_selectable.wasm1p1_feature_required.feature ==
                                   ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types
                           ? 0
                           : 2;
            case expected_result::table_instructions_required:
                return rejected && err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm2_feature_required &&
                               err.err_selectable.wasm2_feature_required.feature ==
                                   ::uwvm2::parser::wasm::base::wasm2_feature_kind::table_instructions
                           ? 0
                           : 3;
        }
        return 4;
    }
}

int main()
{
    // The MVP funcref table type remains legal when both post-MVP groups are disabled.
    if(parse_case(funcref_table_module, true, true, expected_result::accepted) != 0) { return 1; }
    if(parse_case(externref_table_module, false, false, expected_result::accepted) != 0) { return 2; }
    if(parse_case(externref_table_module, true, false, expected_result::reference_types_required) != 0) { return 3; }
    if(parse_case(externref_table_module, false, true, expected_result::table_instructions_required) != 0) { return 4; }
    // When both groups are absent, the table-specific Wasm 2.0 gate is reported first.
    if(parse_case(externref_table_module, true, true, expected_result::table_instructions_required) != 0) { return 5; }
    return 0;
}
