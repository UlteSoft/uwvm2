// macOS 4 GiB qualification: emit this subset of CLI callback definitions
// separately so a single compiler process never owns the whole table AST.
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wundefined-inline"
#endif
#include <uwvm2/uwvm/cmdline/impl.h>
#include "version.h"
#include "help.h"
#include "mode.h"
#include "debug_test.h"
#include "wasm_set_main_module_name.h"
#include "wasm_set_start_func.h"
#include "wasm_preload_library.h"
#include "wasm_register_dl.h"
#include "wasm_reset_import.h"
#include "wasm_set_preload_module_attribute.h"
#include "wasm_depend_recursion_limit.h"
#include "wasm_set_memory_limit.h"
#include "wasm_set_parser_limit.h"
#include "wasm_set_initializer_limit.h"
#include "wasm_list_weak_symbol_module.h"
#include "wasm_feature.h"
#include "runtime_custom_mode.h"
#include "runtime_custom_compiler.h"

extern "C" void const* uwvm_low_memory_callback_core_anchor() noexcept
{
    return &::uwvm2::uwvm::cmdline::hash_table;
}
