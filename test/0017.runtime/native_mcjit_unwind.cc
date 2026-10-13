// Real native MCJIT unwind oracle. Counts registered function regions in
// three recursive C-ABI activations; no stack guessing or debugger authority.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <llvm/Object/ObjectFile.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_function_address.h>
#include <memory>
#include <cstring>
#include <vector>
#include <fast_io.h>
#if defined(__linux__) && defined(__mips__)
struct native_dwarf_eh_bases { void* text{}; void* data{}; void* function{}; };
extern "C" void native_register_frame(void const*) noexcept __asm__("__register_frame");
extern "C" void native_deregister_frame(void const*) noexcept __asm__("__deregister_frame");
extern "C" void const* native_find_fde(void const*,native_dwarf_eh_bases*) noexcept __asm__("_Unwind_Find_FDE");
#endif
namespace abi = ::uwvm2::runtime::compiler::llvm_jit::native_unwind_abi;
namespace support = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native MCJIT unwind failure line=",__LINE__); return 1; } } while(false)
struct trace_state
{
    ::std::uintptr_t recursive{},root{};
    unsigned recursive_frames{},root_frames{},total{};
    bool bad_order{}; _Unwind_Reason_Code reason{};
};
static _Unwind_Reason_Code frame(_Unwind_Context* context,void* opaque) noexcept
{
    auto& state{*static_cast<trace_state*>(opaque)}; ++state.total;
    if(state.total > 256u) { state.bad_order = true; return _URC_END_OF_STACK; }
    auto const region{static_cast<::std::uintptr_t>(abi::get_region_start_noexcept(context))};
    if(region==state.recursive) { state.bad_order |= state.root_frames!=0u; ++state.recursive_frames; }
    else if(region==state.root) { state.bad_order |= state.recursive_frames!=3u; ++state.root_frames; }
    return _URC_NO_REASON;
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void capture(void* opaque) noexcept
{
    auto& state{*static_cast<trace_state*>(opaque)};
    state.reason=abi::backtrace_noexcept(frame,opaque);
    ::std::atomic_signal_fence(::std::memory_order_seq_cst);
}
struct object_capture final : ::llvm::JITEventListener
{
    char const* file{};
    ::std::uint8_t const* live_eh{}; ::std::size_t live_eh_size{};
    void notifyObjectLoaded(ObjectKey,::llvm::object::ObjectFile const& object,::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) override
    {
        for(auto const& section: object.sections())
        {
            auto name{section.getName()};if(!name) { ::llvm::consumeError(name.takeError());continue; }
            if(*name!=".eh_frame" || section.getSize()==0u || section.getSize()>65536u) { continue; }
            auto const address{loaded.getSectionLoadAddress(section)};
            if(address!=0u && live_eh==nullptr)
            { live_eh=reinterpret_cast<::std::uint8_t const*>(address);live_eh_size=section.getSize(); }
        }
        if(file==nullptr) { return; }
        auto const buffer{object.getMemoryBufferRef().getBuffer()};
        ::fast_io::native_file output{::fast_io::mnp::os_c_str(file),::fast_io::open_mode::out};
        ::fast_io::io::print(output,::fast_io::mnp::strvw(::std::string_view{buffer.data(),buffer.size()}));
    }
};
int main(int argc,char** argv)
{
    CHECK(argc<=3); bool const unexported{argc == 2 && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[1])} == "--expect-unexported"};
    bool const shifted_diagnostic{argc==3}; CHECK(!::llvm::InitializeNativeTarget()); CHECK(!::llvm::InitializeNativeTargetAsmPrinter());
    ::llvm::LLVMContext context{}; auto module{::std::make_unique<::llvm::Module>("native-live-unwind",context)};
    ::llvm::IRBuilder<> ir{context};auto* const pointer{ir.getPtrTy()};
    auto* const root_type{::llvm::FunctionType::get(ir.getVoidTy(),{pointer,pointer},false)};
    auto* const recursive_type{::llvm::FunctionType::get(ir.getVoidTy(),{pointer,ir.getInt32Ty(),pointer},false)};
    auto* const capture_type{::llvm::FunctionType::get(ir.getVoidTy(),{pointer},false)};
    auto* const recursive{::llvm::Function::Create(recursive_type,::llvm::GlobalValue::ExternalLinkage,"native_unwind_recursive",*module)};
    auto* const root{::llvm::Function::Create(root_type,::llvm::GlobalValue::ExternalLinkage,"native_unwind_root",*module)};
    for(auto* function: {recursive,root})
    { function->addFnAttr(::llvm::Attribute::NoInline);function->addFnAttr("disable-tail-calls","true");
      function->setUWTableKind(::llvm::UWTableKind::Async); }
    auto* const entry{::llvm::BasicBlock::Create(context,"entry",recursive)};
    auto* const leaf{::llvm::BasicBlock::Create(context,"leaf",recursive)};
    auto* const recurse{::llvm::BasicBlock::Create(context,"recurse",recursive)};
    ir.SetInsertPoint(entry);ir.CreateCondBr(ir.CreateICmpEQ(recursive->getArg(1u),ir.getInt32(0u)),leaf,recurse);
    ir.SetInsertPoint(leaf);ir.CreateCall(capture_type,recursive->getArg(2u),{recursive->getArg(0u)});ir.CreateRetVoid();
    ir.SetInsertPoint(recurse);ir.CreateCall(recursive,{recursive->getArg(0u),ir.CreateSub(recursive->getArg(1u),ir.getInt32(1u)),recursive->getArg(2u)});ir.CreateRetVoid();
    ir.SetInsertPoint(::llvm::BasicBlock::Create(context,"entry",root));
    ir.CreateCall(recursive,{root->getArg(0u),ir.getInt32(2u),root->getArg(1u)});ir.CreateRetVoid();CHECK(!::llvm::verifyModule(*module));
    ::llvm::SmallVector<::std::string,32u> attributes{};
    for(auto const& [name,enabled]: ::llvm::sys::getHostCPUFeatures()) { attributes.emplace_back((enabled?"+":"-")+name.str()); }
    ::llvm::Triple const triple{::llvm::Triple::normalize(::llvm::sys::getDefaultTargetTriple())};
    ::fast_io::io::perrln("native MCJIT target=",::fast_io::mnp::os_c_str(triple.str().c_str())," EHABI-compatible=",triple.isTargetEHABICompatible());
    support::llvm_jit_append_native_host_vector_features(triple,::llvm::sys::getHostCPUName(),attributes);
    if(triple.isMIPS()) { attributes.emplace_back("+noabicalls");attributes.emplace_back("+long-calls"); }
    ::llvm::EngineBuilder target_builder{};target_builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
    support::llvm_jit_mcjit_configure_code_model(target_builder,triple);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{target_builder.selectTarget(triple,{},support::llvm_jit_abi_host_cpu_name(triple,::llvm::sys::getHostCPUName()),attributes)}};
    CHECK(target);module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    // The paired SDK no longer requires temporary-label retention.
