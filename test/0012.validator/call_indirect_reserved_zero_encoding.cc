#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
# include <uwvm2/validation/standard/wasm1p1/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2 = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using module_storage_t =
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;
    using validation_error = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    using cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode;

    inline constexpr ::std::uint8_t canonical_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u, 0x0au, 0x09u, 0x01u, 0x07u,
        0x00u, 0x41u, 0x00u, 0x11u, 0x00u, 0x00u, 0x0bu};

    inline constexpr ::std::uint8_t nonminimal_zero_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u, 0x0au, 0x0au, 0x01u, 0x08u,
        0x00u, 0x41u, 0x00u, 0x11u, 0x00u, 0x80u, 0x00u, 0x0bu};

    inline constexpr ::std::uint8_t nonzero_table_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u, 0x0au, 0x09u, 0x01u, 0x07u,
        0x00u, 0x41u, 0x00u, 0x11u, 0x00u, 0x01u, 0x0bu};

    inline constexpr ::std::uint8_t invalid_type_nonzero_table_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u, 0x0au, 0x09u, 0x01u, 0x07u,
        0x00u, 0x41u, 0x00u, 0x11u, 0x01u, 0x01u, 0x0bu};

    inline constexpr ::std::uint8_t invalid_type_overflowing_table_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u, 0x0au, 0x0du, 0x01u, 0x0bu,
        0x00u, 0x41u, 0x00u, 0x11u, 0x01u, 0xffu, 0xffu, 0xffu, 0xffu, 0x1fu, 0x0bu};

    template <::std::size_t N>
    [[nodiscard]] module_storage_t parse(::std::uint8_t const (&bytes)[N], fs_para_t const& policy)
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        return ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
            begin, begin + N, err, policy);
    }

    struct function_body
    {
        ::std::size_t function_index{};
        ::std::byte const* begin{};
        ::std::byte const* end{};
    };

    [[nodiscard]] function_body get_function_body(module_storage_t const& module)
    {
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        if(codesec.codes.size() != 1uz) { ::fast_io::fast_terminate(); }
        auto const& code{codesec.codes.index_unchecked(0uz)};
        return {importsec.importdesc.index_unchecked(0u).size(),
                reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                reinterpret_cast<::std::byte const*>(code.body.code_end)};
    }

    [[nodiscard]] validation_error validate_wasm1p1(module_storage_t const& module, fs_para_t const& policy)
    {
        auto const body{get_function_body(module)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(
                ::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                module,
                body.function_index,
                body.begin,
                body.end,
                err,
                policy);
        }
        catch(::fast_io::error const&)
        {}
        return err;
    }

    [[nodiscard]] validation_error validate_wasm1(module_storage_t const& module)
    {
        auto const body{get_function_body(module)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm1::validate_code(
                ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{},
                module,
                body.function_index,
                body.begin,
                body.end,
                err);
        }
        catch(::fast_io::error const&)
        {}
        return err;
    }

    [[nodiscard]] ::uwvm2::validation::error::code_validation_error_impl
        validate_wasm2(module_storage_t const& module, fs_para_t const& policy)
    {
        auto const body{get_function_body(module)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code(
                ::uwvm2::validation::standard::wasm2::wasm2_code_version{}, module, body.function_index, body.begin, body.end, err, policy);
        }
        catch(::fast_io::error const&)
        {}
        return err;
    }

    [[nodiscard]] bool is_call_indirect_error_at_opcode(module_storage_t const& module,
                                                         validation_error const& err,
                                                         error_code const expected) noexcept
    {
        auto const body{get_function_body(module)};
        if(static_cast<::std::size_t>(body.end - body.begin) <= 2uz) { return false; }
        // Every fixture starts with `i32.const 0`; pin the diagnostic to the following call_indirect opcode itself.
        auto const* const opcode{body.begin + 2uz};
        return *opcode == ::std::byte{0x11u} && err.err_code == expected && err.err_curr == opcode;
    }
}

int main()
{
    fs_para_t all_enabled{};
    auto const canonical{parse(canonical_module, all_enabled)};
    auto const nonminimal{parse(nonminimal_zero_module, all_enabled)};
    auto const nonzero{parse(nonzero_table_module, all_enabled)};
    auto const invalid_type_nonzero{parse(invalid_type_nonzero_table_module, all_enabled)};
    auto const invalid_type_overflowing{parse(invalid_type_overflowing_table_module, all_enabled)};

    if(validate_wasm1p1(canonical, all_enabled).err_code != error_code::ok) { return 1; }
    if(validate_wasm1p1(nonminimal, all_enabled).err_code != error_code::ok) { return 2; }
    if(validate_wasm1(canonical).err_code != error_code::ok) { return 3; }
    if(!is_call_indirect_error_at_opcode(nonminimal, validate_wasm1(nonminimal), error_code::invalid_table_index)) { return 4; }
    if(validate_wasm2(nonminimal, all_enabled).err_code != error_code::ok) { return 5; }

    auto explicit_mvp{all_enabled};
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(explicit_mvp).cli_mode = cli_mode::direct_wasmmvp;
    if(!is_call_indirect_error_at_opcode(nonminimal, validate_wasm1p1(nonminimal, explicit_mvp), error_code::invalid_table_index))
    { return 6; }
    if(!is_call_indirect_error_at_opcode(
           invalid_type_nonzero, validate_wasm1p1(invalid_type_nonzero, explicit_mvp), error_code::invalid_table_index))
    { return 11; }

    auto multiple_tables_disabled{all_enabled};
    auto& para{::uwvm2::parser::wasm::standard::wasm2::features::get_wasm2_parameter(multiple_tables_disabled)};
    para.disable_multiple_tables = true;
    if(validate_wasm2(nonminimal, multiple_tables_disabled).err_code != error_code::ok) { return 7; }
    if(!is_call_indirect_error_at_opcode(invalid_type_overflowing,
                                         validate_wasm2(invalid_type_overflowing, multiple_tables_disabled),
                                         error_code::invalid_table_index))
    { return 12; }
    auto const disabled_error{validate_wasm2(nonzero, multiple_tables_disabled)};
    if(disabled_error.err_code != error_code::wasm2_feature_required) { return 8; }
    if(disabled_error.err_selectable.wasm2_feature_required.feature !=
       ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables)
    { return 9; }
    if(disabled_error.err_selectable.wasm2_feature_required.value != 0x11u) { return 10; }
    if(!is_call_indirect_error_at_opcode(nonzero, disabled_error, error_code::wasm2_feature_required)) { return 14; }
    if(!is_call_indirect_error_at_opcode(invalid_type_nonzero,
                                         validate_wasm2(invalid_type_nonzero, multiple_tables_disabled),
                                         error_code::illegal_type_index))
    { return 13; }
    return 0;
}
