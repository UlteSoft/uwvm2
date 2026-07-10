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
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x03u, 0x02u, 0x00u, 0x00u, 0x0au, 0x14u, 0x02u, 0x02u, 0x00u, 0x0bu, 0x0fu, 0x00u,
        0x00u, 0x55u, 0x41u, 0x01u, 0x00u, 0x70u, 0x00u, 0x5cu, 0x1bu, 0x41u, 0x00u, 0x0du, 0x00u,
        0x0bu};

    inline constexpr void configure_features(fs_para_t& fs_para) noexcept
    {
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        para.disable_multi_value = false;
        para.disable_reference_types = false;
        para.disable_bulk_memory = false;
        para.controllable_allow_multi_result_vector = false;
        para.controllable_allow_multi_table = false;
    }

    [[nodiscard]] int run_test() noexcept
    {
        auto const* begin{reinterpret_cast<::std::byte const*>(module_bytes)};
        auto const* end{begin + sizeof(module_bytes)};

        fs_para_t fs_para{};
        configure_features(fs_para);

        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        module_storage_t module_storage{};
        try
        {
            module_storage =
                ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature>(begin, end, parse_err, fs_para);
        }
        catch(::fast_io::error const&)
        {
            return 1;
        }

        if(parse_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return 2; }

        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1_feature, wasm1p1_feature>>(module_storage.sections)};
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1_feature, wasm1p1_feature>>(module_storage.sections)};

        if(codesec.codes.size() != 2uz) { return 3; }

        for(::std::size_t local_idx{}; local_idx != codesec.codes.size(); ++local_idx)
        {
            auto const& code{codesec.codes.index_unchecked(local_idx)};
            auto const* const code_begin{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const* const code_end{reinterpret_cast<::std::byte const*>(code.body.code_end)};

            ::uwvm2::validation::error::code_validation_error_impl validate_err{};
            bool threw{};
            try
            {
                ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                       module_storage,
                                                                       import_func_count + local_idx,
                                                                       code_begin,
                                                                       code_end,
                                                                       validate_err,
                                                                       fs_para);
            }
            catch(::fast_io::error const&)
            {
                threw = true;
            }

            if(local_idx == 0uz)
            {
                if(threw || validate_err.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) { return 10; }
            }
            else
            {
                if(!threw && validate_err.err_code == ::uwvm2::validation::error::code_validation_error_code::ok) { return 20; }
                if(validate_err.err_code != ::uwvm2::validation::error::code_validation_error_code::end_result_mismatch) { return 21; }
            }
        }

        return 0;
    }
}  // namespace

int main()
{
    return run_test();
}
