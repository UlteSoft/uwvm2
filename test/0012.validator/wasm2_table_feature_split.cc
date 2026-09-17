#include <concepts>
#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
# include <uwvm2/validation/concepts/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2 = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using feature_kind = ::uwvm2::parser::wasm::base::wasm2_feature_kind;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using module_storage_t =
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;

    // (module (table 1 funcref) (func i32.const 0 table.get 0 drop))
    inline constexpr ::std::uint8_t table_get_zero_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x01u,
        0x0au, 0x09u, 0x01u, 0x07u, 0x00u, 0x41u, 0x00u, 0x25u, 0x00u, 0x1au, 0x0bu};

    // (module (table 1 funcref) (table 1 funcref) (func i32.const 0 table.get 1 drop))
    inline constexpr ::std::uint8_t table_get_one_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x04u, 0x07u, 0x02u, 0x70u, 0x00u, 0x01u,
        0x70u, 0x00u, 0x01u, 0x0au, 0x09u, 0x01u, 0x07u, 0x00u, 0x41u, 0x00u, 0x25u, 0x01u,
        0x1au, 0x0bu};

    [[noreturn]] inline void fail(char const* message)
    {
        ::fast_io::io::perrln("wasm2_table_feature_split: ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }

    inline void expect(bool condition, char const* message)
    {
        if(!condition) [[unlikely]] { fail(message); }
    }

    template <::std::size_t N>
    [[nodiscard]] module_storage_t parse(::std::uint8_t const (&bytes)[N], fs_para_t const& fs_para)
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        try
        {
            auto module{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, begin + N, err, fs_para)};
            expect(err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok, "parser returned a non-ok error");
            return module;
        }
        catch(::fast_io::error const&)
        {
            fail("unexpected parser rejection");
        }
    }

    template <::std::size_t N>
    inline void expect_parse_rejection(::std::uint8_t const (&bytes)[N], fs_para_t const& fs_para)
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        try
        {
            static_cast<void>(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, begin + N, err, fs_para));
        }
        catch(::fast_io::error const&)
        {
            expect(err.err_code == ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1_not_allow_multi_table,
                   "multiple-tables parser rejection used the wrong error");
            return;
        }
        fail("multiple-tables-disabled parser accepted two tables");
    }

    inline void validate(module_storage_t const& module, fs_para_t const& fs_para, bool should_succeed, feature_kind expected_feature)
    {
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        expect(codesec.codes.size() == 1uz, "test module must contain one local function");

        auto const& code{codesec.codes.index_unchecked(0uz)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
#if defined(UWVM_TEST_LEGACY_TABLE_POLICY)
            ::uwvm2::validation::standard::wasm1p1::validate_code(
                ::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
#else
            ::uwvm2::validation::concepts::dispatch_validate_code(
#endif
                module,
                importsec.importdesc.index_unchecked(0u).size(),
                reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                reinterpret_cast<::std::byte const*>(code.body.code_end),
                err,
                fs_para);
        }
        catch(::fast_io::error const&)
        {
            expect(!should_succeed, "validator rejected an enabled feature combination");
            expect(err.err_code == ::uwvm2::validation::error::code_validation_error_code::wasm2_feature_required,
                   "validator rejection used the wrong error code");
            expect(err.err_selectable.wasm2_feature_required.feature == expected_feature,
                   "validator rejection named the wrong feature group");
            return;
        }
        expect(should_succeed, "validator accepted a disabled feature");
    }
}

int main()
{
    using wasm1_code_version = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version;
    using wasm1p1_code_version = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_code_version;
    using wasm2_code_version = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_code_version;

    static_assert(::std::same_as<::uwvm2::validation::concepts::code_version_type_t<wasm1>, wasm1_code_version>);
    static_assert(::uwvm2::validation::concepts::can_validate_code<wasm1_code_version, wasm1>);
    static_assert(::std::same_as<::uwvm2::validation::concepts::code_version_type_t<wasm1, wasm1p1>, wasm1p1_code_version>);
    static_assert(::uwvm2::validation::concepts::can_validate_code_with_parameter<wasm1p1_code_version, wasm1, wasm1p1>);
    static_assert(::std::same_as<::uwvm2::validation::concepts::code_version_type_t<wasm1, wasm1p1, wasm2>, wasm2_code_version>);
    static_assert(::uwvm2::validation::concepts::can_validate_code_with_parameter<wasm2_code_version, wasm1, wasm1p1, wasm2>);

    fs_para_t defaults{};
    auto const table_zero{parse(table_get_zero_module, defaults)};
    validate(table_zero, defaults, true, feature_kind::table_instructions);

    auto reference_types_off{defaults};
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(reference_types_off).disable_reference_types = true;
    validate(table_zero, reference_types_off, true, feature_kind::table_instructions);

    auto table_instructions_off{defaults};
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(table_instructions_off).disable_table_instructions = true;
    validate(table_zero, table_instructions_off, false, feature_kind::table_instructions);

    auto const table_one{parse(table_get_one_module, defaults)};
    auto multiple_tables_off{defaults};
    ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(multiple_tables_off).disable_multiple_tables = true;
    expect_parse_rejection(table_get_one_module, multiple_tables_off);
    validate(table_one, multiple_tables_off, false, feature_kind::multiple_tables);
}
