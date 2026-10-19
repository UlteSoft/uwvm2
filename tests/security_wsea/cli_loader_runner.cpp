// Exercise the production CLI prepass, preload callbacks, immutable loader and
// initializer gate without compiling or executing a guest backend.
// Build with UWVM=2, UWVM_DISABLE_INT, UWVM_DISABLE_JIT and optionally
// UWVM_WSEA_OPENSSL=1; this is intentionally distinct from a full VM run.
#include <cstddef>
#include <cstdio>
#include <vector>
#include <uwvm2/uwvm/cmdline/impl.h>
#include <uwvm2/uwvm/cmdline/callback/impl.h>
#include <uwvm2/uwvm/wasm/loader/impl.h>

// Split the large initializer AST into a second serially compiled TU.
void wsea_test_initialize_modules();

int main(int argc, char** argv)
{
    namespace command = uwvm2::uwvm::cmdline;
    namespace wasm = uwvm2::uwvm::wasm;
    std::vector<char8_t const*> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for(int index{}; index != argc; ++index)
    { arguments.push_back(reinterpret_cast<char8_t const*>(argv[index])); }
    auto parsed{command::parsing(arguments.size(), arguments.data())};
    if(parsed == command::parsing_return_val::return0) { return 0; }
    if(parsed != command::parsing_return_val::def) { return 2; }
    if(command::wasm_file_ppos == nullptr) { std::puts("No module selected for loading"); return 64; }
    auto& main_file{wasm::storage::active_execute_wasm()};
    if(wasm::loader::load_wasm_file(main_file, command::wasm_file_ppos->str,
        main_file.module_name, wasm::storage::wasm_parameter) != wasm::loader::load_wasm_file_rtl::ok)
    { return 2; }
    if(wasm::loader::construct_all_module_and_check_duplicate_module() != wasm::loader::load_and_check_modules_rtl::ok)
    { return 2; }
    wsea_test_initialize_modules();
    std::puts("ALLOW production CLI / loader / initializer gates");
}
