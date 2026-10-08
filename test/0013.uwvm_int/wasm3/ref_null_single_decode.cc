// Core 3 ref.null successful heaps decode once; failed heaps retain the existing diagnostic policy.
// The integrated cases use an actually parsed/initialized function-only source module, not fabricated runtime authority.
#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#endif

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    using error_t = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    unsigned checks{};

    void require(bool condition, unsigned line)
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::print(::fast_io::err(), "FAIL ref.null single decode line ", line, "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    void append_bytes(byte_vec& output, ::std::initializer_list<unsigned> values)
    { for(auto value : values) { append_u8(output, value); } }

    auto features(bool typed)
    {
        auto result{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result)};
        policy.disable_function_references = !typed;
        policy.disable_gc = true;
        policy.disable_exceptions = true;
        return result;
    }

    auto const& codes()
    {
        auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        return []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
            ::uwvm2::utils::container::tuple<Fs...>) -> auto const&
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);
        }(parsed.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }

    template<typename Operation>
    void observe_parse_failure(Operation operation, bool expected_valid, error_t const& error)
    {
        bool rejected{};
        try { operation(); }
        catch(::fast_io::error const& failure)
        {
            // Allocation/native/LLVM failures cannot count as a specification rejection.
            REQUIRE(failure.domain == ::fast_io::parse_domain_value);
            REQUIRE(failure.code == static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid)));
            rejected = true;
        }
        REQUIRE(rejected != expected_valid);
        REQUIRE((error.err_code == error_code::ok) == expected_valid);
    }

    void integrated_cases()
    {
        struct sample { bool valid; bool typed; unsigned result; ::std::initializer_list<unsigned> body; };
        sample const cases[]{
            {true, false, 0x70u, {0xd0u, 0x70u}},
            {true, false, 0x6fu, {0xd0u, 0x6fu}},
            {true, true, 0x70u, {0xd0u, 0u}},
            {true, true, 0x70u, {0xd0u, 11u}},
            {true, true, 0x70u, {0xd0u, 0x80u, 1u}},
            {true, true, 0x70u, {0xd0u, 0x80u, 0x80u, 0x80u, 0x80u, 0u}},
            // ref.as_non_null preserves the concrete function heap while narrowing nullability.
            // These are validation cases only: a null input must still trap if executed.
            {true, true, 0x70u, {0xd0u, 0u, 0xd4u}},
            {true, true, 0x70u, {0xd0u, 0x80u, 1u, 0xd4u}},
            {false, true, 0x6fu, {0xd0u, 0u}},
            {false, true, 0x6fu, {0xd0u, 0u, 0xd4u}},
            {false, false, 0x70u, {0xd0u, 0u}},
            {false, true, 0x70u, {0xd0u, 0x82u, 1u}},
            {false, true, 0x70u, {0xd0u, 0xffu, 0xffu, 0xffu, 0xffu, 0x0fu}},
            {false, true, 0x70u, {0xd0u, 0xf0u, 0x7fu}},
            {false, true, 0x70u, {0xd0u, 0x80u, 0x80u, 0x80u, 0x80u, 0x10u}}
        };
        for(auto const& current : cases)
        {
            module_builder builder;
            builder.types.resize(129);
            func_body body;
            append_bytes(body.code, current.body);
            append_bytes(body.code, {0x0bu});
            builder.add_func({{}, {static_cast<::std::uint8_t>(current.result)}}, ::std::move(body));
            auto source{builder.build()};
            // Parsing with the original legacy feature policy keeps a real function-only indexed
            // type context. Code validation uses the independently selected scoped policy below.
            // No retained runtime type, owner, tuple or source byte is manually altered.
            auto parsing_features{features(false)};
            auto prepared{prepare_runtime_from_wasm(source, u8"ref-null-single-decode", {}, parsing_features)};
            auto validation_features{features(current.typed)};
            auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
            auto const& body_bytes{codes().codes.index_unchecked(0).body};
            error_t pure_error{};
            observe_parse_failure([&]
            {
                v3::validate_code(v3::wasm3_code_version{}, parsed, 0,
                    reinterpret_cast<::std::byte const*>(body_bytes.expr_begin),
                    reinterpret_cast<::std::byte const*>(body_bytes.code_end), pure_error, validation_features);
            }, current.valid, pure_error);
            error_t fused_error{};
            observe_parse_failure([&]
            {
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
                namespace jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
                jit::compile_option options;
                options.validator_feature_parameter = &validation_features;
                options.verify_llvm_jit_ir = true;
                auto product{jit::compile_all_from_uwvm(*prepared.mod, options, fused_error, 0)};
                REQUIRE(product.llvm_jit_module.emitted);
                REQUIRE(!::llvm::verifyModule(*product.llvm_jit_module.llvm_module, &::llvm::errs()));
#else
                optable::compile_option options;
                auto product{compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(
                    *prepared.mod, options, fused_error, &validation_features)};
                (void)product;
#endif
            }, current.valid, fused_error);
            REQUIRE(pure_error.err_code == fused_error.err_code);
        }
    }

    void bounded_heap_cases()
    {
        using heap_error = v3::function_heap_immediate_error;
        struct sample { bool enabled; heap_error error; ::std::initializer_list<unsigned> bytes; };
        sample const cases[]{
            {true, heap_error::ok, {0u}},
            {true, heap_error::ok, {0x80u, 1u}},
            {true, heap_error::ok, {0x70u}},
            {true, heap_error::ok, {0x80u, 0x80u, 0x80u, 0x80u, 0u}},
            {false, heap_error::function_references_disabled, {0u}},
            {true, heap_error::unknown_type, {0x81u, 1u}},
            {true, heap_error::binary, {}},
            {true, heap_error::binary, {0x80u}},
            {true, heap_error::binary, {0x80u, 0x80u}},
            {true, heap_error::binary, {0x80u, 0x80u, 0x80u, 0x80u, 0x10u}},
            {true, heap_error::binary, {0xf0u, 0x7fu}}
        };
        for(auto const& current : cases)
        {
            byte_vec bytes;
            append_bytes(bytes, current.bytes);
            // [sentinel opcode][heap bytes ... end)
            // [safe           ] heap may be empty; the sentinel still supplies a real diagnostic opcode.
            bytes.insert(bytes.begin(), static_cast<::std::byte>(0xd0u));
            auto const opcode{bytes.data()};
            // [sentinel opcode][heap bytes ... end)
            // [safe           ] adding one follows the actual one-byte sentinel extent.
            auto const begin{opcode + 1uz};
            // [allocated bytes ...][end]
            // [safe               ] begin/end stay in this live byte_vec allocation.
            auto const end{opcode + bytes.size()};
            auto cursor{begin};
            auto const decoded{v3::scan_function_ref_null_heap(cursor, end, current.enabled, 129uz)};
            REQUIRE(decoded.error == current.error);
            REQUIRE(cursor == (current.error == heap_error::ok ? end : begin));
            auto wrapper_cursor{begin};
            error_t error{};
            bool rejected{};
            try
            {
                auto const carrier{v3::read_function_ref_null_carrier(
                    wrapper_cursor, end, current.enabled, 129uz, opcode, error)};
                REQUIRE(current.error == heap_error::ok);
                REQUIRE(carrier == decoded.carrier);
            }
            catch(::fast_io::error const& failure)
            {
                REQUIRE(failure.domain == ::fast_io::parse_domain_value);
                REQUIRE(failure.code == static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid)));
                rejected = true;
            }
            REQUIRE(rejected == (current.error != heap_error::ok));
            REQUIRE(wrapper_cursor == cursor);
        }
    }
}

int main()
{
    integrated_cases();
    bounded_heap_cases();
    ::fast_io::print(::fast_io::out(), "PASS ref.null single decode checks=", checks, "\n");
}
