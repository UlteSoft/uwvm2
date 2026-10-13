// Exact production bridge-name prefixes for relocations in cached JIT objects.
// This utility emits names only; it never generates or executes guest code.
#define UWVM_USE_LLVM_JIT
#define UWVM_DISABLE_INT
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate.h>
#include <cstdio>
namespace details = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
template<auto Function> void emit(char const* key,bool comma=true)
{
    auto const name{details::get_llvm_runtime_bridge_function_symbol_name<Function>()};
    std::printf("\"%s\":\"%.*s\"%s\n",key,int(name.size()),reinterpret_cast<char const*>(name.data()),comma?",":"");
}
int main()
{
    std::puts("{");
    emit<uwvm2::runtime::lib::llvm_jit_push_call_stack_frame>("push");
    emit<uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame>("pop");
    emit<uwvm2::runtime::lib::details::llvm_jit_throw_numeric_abi_bridge>("throw");
    emit<uwvm2::runtime::lib::details::llvm_jit_exception_matches_tag_abi_bridge>("matches");
    emit<uwvm2::runtime::lib::details::llvm_jit_exception_copy_numeric_payload_abi_bridge>("copy",false);
    std::puts("}");
}
