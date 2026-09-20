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
    using feature_parameter_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;

    // (func (param i64 i32) (result i64)
    //   local.get 0
    //   local.get 1
    //   if (type (func (param i64) (result i64)))
    //     drop
    //     i64.const -1
    //   end)
    //
    // The implicit false arm is the identity function over the i64 block parameter.
    inline constexpr ::std::uint8_t matching_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x0cu, 0x02u, 0x60u, 0x01u, 0x7eu,
                                                      0x01u, 0x7eu, 0x60u, 0x02u, 0x7eu, 0x7fu, 0x01u, 0x7eu, 0x03u, 0x02u, 0x01u, 0x01u, 0x0au, 0x0eu,
                                                      0x01u, 0x0cu, 0x00u, 0x20u, 0x00u, 0x20u, 0x01u, 0x04u, 0x00u, 0x1au, 0x42u, 0x7fu, 0x0bu, 0x0bu};

    // Same arity is not enough: the implicit false arm leaves i64, while the block result is i32.
    inline constexpr ::std::uint8_t mismatching_module[]{0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x0cu, 0x02u, 0x60u, 0x01u, 0x7eu,
                                                         0x01u, 0x7fu, 0x60u, 0x02u, 0x7eu, 0x7fu, 0x01u, 0x7fu, 0x03u, 0x02u, 0x01u, 0x01u, 0x0au, 0x0eu,
                                                         0x01u, 0x0cu, 0x00u, 0x20u, 0x00u, 0x20u, 0x01u, 0x04u, 0x00u, 0x1au, 0x41u, 0x07u, 0x0bu, 0x0bu};

    struct validation_result
    {
        bool rejected{};
        error_code code{error_code::ok};
    };

    template <::std::size_t N, typename Validate>
    [[nodiscard]] validation_result validate_module(::std::uint8_t const (&bytes)[N], Validate&& validate) noexcept
    {
        feature_parameter_t features{};
        auto& wasm1p1_parameter{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(features)};
        wasm1p1_parameter.disable_multi_value = false;
        wasm1p1_parameter.controllable_allow_multi_result_vector = false;

        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        ::uwvm2::parser::wasm::base::error_impl parse_error{};
        module_storage_t module{};
        try
        {
            module = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(begin, begin + N, parse_error, features);
        }
        catch(::fast_io::error const&)
        {
            return {true, error_code::missing_end};
        }
        if(parse_error.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return {true, error_code::missing_end}; }

        auto const& codes{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        if(codes.codes.size() != 1uz) { return {true, error_code::missing_end}; }
        auto const& code{codes.codes.index_unchecked(0uz)};

        ::uwvm2::validation::error::code_validation_error_impl error{};
        try
        {
            validate(module,
                     reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                     reinterpret_cast<::std::byte const*>(code.body.code_end),
                     error,
                     features);
        }
        catch(::fast_io::error const&)
        {
            return {true, error.err_code};
        }
        return {false, error.err_code};
    }

    [[nodiscard]] int run_test() noexcept
    {
        auto validate_wasm1p1 = [](module_storage_t const& module,
                                   ::std::byte const* code_begin,
                                   ::std::byte const* code_end,
                                   ::uwvm2::validation::error::code_validation_error_impl& error,
                                   feature_parameter_t const& features)
        {
            ::uwvm2::validation::standard::wasm1p1::validate_code(::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                                                                  module,
                                                                  0uz,
                                                                  code_begin,
                                                                  code_end,
                                                                  error,
                                                                  features);
        };
        auto validate_wasm2 = [](module_storage_t const& module,
                                 ::std::byte const* code_begin,
                                 ::std::byte const* code_end,
                                 ::uwvm2::validation::error::code_validation_error_impl& error,
                                 feature_parameter_t const& features)
        {
            ::uwvm2::validation::standard::wasm2::validate_code(::uwvm2::validation::standard::wasm2::wasm2_code_version{},
                                                                module,
                                                                0uz,
                                                                code_begin,
                                                                code_end,
                                                                error,
                                                                features);
        };

        auto const wasm1p1_matching{validate_module(matching_module, validate_wasm1p1)};
        if(wasm1p1_matching.rejected || wasm1p1_matching.code != error_code::ok) { return 1; }
        auto const wasm2_matching{validate_module(matching_module, validate_wasm2)};
        if(wasm2_matching.rejected || wasm2_matching.code != error_code::ok) { return 2; }

        auto const wasm1p1_mismatching{validate_module(mismatching_module, validate_wasm1p1)};
        if(!wasm1p1_mismatching.rejected || wasm1p1_mismatching.code != error_code::if_missing_else) { return 3; }
        auto const wasm2_mismatching{validate_module(mismatching_module, validate_wasm2)};
        if(!wasm2_mismatching.rejected || wasm2_mismatching.code != error_code::if_missing_else) { return 4; }
        return 0;
    }
}  // namespace

int main() { return run_test(); }
