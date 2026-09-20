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
    using cli_mode = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_feature_cli_mode;
    using parse_error_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code;
    using validation_error_code = ::uwvm2::validation::error::code_validation_error_code;
    using fs_para_t = ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1, wasm2>;
    using module_storage_t =
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<wasm1, wasm1p1, wasm2>;

    // These encodings are intentionally outside WebAssembly Release 2.0. Binary-format version 1 does not imply that
    // post-2.0 proposals are enabled: each proposal needs an explicit parser, validator, loader, and backend implementation.

    // Exception Handling: section id 13 is the tag section.
    // (module (type (func)) (tag (type 0)))
    inline constexpr ::std::uint8_t exception_tag_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x0du, 0x03u, 0x01u, 0x00u, 0x00u};

    // Exception Handling: throw has opcode 0x08 followed by a tag index. The deliberately absent tag section lets the
    // current parser reach the instruction so the unsupported opcode family is tested independently of section dispatch.
    inline constexpr ::std::uint8_t exception_throw_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x0au, 0x06u, 0x01u, 0x04u, 0x00u, 0x08u, 0x00u, 0x0bu};

    // Threads: limits flag 0x03 means shared memory with a required maximum.
    inline constexpr ::std::uint8_t shared_memory_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x05u, 0x04u, 0x01u, 0x03u, 0x01u, 0x01u};

    // Threads: 0xfe is the atomic instruction prefix; 0x10 is i32.atomic.load. A plain memory keeps section parsing
    // inside the supported Core 2.0 surface so rejection is pinned to the atomic opcode itself.
    inline constexpr ::std::uint8_t atomic_load_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x05u, 0x04u, 0x01u, 0x01u, 0x01u, 0x01u,
        0x0au, 0x0bu, 0x01u, 0x09u, 0x00u, 0x41u, 0x00u, 0xfeu, 0x10u, 0x02u, 0x00u, 0x1au, 0x0bu};

    // Memory64: limits bit 2 (0x04) selects a 64-bit memory and makes min/max u64. This fixture, rather than a WASI
    // target name or pointer-width ABI such as "wasm64-wasi", is the binary-level signal that Memory64 is in use.
    inline constexpr ::std::uint8_t memory64_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x05u, 0x03u, 0x01u, 0x04u, 0x01u};

    // Multi-memory: the memory section declares two ordinary 32-bit memories.
    inline constexpr ::std::uint8_t multi_memory_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x05u, 0x05u, 0x02u, 0x00u, 0x01u, 0x00u, 0x01u};

    // A supported () -> () function proves that each policy reaches normal parsing and validation.
    inline constexpr ::std::uint8_t baseline_module[]{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x04u, 0x01u, 0x60u, 0x00u, 0x00u,
        0x03u, 0x02u, 0x01u, 0x00u,
        0x0au, 0x04u, 0x01u, 0x02u, 0x00u, 0x0bu};

    [[noreturn]] inline void fail(char const* fixture, char const* message)
    {
        ::fast_io::io::perrln("unsupported_post_wasm2_proposals [",
                              ::fast_io::mnp::os_c_str(fixture),
                              "]: ",
                              ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }

    inline void expect(bool condition, char const* fixture, char const* message)
    {
        if(!condition) [[unlikely]] { fail(fixture, message); }
    }

    template <::std::size_t N>
    [[nodiscard]] module_storage_t parse_success(::std::uint8_t const (&bytes)[N], fs_para_t const& policy, char const* fixture)
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        try
        {
            auto module{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, begin + N, err, policy)};
            expect(err.err_code == parse_error_code::ok, fixture, "parser returned a non-ok error");
            return module;
        }
        catch(::fast_io::error const&)
        {
            fail(fixture, "unexpected parser rejection");
        }
    }

    template <::std::size_t N>
    inline void expect_parse_rejection(::std::uint8_t const (&bytes)[N],
                                       fs_para_t const& policy,
                                       char const* fixture,
                                       parse_error_code expected_code,
                                       ::std::size_t expected_offset,
                                       ::std::uint_least8_t expected_detail,
                                       bool const check_detail = true)
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(bytes)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        try
        {
            static_cast<void>(::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<wasm1, wasm1p1, wasm2>(
                begin, begin + N, err, policy));
        }
        catch(::fast_io::error const&)
        {
            expect(err.err_code == expected_code, fixture, "parser rejection used the wrong error code");
            expect(err.err_curr == begin + expected_offset, fixture, "parser rejection used the wrong byte cursor");
            if(check_detail)
            { expect(err.err_selectable.u8 == expected_detail, fixture, "parser rejection recorded the wrong feature byte"); }
            return;
        }
        fail(fixture, "unsupported proposal was accepted by the parser");
    }

    [[nodiscard]] inline auto get_function_body(module_storage_t const& module)
    {
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<wasm1, wasm1p1, wasm2>>(module.sections)};
        expect(codesec.codes.size() == 1uz, "function body", "fixture must contain one local function");
        auto const& code{codesec.codes.index_unchecked(0uz)};
        struct result
        {
            ::std::size_t function_index;
            ::std::byte const* begin;
            ::std::byte const* end;
        };
        return result{importsec.importdesc.index_unchecked(0u).size(),
                      reinterpret_cast<::std::byte const*>(code.body.expr_begin),
                      reinterpret_cast<::std::byte const*>(code.body.code_end)};
    }

    template <::std::size_t N>
    inline void expect_illegal_opcode(::std::uint8_t const (&bytes)[N],
                                      fs_para_t const& policy,
                                      char const* fixture,
                                      ::std::uint_least8_t expected_opcode,
                                      ::std::size_t expected_opcode_offset)
    {
        auto const module{parse_success(bytes, policy, fixture)};
        auto const body{get_function_body(module)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code_with_runtime_policy(
                module, body.function_index, body.begin, body.end, err, policy);
        }
        catch(::fast_io::error const&)
        {
            expect(err.err_code == validation_error_code::illegal_opbase, fixture, "validator rejection used the wrong error code");
            expect(err.err_curr == body.begin + expected_opcode_offset,
                   fixture,
                   "validator rejection was not pinned to the unsupported opcode");
            expect(err.err_selectable.u8 == expected_opcode, fixture, "validator rejection recorded the wrong opcode");
            return;
        }
        fail(fixture, "unsupported opcode was accepted by the validator");
    }

    inline void expect_baseline(fs_para_t const& policy)
    {
        auto const module{parse_success(baseline_module, policy, "baseline")};
        auto const body{get_function_body(module)};
        ::uwvm2::validation::error::code_validation_error_impl err{};
        try
        {
            ::uwvm2::validation::standard::wasm2::validate_code_with_runtime_policy(
                module, body.function_index, body.begin, body.end, err, policy);
        }
        catch(::fast_io::error const&)
        {
            fail("baseline", "supported Core module was rejected");
        }
        expect(err.err_code == validation_error_code::ok, "baseline", "validator returned a non-ok error");
    }
}

