// Actual MIPS N64 object + production CFI parser + relocated EH metadata.
// A test-only metadata owner captures registration callbacks; foreign EH
// never enters the host unwinder. The object executes separately on MIPS Linux.
// This test never grants a Wasm activation or native caller/finish authority.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/native_debug_cfi.h>
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
#include <fast_io_dsal/string_view.h>
#include <array>
#include <cstring>
#include <memory>
extern "C" { void LLVMInitializeMipsTargetInfo(); void LLVMInitializeMipsTarget();
void LLVMInitializeMipsTargetMC(); void LLVMInitializeMipsAsmPrinter(); void LLVMInitializeMipsAsmParser(); void LLVMInitializeMipsDisassembler(); }
// This friend exists only in this standalone fixture. It grants no debugger
// activation, stack borrow, execution event or host unwind registration.
namespace uwvm2::runtime::compiler::llvm_jit::details {
class runtime_llvm_jit_section_memory_manager final : public ::llvm::SectionMemoryManager {
    native_debug_registered_cfi metadata_{};
public:
    unsigned registrations{};
    using ::llvm::SectionMemoryManager::notifyObjectLoaded;
    void notifyObjectLoaded(::llvm::RuntimeDyld&, ::llvm::object::ObjectFile const& object) override
    { metadata_.observe_object(object.getArch(),object.getBytesInAddress(),object.isLittleEndian()); }
    void registerEHFrames(::std::uint8_t* bytes, ::std::uint64_t address, ::std::size_t size) override
    { metadata_.observe(bytes,address,size); ++registrations; }
    void deregisterEHFrames() override { metadata_.retire(); }
    bool has_finalization_failure() const noexcept { return !metadata_.valid_; }
    bool copy_debug_native_cfi_row(::std::uintptr_t begin,::std::uintptr_t end,::std::uintptr_t pc,native_debug_cfi_row& out) const noexcept
    { return metadata_.copy_row(begin,end,pc,out); }
    bool copy_debug_native_cfa_row(::std::uintptr_t begin,::std::uintptr_t end,::std::uintptr_t pc,native_debug_cfa_row& out) const noexcept
    {
        out={}; native_debug_cfi_row row{};
        if(!metadata_.copy_row(begin,end,pc,row,false) || !row.cfa_usable) { return false; }
        out={row.begin,row.end,row.cfa_register,row.cfa_offset,true}; return true;
    }
};
}
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("Mips registered CFI failure line=", __LINE__); return 1; } } while(false)
struct symbols final : ::llvm::JITEventListener
{
    struct extent { ::std::uintptr_t begin{},end{}; };
    char const* output{}; bool little{true};
    ::std::array<extent,4u> actual{}; ::std::uintptr_t body_marker{}; bool valid{true};
    void notifyObjectLoaded(ObjectKey,::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        if(object.getArch()!=(little ? ::llvm::Triple::mips64el : ::llvm::Triple::mips64) || object.getBytesInAddress()!=8u || object.isLittleEndian()!=little)
        { valid=false; return; }
        if(output!=nullptr) {
            auto const buffer{object.getMemoryBufferRef().getBuffer()};
            ::fast_io::native_file file{::fast_io::mnp::os_c_str(output),::fast_io::open_mode::out};
            ::fast_io::io::print(file,::fast_io::mnp::strvw(::std::string_view{buffer.data(),buffer.size()}));
        }
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
    CHECK(argc==4);
    auto const endian{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    CHECK(endian=="le" || endian=="be"); bool const little{endian=="le"};
    if(argc>=2)
    {
        auto const end{argv[1]+::std::strlen(argv[1])};
        auto const parsed{::fast_io::parse_by_scan(argv[1],end,::fast_io::mnp::dec_get<true,true>(optimization))};
        CHECK(parsed.code==::fast_io::parse_code::ok && parsed.iter==end);
    }
    CHECK(optimization==0u || optimization==3u);
    ::LLVMInitializeMipsTargetInfo(); ::LLVMInitializeMipsTarget();
    ::LLVMInitializeMipsTargetMC(); ::LLVMInitializeMipsAsmPrinter(); ::LLVMInitializeMipsAsmParser(); ::LLVMInitializeMipsDisassembler();
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("mips64_c_abi_registered_cfi",context)};
    module->setTargetTriple(::llvm::Triple{little ? "mips64el-unknown-linux-gnuabi64" : "mips64-unknown-linux-gnuabi64"});
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
        ".cfi_escape 0x10, 0x1f, 0x04, 0x9c, 0x11, 0x78, 0x22\n\tnop","",true)};
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
    // The no-argument C shim permits independent target execution of this
    // exact emitted object, including its 64-argument N64 stack ABI.
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
    auto observer{manager.get()}; symbols loaded{}; loaded.little=little; loaded.output=argv[3];
    ::llvm::EngineBuilder engine_builder{::std::move(module)};
    engine_builder.setEngineKind(::llvm::EngineKind::JIT).setMArch(little ? "mips64el" : "mips64").setMCPU("mips64r2")
        .setOptLevel(optimization==3u ? ::llvm::CodeGenOptLevel::Aggressive : ::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(manager));
