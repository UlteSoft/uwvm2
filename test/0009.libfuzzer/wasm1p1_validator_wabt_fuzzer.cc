/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
 * @version     2.0.0
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// libwabt uses assert() in headers and may abort on malformed inputs when NDEBUG is not set.
// For differential fuzzing, keep those assertions disabled in this target.
#ifndef NDEBUG
# define NDEBUG 1
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <utility>

#ifndef UWVM_MODULE
# include <fast_io.h>

# include <uwvm2/validation/standard/wasm1p1/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/binfmt.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-function"
#  pragma clang diagnostic ignored "-Wunused-parameter"
# endif

# include "wabt/binary-reader-ir.h"
# include "wabt/validator.h"

# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
#else
# error "Module testing is not currently supported"
#endif

namespace
{
    using wasm1_feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1_feature = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1_feature, wasm1p1_feature>;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1_feature, wasm1p1_feature>;

    extern "C" char const* __asan_default_options() { return "detect_leaks=0"; }

    struct validation_result_t
    {
        bool parsed{};
        bool validated{};
        ::uwvm2::parser::wasm::base::wasm_parse_error_code parse_err_code{};
        ::uwvm2::validation::error::code_validation_error_code validate_err_code{};
        ::std::size_t validate_function_index{};
    };

    struct feature_config_t
    {
        ::std::uint8_t mask{};
        bool multi_value{};
        bool reference_types{};
        bool bulk_memory{};
        bool sign_extension{};
        bool nontrapping_float_to_int{};
        bool simd{};
    };

    [[nodiscard]] static bool uwvm_fuzz_debug() noexcept
    {
        auto const* v = ::std::getenv("UWVM_FUZZ_DEBUG");
        return v != nullptr && *v != '\0' && !(v[0] == '0' && v[1] == '\0');
    }

    [[nodiscard]] static constexpr bool is_wasm_binfmt_ver1(::std::uint8_t const* data, ::std::size_t size) noexcept
    {
        return size >= 8uz && data[0] == 0x00u && data[1] == 0x61u && data[2] == 0x73u && data[3] == 0x6du && data[4] == 0x01u &&
               data[5] == 0x00u && data[6] == 0x00u && data[7] == 0x00u;
    }

    [[nodiscard]] inline constexpr feature_config_t feature_config_from_mask(::std::uint8_t mask) noexcept
    {
        return feature_config_t{.mask = static_cast<::std::uint8_t>(mask & 0x3fu),
                                .multi_value = (mask & 0x01u) != 0u,
                                .reference_types = (mask & 0x02u) != 0u,
                                .bulk_memory = (mask & 0x04u) != 0u,
                                .sign_extension = (mask & 0x08u) != 0u,
                                .nontrapping_float_to_int = (mask & 0x10u) != 0u,
                                .simd = (mask & 0x20u) != 0u};
    }

    inline constexpr void configure_uwvm_wasm1p1_features(fs_para_t& fs_para, feature_config_t const config) noexcept
    {
        auto& para{::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para)};
        para.disable_multi_value = !config.multi_value;
        para.disable_reference_types = !config.reference_types;
        para.disable_bulk_memory = !config.bulk_memory;
        para.disable_sign_extension = !config.sign_extension;
        para.disable_nontrapping_float_to_int = !config.nontrapping_float_to_int;
        para.disable_simd = !config.simd;

