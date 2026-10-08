// Genuine MCJIT musttail oracle: exact C ABI, private bounded-stack witness.
// No debugger/native memory or register authority is created by this test.
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <cstdint>
#include <memory>
#include <limits>
#include <fast_io.h>
#ifndef UWVM_RUNTIME_LLVM_JIT
# define UWVM_RUNTIME_LLVM_JIT 1
#endif
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>

namespace {
std::uintptr_t low{}, high{};
std::uint32_t calls{}, next_count{};
bool sequence_valid{};
}
extern "C" __attribute__((noinline)) void uwvm_musttail_stack_witness(std::uint32_t count) noexcept
{
    volatile unsigned char private_marker{};
    auto const address{reinterpret_cast<std::uintptr_t>(&private_marker)};
    if(calls == 0u) { low = high = address; }
    else { if(address < low) { low = address; } if(address > high) { high = address; } }
    if(calls > 4096u || count != next_count)
    {
        fast_io::io::perrln("FAIL private musttail numeric count sequence count=",count," expected=",next_count," calls=",calls);
        fast_io::fast_terminate();
    }
    sequence_valid = sequence_valid && count == next_count;
    if(count != 0u) { next_count = count - 1u; }
    ++calls;
}
#define CHECK(x) do { if(!(x)) { fast_io::io::perrln("MCJIT musttail failure line=",__LINE__); return 1; } } while(false)
struct owned_object_witness : llvm::JITEventListener
{
    char const* directory{}; unsigned generation{};
    void notifyObjectLoaded(ObjectKey,llvm::object::ObjectFile const& object,llvm::RuntimeDyld::LoadedObjectInfo const&) override
    {
        if(directory == nullptr) { return; }
        auto const path{fast_io::concat_fast_io(fast_io::mnp::os_c_str(directory), "/musttail-owned-g",generation,".o")};
        fast_io::native_file file{path,fast_io::open_mode::out};auto const bytes{object.getData()};
        fast_io::io::print(file,fast_io::mnp::strvw(bytes));
    }
};

