#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm2/impl.h>
# include <uwvm2/validation/standard/wasm2/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using wasm2 = ::uwvm2::parser::wasm::standard::wasm2::features::wasm2;
    using feature_parameter_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using module_storage_t =
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;

    // ff 7f decodes numerically to -1 but is not the single-byte i32 value-type alternative required by blocktype.
    inline constexpr ::std::uint8_t overlong_i32_blocktype_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x0au, 0x0bu, 0x01u, 0x09u, 0x00u, 0x02u, 0xffu, 0x7fu, 0x41u, 0x00u, 0x0bu, 0x1au, 0x0bu};

    [[nodiscard]] int run_test() noexcept
    {
        feature_parameter_t features{};
        auto const* const begin{reinterpret_cast<::std::byte const*>(overlong_i32_blocktype_module)};
        auto const* const end{begin + sizeof(overlong_i32_blocktype_module)};

        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        module_storage_t module{};
        try
        {
            module = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, end, parse_err, features);
        }
        catch(::fast_io::error const&)
        {
            return 1;
        }
        if(parse_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return 2; }

        auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        if(codes.codes.size() != 1uz) { return 3; }
        auto const& code{codes.codes.index_unchecked(0uz)};

        ::uwvm2::validation::error::code_validation_error_impl err{};
        bool rejected{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code(
                ::uwvm2::validation::standard::wasm2::wasm2_code_version{},
                module,
                0uz,
                reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                reinterpret_cast<::std::byte const*>(code.body.code_end),
                err,
                features);
        }
        catch(::fast_io::error const&)
        {
            rejected = true;
        }

        return rejected && err.err_code == ::uwvm2::validation::error::code_validation_error_code::illegal_block_type ? 0 : 4;
    }
}

int main()
{
    return run_test();
}
