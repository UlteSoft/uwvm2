// Actual i386 TailCC object + production manager + relocated registered CFI.
// On a 64-bit host RuntimeDyld maps target sections to distinct 32-bit labels.
// These labels are metadata DATA; no i386 code, live stack or finish executes.
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
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("i386 registered CFI failure line=", __LINE__); return 1; } } while(false)
struct symbols final : ::llvm::JITEventListener
{
    struct extent { ::std::uintptr_t begin{},end{}; };
    ::std::array<extent,3u> actual{}; bool valid{true};
    ::llvm::ExecutionEngine* engine{};
    void notifyObjectLoaded(ObjectKey,::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        if(object.getArch()!=::llvm::Triple::x86 || object.getBytesInAddress()!=4u || !object.isLittleEndian())
        { valid=false; return; }
        if constexpr(sizeof(void*)>4u)
        {
            // A real 32-bit target relocation without truncating host pointers.
            // Every section has its own non-overlapping bounded target label.
            unsigned index{};
            for(auto const& section: object.sections())
            {
                auto const local{loaded.getSectionLoadAddress(section)};
                if(local==0u) { continue; }
                if(!engine || ++index>128u || section.getSize()>0x10000u) { valid=false; return; }
                engine->mapSectionAddress(reinterpret_cast<void*>(local),0x10000000u+index*0x10000u);
            }
        }
        for(auto const& [symbol,size] : ::llvm::object::computeSymbolSizes(object))
        {
            auto name{symbol.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); valid=false; return; }
            unsigned index{};
            if(*name=="uwvm_cfi_leaf") { index=0u; }
            else if(*name=="uwvm_cfi_caller") { index=1u; }
            else if(*name=="uwvm_cfi_expression") { index=2u; }
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
    ::LLVMInitializeX86TargetInfo(); ::LLVMInitializeX86Target();
    ::LLVMInitializeX86TargetMC(); ::LLVMInitializeX86AsmPrinter(); ::LLVMInitializeX86AsmParser();
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("i386_tailcc_registered_cfi",context)};
    module->setTargetTriple(::llvm::Triple{"i386-unknown-linux-gnu"});
    auto i32{::llvm::Type::getInt32Ty(context)};
    ::std::array<::llvm::Type*,64u> types{}; types.fill(i32);
    auto type{::llvm::FunctionType::get(i32,types,false)};
    auto leaf{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_leaf",*module)};
    auto caller{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_caller",*module)};
    auto expression{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_expression",*module)};
    for(auto f:{leaf,caller,expression})
    {
        f->setUWTableKind(::llvm::UWTableKind::Async); f->setCallingConv(::llvm::CallingConv::Tail);
        f->addFnAttr(::llvm::Attribute::NoInline); f->addFnAttr("frame-pointer","all");
    }
    ::llvm::IRBuilder<> builder{::llvm::BasicBlock::Create(context,"entry",leaf)};
    builder.CreateRet(builder.CreateAdd(leaf->getArg(63u),builder.getInt32(1u)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",caller));
    auto slot{builder.CreateAlloca(i32,builder.getInt32(128u))}; builder.CreateStore(caller->getArg(0u),slot,true);
    ::std::array<::llvm::Value*,64u> args{};
    for(unsigned n{};n!=64u;++n) { args[n]=caller->getArg(n); }
    auto call{builder.CreateCall(leaf,args)}; call->setCallingConv(::llvm::CallingConv::Tail);
    builder.CreateRet(builder.CreateAdd(call,builder.CreateLoad(i32,slot,true)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",expression));
    auto assembly{::llvm::InlineAsm::get(::llvm::FunctionType::get(builder.getVoidTy(),false),
        ".cfi_escape 0x10, 0x08, 0x04, 0x9c, 0x11, 0x7c, 0x22\n\tnop","",true)};
    builder.CreateCall(assembly); builder.CreateRet(expression->getArg(0u));
    CHECK(!::llvm::verifyModule(*module));
    auto manager{::std::make_unique<cfi::runtime_llvm_jit_section_memory_manager>()};
    auto observer{manager.get()}; symbols loaded{};
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder(::std::move(module))
        .setEngineKind(::llvm::EngineKind::JIT).setMArch("x86").setMCPU("i686")
        .setOptLevel(optimization==3u ? ::llvm::CodeGenOptLevel::Aggressive : ::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(manager)).create()}};
    CHECK(engine); loaded.engine=engine.get(); engine->RegisterJITEventListener(&loaded); engine->finalizeObject();
    CHECK(!observer->has_finalization_failure() && loaded.valid);
    unsigned full{},scalar_only{},expressions{},unavailable_slots{}; bool fp_cfa{};
    for(unsigned n{};n!=3u;++n)
    {
        auto const f{loaded.actual[n]}; CHECK(f.end>f.begin && f.end<=UINT32_MAX);
        cfi::native_debug_cfa_row entry{},epilogue{};
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,entry));
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.end-1u,epilogue));
        cfi::native_debug_cfi_row first{};
        CHECK(observer->copy_debug_native_cfi_row(f.begin,f.end,f.begin,first) &&
            first.cfa_register==4u && first.cfa_offset==4 &&
            first.registers[8u].kind==cfi::native_debug_cfi_rule_kind::cfa_memory && first.registers[8u].offset==-4);
        ::std::array<::std::uint64_t,9u> registers{}; registers[4u]=0x1000u;
        cfi::native_debug_cfi_i386_owned_word word{0x1000u,0x12345678u};
        cfi::native_debug_cfi_i386_caller recovered{};
        CHECK(cfi::evaluate_native_debug_cfi_i386_sparse(first,registers,1u<<4u,0x1000u,0x1004u,{&word,1u},recovered));
        CHECK(recovered.return_pc==0x12345678u && recovered.cfa==0x1004u && recovered.known==((1u<<4u)|(1u<<8u)));
        for(auto pc{f.begin};pc!=f.end;++pc)
        {
            cfi::native_debug_cfa_row scalar{};
            CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,pc,scalar) && scalar.usable &&
                  (scalar.cfa_register==4u || scalar.cfa_register==5u));
            fp_cfa=fp_cfa || scalar.cfa_register==5u;
            cfi::native_debug_cfi_row row{};
            if(observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row))
            {
                ++full; CHECK(row.usable && row.cfa_usable &&
                    row.registers[8u].kind==cfi::native_debug_cfi_rule_kind::cfa_memory && row.registers[8u].offset==-4);
                // Every emitted prologue/body/epilogue row is evaluated from
                // owned 32-bit DATA, never a guessed live frame or host stack.
                registers[5u]=0x1020u;
                auto const cfa_value{static_cast<::std::int64_t>(registers[row.cfa_register])+row.cfa_offset};
                CHECK(cfa_value>0x1000 && cfa_value<0x10000);
                ::std::array<cfi::native_debug_cfi_i386_owned_word,5u> slots{}; unsigned count{};
                for(unsigned reg : {3u,5u,6u,7u,8u})
                {
                    auto const& rule{row.registers[reg]};
                    if(rule.kind!=cfi::native_debug_cfi_rule_kind::cfa_memory) { continue; }
                    auto const address{cfa_value+rule.offset};
                    if(address<0x1000 || address+4>cfa_value)
                    {
                        // LLVM can retain a saved-register rule after its
                        // slot moved below SP in the epilogue. It is unknown,
                        // never permission to read the retired stack slot.
                        CHECK(reg!=8u); ++unavailable_slots; continue;
                    }
                    slots[count++]={static_cast<::std::uintptr_t>(address),reg==8u ? 0x12345678u : reg+100u};
                }
                CHECK(cfi::evaluate_native_debug_cfi_i386_sparse(row,registers,(1u<<4u)|(1u<<5u),
                    0x1000u,static_cast<::std::uintptr_t>(cfa_value),{slots.data(),count},recovered));
                CHECK(recovered.return_pc==0x12345678u && recovered.cfa==static_cast<::std::uintptr_t>(cfa_value));
                CHECK(!cfi::evaluate_native_debug_cfi_i386_sparse(row,registers,(1u<<4u)|(1u<<5u),
                    0x1000u,static_cast<::std::uintptr_t>(cfa_value)-1u,{slots.data(),count},recovered) && !recovered.known);
            }
            else
            {
                ++scalar_only; CHECK(!row.usable && row.begin==0u);
                if(n==2u) { ++expressions; }
            }
        }
        ::fast_io::io::println("actual i386 TailCC function=",n,
            " entry-cfa-register=",entry.cfa_register," entry-cfa-offset=",entry.cfa_offset,
            " return-cfa-register=",epilogue.cfa_register," return-cfa-offset=",epilogue.cfa_offset);
        CHECK(!observer->copy_debug_native_cfa_row(f.begin-1u,f.end,f.begin,epilogue));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end+1u,f.begin,epilogue));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.end,epilogue));
    }
    CHECK(full!=0u && scalar_only!=0u && expressions!=0u && fp_cfa);
    observer->deregisterEHFrames();
    for(auto const f:loaded.actual)
    { cfi::native_debug_cfa_row row{}; CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,row)); }
    engine->UnregisterJITEventListener(&loaded);
    ::fast_io::io::println("actual i386 TailCC registered CFI: PASS optimization=",optimization,
        " full-rule-bytes=",full," cfa-only-bytes=",scalar_only," unavailable-preserved-slots=",unavailable_slots,
        " actual-i386-execution=false live-caller-qualified=false native-finish-qualified=false");
}
