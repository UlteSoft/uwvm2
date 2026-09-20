#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/validation/standard/wasm1p1/impl.h>
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

    constexpr ::std::uint8_t module_bytes[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x05u, 0x01u, 0x60u, 0x00u, 0x01u, 0x7fu,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x0au, 0x0du, 0x01u, 0x0bu, 0x00u, 0x41u, 0x07u, 0x41u, 0x09u, 0x41u, 0x01u, 0x1cu, 0x01u, 0x7fu, 0x0bu};

    [[nodiscard]] int run_test() noexcept
    {
        fs_para_t enabled{};
        auto& enabled_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(enabled)};
        enabled_para.disable_reference_types = false;

        auto const* begin{reinterpret_cast<::std::byte const*>(module_bytes)};
        auto const* end{begin + sizeof(module_bytes)};
        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        module_storage_t module_storage{};
        try
        {
            module_storage =
                ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature>(begin, end, parse_err, enabled);
        }
        catch(::fast_io::error const&)
        {
            return 1;
        }

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1_feature, wasm1p1_feature>>(module_storage.sections)};
        if(codesec.codes.size() != 1uz) { return 2; }
        auto const& code{codesec.codes.index_unchecked(0uz)};
        auto const* code_begin{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
        auto const* code_end{reinterpret_cast<::std::byte const*>(code.body.code_end)};

        ::uwvm2::validation::error::code_validation_error_impl enabled_err{};
        try
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                   module_storage,
                                                                   0uz,
                                                                   code_begin,
                                                                   code_end,
                                                                   enabled_err,
                                                                   enabled);
        }
        catch(::fast_io::error const&)
        {
            return 3;
        }
        if(enabled_err.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) { return 4; }

        auto disabled{enabled};
        ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(disabled).disable_reference_types = true;
        ::uwvm2::validation::error::code_validation_error_impl disabled_err{};
        bool rejected{};
        try
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                   module_storage,
                                                                   0uz,
                                                                   code_begin,
                                                                   code_end,
                                                                   disabled_err,
                                                                   disabled);
        }
        catch(::fast_io::error const&)
        {
            rejected = true;
        }

        return rejected && disabled_err.err_code == ::uwvm2::validation::error::code_validation_error_code::wasm1p1_feature_required &&
                       disabled_err.err_selectable.wasm1p1_feature_required.feature ==
                           ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::reference_types
                   ? 0
                   : 5;
    }
}

int main()
{
    return run_test();
}
