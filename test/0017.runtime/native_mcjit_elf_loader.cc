// Actual MCJIT ELF loader execution oracle. Requires a source-matched SDK.
// This isolates calls/data/EH relocation failures before the separate real
// Wasm native-debugger witness; it does not grant debugger authority.
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <cstdint>
#include <memory>
#include <fast_io.h>
#ifndef UWVM_RUNTIME_LLVM_JIT
# define UWVM_RUNTIME_LLVM_JIT 1
#endif
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>

extern "C" ::std::uint64_t uwvm_native_loader_host(::std::uint64_t a, ::std::uint64_t b) noexcept
{ return (a * 19u + b) ^ UINT64_C(0x9abcde0123456789); }
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("MCJIT ELF loader failure line=",__LINE__); return 1; } } while(false)
int main()
{
    CHECK(!::llvm::InitializeNativeTarget());
    CHECK(!::llvm::InitializeNativeTargetAsmPrinter());
    CHECK(!::llvm::InitializeNativeTargetAsmParser());
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("native-elf-loader",context)};
    ::llvm::IRBuilder<> ir{context};
    auto* const i64{ir.getInt64Ty()};
    auto* const type{::llvm::FunctionType::get(i64,{i64,i64},false)};
    auto* const global{new ::llvm::GlobalVariable(*module,i64,true,::llvm::GlobalValue::InternalLinkage,
                                               ir.getInt64(UINT64_C(0x1029384756abcdef)),"loader.numeric")};
    auto* const callee{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"loader_callee",*module)};
    callee->setSection(".text.loader.callee"); callee->addFnAttr(::llvm::Attribute::NoInline);
    ir.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",callee));
    ir.CreateRet(ir.CreateAdd(ir.CreateMul(callee->getArg(0u),ir.getInt64(7u)),callee->getArg(1u)));
    auto* const host{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_native_loader_host",*module)};
    auto* const entry{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"loader_entry",*module)};
    entry->setSection(".text.loader.entry");
    ir.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",entry));
    auto* const intermediate{ir.CreateCall(callee,{entry->getArg(0u),entry->getArg(1u)})};
    auto* const result{ir.CreateCall(host,{intermediate,ir.CreateLoad(i64,global)})};
    // V9 raw ABI bridges also need full-width shift selection. A generic
    // host CPU report must not make a 64-bit SPARC process select V8 here.
    auto* const shifted{ir.CreateOr(ir.CreateLShr(result,ir.getInt64(16u)),
                                   ir.CreateShl(result,ir.getInt64(48u)))};
    ir.CreateRet(ir.CreateXor(shifted,entry->getArg(0u)));
    CHECK(!::llvm::verifyModule(*module));
    ::llvm::sys::DynamicLibrary::AddSymbol("uwvm_native_loader_host",reinterpret_cast<void*>(
        reinterpret_cast<::std::uintptr_t>(::std::addressof(uwvm_native_loader_host))));
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT)
        .setMCPU(::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_abi_host_cpu_name(
            ::llvm::Triple{::llvm::sys::getProcessTriple()}, ::llvm::sys::getHostCPUName()))
        .setRelocationModel(::llvm::Reloc::Static)
        .setOptLevel(::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::make_unique<::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()).create()}};
    CHECK(engine); engine->finalizeObject(); CHECK(!engine->hasError());
    auto const address{engine->getFunctionAddress("loader_entry")}; CHECK(address);
    auto* const invoke{reinterpret_cast<::std::uint64_t (*)(::std::uint64_t,::std::uint64_t)>(
        static_cast<::std::uintptr_t>(address))};
    constexpr ::std::uint64_t inputs[]{0u,1u,42u,UINT64_C(0x8000000012345678),UINT64_MAX};
    for(auto const a: inputs)
    {
        auto const b{a ^ UINT64_C(0x1122334455667788)};
        auto const raw{uwvm_native_loader_host(a * 7u + b,UINT64_C(0x1029384756abcdef))};
        auto const expected{((raw >> 16u) | (raw << 48u)) ^ a};
        CHECK(invoke(a,b)==expected);
    }
    // Retire the engine normally: its registered EH frames and executable
    // allocations must not survive object ownership, even on windowed SPARC.
    engine.reset();
    ::fast_io::io::println("PASS actual MCJIT cross-section call, external tail stub, full-width numeric data, preserved call ABI, normal EH retirement");
}
