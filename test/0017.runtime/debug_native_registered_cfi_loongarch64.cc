// Actual LoongArch C-ABI object + production manager + relocated registered CFI.
// Native LoongArch executes a genuine C->C-ABI shim; no Wasm issuer or finish executes.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
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
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("LoongArch registered CFI failure line=", __LINE__); return 1; } } while(false)
struct symbols final : ::llvm::JITEventListener
{
    struct extent { ::std::uintptr_t begin{},end{}; };
    ::std::array<extent,4u> actual{}; ::std::uintptr_t body_marker{}; bool valid{true};
    void notifyObjectLoaded(ObjectKey,::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        if(object.getArch()!=::llvm::Triple::loongarch64 || object.getBytesInAddress()!=8u || !object.isLittleEndian())
        { valid=false; return; }
        for(auto const& [symbol,size] : ::llvm::object::computeSymbolSizes(object))
        {
            auto name{symbol.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); valid=false; return; }
            unsigned index{};
            if(*name=="uwvm_cfi_leaf") { index=0u; }
            else if(*name=="uwvm_cfi_caller") { index=1u; }
            else if(*name=="uwvm_cfi_expression") { index=2u; }
            else if(*name=="uwvm_cfi_branch") { index=3u; }
            else if(*name=="uwvm_cfi_branch_body") { index=4u; }
            else { continue; }
            auto address{symbol.getAddress()}; auto section{symbol.getSection()};
            if(!address || !section)
            {
                if(!address) { ::llvm::consumeError(address.takeError()); }
                if(!section) { ::llvm::consumeError(section.takeError()); }
                valid=false; return;
            }
            if(*section==object.section_end() || (index!=4u && size==0u) || *address<(*section)->getAddress())
            { valid=false; return; }
            auto const base{loaded.getSectionLoadAddress(**section)}, offset{*address-(*section)->getAddress()};
            constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
            if(base==0u || base>limit || offset>limit-base || size>limit-base-offset)
            { valid=false; return; }
            if(index==4u)
            {
                // A zero-sized NOTYPE label is a scalar test observation,
                // never a function extent or native-read permission.
                if(body_marker!=0u || offset>=(*section)->getSize()) { valid=false; return; }
                body_marker=static_cast<::std::uintptr_t>(base+offset); continue;
            }
            if(actual[index].begin!=0u) { valid=false; return; }
            actual[index]={static_cast<::std::uintptr_t>(base+offset),static_cast<::std::uintptr_t>(base+offset+size)};
        }
    }
};
int main(int argc,char const* const* argv)
{
    unsigned optimization{};
    CHECK(argc>=1 && argc<=3);
    bool const legacy{argc==3 && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}=="legacy-layout"};
    CHECK(argc!=3 || legacy);
    if(argc>=2)
    {
        auto const end{argv[1]+::std::strlen(argv[1])};
        auto const parsed{::fast_io::parse_by_scan(argv[1],end,::fast_io::mnp::dec_get<true,true>(optimization))};
        CHECK(parsed.code==::fast_io::parse_code::ok && parsed.iter==end);
    }
    CHECK(optimization==0u || optimization==3u);
    CHECK(!legacy || optimization==0u);
    ::LLVMInitializeLoongArchTargetInfo(); ::LLVMInitializeLoongArchTarget();
    ::LLVMInitializeLoongArchTargetMC(); ::LLVMInitializeLoongArchAsmPrinter(); ::LLVMInitializeLoongArchAsmParser();
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("loongarch64_c_abi_registered_cfi",context)};
    module->setTargetTriple(::llvm::Triple{"loongarch64-unknown-linux-gnu"});
    auto i64{::llvm::Type::getInt64Ty(context)};
    ::std::array<::llvm::Type*,64u> types{}; types.fill(i64);
    auto type{::llvm::FunctionType::get(i64,types,false)};
    auto leaf{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_leaf",*module)};
    auto caller{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_caller",*module)};
    auto expression{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_expression",*module)};
    for(auto f:{leaf,caller,expression})
    {
        f->setUWTableKind(::llvm::UWTableKind::Async); f->setCallingConv(::llvm::CallingConv::C);
        f->addFnAttr(::llvm::Attribute::NoInline); f->addFnAttr("frame-pointer","all");
    }
    ::llvm::IRBuilder<> builder{::llvm::BasicBlock::Create(context,"entry",leaf)};
    builder.CreateRet(builder.CreateAdd(leaf->getArg(63u),builder.getInt64(1u)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",caller));
    auto slot{builder.CreateAlloca(i64,builder.getInt32(128u))}; builder.CreateStore(caller->getArg(0u),slot,true);
    ::std::array<::llvm::Value*,64u> args{};
    for(unsigned n{};n!=64u;++n) { args[n]=caller->getArg(n); }
    auto call{builder.CreateCall(leaf,args)}; call->setCallingConv(::llvm::CallingConv::C);
    builder.CreateRet(builder.CreateAdd(call,builder.CreateLoad(i64,slot,true)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",expression));
    auto assembly{::llvm::InlineAsm::get(::llvm::FunctionType::get(builder.getVoidTy(),false),
        ".cfi_escape 0x10, 0x01, 0x04, 0x9c, 0x11, 0x78, 0x22\n\tnop","",true)};
    builder.CreateCall(assembly); builder.CreateRet(expression->getArg(0u));
    // A return block precedes another live framed block in object layout.
    // This catches an epilogue row leaking into later calls when MCJIT omits
    // CFI fixup. The label observes actual emitted TEXT; it grants no issuer.
    auto branch{::llvm::Function::Create(::llvm::FunctionType::get(i64,{i64},false),
        ::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_branch",*module)};
    branch->setUWTableKind(::llvm::UWTableKind::Async); branch->setCallingConv(::llvm::CallingConv::C);
    branch->addFnAttr(::llvm::Attribute::NoInline); branch->addFnAttr("frame-pointer","all");
    auto start{::llvm::BasicBlock::Create(context,"entry",branch)};
    auto early{::llvm::BasicBlock::Create(context,"early.return",branch)};
    auto later{::llvm::BasicBlock::Create(context,"later.body",branch)};
    builder.SetInsertPoint(start);
    auto branch_slot{builder.CreateAlloca(i64)}; builder.CreateStore(branch->getArg(0u),branch_slot,true);
    builder.CreateCondBr(builder.CreateICmpEQ(branch->getArg(0u),builder.getInt64(0u)),early,later);
    builder.SetInsertPoint(early); builder.CreateRet(builder.CreateLoad(i64,branch_slot,true));
    builder.SetInsertPoint(later);
    auto marker{::llvm::InlineAsm::get(::llvm::FunctionType::get(builder.getVoidTy(),false),
        ".globl uwvm_cfi_branch_body\nuwvm_cfi_branch_body:","",true)};
    builder.CreateCall(marker);
    for(unsigned n{};n!=64u;++n) { args[n]=builder.getInt64(n+1u); }
    auto branch_call{builder.CreateCall(leaf,args)}; branch_call->setCallingConv(::llvm::CallingConv::C);
    branch_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
    builder.CreateRet(builder.CreateAdd(branch_call,builder.CreateLoad(i64,branch_slot,true)));
    // LoongArch currently uses C for generated Wasm typed entries, as
    // get_llvm_jit_typed_calling_conv in single_func_emit.h requires. A real
    // no-argument C shim exercises its 64-argument stack ABI on this target.
    auto shim{::llvm::Function::Create(::llvm::FunctionType::get(i64,false),
        ::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_execute",*module)};
    shim->setCallingConv(::llvm::CallingConv::C);
    shim->setUWTableKind(::llvm::UWTableKind::Async);
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",shim));
    for(unsigned n{};n!=64u;++n) { args[n]=builder.getInt64(n+1u); }
    auto executed{builder.CreateCall(caller,args)}; executed->setCallingConv(::llvm::CallingConv::C);
    builder.CreateRet(executed);
    CHECK(!::llvm::verifyModule(*module));
    auto manager{::std::make_unique<cfi::runtime_llvm_jit_section_memory_manager>()};
    auto observer{manager.get()}; symbols loaded{};
    ::llvm::EngineBuilder engine_builder{::std::move(module)};
    engine_builder.setEngineKind(::llvm::EngineKind::JIT).setMArch("loongarch64").setMCPU("generic")
        .setOptLevel(optimization==3u ? ::llvm::CodeGenOptLevel::Aggressive : ::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(manager));
    if(!legacy) { cfi::llvm_jit_mcjit_configure_host_unwind_abi(engine_builder); }
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{engine_builder.create()}};
    CHECK(engine); engine->RegisterJITEventListener(&loaded); engine->finalizeObject();
    CHECK(!observer->has_finalization_failure() && loaded.valid);
    unsigned full{},scalar_only{},expressions{}; bool fp_cfa{};
    for(unsigned n{};n!=4u;++n)
    {
        auto const f{loaded.actual[n]}; CHECK(f.end>f.begin && (f.begin&3u)==0u);
        cfi::native_debug_cfa_row entry{},epilogue{};
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,entry));
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.end-1u,epilogue));
        CHECK(epilogue.cfa_register==3u && epilogue.cfa_offset==0);
        for(auto pc{f.begin};pc!=f.end;++pc)
        {
            cfi::native_debug_cfa_row scalar{};
            CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,pc,scalar) && scalar.usable &&
                  (scalar.cfa_register==3u || scalar.cfa_register==22u));
            fp_cfa=fp_cfa || scalar.cfa_register==22u;
            cfi::native_debug_cfi_row row{};
            if(observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row))
            {
                ++full; CHECK(row.usable && row.cfa_usable &&
                    row.registers[1u].kind!=cfi::native_debug_cfi_rule_kind::unavailable);
            }
            else
            {
                ++scalar_only; CHECK(!row.usable && row.begin==0u);
                if(n==2u) { ++expressions; }
            }
        }
        ::fast_io::io::println("actual LoongArch C-ABI function=",n,
            " entry-cfa-register=",entry.cfa_register," entry-cfa-offset=",entry.cfa_offset,
            " return-cfa-register=",epilogue.cfa_register," return-cfa-offset=",epilogue.cfa_offset);
        CHECK(!observer->copy_debug_native_cfa_row(f.begin-1u,f.end,f.begin,epilogue));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end+1u,f.begin,epilogue));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.end,epilogue));
    }
    CHECK(full!=0u && scalar_only!=0u && expressions!=0u && fp_cfa);
    auto const branch_extent{loaded.actual[3u]};
    auto const body_marker{loaded.body_marker};
    CHECK(body_marker>branch_extent.begin && body_marker<branch_extent.end);
    cfi::native_debug_cfi_row body_row{};
    CHECK(observer->copy_debug_native_cfi_row(branch_extent.begin,branch_extent.end,body_marker,body_row));
    struct description
    {
        bool available{true},little_endian{true}; unsigned description_version{1u},pointer_bits{64u};
        ::std::size_t maximum_instruction_bytes{4u},minimum_instruction_alignment{1u};
        ::std::array<char,64u> triple{},cpu{},features{};
        ::std::size_t triple_size{},cpu_size{},features_size{};
    } target{};
    constexpr char triple[]{"loongarch64-unknown-linux-gnu"};
    target.triple_size=sizeof(triple)-1u; ::std::memcpy(target.triple.data(),triple,sizeof(triple));
    ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder decoder{target}; CHECK(decoder);
    unsigned earlier_returns{};
    for(auto pc{branch_extent.begin};pc<body_marker;pc+=4u)
    {
        auto instruction{decoder.decode(pc,{reinterpret_cast<unsigned char const*>(pc),4u})}; CHECK(instruction);
        earlier_returns+=instruction.semantics().kind==::uwvm2::uwvm::debugger::native_instruction_semantics::flow::return_instruction;
    }
    if(optimization==0u) { CHECK(earlier_returns!=0u); }
    bool const framed{body_row.cfa_register==22u &&
        body_row.registers[1u].kind==cfi::native_debug_cfi_rule_kind::cfa_memory};
    CHECK(legacy ? !framed : framed);
    ::fast_io::io::println("actual LoongArch later body: earlier-returns=",earlier_returns,
        " saved-RA-and-FP-CFA=",framed," legacy-layout=",legacy);

#if defined(__loongarch64) && __SIZEOF_POINTER__ == 8
    auto address{engine->getFunctionAddress("uwvm_cfi_execute")}; CHECK(address!=0u);
    auto entry{reinterpret_cast<::std::uint64_t (*)()>(static_cast<::std::uintptr_t>(address))};
    CHECK(entry()==66u);
    address=engine->getFunctionAddress("uwvm_cfi_branch"); CHECK(address!=0u);
    auto branch_entry{reinterpret_cast<::std::uint64_t (*)(::std::uint64_t)>(static_cast<::std::uintptr_t>(address))};
    CHECK(branch_entry(0u)==0u && branch_entry(1u)==66u);
#endif
    observer->deregisterEHFrames();
    for(auto const f:loaded.actual)
    { cfi::native_debug_cfa_row row{}; CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,row)); }
    engine->UnregisterJITEventListener(&loaded);
    ::fast_io::io::println("actual LoongArch C-ABI registered CFI: PASS optimization=",optimization,
        " legacy-layout=",legacy," full-rule-bytes=",full," cfa-only-bytes=",scalar_only,
        " actual-LoongArch-C-ABI-shim-execution=true live-Wasm-caller-qualified=false native-finish-qualified=false");
}

