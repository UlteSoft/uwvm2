// Actual ARM AAPCS object + production manager + relocated registered CFI.
// On a 64-bit host RuntimeDyld maps target sections to distinct 32-bit labels.
// These labels are metadata DATA; no ARM code, live stack or finish executes.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Verifier.h>
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/TargetSelect.h>
#include <fast_io.h>
#include <array>
#include <cstring>
#include <memory>
namespace cfi = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("ARM registered CFI failure line=", __LINE__); return 1; } } while(false)
struct symbols final : ::llvm::JITEventListener
{
    struct extent { ::std::uintptr_t begin{},end{}; };
    ::std::array<extent,3u> actual{}; ::std::array<::std::uintptr_t,3u> retired{}; bool valid{true},tailcc{};
    ::llvm::ExecutionEngine* engine{};
    char const* object_path{}; bool saved_object{}; unsigned debug_frame_count{}, rejected_objects{}, guarded_objects{};
    ::std::array<::std::size_t,3u> text_offsets{};
    void fail(unsigned line) noexcept
    { ::fast_io::io::perrln("ARM CFI object listener failure line=",line," malformed=",rejected_objects," guarded=",guarded_objects);valid=false; }
    void notifyObjectLoaded(ObjectKey key,::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        if(object.getArch()!=::llvm::Triple::arm || object.getBytesInAddress()!=4u || !object.isLittleEndian())
        { fail(__LINE__); return; }
        if(!object_path || saved_object) { fail(__LINE__); return; }
        {
            ::fast_io::native_file file{::fast_io::mnp::os_c_str(object_path),::fast_io::open_mode::out};
            auto bytes{object.getMemoryBufferRef().getBuffer()};
            ::fast_io::io::print(file,::fast_io::mnp::strvw(bytes.data(),bytes.data()+bytes.size()));
            saved_object=true;
        }
        for(auto const& section:object.sections())
        {
            auto name{section.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); fail(__LINE__); return; }
            if(*name==".debug_frame") { ++debug_frame_count; }
        }
        if constexpr(sizeof(void*)>4u)
        {
            // A real 32-bit target relocation without truncating host pointers.
            // Every section has its own non-overlapping bounded target label.
            unsigned index{};
            for(auto const& section: object.sections())
            {
                auto const local{loaded.getSectionLoadAddress(section)};
                if(local==0u) { continue; }
                if(!engine || ++index>128u || section.getSize()>0x10000u) { fail(__LINE__); return; }
                engine->mapSectionAddress(reinterpret_cast<void*>(local),0x10000000u+index*0x10000u);
            }
        }
        for(auto const& [symbol,size] : ::llvm::object::computeSymbolSizes(object))
        {
            auto name{symbol.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); fail(__LINE__); return; }
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
                fail(__LINE__); return;
            }
            if(*section==object.section_end() || size==0u || *address<(*section)->getAddress())
            { fail(__LINE__); return; }
            auto const base{loaded.getSectionLoadAddress(**section)}, offset{*address-(*section)->getAddress()};
            constexpr auto limit{(::std::numeric_limits<::std::uintptr_t>::max)()};
            if(base==0u || base>limit || offset>limit-base || size>limit-base-offset || actual[index].begin!=0u)
            { fail(__LINE__); return; }
            actual[index]={static_cast<::std::uintptr_t>(base+offset),static_cast<::std::uintptr_t>(base+offset+size)};
            auto text{(*section)->getContents()}; auto original_bytes{object.getMemoryBufferRef().getBuffer()};
            if(!text) { ::llvm::consumeError(text.takeError()); fail(__LINE__);return; }
            if(text->data()<original_bytes.data() || text->data()>original_bytes.data()+original_bytes.size() ||
                text->size()>static_cast<::std::size_t>(original_bytes.data()+original_bytes.size()-text->data()) ||
                offset>text->size() || size>text->size()-offset) { fail(__LINE__);return; }
            text_offsets[index]=static_cast<::std::size_t>(text->data()-original_bytes.data()+offset);
            if(tailcc)
            {
                auto contents{(*section)->getContents()};
                if(!contents) { ::llvm::consumeError(contents.takeError());fail(__LINE__);return; }
                if(size<12u || offset>contents->size() || size>contents->size()-offset) { fail(__LINE__);return; }
                auto const read{[&](::std::uint64_t at)
                {
                    auto p{reinterpret_cast<unsigned char const*>(contents->data())+at};
                    return static_cast<::std::uint32_t>(p[0u]) | (static_cast<::std::uint32_t>(p[1u])<<8u) |
                        (static_cast<::std::uint32_t>(p[2u])<<16u) | (static_cast<::std::uint32_t>(p[3u])<<24u);
                }};
                // Actual compiler operand values, independently retained and
                // disassembled in the cgroup; no fixed live-frame SP guess.
                auto const restore{read(offset+size-12u)};
                if((restore&0xffff0000u)!=0xe8bd0000u || !(restore&(1u<<11u)) ||
                    !(restore&(1u<<14u)) || (restore&(1u<<15u)) ||
                    read(offset+size-8u)!=0xe28dd0f0u || read(offset+size-4u)!=0xe12fff1eu)
                { fail(__LINE__);return; }
                retired[index]=actual[index].end-8u;
            }
        }
        auto const original{object.getMemoryBufferRef().getBuffer()};
        ::std::size_t frame_offset{}, reloc_offset{}, frame_size{}, reloc_size{};
        for(auto const& section: object.sections())
        {
            auto name{section.getName()}; auto contents{section.getContents()};
            if(!name || !contents)
            {
                if(!name) { ::llvm::consumeError(name.takeError()); }
                if(!contents) { ::llvm::consumeError(contents.takeError()); }
                fail(__LINE__); return;
            }
            if(*name!=".debug_frame" && *name!=".rel.debug_frame") { continue; }
            if(contents->data()<original.data() || contents->data()>original.data()+original.size() ||
                contents->size()>static_cast<::std::size_t>(original.data()+original.size()-contents->data()))
            { fail(__LINE__); return; }
            auto const offset{static_cast<::std::size_t>(contents->data()-original.data())};
            if(*name==".debug_frame") { frame_offset=offset;frame_size=contents->size(); }
            else { reloc_offset=offset;reloc_size=contents->size(); }
        }
        if(!frame_offset || frame_size<40u || !reloc_offset || reloc_size<16u) { fail(__LINE__);return; }
        for(unsigned mutation{};mutation!=8u;++mutation)
        {
            ::std::vector<char> bytes(original.begin(),original.end());
            auto const write32{[&](::std::size_t offset,::std::uint32_t value)
            { for(unsigned n{};n!=4u;++n) { bytes[offset+n]=static_cast<char>(value>>(8u*n)); } }};
            auto const read32{[&](::std::size_t offset)
            {
                ::std::uint32_t value{};
                for(unsigned n{};n!=4u;++n) { value|=static_cast<::std::uint32_t>(static_cast<unsigned char>(bytes[offset+n]))<<(8u*n); }
                return value;
            }};
            auto const cie_field{read32(reloc_offset)},text_field{read32(reloc_offset+8u)};
            if(cie_field>frame_size-4u || text_field>frame_size-8u) { fail(__LINE__);return; }
            switch(mutation)
            {
                case 0u: bytes[reloc_offset+4u]=0;break; // Unsupported relocation.
                case 1u: write32(reloc_offset+8u,cie_field);break; // Duplicate field.
                case 2u: write32(reloc_offset+4u,2u);break; // Undefined symbol zero.
                case 3u: write32(reloc_offset+4u,read32(reloc_offset+12u));break; // CIE points at text.
                case 4u: write32(frame_offset+text_field+4u,UINT32_MAX);break; // FDE widens beyond original text.
                case 5u: write32(reloc_offset+8u,static_cast<::std::uint32_t>(frame_size));break; // Out of section.
                case 6u: write32(frame_offset,UINT32_MAX);break; // Unsupported DWARF64 record.
                case 7u: // ELF Arch=arm with Thumb mappings must not supply A32 rows.
                {
                    unsigned changed{};
                    for(auto const& symbol:object.symbols())
                    {
                        auto name{symbol.getName()};
                        if(!name) { ::llvm::consumeError(name.takeError());fail(__LINE__);return; }
                        if(*name!="$a" && !name->starts_with("$a.")) { continue; }
                        if(name->data()<original.data() || name->data()+name->size()>original.data()+original.size())
                        { fail(__LINE__);return; }
                        bytes[static_cast<::std::size_t>(name->data()-original.data())+1u]='t';++changed;
                    }
                    if(changed==0u) { fail(__LINE__);return; }
                    break;
                }
            }
            auto malformed{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
                ::llvm::StringRef{bytes.data(),bytes.size()},"owned-ARM-debug-frame-negative"})};
            if(!malformed) { ::llvm::consumeError(malformed.takeError());fail(__LINE__);return; }
            cfi::runtime_llvm_jit_section_memory_manager candidate{};
            candidate.notifyObjectLoaded(key,**malformed,loaded);
            if(candidate.has_finalization_failure()) { fail(__LINE__);return; } // Private failure cannot break EHABI.
            for(auto const extent:actual)
            {
                cfi::native_debug_cfa_row row{};
                if(candidate.copy_debug_native_cfa_row(extent.begin,extent.end,extent.begin,row) || row.usable)
                { fail(__LINE__);return; }
            }
            ++rejected_objects;
        }
        // The actual compiler's leaf is already framed at +8. Replace that
        // bounded original instruction, retaining the genuine stale CFI. This
        // exercises production refusal, rather than merely mirroring a bitmask.
        // Conditional writes are vetoed even when their condition could be false.
        constexpr ::std::array<::std::uint32_t,14u> clobbers{
            0xe8bd4800u, 0xe59db004u, 0xe1a0b000u, 0xe280b008u,
            0x01a0b000u, 0xe5bb0004u, 0xe5ab0004u, 0xe00b0190u,
            0xe08b0291u, 0xe8ab0003u, 0xe8bb0003u, 0xe1a0b00du,
            0xe1c0a0d0u, 0xe1b0af9fu};
        auto const extent{actual[0u]};
        if(extent.end-extent.begin<16u) { fail(__LINE__);return; }
        for(auto word:clobbers)
        {
            ::std::vector<char> bytes(original.begin(),original.end());
            for(unsigned n{};n!=4u;++n) { bytes[text_offsets[0u]+8u+n]=static_cast<char>(word>>(8u*n)); }
            auto changed{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
                ::llvm::StringRef{bytes.data(),bytes.size()},"owned-ARM-stale-frame-register-negative"})};
            if(!changed) { ::llvm::consumeError(changed.takeError());fail(__LINE__);return; }
            cfi::runtime_llvm_jit_section_memory_manager candidate{};
            candidate.notifyObjectLoaded(key,**changed,loaded);
            cfi::native_debug_cfa_row scalar{};
            if(candidate.has_finalization_failure() ||
                !candidate.copy_debug_native_cfa_row(extent.begin,extent.end,extent.begin+8u,scalar))
            { fail(__LINE__);return; }
            for(auto pc{extent.begin+12u};pc<extent.end;pc+=4u)
            {
                cfi::native_debug_cfi_row row{};
                if(candidate.copy_debug_native_cfa_row(extent.begin,extent.end,pc,scalar) || scalar.usable ||
                    candidate.copy_debug_native_cfi_row(extent.begin,extent.end,pc,row) || row.usable)
                { fail(__LINE__);return; }
            }
            ++guarded_objects;
        }
    }
};
int main(int argc,char const* const* argv)
{
    unsigned optimization{},tailcc{};
    CHECK(argc==3 || argc==4);
    if(argc>=3)
    {
        auto const end{argv[1]+::std::strlen(argv[1])};
        auto const parsed{::fast_io::parse_by_scan(argv[1],end,::fast_io::mnp::dec_get<true,true>(optimization))};
        CHECK(parsed.code==::fast_io::parse_code::ok && parsed.iter==end);
    }
    CHECK(optimization==0u || optimization==3u);
    if(argc==4)
    {
        auto const end{argv[3]+::std::strlen(argv[3])};
        auto const parsed{::fast_io::parse_by_scan(argv[3],end,::fast_io::mnp::dec_get<true,true>(tailcc))};
        CHECK(parsed.code==::fast_io::parse_code::ok && parsed.iter==end && tailcc<=1u);
    }
    // This provider has unpatched ARM O0 FastISel. Its SelectionDAG path
    // supports actual TailCC. Keep that profile separate from standard AAPCS
    // and do not present either as a qualified native ARM runtime SDK.
    CHECK(tailcc==0u || optimization==3u);
    auto const convention{tailcc ? ::llvm::CallingConv::Tail : ::llvm::CallingConv::C};
    ::LLVMInitializeARMTargetInfo(); ::LLVMInitializeARMTarget();
    ::LLVMInitializeARMTargetMC(); ::LLVMInitializeARMAsmPrinter(); ::LLVMInitializeARMAsmParser();
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("ARM_aapcs_registered_cfi",context)};
    module->setTargetTriple(::llvm::Triple{"armv7-unknown-linux-gnueabihf"});
    auto i32{::llvm::Type::getInt32Ty(context)};
    ::std::array<::llvm::Type*,64u> types{}; types.fill(i32);
    auto type{::llvm::FunctionType::get(i32,types,false)};
    auto leaf{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_leaf",*module)};
    auto caller{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_caller",*module)};
    auto expression{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_expression",*module)};
    ::std::array<::llvm::DISubprogram*,3u> scopes{}; unsigned scope_index{};
    for(auto f:{leaf,caller,expression})
    {
        f->setUWTableKind(::llvm::UWTableKind::Async); f->setCallingConv(convention);
        f->addFnAttr(::llvm::Attribute::NoInline); f->addFnAttr("frame-pointer","all");
        scopes[scope_index]=::uwvm2::runtime::compiler::llvm_jit::native_provenance::attach(*f,"arm-cfi-probe");
        CHECK(scopes[scope_index++]);
    }
    ::llvm::IRBuilder<> builder{::llvm::BasicBlock::Create(context,"entry",leaf)};
    CHECK(::uwvm2::runtime::compiler::llvm_jit::native_provenance::location(builder,scopes[0u],0u,8u));
    builder.CreateRet(builder.CreateAdd(leaf->getArg(63u),builder.getInt32(1u)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",caller));
    CHECK(::uwvm2::runtime::compiler::llvm_jit::native_provenance::location(builder,scopes[1u],1u,8u));
    auto slot{builder.CreateAlloca(i32,builder.getInt32(128u))}; builder.CreateStore(caller->getArg(0u),slot,true);
    ::std::array<::llvm::Value*,64u> args{};
    for(unsigned n{};n!=64u;++n) { args[n]=caller->getArg(n); }
    auto call{builder.CreateCall(leaf,args)}; call->setCallingConv(convention);
    builder.CreateRet(builder.CreateAdd(call,builder.CreateLoad(i32,slot,true)));
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",expression));
    CHECK(::uwvm2::runtime::compiler::llvm_jit::native_provenance::location(builder,scopes[2u],2u,8u));
    auto assembly{::llvm::InlineAsm::get(::llvm::FunctionType::get(builder.getVoidTy(),false),
        ".cfi_escape 0x10, 0x0e, 0x04, 0x9c, 0x11, 0x7c, 0x22\n\tnop","",true)};
    builder.CreateCall(assembly); builder.CreateRet(expression->getArg(0u));
    // Emit a genuine C ABI entry for the separately cross-linked QEMU
    // fixture. TailCC callees receive 60 stack arguments and pop their own
    // 240-byte area. The host MCJIT never executes this ARM machine code.
    auto bridge{::llvm::Function::Create(::llvm::FunctionType::get(i32,false),
        ::llvm::GlobalValue::ExternalLinkage,"uwvm_cfi_arm_execution_bridge",*module)};
    bridge->setCallingConv(::llvm::CallingConv::C);bridge->addFnAttr(::llvm::Attribute::NoInline);
    bridge->addFnAttr("frame-pointer","all");
    auto bridge_scope{::uwvm2::runtime::compiler::llvm_jit::native_provenance::attach(*bridge,"arm-cfi-execution-bridge")};
    CHECK(bridge_scope);
    builder.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",bridge));
    CHECK(::uwvm2::runtime::compiler::llvm_jit::native_provenance::location(builder,bridge_scope,3u,8u));
    for(unsigned n{};n!=64u;++n) { args[n]=builder.getInt32(n); }
    auto bridged_call{builder.CreateCall(caller,args)};bridged_call->setCallingConv(convention);
    builder.CreateRet(bridged_call);
    CHECK(!::llvm::verifyModule(*module));
    auto manager{::std::make_unique<cfi::runtime_llvm_jit_section_memory_manager>()};
    auto observer{manager.get()}; symbols loaded{}; loaded.object_path=argv[2]; loaded.tailcc=tailcc!=0u;
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder(::std::move(module))
        .setEngineKind(::llvm::EngineKind::JIT).setMArch("arm").setMCPU("cortex-a15")
        .setOptLevel(optimization==3u ? ::llvm::CodeGenOptLevel::Aggressive : ::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(manager)).create()}};
    CHECK(engine); loaded.engine=engine.get(); engine->RegisterJITEventListener(&loaded); engine->finalizeObject();
    CHECK(!observer->has_finalization_failure() && loaded.valid);
    CHECK(loaded.saved_object && loaded.debug_frame_count==1u && loaded.rejected_objects==8u && loaded.guarded_objects==14u);
    unsigned full{},cfa_only{},expressions{},retired_instructions{};
    for(unsigned n{};n!=3u;++n)
    {
        auto const f{loaded.actual[n]}; CHECK(f.end>f.begin && f.end<=UINT32_MAX);
        cfi::native_debug_cfa_row entry{};
        CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,f.begin,entry) &&
            entry.cfa_register==13u && entry.cfa_offset==0);
        cfi::native_debug_cfi_row first{};
        CHECK(!observer->copy_debug_native_cfi_row(f.begin,f.end,f.begin,first) && !first.usable);
        for(auto pc{f.begin};pc<f.end;pc+=4u)
        {
            cfi::native_debug_cfa_row scalar{};
            if(loaded.retired[n] != 0u && pc >= loaded.retired[n])
            {
                cfi::native_debug_cfi_row row{};
                CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,pc,scalar) && !scalar.usable &&
                    !observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row) && !row.usable && row.begin==0u);
                ++retired_instructions;continue;
            }
            CHECK(observer->copy_debug_native_cfa_row(f.begin,f.end,pc,scalar) && scalar.usable &&
                (scalar.cfa_register==13u || scalar.cfa_register==11u));
            cfi::native_debug_cfi_row row{};
            if(observer->copy_debug_native_cfi_row(f.begin,f.end,pc,row))
            {
                ++full;CHECK(row.usable && row.registers[14u].kind==cfi::native_debug_cfi_rule_kind::cfa_memory);
                ::std::array<::std::uint64_t,16u> registers{};
                registers[13u]=0x1000u;registers[row.cfa_register]=0x2000u-row.cfa_offset;
                ::std::array<cfi::native_debug_cfi_arm_owned_word,8u> slots{};unsigned count{};
                for(unsigned reg : {4u,5u,6u,7u,8u,10u,11u,14u})
                {
                    auto const& rule{row.registers[reg]};
                    if(rule.kind!=cfi::native_debug_cfi_rule_kind::cfa_memory) { continue; }
                    auto const address{static_cast<::std::int64_t>(0x2000u)+rule.offset};
                    CHECK(address>=static_cast<::std::int64_t>(registers[13u]) && address+4<=0x2000);
                    slots[count++]={static_cast<::std::uintptr_t>(address),reg==14u ? 0x20010000u : reg+100u};
                }
                cfi::native_debug_cfi_arm_caller recovered{};
                CHECK(cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,(1u<<13u)|(1u<<11u),
                    static_cast<::std::uintptr_t>(registers[13u]),0x2000u,{slots.data(),count},recovered));
                CHECK(recovered.return_pc==0x20010000u && recovered.cfa==0x2000u &&
                    !(recovered.known&((1u<<9u)|(1u<<15u))) && recovered.registers[9u]==0u && recovered.registers[15u]==0u);
                CHECK(!cfi::evaluate_native_debug_cfi_arm_sparse(row,registers,(1u<<13u)|(1u<<11u),
                    static_cast<::std::uintptr_t>(registers[13u]),0x1ffcu,{slots.data(),count},recovered) && !recovered.known);
            }
            else { ++cfa_only;if(n==2u && pc>=f.begin+8u) { ++expressions; } }
        }
        CHECK(!observer->copy_debug_native_cfa_row(f.begin-4u,f.end,f.begin,entry));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end+4u,f.begin,entry));
        CHECK(!observer->copy_debug_native_cfa_row(f.begin,f.end,f.end,entry));
        ::fast_io::io::println("actual ARM function=",n," begin=",::fast_io::mnp::hex(f.begin),
            " end=",::fast_io::mnp::hex(f.end));
    }
    CHECK(full!=0u && cfa_only!=0u && expressions!=0u && retired_instructions==(tailcc ? 6u : 0u));
    observer->deregisterEHFrames();
    for(auto const extent:loaded.actual)
    { cfi::native_debug_cfa_row row{};CHECK(!observer->copy_debug_native_cfa_row(extent.begin,extent.end,extent.begin,row)); }
    engine->UnregisterJITEventListener(&loaded);
    ::fast_io::io::println("actual ARM registered debug_frame: PASS optimization=",optimization," TailCC=",tailcc,
        " full-instructions=",full," CFA-only-instructions=",cfa_only," rejected-objects=",loaded.rejected_objects," stale-r11-objects-refused=",loaded.guarded_objects," retired-frame-instructions-refused=",retired_instructions,
        " actual-ARM-execution=false live-caller-qualified=false native-finish-qualified=false");
}