int arm_tailcc_normal_call_oracle(llvm::Triple const& target)
{
    if(!target.isARM()) { return 0; }
    for(auto const level : {llvm::CodeGenOptLevel::None,llvm::CodeGenOptLevel::Default})
    {
        llvm::LLVMContext context{};
        llvm::IRBuilder<> ir{context};
        auto module{std::make_unique<llvm::Module>("arm-tailcc-normal-calls",context)};
        auto* const i32{ir.getInt32Ty()};
        auto* const type{llvm::FunctionType::get(i32,{i32},false)};
        auto* const slot{new llvm::GlobalVariable(*module,i32,false,llvm::GlobalValue::InternalLinkage,
            ir.getInt32(0u),"tailcc_numeric_side_effect")};
        auto* const leaf{llvm::Function::Create(type,llvm::GlobalValue::InternalLinkage,"tailcc_leaf",*module)};
        auto* const caller{llvm::Function::Create(type,llvm::GlobalValue::InternalLinkage,"tailcc_caller",*module)};
        auto* const effect{llvm::Function::Create(llvm::FunctionType::get(ir.getVoidTy(),{i32},false),
            llvm::GlobalValue::InternalLinkage,"tailcc_void_return",*module)};
        for(auto* const function : {leaf,caller,effect})
        {
            function->setCallingConv(llvm::CallingConv::Tail);
            function->addFnAttr(llvm::Attribute::NoInline);
            function->addFnAttr("disable-tail-calls","true");
        }
        ir.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",leaf));
        ir.CreateRet(ir.CreateAdd(ir.CreateMul(leaf->getArg(0u),ir.getInt32(17u)),ir.getInt32(23u)));
        ir.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",effect));
        ir.CreateStore(effect->getArg(0u),slot)->setVolatile(true);
        ir.CreateRetVoid();
        ir.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",caller));
        auto* const value{ir.CreateCall(leaf,{caller->getArg(0u)})};
        value->setCallingConv(llvm::CallingConv::Tail);
        value->setTailCallKind(llvm::CallInst::TCK_NoTail);
        ir.CreateRet(ir.CreateXor(value,ir.getInt32(UINT32_C(0x13579bdf))));
        auto* const entry{llvm::Function::Create(type,llvm::GlobalValue::ExternalLinkage,"tailcc_c_entry",*module)};
        ir.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",entry));
        auto* const result{ir.CreateCall(caller,{entry->getArg(0u)})};
        result->setCallingConv(llvm::CallingConv::Tail);
        result->setTailCallKind(llvm::CallInst::TCK_NoTail);
        auto* const side{ir.CreateCall(effect,{result})};
        side->setCallingConv(llvm::CallingConv::Tail);
        side->setTailCallKind(llvm::CallInst::TCK_NoTail);
        auto* const load{ir.CreateLoad(i32,slot)};load->setVolatile(true);
        ir.CreateRet(ir.CreateAdd(result,load));
        CHECK(!llvm::verifyModule(*module));
        llvm::EngineBuilder builder{std::move(module)};
        auto engine{std::unique_ptr<llvm::ExecutionEngine>{builder.setEngineKind(llvm::EngineKind::JIT)
            .setRelocationModel(llvm::Reloc::Static).setOptLevel(level)
            .setMCPU(uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_abi_host_cpu_name(target,llvm::sys::getHostCPUName()))
            .setMCJITMemoryManager(std::make_unique<uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()).create()}};
        CHECK(engine);engine->finalizeObject();CHECK(!engine->hasError());
        auto const address{engine->getFunctionAddress("tailcc_c_entry")};CHECK(address);
        using invoke = std::uint32_t (*)(std::uint32_t);
        for(auto const input : {UINT32_C(0),UINT32_C(1),UINT32_C(0x80000000),UINT32_C(0xffffffff)})
        {
            auto const expected{((input * 17u + 23u) ^ UINT32_C(0x13579bdf)) * 2u};
            CHECK(reinterpret_cast<invoke>(static_cast<std::uintptr_t>(address))(input) == expected);
        }
        fast_io::io::println("PASS actual ARM TailCC normal integer/void calls and returns optimization=",
            fast_io::mnp::os_c_str(level == llvm::CodeGenOptLevel::None ? "O0-FastISel-fallback" : "optimized-SelectionDAG"),
            " exact-numeric-results=true C-entry=true");
    }
    return 0;
}