int main()
{
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_memory<wasm1, wasm1p1, wasm2>(),
                  "WebAssembly 2.0 must not silently opt into the post-2.0 multi-memory proposal");

    // Full keeps the historical `direct_wasmmvp` spelling while ROS calls the same enum value `direct_mvp`.
    // Value 1 is the stable MVP policy value in both layouts, so the shared regression stays source-identical.
    constexpr cli_mode direct_mvp_mode{static_cast<cli_mode>(1u)};
    constexpr cli_mode supported_core_modes[]{direct_mvp_mode, cli_mode::direct_wasm1p1, cli_mode::direct_wasm2};
    for(auto const mode : supported_core_modes)
    {
        fs_para_t policy{};
        ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(policy).cli_mode = mode;

        expect_baseline(policy);
        expect_parse_rejection(exception_tag_module, policy, "exception tag section", parse_error_code::illegal_section_id, 14uz, 0x0du);
        expect_illegal_opcode(exception_throw_module, policy, "exception throw opcode", 0x08u, 0uz);
        expect_parse_rejection(shared_memory_module, policy, "shared memory", parse_error_code::limit_type_illegal_flag, 11uz, 0x03u);
        expect_illegal_opcode(atomic_load_module, policy, "atomic opcode prefix", 0xfeu, 2uz);
        expect_parse_rejection(memory64_module, policy, "memory64", parse_error_code::limit_type_illegal_flag, 11uz, 0x04u);
        expect_parse_rejection(multi_memory_module,
                               policy,
                               "multi-memory",
                               parse_error_code::wasm1_not_allow_multi_memory,
                               10uz,
                               0x00u,
                               false);
    }
}
