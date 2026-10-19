// Second TU for the production CLI/loader security regression runner.
#include <uwvm2/uwvm/runtime/initializer/impl.h>

void wsea_test_initialize_modules()
{
    namespace wasm = uwvm2::uwvm::wasm;
    namespace initializer = uwvm2::uwvm::runtime::initializer;
    // Each source reaches the real authorization/capability initializer gate;
    // no compiler or guest start function runs in this regression runner.
    for(auto const& [name, module] : wasm::storage::all_module)
    {
        using kind = wasm::type::module_type_t;
        if(module.type != kind::exec_wasm && module.type != kind::preloaded_wasm) { continue; }
        uwvm2::uwvm::runtime::storage::wasm_module_storage_t instance{};
        initializer::details::current_initializing_module_name = name;
        initializer::details::initialize_from_wasm_file(*module.module_storage_ptr.wf, instance);
    }
}
