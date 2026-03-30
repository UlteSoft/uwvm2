#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

void test_runtime_entry()
{
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t module{};
    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option opt{};
    ::uwvm2::validation::error::code_validation_error_impl err{};

    [[maybe_unused]] auto storage{::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_all_from_uwvm(module, opt, err, 0uz)};
}

void test_parser_entry()
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature>;

    [[maybe_unused]] module_storage_t module_storage{};
    [[maybe_unused]] bool ok{::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::validate_all_wasm_code_for_module(module_storage, {}, {})};
}

int main()
{
    // NOTE: do not call the compile-only instantiation helpers.
    return 0;
}