#if defined(__linux__) && defined(__mips__) && defined(__mips64) && __SIZEOF_POINTER__ == 8
    // Exercise the actual native production configuration rather than a
    // fixture-only TargetOptions value. Foreign-host object tests still need
    // an explicit target setting because the helper uses the process ABI.
    cfi::llvm_jit_mcjit_configure_host_unwind_abi(engine_builder);
#else
    ::llvm::TargetOptions options{}; options.EnableCFIFixup=true;
    engine_builder.setTargetOptions(options);
#endif
    // MIPS MCJIT forces static relocation even when PIC is requested.
    // Keep this fact explicit; the target oracle links this exact object.
    engine_builder.setRelocationModel(::llvm::Reloc::Static);
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{engine_builder.create()}};
    CHECK(engine && engine->getTargetMachine() && engine->getTargetMachine()->Options.EnableCFIFixup);
    engine->RegisterJITEventListener(&loaded); engine->finalizeObject();
    CHECK(!observer->has_finalization_failure() && loaded.valid && observer->registrations!=0u);
    unsigned full{},scalar_only{},expressions{}; bool fp_cfa{};
    for(unsigned n{};n!=4u;++n)
    {
        auto const f{loaded.actual[n]}; CHECK(f.end>f.begin && (f.begin&3u)==0u);
        cfi::native_debug_cfa_row entry{},epilogue{};
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,entry));
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.end-1u,epilogue));
        CHECK(epilogue.cfa_register==29u && epilogue.cfa_offset==0);
        for(auto pc{f.begin};pc!=f.end;++pc)
        {
            cfi::native_debug_cfa_row scalar{};
            CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,pc,scalar) && scalar.usable &&
                  (scalar.cfa_register==29u || scalar.cfa_register==30u));
            fp_cfa=fp_cfa || scalar.cfa_register==30u;
            cfi::native_debug_cfi_row row{};
            if(observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row))
            {
                ++full; CHECK(row.usable && row.cfa_usable &&
                    row.registers[31u].kind!=cfi::native_debug_cfi_rule_kind::unavailable);
            }
            else
            {
                ++scalar_only; CHECK(!row.usable && row.begin==0u);
                if(n==2u) { ++expressions; }
            }
        }
        ::fast_io::io::println("actual Mips C-ABI function=",n,
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
        // Match the actual Mips MCAsmInfo description; this N64 object is
        // separately checked as four-byte code below.
        ::std::size_t maximum_instruction_bytes{4u},minimum_instruction_alignment{1u};
        ::std::array<char,64u> triple{},cpu{},features{};
        ::std::size_t triple_size{},cpu_size{},features_size{};
    } target{};
    target.little_endian=little;
    char const* triple{little ? "mips64el-unknown-linux-gnuabi64" : "mips64-unknown-linux-gnuabi64"};
    target.triple_size=::std::strlen(triple); ::std::memcpy(target.triple.data(),triple,target.triple_size);
    constexpr char cpu[]{"mips64r2"}; target.cpu_size=sizeof(cpu)-1u; ::std::memcpy(target.cpu.data(),cpu,sizeof(cpu));
    ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::decoder decoder{target}; CHECK(decoder);
    auto word_data=[&](::std::uint32_t word,::std::uintptr_t pc=0x1000u) noexcept {
        ::std::array<unsigned char,4u> bytes{};
        for(unsigned n{};n!=4u;++n) { bytes[little ? n : 3u-n]=static_cast<unsigned char>(word>>(n*8u)); }
        return decoder.decode(pc,bytes);
    };
    auto ra_return{word_data(0x03e00008u)};
    CHECK(ra_return && ra_return.semantics().kind==::uwvm2::uwvm::debugger::native_instruction_semantics::flow::return_instruction);
    CHECK(!ra_return.safe_for_single_instruction() && !ra_return.safe_for_call_continuation() && !ra_return.safe_for_public_display());
    auto other_jump{word_data(0x03200008u)};
    CHECK(other_jump && other_jump.semantics().kind!=::uwvm2::uwvm::debugger::native_instruction_semantics::flow::return_instruction);
    CHECK(!other_jump.safe_for_single_instruction() && !other_jump.safe_for_call_continuation() && !other_jump.safe_for_public_display());
    CHECK(!word_data(0x03e00008u,0x1001u));
    for(auto word : {0x68830004u,0x6c830004u}) {
        auto load{word_data(word)}; CHECK(load && load.semantics().size==4u);
        CHECK(!load.safe_for_public_display() && !load.safe_for_call_continuation());
    }
    unsigned return_rows{};
    for(unsigned n : {0u,1u,3u}) {
        auto const f{loaded.actual[n]};
        for(auto pc{f.begin};pc<f.end;pc+=4u) {
            auto instruction{decoder.decode(pc,{reinterpret_cast<unsigned char const*>(pc),4u})};
            if(!instruction) {
                // Diagnose only this fixture-owned function; no runtime/VM
                // register, stack, code pointer or debugger authority enters.
                namespace mc = ::uwvm2::uwvm::debugger::native_owned_instruction_semantics::details;
                ::llvm::Triple tt{triple}; ::std::string error{};
                auto const* actual{mc::lookup_target(tt,error)}; CHECK(actual);
                auto registers{mc::make_registers(*actual,tt)}; CHECK(registers);
                ::llvm::MCTargetOptions opts{};
                auto assembly{mc::make_assembly(*actual,*registers,tt,opts)}; CHECK(assembly);
                auto subtarget{mc::make_subtarget(*actual,tt,{cpu,sizeof(cpu)-1u},{})}; CHECK(subtarget);
                auto context{mc::make_context(tt,*assembly,*registers,*subtarget)};
                ::std::unique_ptr<::llvm::MCDisassembler> raw{actual->createMCDisassembler(*subtarget,*context)}; CHECK(raw);
                ::std::unique_ptr<::llvm::MCInstrInfo> info{actual->createMCInstrInfo()}; CHECK(info);
                ::llvm::MCInst inst{}; ::std::uint64_t width{};
                auto status{raw->getInstruction(inst,width,{reinterpret_cast<unsigned char const*>(pc),4u},pc,::llvm::nulls())};
                ::std::uint32_t word{}; ::std::memcpy(&word,reinterpret_cast<void const*>(pc),4u);
                if(little!=(::std::endian::native==::std::endian::little)) { word=::std::byteswap(word); }
                ::fast_io::io::perrln("owned MC decode refusal: function=",n," offset=",pc-f.begin,
                    " word=",::fast_io::mnp::hex(word)," status=",static_cast<unsigned>(status)," width=",width,
                    " opcode=",inst.getOpcode()," operands=",inst.getNumOperands());
                if(inst.getOpcode()<info->getNumOpcodes()) {
                    auto const& desc{info->get(inst.getOpcode())}; auto const name{info->getName(inst.getOpcode())};
                    ::fast_io::io::perrln("actual descriptor name=",::fast_io::mnp::strvw(::std::string_view{name.data(),name.size()}),
                        " operands=",desc.getNumOperands()," pseudo=",desc.isPseudo()," meta=",desc.isMetaInstruction());
                }
            }
            CHECK(instruction);
            if(instruction.semantics().kind!=::uwvm2::uwvm::debugger::native_instruction_semantics::flow::return_instruction) { continue; }
            cfi::native_debug_cfi_row row{}; CHECK(observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row));
            CHECK(row.cfa_register==29u && row.cfa_offset==0 && row.registers[31u].kind==cfi::native_debug_cfi_rule_kind::same);
            CHECK(pc+8u<=f.end); // Conventional N64 return plus its actual delay slot.
            auto delay{decoder.decode(pc+4u,{reinterpret_cast<unsigned char const*>(pc+4u),4u})}; CHECK(delay);
            CHECK(delay.safe_for_single_instruction()); ++return_rows;
        }
    }
    CHECK(return_rows>=3u);
    unsigned earlier_returns{};
    for(auto pc{branch_extent.begin};pc<body_marker;pc+=4u)
    {
        auto instruction{decoder.decode(pc,{reinterpret_cast<unsigned char const*>(pc),4u})}; CHECK(instruction);
        earlier_returns+=instruction.semantics().kind==::uwvm2::uwvm::debugger::native_instruction_semantics::flow::return_instruction;
    }
    if(optimization==0u) { CHECK(earlier_returns!=0u); }
    bool const framed{body_row.cfa_register==30u &&
        body_row.registers[31u].kind==cfi::native_debug_cfi_rule_kind::cfa_memory};
    CHECK(framed);
    ::fast_io::io::println("actual Mips later body: earlier-returns=",earlier_returns,
        " saved-RA-and-FP-CFA=",framed);

#if defined(__mips__) && __SIZEOF_POINTER__ == 8
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
    ::fast_io::io::println("actual Mips C-ABI relocated CFI: PASS optimization=",optimization,
        " endian-little=",little," restored-return-rows=",return_rows," full-rule-bytes=",full," cfa-only-bytes=",scalar_only,
        " exported-N64-static-object=true test-only-metadata-owner=true host-unwind-registration=false live-Wasm-caller-qualified=false native-finish-qualified=false");
}