#if !defined(LLVM_UWVM_ROS_ELF_PPC32_MCJIT) && !defined(LLVM_UWVM_ROS_ELF_SPARCV9_MCJIT)
    if(support::llvm_jit_mcjit_needs_unique_temp_labels(triple)) { target->Options.MCOptions.MCSaveTempLabels=true; }
#endif
    auto memory{::std::make_unique<support::runtime_llvm_jit_section_memory_manager>()};auto* const observer{memory.get()};
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(memory)).create(target.release())}};CHECK(engine);
    object_capture object{};if(argc>=2 && !unexported) { object.file=argv[1]; }engine->RegisterJITEventListener(&object);
    engine->finalizeObject();engine->UnregisterJITEventListener(&object);
#if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__)
    if(unexported)
    {
        CHECK(!support::arm_ehabi_lookup_is_exported() && observer->has_finalization_failure());
        engine.reset(); ::fast_io::io::println("PASS missing ARM EHABI lookup export refuses execution"); return 0;
    }
#else
    CHECK(!unexported);
#endif
    CHECK(!engine->hasError() && !observer->has_finalization_failure());
    if(object.file!=nullptr && object.live_eh!=nullptr)
    {
        // This bounded range belongs to this component's actual live MCJIT
        // section. It is saved privately after relocation, before engine drain.
        auto const path{::fast_io::concat_fast_io(::fast_io::mnp::os_c_str(object.file),".live-eh-frame.bin")};
        ::fast_io::native_file output{path,::fast_io::open_mode::out};
        ::fast_io::io::print(output,::fast_io::mnp::strvw(::std::string_view{
            reinterpret_cast<char const*>(object.live_eh),object.live_eh_size}));
    }
    trace_state state{};state.recursive=engine->getFunctionAddress("native_unwind_recursive");state.root=engine->getFunctionAddress("native_unwind_root");
    auto const root_callable{state.root};
    state.recursive=::uwvm2::runtime::lib::details::native_function_code_address(state.recursive);
    state.root=::uwvm2::runtime::lib::details::native_function_code_address(state.root);
    CHECK(state.recursive && state.root);
