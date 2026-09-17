// Companion to check_validation_control_parity.py. Deliberately exercise the
// compiler-owned validators directly: a standard prepass must not hide drift.
#if defined(UWVM_DISABLE_INT) && !defined(UWVM2TEST_STRICT_NO_INTERPRETER)
# define UWVM2TEST_STRICT_NO_INTERPRETER 1
#endif
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

template <typename Mode>
constexpr Mode mvp_mode() noexcept
{
    if constexpr(requires { Mode::direct_wasmmvp; }) { return Mode::direct_wasmmvp; }
    else { return Mode::direct_mvp; }
}

int main(int argc, char** argv)
{
    if(argc == 1) { return 0; } // This batch probe is driven by the companion script.
    if(argc != 2) { return 2; }
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace jc = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
#ifndef UWVM_DISABLE_INT
    namespace ic = ::uwvm2::runtime::compiler::uwvm_int;
#endif
    namespace feature = ::uwvm2::parser::wasm::standard::wasm1p1::features;
    using error = ::uwvm2::validation::error::code_validation_error_impl;
    using code = ::uwvm2::validation::error::code_validation_error_code;
    strict::wasm_feature_parameter_t policy{};
    auto& p{feature::get_wasm1p1_parameter(policy)};
    std::string const profile{argv[1]};
    if(profile != "mvp" && profile != "wasm1p1" && profile != "wasm2") { return 2; }
    using mode = feature::wasm_feature_cli_mode;
    p.cli_mode = profile == "mvp" ? mvp_mode<mode>() : profile == "wasm2" ? mode::direct_wasm2 : mode::direct_wasm1p1;
    bool const mvp{profile == "mvp"};
    p.disable_sign_extension = p.disable_nontrapping_float_to_int = p.disable_multi_value = mvp;
    p.disable_reference_types = p.disable_table_instructions = p.disable_multiple_tables = mvp;
    p.disable_bulk_memory = p.disable_simd = mvp;
    p.controllable_allow_multi_result_vector = p.controllable_allow_multi_table = mvp;
    for(std::string path; std::getline(std::cin, path);)
    {
        std::ifstream file{path, std::ios::binary};
        if(!file) { return 3; }
        std::string const data{std::istreambuf_iterator<char>{file}, {}};
        strict::byte_vec bytes{};
        for(unsigned char b : data) { bytes.push_back(static_cast<std::byte>(b)); }
        auto prepared{strict::prepare_runtime_from_wasm(bytes, u8"validation_control", {}, policy)};
        if(prepared.mod == nullptr) { return 4; }
        auto const view{jc::details::build_runtime_validation_module(*prepared.mod)};
        error standard{}, interpreter{}, jit{};
#ifndef UWVM_DISABLE_INT
        ic::optable::compile_option options{};
        ic::optable::uwvm_interpreter_full_function_symbol_t storage{};
        ic::compile_all_from_uwvm::details::initialize_local_defined_call_info(*prepared.mod, options, storage);
#endif
        for(std::size_t i{}; i != prepared.mod->local_defined_function_vec_storage.size(); ++i)
        {
            error local_error{};
            auto const local{jc::details::get_runtime_local_func_storage(*prepared.mod, i, local_error)};
            if(local_error.err_code != code::ok) { return 5; }
            if(standard.err_code == code::ok)
            {
                try { jc::details::validate_runtime_local_func_with_code_version_strategy(view, local, standard, &policy); }
                catch(::fast_io::error const&) {}
            }
#ifndef UWVM_DISABLE_INT
            if(interpreter.err_code == code::ok)
            {
                try { ic::compile_all_from_uwvm::details::compile_all_from_uwvm_local_func<
                    ic::optable::uwvm_interpreter_translate_option_t{}>(*prepared.mod, options, storage, i, &policy, interpreter); }
                catch(::fast_io::error const&) {}
            }
#endif
            if(jit.err_code == code::ok)
            {
                // ROS intentionally has no lazy/tiered target-table parameters.
                try { jc::details::validate_runtime_local_func(view, local, jit, nullptr, false, false,
                    false, false, &policy); }
                catch(::fast_io::error const&) {}
            }
        }
        std::cout << static_cast<unsigned>(standard.err_code) << ' '
                  << static_cast<unsigned>(interpreter.err_code) << ' '
                  << static_cast<unsigned>(jit.err_code) << '\n';
    }
}
