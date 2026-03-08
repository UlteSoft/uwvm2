template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
inline constexpr ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t
    compile_all_from_uwvm_single_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                      [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                      ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t storage{};

#include "single_func_context.h"
#include "single_func_emit_helpers.h"
#include "single_func_validation_dispatch.h"
#include "single_func_call_info.h"
}