int main(int argc,char** argv)
{
    CHECK(argc <= 2);
    CHECK(!llvm::InitializeNativeTarget()); CHECK(!llvm::InitializeNativeTargetAsmPrinter()); CHECK(!llvm::InitializeNativeTargetAsmParser());
    llvm::LLVMContext context{}; llvm::IRBuilder<> ir{context};
    auto module{std::make_unique<llvm::Module>("native-musttail",context)};
    auto* const i32{ir.getInt32Ty()};
    auto* const witness_type{llvm::FunctionType::get(ir.getVoidTy(),{i32},false)};
    auto* const witness{llvm::Function::Create(witness_type,llvm::GlobalValue::ExternalLinkage,"uwvm_musttail_stack_witness",*module)};
    auto const target{llvm::Triple{llvm::sys::getProcessTriple()}};
    CHECK(arm_tailcc_normal_call_oracle(target) == 0);
    // PPC32 includes arguments beyond the eight integer registers. V9 keeps
    // this proof within its qualified register-only sibling-call ABI.
    unsigned const numeric_arguments{target.isPPC() || target.isMIPS() ? 10u : 2u};
    llvm::SmallVector<llvm::Type*,12u> parameters(numeric_arguments + 1u,i32);
    auto* const type{llvm::FunctionType::get(i32,parameters,false)};
    llvm::Function* entries[2]{};
    for(unsigned mode{}; mode != 2u; ++mode)
    {
        llvm::Function* pair[2]{};
        for(unsigned side{}; side != 2u; ++side)
        {
            auto const name{fast_io::concat_std("tail_",mode,"_",side)};
            pair[side] = llvm::Function::Create(type,llvm::GlobalValue::ExternalLinkage,name,*module);
            pair[side]->setSection(fast_io::concat_std(".text.",name));
            // These exact MCJIT-owned definitions cannot be interposed. MIPS
            // must still reject a foreign/preemptable callee's unknown GP ABI.
            if(target.isMIPS() || target.getArch() == llvm::Triple::ppc64 || target.getArch() == llvm::Triple::ppc64le)
            { pair[side]->setDSOLocal(true); }
            if(target.getArch() == llvm::Triple::ppc64 || target.getArch() == llvm::Triple::ppc64le)
            { pair[side]->addFnAttr("uwvm-ppc64-toc-tail", "1"); }
            pair[side]->addFnAttr(llvm::Attribute::NoInline);
            pair[side]->addFnAttr("disable-tail-calls","true");
        }
        entries[mode] = pair[0];
        for(unsigned side{}; side != 2u; ++side)
        {
            auto* const function{pair[side]}; auto* const other{pair[1u-side]};
            auto* const entry{llvm::BasicBlock::Create(context,"entry",function)};
            auto* const done{llvm::BasicBlock::Create(context,"done",function)};
            auto* const again{llvm::BasicBlock::Create(context,"again",function)};
            ir.SetInsertPoint(entry); ir.CreateCall(witness,{function->getArg(0u)});
            ir.CreateCondBr(ir.CreateICmpEQ(function->getArg(0u),ir.getInt32(0u)),done,again);
            ir.SetInsertPoint(done); llvm::Value* result{ir.getInt32(0u)};
            for(unsigned i{1u}; i <= numeric_arguments; ++i)
            { result = ir.CreateAdd(result,ir.CreateMul(function->getArg(i),ir.getInt32(i))); }
            ir.CreateRet(result); ir.SetInsertPoint(again);
            llvm::SmallVector<llvm::Value*,12u> args{ir.CreateSub(function->getArg(0u),ir.getInt32(1u))};
            for(unsigned i{1u}; i <= numeric_arguments; ++i)
            { args.push_back(ir.CreateAdd(function->getArg(i),ir.getInt32(i + 1u))); }
            llvm::Value* selected{other};
            if(mode == 1u)
            {
                // A mutable global prevents folding the actual indirect ABI.
                auto* const slot{new llvm::GlobalVariable(*module,other->getType(),false,llvm::GlobalValue::ExternalLinkage,other,
                    fast_io::concat_std("tail_pointer_",side))};
                auto* const load{ir.CreateLoad(other->getType(),slot)}; load->setVolatile(true); selected = load;
            }
            auto* const call{ir.CreateCall(type,selected,args)};
            call->setTailCallKind(llvm::CallInst::TCK_MustTail); ir.CreateRet(call);
        }
    }
    CHECK(!llvm::verifyModule(*module));
    llvm::sys::DynamicLibrary::AddSymbol("uwvm_musttail_stack_witness",reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(&uwvm_musttail_stack_witness)));
    auto foreign_module{llvm::CloneModule(*module)};
    llvm::EngineBuilder engine_builder{std::move(module)};
    if(!target.isPPC64()) { engine_builder.setRelocationModel(llvm::Reloc::Static); }
    uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_configure_code_model(engine_builder,target);
    llvm::SmallVector<std::string,2u> attributes{};
    // Match the real runtime's static MIPS ABI. Far-call relocator stubs do
    // not establish the PIC host witness's t9/GP; full-width register calls do.
    if(target.isMIPS()) { attributes.emplace_back("+noabicalls"); attributes.emplace_back("+long-calls"); }
    engine_builder.setMAttrs(attributes);
    auto engine{std::unique_ptr<llvm::ExecutionEngine>{engine_builder
        .setEngineKind(llvm::EngineKind::JIT)
        .setMCPU(uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_abi_host_cpu_name(target,llvm::sys::getHostCPUName()))
        .setOptLevel(llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(std::make_unique<uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()).create()}};
    CHECK(engine); owned_object_witness first{};first.directory=argc == 2 ? argv[1] : nullptr;first.generation=1u;
    engine->RegisterJITEventListener(&first);
    if(argc == 2) { fast_io::io::perrln("PRIVATE phase first finalize begin"); }
    engine->finalizeObject();
    if(argc == 2) { fast_io::io::perrln("PRIVATE phase first finalize complete"); }
    engine->UnregisterJITEventListener(&first);CHECK(!engine->hasError());
    llvm::EngineBuilder foreign_builder{std::move(foreign_module)};
    if(!target.isPPC64()) { foreign_builder.setRelocationModel(llvm::Reloc::Static); }
    uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_configure_code_model(foreign_builder,target);
    auto foreign{std::unique_ptr<llvm::ExecutionEngine>{foreign_builder
        .setMAttrs(attributes).setEngineKind(llvm::EngineKind::JIT)
        .setMCPU(uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_abi_host_cpu_name(target,llvm::sys::getHostCPUName()))
        .setOptLevel(llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(std::make_unique<uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()).create()}};
    CHECK(foreign);owned_object_witness second{};second.directory=first.directory;second.generation=2u;
    foreign->RegisterJITEventListener(&second);
    if(argc == 2) { fast_io::io::perrln("PRIVATE phase replacement finalize begin"); }
    foreign->finalizeObject();
    if(argc == 2) { fast_io::io::perrln("PRIVATE phase replacement finalize complete"); }
    foreign->UnregisterJITEventListener(&second);CHECK(!foreign->hasError());
    constexpr std::uint32_t iterations{4096u};
    for(unsigned mode{}; mode != 3u; ++mode)
    {
        if(mode == 2u)
        {
            // Real replacement between distinct MCJIT allocations/TOCs. Only
            // this private test's owned writable numeric call slots are used;
            // no debugger memory/register capability is introduced.
            auto const first_slot{engine->getGlobalValueAddress("tail_pointer_0")};
            auto const second_slot{foreign->getGlobalValueAddress("tail_pointer_1")};
            auto const first_target{foreign->getFunctionAddress("tail_1_1")};
            auto const second_target{engine->getFunctionAddress("tail_1_0")};
            CHECK(first_slot && second_slot && first_target && second_target);
            CHECK(first_target != engine->getFunctionAddress("tail_1_1"));
            *reinterpret_cast<std::uintptr_t*>(static_cast<std::uintptr_t>(first_slot)) = static_cast<std::uintptr_t>(first_target);
            *reinterpret_cast<std::uintptr_t*>(static_cast<std::uintptr_t>(second_slot)) = static_cast<std::uintptr_t>(second_target);
        }
        auto const name{fast_io::concat_std("tail_",mode == 2u ? 1u : mode,"_0")};
        if(argc == 2) { fast_io::io::perrln("PRIVATE phase entry lookup begin mode=",mode); }
        auto const address{engine->getFunctionAddress(name)}; CHECK(address);
        if(argc == 2) { fast_io::io::perrln("PRIVATE phase entry lookup complete mode=",mode); }
        low = high = 0u; calls = 0u; next_count = iterations; sequence_valid = true;
        std::uint32_t expected{}, actual{};
        for(unsigned i{1u}; i <= numeric_arguments; ++i)
        { expected += (UINT32_C(0x80000000) + i + iterations * (i + 1u)) * i; }
        fast_io::io::perrln("START actual C musttail mode=",mode," numeric-arguments=",numeric_arguments);
        if(numeric_arguments == 10u)
        {
            using invoke = std::uint32_t (*)(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,
                std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
            actual = reinterpret_cast<invoke>(static_cast<std::uintptr_t>(address))(iterations,0x80000001u,0x80000002u,0x80000003u,
                0x80000004u,0x80000005u,0x80000006u,0x80000007u,0x80000008u,0x80000009u,0x8000000au);
        }
        else
        {
            using invoke = std::uint32_t (*)(std::uint32_t,std::uint32_t,std::uint32_t);
            actual = reinterpret_cast<invoke>(static_cast<std::uintptr_t>(address))(iterations,0x80000001u,0x80000002u);
        }
        CHECK(actual == expected); CHECK(sequence_valid && calls == iterations + 1u);
        CHECK(high >= low && high - low <= 65536u);
        fast_io::io::println("PASS actual C musttail mode=",mode," numeric-arguments=",numeric_arguments,
            " iterations=",iterations," bounded-private-stack=true exact-results=true cross-engine=",mode == 2u);
    }
    engine.reset();
}