#if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__)
    int entries{};
    CHECK(support::get_arm_ehabi_registry().find(state.recursive + 4u, entries) != 0u && entries > 0);
    entries = 0; CHECK(support::get_arm_ehabi_registry().find(state.root + 4u, entries) != 0u && entries > 0);
    // The private dynamic registry cannot substitute a JIT table for the host
    // capture function. The exported unwinder hook preserves glibc lookup.
    entries = 0; auto const host{reinterpret_cast<::std::uintptr_t>(&capture) & ~::std::uintptr_t{1u}};
    CHECK(support::get_arm_ehabi_registry().find(host, entries) == 0u);
    CHECK(::__gnu_Unwind_Find_exidx(host, &entries) != 0u && entries > 0);
#endif
    ::fast_io::io::println("private-JIT entry-distance=",state.root>=state.recursive?state.root-state.recursive:state.recursive-state.root);
#if defined(__linux__) && defined(__mips__)
    native_dwarf_eh_bases recursive_bases{},root_bases{};
    auto const recursive_fde{native_find_fde(reinterpret_cast<void const*>(state.recursive+4u),&recursive_bases)};
    auto const root_fde{native_find_fde(reinterpret_cast<void const*>(state.root+4u),&root_bases)};
    ::fast_io::io::println("registered-FDE recursive=",recursive_fde!=nullptr," root=",root_fde!=nullptr,
        " exact-recursive=",reinterpret_cast<::std::uintptr_t>(recursive_bases.function)==state.recursive,
        " exact-root=",reinterpret_cast<::std::uintptr_t>(root_bases.function)==state.root);
#endif
    ::std::vector<::std::uint64_t> diagnostic_eh{};void* diagnostic_section{};
#if defined(__linux__) && defined(__mips__)
    if(shifted_diagnostic)
    {
        CHECK(object.live_eh!=nullptr && object.live_eh_size<=65536u);
        // Private oracle only: move this component's unchanged owned CFI by
        // four bytes to test unaligned absolute sdata8 decoding. The original
        // production registration is retained. No code or metadata is patched.
        diagnostic_eh.resize((object.live_eh_size+19u)/8u);
        // First distinguish termination from address alignment. A same-aligned
        // zero-terminated copy preserves all original record alignments.
        auto* const unshifted{diagnostic_eh.data()};
        ::std::memcpy(unshifted,object.live_eh,object.live_eh_size);
        native_register_frame(unshifted);native_dwarf_eh_bases unshifted_bases{};
        ::fast_io::io::println("private-terminated-FDE recursive=",
            native_find_fde(reinterpret_cast<void const*>(state.recursive+4u),&unshifted_bases)!=nullptr);
        native_deregister_frame(unshifted);
        ::std::memset(diagnostic_eh.data(),0u,diagnostic_eh.size()*sizeof(::std::uint64_t));
        diagnostic_section=reinterpret_cast<::std::uint8_t*>(diagnostic_eh.data())+4u;
        ::std::memcpy(diagnostic_section,object.live_eh,object.live_eh_size);
        native_register_frame(diagnostic_section);
        native_dwarf_eh_bases shifted{};
        ::fast_io::io::println("private-shifted-FDE recursive=",
            native_find_fde(reinterpret_cast<void const*>(state.recursive+4u),&shifted)!=nullptr);
    }
#else
    CHECK(!shifted_diagnostic);
#endif
    using capture_type_t=void(*)(void*) noexcept;using root_type_t=void(*)(void*,capture_type_t) noexcept;
    reinterpret_cast<root_type_t>(root_callable)(&state,&capture);
    ::fast_io::io::println("native unwind registered regions recursive=",state.recursive_frames," root=",state.root_frames,
        " total=",state.total," order-error=",state.bad_order," reason=",static_cast<unsigned>(state.reason));
#if defined(__linux__) && defined(__mips__)
    if(diagnostic_section!=nullptr) { native_deregister_frame(diagnostic_section); }
#endif
    if(shifted_diagnostic) { ::fast_io::io::println("Private relocation-alignment diagnostic; not a production pass");return 1; }
    CHECK(!state.bad_order && state.recursive_frames==3u && state.root_frames==1u);
    engine.reset();
#if defined(UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT) && UWVM2_ENABLE_LINUX_ARM_EHABI_PRODUCT == 1 && \
    defined(__linux__) && defined(__arm__) && !defined(__ARM_DWARF_EH__) && defined(__GLIBC__)
    entries = 0; CHECK(support::get_arm_ehabi_registry().find(state.recursive + 4u, entries) == 0u && entries == 0);
    CHECK(::__gnu_Unwind_Find_exidx(state.recursive + 4u, &entries) == 0u && entries == 0);
    ::fast_io::io::println("PASS ARM EHABI actual table extents, host exclusion, and post-drain retirement");
#endif
    ::fast_io::io::println("PASS real recursive native unwinding and registered EH frame retirement");
}
