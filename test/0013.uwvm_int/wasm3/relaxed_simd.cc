#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#include "relaxed_simd_vectors.h"

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace policy = uwvm2::validation::standard::wasm3;
    using error_t = uwvm2::validation::error::code_validation_error_impl;
    using error_code = uwvm2::validation::error::code_validation_error_code;

    byte_vec build(unsigned opcode, unsigned arity, int malformed = 0)
    {
        module_builder module{};
        func_type type{};
        type.params.assign(arity, 0x7b);
        type.results = {0x7b};
        func_body body{};
        if(malformed == 4) { append_u8(body.code, 0x00); } // polymorphic stack is legal
        for(unsigned i{}; i != arity && malformed != 4; ++i)
        {
            if(malformed == 1 && i == 0) { continue; }
            append_u8(body.code, malformed == 2 && i == 0 ? 0x41 : 0x20);
            append_u32_leb(body.code, i);
        }
        append_u8(body.code, 0xfd);
        if(malformed == 3)
        {
            for(auto b: {0x80, 0x80, 0x80, 0x80, 0x10, 0x0b}) { append_u8(body.code, b); } // u32 overflow
        }
        else { append_u32_leb(body.code, opcode); append_u8(body.code, 0x0b); }
        module.add_func(std::move(type), std::move(body));
        return module.build();
    }

    template <optable::uwvm_interpreter_translate_option_t Option>
    int check()
    {
        auto features{make_wasm1p1_feature_parameter()};
        optable::compile_option options{};
        auto& selected{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        selected.disable_relaxed_simd = false;
        for(auto const& item: relaxed_cases)
        {
            auto wasm{build(item.opcode, item.arity)};
            auto prepared{prepare_runtime_from_wasm(wasm, u8"relaxed_simd", {}, features)};
            error_t error{};
            auto compiled{compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod, options, error, &features)};
            UWVM2TEST_REQUIRE(error.err_code == error_code::ok);
            byte_vec arguments{};
            for(unsigned i{}; i != item.arity; ++i)
                for(auto b: item.args[i]) { arguments.push_back(static_cast<std::byte>(b)); }
            auto result{interpreter_runner<Option>::run(compiled.local_funcs.index_unchecked(0),
                prepared.mod->local_defined_function_vec_storage.index_unchecked(0), arguments, nullptr, nullptr)};
            UWVM2TEST_REQUIRE(result.results.size() == 16);
            for(unsigned i{}; i != 16; ++i)
            {
                if(((std::to_integer<unsigned>(result.results[i]) ^ item.expected[i]) & item.mask[i]) != 0)
                {
                    std::fprintf(stderr, "opcode=%x byte=%u got=%x expected=%x\n", item.opcode, i,
                        std::to_integer<unsigned>(result.results[i]), item.expected[i]);
                    return 1;
                }
            }
        }
        // Every new opcode is validated with its own arity, feature gate and unreachable-stack rule.
        for(unsigned opcode{0x100}; opcode <= 0x114; ++opcode)
        {
            auto const arity{opcode == 0x114 ? 1u : policy::relaxed_simd_operand_count(opcode)};
            for(int malformed{}; malformed != 6; ++malformed)
            {
                bool const disabled{malformed == 5};
                auto wasm{build(opcode, arity, disabled ? 0 : malformed)};
                auto prepared{prepare_runtime_from_wasm(wasm, u8"relaxed_simd_validation", {}, features)};
                selected.disable_relaxed_simd = disabled;
                error_t integrated{};
                try { (void)compiler::compile_all_from_uwvm_single_func<Option>(*prepared.mod, options, integrated, &features); }
                catch(fast_io::error const&) {}
                bool const accepted{opcode != 0x114 && (malformed == 0 || malformed == 4)};
                UWVM2TEST_REQUIRE((integrated.err_code == error_code::ok) == accepted);
                auto const& parsed{uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
                auto const& codes = []<uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections, uwvm2::utils::container::tuple<Fs...>) -> auto const&
                { return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections); }
                    (parsed.sections, uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
                auto const& body{codes.codes.index_unchecked(0).body};
                error_t standalone{};
                try { uwvm2::validation::standard::wasm3::validate_code_with_runtime_policy(parsed, 0,
                    reinterpret_cast<std::byte const*>(body.expr_begin), reinterpret_cast<std::byte const*>(body.code_end), standalone, features); }
                catch(fast_io::error const&) {}
                UWVM2TEST_REQUIRE((standalone.err_code == error_code::ok) == accepted);
                selected.disable_relaxed_simd = false;
            }
        }
        return 0;
    }
}
int main()
{
    install_unexpected_traps();
    constexpr optable::uwvm_interpreter_translate_option_t uncached{.is_tail_call = false};
    constexpr optable::uwvm_interpreter_translate_option_t ring{
        .is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 5uz,
        .i64_stack_top_begin_pos = 3uz, .i64_stack_top_end_pos = 5uz,
        .f32_stack_top_begin_pos = 5uz, .f32_stack_top_end_pos = 7uz,
        .f64_stack_top_begin_pos = 5uz, .f64_stack_top_end_pos = 7uz};
    UWVM2TEST_REQUIRE(check<uncached>() == 0);
    UWVM2TEST_REQUIRE(check<ring>() == 0);
    std::puts("PASS relaxed SIMD: 25 vectors, 126 validation cases, uncached and register-ring");
}
