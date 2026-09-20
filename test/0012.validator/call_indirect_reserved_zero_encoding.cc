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

    [[nodiscard]] ::uwvm2::validation::error::code_validation_error_impl
        validate(module_storage_t const& module, fs_para_t const& policy)
    {
        auto const body{get_function_body(module)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code_with_runtime_policy(
                module, body.function_index, body.begin, body.end, err, policy);
        }
        catch(::fast_io::error const&)
        {}
        return err;
    }

    [[nodiscard]] ::uwvm2::validation::error::code_validation_error_impl
        validate_explicit_wasm2(module_storage_t const& module, fs_para_t const& policy)
    {
        auto const body{get_function_body(module)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code(
                ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_code_version{},
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
    fs_para_t wasm1p1_policy{};
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm1p1_policy).cli_mode = cli_mode::direct_wasm1p1;
    auto const canonical{parse(canonical_module, wasm1p1_policy)};
    auto const nonminimal{parse(nonminimal_zero_module, wasm1p1_policy)};
    auto const nonzero{parse(nonzero_table_module, wasm1p1_policy)};
    auto const invalid_type_nonzero{parse(invalid_type_nonzero_table_module, wasm1p1_policy)};
    auto const invalid_type_overflowing{parse(invalid_type_overflowing_table_module, wasm1p1_policy)};
    if(validate(canonical, wasm1p1_policy).err_code != error_code::ok) { return 1; }
    if(validate(nonminimal, wasm1p1_policy).err_code != error_code::ok) { return 2; }
    if(validate_wasm1(canonical).err_code != error_code::ok) { return 3; }
    if(!is_call_indirect_error_at_opcode(nonminimal, validate_wasm1(nonminimal), error_code::invalid_table_index)) { return 4; }

    auto mvp_policy{wasm1p1_policy};
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(mvp_policy).cli_mode = cli_mode::direct_mvp;
    if(!is_call_indirect_error_at_opcode(nonminimal, validate(nonminimal, mvp_policy), error_code::invalid_table_index)) { return 5; }
    if(!is_call_indirect_error_at_opcode(
           invalid_type_nonzero, validate(invalid_type_nonzero, mvp_policy), error_code::invalid_table_index))
    { return 14; }

    auto wasm2_policy{wasm1p1_policy};
    auto& wasm2_para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(wasm2_policy)};
    wasm2_para.cli_mode = cli_mode::direct_wasm2;
    if(validate(nonminimal, wasm2_policy).err_code != error_code::ok) { return 6; }
    if(!is_call_indirect_error_at_opcode(invalid_type_overflowing,
                                         validate(invalid_type_overflowing, wasm2_policy),
                                         error_code::invalid_table_index))
    { return 15; }

    fs_para_t unspecified_policy{};
    if(validate_explicit_wasm2(nonminimal, unspecified_policy).err_code != error_code::ok) { return 7; }

    wasm2_para.disable_multiple_tables = true;
    if(validate(nonminimal, wasm2_policy).err_code != error_code::ok) { return 8; }
    auto const disabled_error{validate(nonzero, wasm2_policy)};
    if(disabled_error.err_code != error_code::wasm2_feature_required) { return 9; }
    if(disabled_error.err_selectable.wasm2_feature_required.feature !=
       ::uwvm2::parser::wasm::base::wasm2_feature_kind::multiple_tables)
    { return 10; }
    if(disabled_error.err_selectable.wasm2_feature_required.value != 0x11u) { return 11; }
    if(!is_call_indirect_error_at_opcode(nonzero, disabled_error, error_code::wasm2_feature_required)) { return 17; }
    if(!is_call_indirect_error_at_opcode(
           invalid_type_nonzero, validate(invalid_type_nonzero, wasm2_policy), error_code::illegal_type_index))
    { return 16; }

    unspecified_policy = wasm2_policy;
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(unspecified_policy).cli_mode =
        cli_mode::unspecified;
    if(validate_explicit_wasm2(nonminimal, unspecified_policy).err_code != error_code::ok) { return 12; }
    if(!is_call_indirect_error_at_opcode(
           nonzero, validate_explicit_wasm2(nonzero, unspecified_policy), error_code::wasm2_feature_required))
    { return 13; }
    return 0;
}
