#ifndef UWVM2TEST_RUNNER_USE_LLVM_JIT
# define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Instructions.h>

#include <cstdio>
#include <string_view>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;

    // Use real Wasm translation, not manually synthesized push/pop instructions. Both functions return normally,
    // and the second calls the first, so instruction mode must emit real prologue and epilogue bridge calls.
    [[nodiscard]] strict::byte_vec make_module()
    {
        strict::module_builder module{};
        strict::func_body leaf{};
        strict::append_u8(leaf.code, strict::u8(strict::wasm_op::i32_const));
        strict::append_i32_leb(leaf.code, 7);
        strict::append_u8(leaf.code, strict::u8(strict::wasm_op::end));
        static_cast<void>(module.add_func(strict::func_type{{}, {strict::k_val_i32}}, ::std::move(leaf)));

        strict::func_body caller{};
        strict::append_u8(caller.code, strict::u8(strict::wasm_op::call));
        strict::append_u32_leb(caller.code, 0u);
        strict::append_u8(caller.code, strict::u8(strict::wasm_op::end));
        static_cast<void>(module.add_func(strict::func_type{{}, {strict::k_val_i32}}, ::std::move(caller)));
        return module.build();
    }

    template <auto Bridge>
    [[nodiscard]] bool is_bridge(::llvm::Function const& function)
    {
        auto const storage{compiler::details::get_llvm_runtime_bridge_function_symbol_name<Bridge>()};
        auto const prefix{::std::string_view{reinterpret_cast<char const*>(storage.data()), storage.size()}};
        auto const name{function.getName()};
        return ::std::string_view{name.data(), name.size()}.starts_with(prefix);
    }

    [[nodiscard]] int check(bool instruction_frames)
    {
        auto wasm{make_module()};
        strict::wasm_feature_parameter_t features{};
        auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"native_unwind_ir", {}, features)};
        if(prepared.mod == nullptr) { return 1; }
        ::uwvm2::validation::error::code_validation_error_impl error{};
        compiler::compile_option options{};
        options.validator_feature_parameter = ::std::addressof(features);
        options.verify_llvm_jit_ir = true;
        options.emit_call_stack_frames = instruction_frames;
        options.emit_unwind_call_stack_frames = !instruction_frames;
        auto compiled{compiler::compile_all_from_uwvm(*prepared.mod, options, error, 0uz)};
        if(error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok ||
           !compiled.llvm_jit_module.emitted || compiled.llvm_jit_module.llvm_module == nullptr) { return 2; }

        auto const& module{*compiled.llvm_jit_module.llvm_module};
        ::std::size_t push_count{}, pop_count{}, indirect_host_calls{}, definitions{};
        for(auto const& function: module)
        {
            if(function.isDeclaration()) { continue; }
            ++definitions;
            if(!function.hasFnAttribute(::llvm::Attribute::NoInline)) { return 3; }
            if(!instruction_frames && function.getUWTableKind() != ::llvm::UWTableKind::Async) { return 4; }
            for(auto const& block: function)
            {
                for(auto const& instruction: block)
                {
                    auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                    if(call == nullptr) { continue; }
                    // The empty side-effect asm after a trap is a compiler barrier, not a host call or function inlining.
                    if(call->isInlineAsm()) { continue; }
                    auto const callee{call->getCalledFunction()};
                    if(callee == nullptr)
                    {
                        // RISC-V64 bridge pointers are built from immediate bytes rather than named declarations.
                        // Count the typed bodies only: raw ABI adapters also contain mandatory buffer-check trap calls.
                        // These bodies have no Wasm indirect calls/imports/traps, so their indirect calls can only be
                        // the requested push/pop bridges, including RISC-V's immediate-address lowering.
                        if(!function.getName().contains("_raw_func_")) { ++indirect_host_calls; }
                        continue;
                    }
                    if(is_bridge<::uwvm2::runtime::lib::llvm_jit_push_call_stack_frame>(*callee)) { ++push_count; }
                    if(is_bridge<::uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame>(*callee)) { ++pop_count; }
                }
            }
        }
        ::std::printf("native-unwind-ir instruction=%d definitions=%zu push=%zu pop=%zu indirect-host=%zu\n",
                      instruction_frames, definitions, push_count, pop_count, indirect_host_calls);
        if(definitions < 2uz) { return 5; }
        if(instruction_frames)
        {
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
            if(indirect_host_calls < 4uz) { return 6; }
#else
            if(push_count < 2uz || pop_count < 2uz || indirect_host_calls != 0uz)
            {
                module.print(::llvm::errs(), nullptr);
                return 6;
            }
#endif
        }
        else if(push_count != 0uz || pop_count != 0uz || indirect_host_calls != 0uz) { return 7; }
        return 0;
    }
}

int main()
{
    if(auto const result{check(true)}; result != 0) { return result; }
    return check(false);
}