        para.controllable_allow_multi_result_vector = para.disable_multi_value;
        para.controllable_allow_multi_table = para.disable_reference_types;
    }

    [[nodiscard]] static validation_result_t validate_with_uwvm(::std::uint8_t const* data, ::std::size_t size, feature_config_t const config)
    {
        validation_result_t result{};

        auto const* begin = reinterpret_cast<::std::byte const*>(data);
        auto const* end = begin + size;

        fs_para_t fs_para{};
        configure_uwvm_wasm1p1_features(fs_para, config);

        ::uwvm2::parser::wasm::base::error_impl parse_err{};
        module_storage_t module_storage{};
        try
        {
            module_storage =
                ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1_feature, wasm1p1_feature>(begin, end, parse_err, fs_para);
        }
        catch(::fast_io::error const&)
        {
            result.parse_err_code = parse_err.err_code;
            return result;
        }

        result.parse_err_code = parse_err.err_code;
        if(parse_err.err_code != ::uwvm2::parser::wasm::base::wasm_parse_error_code::ok) { return result; }

        result.parsed = true;

        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1_feature, wasm1p1_feature>>(module_storage.sections)};
        auto const import_func_count{importsec.importdesc.index_unchecked(0u).size()};

        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1_feature, wasm1p1_feature>>(module_storage.sections)};

        for(::std::size_t local_idx{}; local_idx < codesec.codes.size(); ++local_idx)
        {
            auto const& code{codesec.codes.index_unchecked(local_idx)};
            auto const* const code_begin_ptr{reinterpret_cast<::std::byte const*>(code.body.expr_begin)};
            auto const* const code_end_ptr{reinterpret_cast<::std::byte const*>(code.body.code_end)};

            ::uwvm2::validation::error::code_validation_error_impl v_err{};
            try
            {
                ::uwvm2::validation::standard::wasm1p1::validate_code(
                    ::uwvm2::validation::standard::wasm1p1::wasm1p1_code_version{},
                    module_storage,
                    import_func_count + local_idx,
                    code_begin_ptr,
                    code_end_ptr,
                    v_err,
                    fs_para);
            }
            catch(::fast_io::error const&)
            {
                result.validate_err_code = v_err.err_code;
                result.validate_function_index = import_func_count + local_idx;
                return result;
            }
            if(v_err.err_code != ::uwvm2::validation::error::code_validation_error_code::ok)
            {
                result.validate_err_code = v_err.err_code;
                result.validate_function_index = import_func_count + local_idx;
                return result;
            }
        }

        result.validated = true;
        result.validate_err_code = ::uwvm2::validation::error::code_validation_error_code::ok;
        return result;
    }

    [[nodiscard]] inline constexpr bool wabt_can_model_feature_config(feature_config_t const config) noexcept
    {
        // WABT models reference-types as depending on bulk-memory. UWVM keeps these CLI switches independent.
        return !config.reference_types || config.bulk_memory;
    }

    [[nodiscard]] static wabt::Features wasm1p1_wabt_features(feature_config_t const config)
    {
        using namespace ::wabt;

        Features features;
        features.set_multi_value_enabled(config.multi_value);
        features.set_bulk_memory_enabled(config.bulk_memory);
        features.set_reference_types_enabled(config.reference_types);
        features.set_sign_extension_enabled(config.sign_extension);
        features.set_sat_float_to_int_enabled(config.nontrapping_float_to_int);
        features.set_simd_enabled(config.simd);
        features.disable_exceptions();
        features.disable_threads();
        features.disable_function_references();
        features.disable_tail_call();
        features.disable_code_metadata();
        features.disable_annotations();
        features.disable_gc();
        features.disable_memory64();
        features.disable_multi_memory();
        features.disable_extended_const();
        features.disable_relaxed_simd();
        features.disable_custom_page_sizes();
        return features;
    }

    [[nodiscard]] static validation_result_t validate_with_wabt(::std::uint8_t const* data,
                                                                ::std::size_t size,
                                                                feature_config_t const config,
                                                                ::wabt::Errors* out_errors)
    {
        using namespace ::wabt;

        validation_result_t result{};
        Errors errors{};
        Module module;
        auto validate_features{wasm1p1_wabt_features(config)};

        bool const kStopOnFirstError = true;
        bool const kReadDebugNames = false;
        bool const kFailOnCustomSectionError = false;

        ReadBinaryOptions read_options(validate_features, nullptr, kReadDebugNames, kStopOnFirstError, kFailOnCustomSectionError);
        auto read_result{ReadBinaryIr("<buffer>", data, size, read_options, &errors, &module)};
        if(Failed(read_result))
        {
            if(out_errors != nullptr) { *out_errors = ::std::move(errors); }
            return result;
        }

        result.parsed = true;

        ValidateOptions validate_options(validate_features);
        auto validate_result{ValidateModule(&module, &errors, validate_options)};
        if(out_errors != nullptr) { *out_errors = ::std::move(errors); }
        result.validated = Succeeded(validate_result);
        return result;
    }

    [[nodiscard]] static constexpr bool pick_feature_mask_and_wasm(::std::uint8_t const* data,
                                                                   ::std::size_t size,
                                                                   ::std::uint8_t& feature_mask,
                                                                   ::std::uint8_t const*& wasm_data,
                                                                   ::std::size_t& wasm_size) noexcept
    {
        // Preferred corpus shape: [feature-mask][wasm-binary...].
        if(size > 8uz && is_wasm_binfmt_ver1(data + 1uz, size - 1uz))
        {
            feature_mask = data[0];
            wasm_data = data + 1uz;
            wasm_size = size - 1uz;
            return true;
        }

        // Compatibility with plain .wasm corpora: derive the mask from the tail byte without moving the binary.
        if(is_wasm_binfmt_ver1(data, size))
        {
            feature_mask = data[size - 1uz];
            wasm_data = data;
            wasm_size = size;
            return true;
        }

        return false;
    }
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    if(data == nullptr) { return 0; }

    ::std::uint8_t feature_mask{};
    ::std::uint8_t const* wasm_data{};
    ::std::size_t wasm_size{};
    if(!pick_feature_mask_and_wasm(data, size, feature_mask, wasm_data, wasm_size)) { return 0; }

    auto const feature_config{feature_config_from_mask(feature_mask)};

    auto const uwvm_result{validate_with_uwvm(wasm_data, wasm_size, feature_config)};
    if(!uwvm_result.parsed) { return 0; }

    if(!wabt_can_model_feature_config(feature_config)) { return 0; }

    auto const wabt_result{validate_with_wabt(wasm_data, wasm_size, feature_config, nullptr)};
    if(!wabt_result.parsed) { return 0; }

    if(uwvm_result.validated != wabt_result.validated)
    {
        if(uwvm_fuzz_debug())
        {
            ::wabt::Errors wabt_errors{};
            (void)validate_with_wabt(wasm_data, wasm_size, feature_config, &wabt_errors);

            ::std::fprintf(stderr,
                           "uwvm_validated=%d wabt_validated=%d size=%zu feature_mask=0x%02x multi_value=%d reference_types=%d bulk_memory=%d "
                           "sign_extension=%d nontrapping_float_to_int=%d simd=%d uwvm_parse_err=%u uwvm_validate_err=%u uwvm_validate_func=%zu "
                           "wabt_errors=%zu\n",
                           static_cast<int>(uwvm_result.validated),
                           static_cast<int>(wabt_result.validated),
                           wasm_size,
                           static_cast<unsigned>(feature_config.mask),
                           static_cast<int>(feature_config.multi_value),
                           static_cast<int>(feature_config.reference_types),
                           static_cast<int>(feature_config.bulk_memory),
                           static_cast<int>(feature_config.sign_extension),
                           static_cast<int>(feature_config.nontrapping_float_to_int),
                           static_cast<int>(feature_config.simd),
                           static_cast<unsigned>(uwvm_result.parse_err_code),
                           static_cast<unsigned>(uwvm_result.validate_err_code),
                           uwvm_result.validate_function_index,
                           wabt_errors.size());
            if(!wabt_errors.empty()) { ::std::fprintf(stderr, "wabt_error0: %s\n", wabt_errors.front().message.c_str()); }
            ::std::fflush(stderr);
            return 0;
        }
        __builtin_trap();
    }

    return 0;
}
