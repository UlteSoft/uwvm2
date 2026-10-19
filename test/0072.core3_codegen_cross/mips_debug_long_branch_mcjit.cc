// Source-bound MCJIT long-branch component, executed under the Linux cgroup.
// This creates only its own generated function. It grants no VM/native debugger
// access and does not qualify a complete Wasm product by itself.
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <cstdint>
#include <array>
#include <memory>
#include <string>

struct long_branch_listener final : ::llvm::JITEventListener
{
    char const* output{};
    ::std::uintptr_t begin{};
    ::std::size_t size{};
    bool captured{};
    void notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        for(auto const& [symbol, extent] : ::llvm::object::computeSymbolSizes(object))
        {
            auto name{symbol.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); continue; }
            if(*name != "uwvm_mips_debug_long_branch_component") { continue; }
            auto section{symbol.getSection()}; auto address{symbol.getAddress()};
            if(!section) { ::llvm::consumeError(section.takeError()); continue; }
            if(!address) { ::llvm::consumeError(address.takeError()); continue; }
            if(*section == object.section_end() || extent <= 160000u || extent > 200000u ||
               *address < (**section).getAddress()) { continue; }
            auto base{loaded.getSectionLoadAddress(**section)};
            begin = base + *address - (**section).getAddress();
            size = extent;
        }
        if(begin == 0u || size == 0u) { return; }
        auto const bytes{object.getMemoryBufferRef().getBuffer()};
        ::fast_io::native_file file{::fast_io::mnp::os_c_str(output),
            ::fast_io::open_mode::out | ::fast_io::open_mode::excl,
            static_cast<::fast_io::perms>(0600)};
        auto first{reinterpret_cast<::std::byte const*>(bytes.data())};
        ::fast_io::operations::write_all_bytes(file, first, first + bytes.size());
        file.close(); captured = true;
    }
};
int main(int argc, char** argv)
{
    if(argc != 2 || ::llvm::InitializeNativeTarget() || ::llvm::InitializeNativeTargetAsmPrinter() ||
       ::llvm::InitializeNativeTargetAsmParser()) { return 1; }
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("contained-static-long-branch", context)};
    module->setTargetTriple(::llvm::Triple{::llvm::sys::getProcessTriple()});
    auto type{::llvm::Type::getInt64Ty(context)};
    auto function{::llvm::Function::Create(::llvm::FunctionType::get(type, {type}, false),
        ::llvm::Function::ExternalLinkage, "uwvm_mips_debug_long_branch_component", *module)};
    function->addFnAttr("uwvm.native.debug.pcrel");
    auto entry{::llvm::BasicBlock::Create(context, "entry", function)};
    auto skip{::llvm::BasicBlock::Create(context, "skip", function)};
    auto padding{::llvm::BasicBlock::Create(context, "padding", function)};
    auto distant{::llvm::BasicBlock::Create(context, "distant", function)};
    auto finish{::llvm::BasicBlock::Create(context, "finish", function)};
    ::llvm::IRBuilder<> builder{entry};
    builder.CreateCondBr(builder.CreateICmpNE(function->getArg(0), builder.getInt64(0)), skip, padding);
    builder.SetInsertPoint(skip); builder.CreateBr(distant);
    ::fast_io::string nops{}; nops.reserve(160000u);
    for(unsigned index{}; index != 40000u; ++index) { nops.append("nop\n", 4u); }
    builder.SetInsertPoint(padding);
    auto asm_type{::llvm::FunctionType::get(builder.getVoidTy(), false)};
    auto pad{::llvm::InlineAsm::get(asm_type, {nops.data(), nops.size()}, "", true)};
    builder.CreateCall(pad); builder.CreateBr(finish);
    builder.SetInsertPoint(distant); builder.CreateRet(builder.getInt64(2));
    builder.SetInsertPoint(finish); builder.CreateRet(builder.getInt64(1));
    if(::llvm::verifyModule(*module)) { return 2; }
    ::std::string error{}; // LLVM requires this exact error-output type.
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{
        ::llvm::EngineBuilder(::std::move(module)).setEngineKind(::llvm::EngineKind::JIT)
        .setOptLevel(::llvm::CodeGenOptLevel::None).setErrorStr(&error)
        .setRelocationModel(::llvm::Reloc::Static)
        .setMCPU("mips64r2").setMAttrs(::std::array<::std::string, 3>{"+noabicalls", "+long-calls", "-msa"})
        .setMCJITMemoryManager(::std::make_unique<::llvm::SectionMemoryManager>()).create()};
    if(!engine) { ::fast_io::io::perrln("engine: ", ::fast_io::mnp::strvw(error)); return 3; }
    long_branch_listener listener{}; listener.output = argv[1];
    engine->RegisterJITEventListener(&listener); engine->finalizeObject();
    engine->UnregisterJITEventListener(&listener);
    auto const address{engine->getFunctionAddress("uwvm_mips_debug_long_branch_component")};
    if(engine->hasError() || !listener.captured || address != listener.begin) { return 4; }
    auto call{reinterpret_cast<::std::uint64_t (*)(::std::uint64_t)>(static_cast<::std::uintptr_t>(address))};
    if(call(0u) != 1u || call(1u) != 2u) { return 5; }
    ::fast_io::io::println("PASS static marked long branch; function bytes=", listener.size, "; executions=2");
}
