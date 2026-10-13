#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
#include <string_view>

// Capture the actual Wasm translator output for offline opt/llc inspection. Runtime execution and
// trap-stack assertions belong to run_wasm3_initializers.py; emitting IR is not an execution test.
int main(int argc, char** argv)
{
    if(argc != 4) { return 2; }
    bool const instruction{std::string_view{argv[3]} == "instruction"};
    if(!instruction && std::string_view{argv[3]} != "unwind") { return 2; }
    namespace strict = uwvm2test::uwvm_int_strict;
    namespace compiler = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) { return 3; }
    strict::byte_vec wasm{};
    for(char c; input.get(c);) { wasm.push_back(static_cast<std::byte>(static_cast<unsigned char>(c))); }
    if(!input.eof()) { return 3; }
    strict::wasm_feature_parameter_t parameters{};
    auto& features{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(parameters)};
    features.disable_extended_const = false;
    features.disable_table_initializer = false;
    features.disable_relaxed_simd = false;
    features.disable_multi_memory = false;
    auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"wasm3_initializers_ir", {}, parameters)};
    if(prepared.mod == nullptr) { return 4; }
    uwvm2::validation::error::code_validation_error_impl error{};
    compiler::compile_option options{};
    options.validator_feature_parameter = &parameters;
    options.verify_llvm_jit_ir = true;
    options.emit_call_stack_frames = instruction;
    options.emit_unwind_call_stack_frames = !instruction;
    auto compiled{compiler::compile_all_from_uwvm(*prepared.mod, options, error, 0uz)};
    if(error.err_code != uwvm2::validation::error::code_validation_error_code::ok ||
       !compiled.llvm_jit_module.emitted || compiled.llvm_jit_module.llvm_module == nullptr) { return 5; }
    auto& module{*compiled.llvm_jit_module.llvm_module};
    if(llvm::verifyModule(module, &llvm::errs())) { return 6; }
    std::error_code file_error{};
    llvm::raw_fd_ostream output(argv[2], file_error);
    if(file_error) { return 7; }
    module.print(output, nullptr);
    output.close();
    return output.has_error() ? 8 : 0;
}
