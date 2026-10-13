// Actual AArch64 TailCC object + production manager + relocated registered CFI.
// A host may emit this object. No AArch64 code, live stack or finish executes.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/TargetSelect.h>
#include <fast_io.h>
#include <array>
#include <cstring>
#include <memory>
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("AArch64 registered CFI failure line=", __LINE__); return 1; } } while(false)
struct symbols final : ::llvm::JITEventListener
{
    struct extent { ::std::uintptr_t begin{},end{}; };
    ::std::array<extent,4u> actual{}; bool valid{true};
    void notifyObjectLoaded(ObjectKey,::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        if(object.getArch()!=::llvm::Triple::aarch64 || object.getBytesInAddress()!=8u || !object.isLittleEndian())
        { valid=false; return; }
        for(auto const& [symbol,size] : ::llvm::object::computeSymbolSizes(object))
        {
            auto name{symbol.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); valid=false; return; }
            unsigned index{};
            if(*name=="uwvm_cfi_leaf") { index=0u; }
            else if(*name=="uwvm_cfi_caller") { index=1u; }
            else if(*name=="uwvm_cfi_expression") { index=2u; }
            else if(*name=="uwvm_cfi_signed") { index=3u; }
            else { continue; }
            auto address{symbol.getAddress()}; auto section{symbol.getSection()};
            if(!address || !section)
            {
                if(!address) { ::llvm::consumeError(address.takeError()); }
                if(!section) { ::llvm::consumeError(section.takeError()); }
                valid=false; return;
            }
            if(*section==object.section_end() || size==0u || *address<(*section)->getAddress())
            { valid=false; return; }
            auto const base{loaded.getSectionLoadAddress(**section)}, offset{*address-(*section)->getAddress()};
            constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
            if(base==0u || base>limit || offset>limit-base || size>limit-base-offset || actual[index].begin!=0u)
            { valid=false; return; }
            actual[index]={static_cast<::std::uintptr_t>(base+offset),static_cast<::std::uintptr_t>(base+offset+size)};
        }
    }
};
int main(int argc,char const* const* argv)
{
    unsigned optimization{};
    CHECK(argc==1 || argc==2);
    if(argc==2)
    {
        auto const end{argv[1]+::std::strlen(argv[1])};
        auto const parsed{::fast_io::parse_by_scan(argv[1],end,::fast_io::mnp::dec_get<true,true>(optimization))};
        CHECK(parsed.code==::fast_io::parse_code::ok && parsed.iter==end);
    }
    CHECK(optimization==0u || optimization==3u);
    ::LLVMInitializeAArch64TargetInfo(); ::LLVMInitializeAArch64Target();
    ::LLVMInitializeAArch64TargetMC(); ::LLVMInitializeAArch64AsmPrinter(); ::LLVMInitializeAArch64AsmParser();
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("aarch64_tailcc_registered_cfi",context)};
    module->setTargetTriple(::llvm::Triple{"aarch64-unknown-linux-gnu"});
    auto i64{::llvm::Type::getInt64Ty(context)};
    ::std::array<::llvm::Type*,64u> types{}; types.fill(i64);
    auto type{::llvm::FunctionType::get(i64,types,false)};
    auto leaf{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_leaf",*module)};
    auto caller{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_caller",*module)};
    auto expression{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_expression",*module)};
    auto signed_ra{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_signed",*module)};
    for(auto f:{leaf,caller,expression,signed_ra})
    {
        f->setUWTableKind(::llvm::UWTableKind::Async); f->setCallingConv(::llvm::CallingConv::Tail);
        f->addFnAttr(::llvm::Attribute::NoInline); f->addFnAttr("frame-pointer","all");
    }
    ::llvm::IRBuilder<> builder{::llvm::BasicBlock::Create(context,"entry",leaf)};
    builder.CreateRet(builder.CreateAdd(leaf->getArg(63u),builder.getInt64(1u)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",caller));
    auto slot{builder.CreateAlloca(i64,builder.getInt32(128u))}; builder.CreateStore(caller->getArg(0u),slot,true);
    ::std::array<::llvm::Value*,64u> args{};
    for(unsigned n{};n!=64u;++n) { args[n]=caller->getArg(n); }
    auto call{builder.CreateCall(leaf,args)}; call->setCallingConv(::llvm::CallingConv::Tail);
    builder.CreateRet(builder.CreateAdd(call,builder.CreateLoad(i64,slot,true)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",expression));
    auto assembly{::llvm::InlineAsm::get(::llvm::FunctionType::get(builder.getVoidTy(),false),
        ".cfi_escape 0x10, 0x1e, 0x04, 0x9c, 0x11, 0x78, 0x22\n\tnop","",true)};
    builder.CreateCall(assembly); builder.CreateRet(expression->getArg(0u));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",signed_ra));
    auto signed_assembly{::llvm::InlineAsm::get(::llvm::FunctionType::get(builder.getVoidTy(),false),
        ".cfi_escape 0x2d\n\tnop\n\t.cfi_escape 0x2d\n\tnop","",true)};
    builder.CreateCall(signed_assembly); builder.CreateRet(signed_ra->getArg(0u));
    CHECK(!::llvm::verifyModule(*module));
    auto manager{::std::make_unique<cfi::runtime_llvm_jit_section_memory_manager>()};
    auto observer{manager.get()}; symbols loaded{};
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder(::std::move(module))
        .setEngineKind(::llvm::EngineKind::JIT).setMArch("aarch64").setMCPU("generic")
        .setOptLevel(optimization==3u ? ::llvm::CodeGenOptLevel::Aggressive : ::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(manager)).create()}};
    CHECK(engine); engine->RegisterJITEventListener(&loaded); engine->finalizeObject();
    CHECK(!observer->has_finalization_failure() && loaded.valid);
    unsigned full{},scalar_only{},expressions{},signed_states{}; bool fp_cfa{};
    for(unsigned n{};n!=4u;++n)
    {
        auto const f{loaded.actual[n]}; CHECK(f.end>f.begin && (f.begin&3u)==0u);
        cfi::native_debug_cfa_row entry{},epilogue{};
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,entry));
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.end-1u,epilogue));
        for(auto pc{f.begin};pc!=f.end;++pc)
        {
            cfi::native_debug_cfa_row scalar{};
            CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,pc,scalar) && scalar.usable &&
                  (scalar.cfa_register==31u || scalar.cfa_register==29u));
            fp_cfa=fp_cfa || scalar.cfa_register==29u;
            cfi::native_debug_cfi_row row{};
            if(observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row))
            {
                ++full; CHECK(row.usable && row.cfa_usable &&
                    row.registers[30u].kind!=cfi::native_debug_cfi_rule_kind::unavailable);
            }
            else
            {
                ++scalar_only; CHECK(!row.usable && row.begin==0u);
                if(n==2u) { ++expressions; }
                if(n==3u) { ++signed_states; }
            }
        }
        ::fast_io::io::println("actual AArch64 TailCC function=",n,
            " entry-cfa-register=",entry.cfa_register," entry-cfa-offset=",entry.cfa_offset,
            " return-cfa-register=",epilogue.cfa_register," return-cfa-offset=",epilogue.cfa_offset);
        CHECK(!observer->copy_debug_native_cfa_row(f.begin-1u,f.end,f.begin,epilogue));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end+1u,f.begin,epilogue));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.end,epilogue));
    }
    CHECK(full!=0u && scalar_only!=0u && expressions!=0u && signed_states!=0u && fp_cfa);
    observer->deregisterEHFrames();
    for(auto const f:loaded.actual)
    { cfi::native_debug_cfa_row row{}; CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,row)); }
    engine->UnregisterJITEventListener(&loaded);
    ::fast_io::io::println("actual AArch64 TailCC registered CFI: PASS optimization=",optimization,
        " full-rule-bytes=",full," cfa-only-bytes=",scalar_only," signed-return-refused-bytes=",signed_states,
        " actual-AArch64-execution=false live-caller-qualified=false native-finish-qualified=false");
}

