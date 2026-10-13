// Private native MCJIT diagnostic: distinguish physical frame size from
// long code and actual Invoke -> cleanup -> Resume. No Wasm/ASM authority.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <fast_io.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

extern "C" _Unwind_Reason_Code __gxx_personality_v0(
    int, _Unwind_Action, ::std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
namespace abi = ::uwvm2::runtime::compiler::llvm_jit::native_unwind_abi;
namespace support = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native cleanup diagnostic failure line=", __LINE__); return 1; } } while(false)

struct foreign_signal {};
struct trace_state
{
    ::std::uintptr_t recursive{}, root{};
    unsigned recursive_frames{}, root_frames{}, total{}, cleanups{};
    bool bad_order{};
    _Unwind_Reason_Code reason{};
};
static _Unwind_Reason_Code frame(_Unwind_Context* context, void* opaque) noexcept
{
    auto& state{*static_cast<trace_state*>(opaque)};
    if(++state.total > 256u) { state.bad_order = true; return _URC_END_OF_STACK; }
    auto const region{static_cast<::std::uintptr_t>(abi::get_region_start_noexcept(context))};
    if(region == state.recursive) { state.bad_order |= state.root_frames != 0u; ++state.recursive_frames; }
    else if(region == state.root) { state.bad_order |= state.recursive_frames != 3u; ++state.root_frames; }
    return _URC_NO_REASON;
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void capture(void* opaque)
{
    auto& state{*static_cast<trace_state*>(opaque)};
    state.reason = abi::backtrace_noexcept(frame, opaque);
    ::std::atomic_signal_fence(::std::memory_order_seq_cst);
    throw foreign_signal{};
}
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void cleanup(void* opaque, unsigned depth) noexcept
{
    auto& state{*static_cast<trace_state*>(opaque)};
    state.bad_order |= state.cleanups != depth;
    ++state.cleanups;
}
struct object_capture final : ::llvm::JITEventListener
{
    char const* file{};
    ::std::size_t code_bytes{}, lsda_bytes{}, eh_bytes{};
    void notifyObjectLoaded(ObjectKey, ::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const&) override
    {
        for(auto const& section : object.sections())
        {
            auto name{section.getName()};
            if(!name) { ::llvm::consumeError(name.takeError()); continue; }
            if(section.isText()) { code_bytes += section.getSize(); }
            if(*name == ".gcc_except_table") { lsda_bytes += section.getSize(); }
            if(*name == ".eh_frame") { eh_bytes += section.getSize(); }
        }
        auto const buffer{object.getMemoryBufferRef().getBuffer()};
        ::fast_io::native_file output{::fast_io::mnp::os_c_str(file), ::fast_io::open_mode::out};
        ::fast_io::io::print(output, ::fast_io::mnp::strvw(::std::string_view{buffer.data(), buffer.size()}));
    }
};
int main(int argc, char** argv)
{
    CHECK(argc == 3);
    auto const choice{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[1])}};
    bool const probed_frame{choice == "large-probed-frame" || choice == "large-probed-frame-fp"};
    bool const large_frame{choice == "large-frame" || probed_frame};
    bool const long_code{choice == "long-code"};
    CHECK(large_frame || long_code || choice == "small");
    CHECK(!::llvm::InitializeNativeTarget());
    CHECK(!::llvm::InitializeNativeTargetAsmPrinter());
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("native-foreign-cleanup", context)};
    ::llvm::IRBuilder<> ir{context};
    auto* const pointer{ir.getPtrTy()};
    auto* const root_type{::llvm::FunctionType::get(ir.getVoidTy(), {pointer, pointer, pointer}, false)};
    auto* const recursive_type{::llvm::FunctionType::get(ir.getVoidTy(), {pointer, ir.getInt32Ty(), pointer, pointer}, false)};
    auto* const capture_type{::llvm::FunctionType::get(ir.getVoidTy(), {pointer}, false)};
    auto* const cleanup_type{::llvm::FunctionType::get(ir.getVoidTy(), {pointer, ir.getInt32Ty()}, false)};
    auto* const personality{::llvm::Function::Create(::llvm::FunctionType::get(ir.getInt32Ty(), true),
        ::llvm::GlobalValue::ExternalLinkage, "__gxx_personality_v0", *module)};
    auto* const recursive{::llvm::Function::Create(recursive_type, ::llvm::GlobalValue::ExternalLinkage, "native_cleanup_recursive", *module)};
    auto* const root{::llvm::Function::Create(root_type, ::llvm::GlobalValue::ExternalLinkage, "native_cleanup_root", *module)};
    for(auto* function : {recursive, root})
    {
        function->addFnAttr(::llvm::Attribute::NoInline);
        function->addFnAttr("disable-tail-calls", "true");
        function->setUWTableKind(::llvm::UWTableKind::Async);
        function->setPersonalityFn(personality);
    }
    if(probed_frame)
    {
        // Match real Wasm locals/spills: a split CSR-save allocation followed
        // by a large probe loop. An unprobed alloca does not exercise its CFI.
        recursive->addFnAttr("probe-stack", "inline-asm");
        recursive->addFnAttr("stack-probe-size", "2048");
        if(choice == "large-probed-frame-fp") { recursive->addFnAttr("frame-pointer", "all"); }
    }
    auto* const entry{::llvm::BasicBlock::Create(context, "entry", recursive)};
    auto* const leaf{::llvm::BasicBlock::Create(context, "leaf", recursive)};
    auto* const recurse{::llvm::BasicBlock::Create(context, "recurse", recursive)};
    auto* const normal{::llvm::BasicBlock::Create(context, "normal", recursive)};
    auto* const unwind{::llvm::BasicBlock::Create(context, "cleanup", recursive)};
    ir.SetInsertPoint(entry);
    auto const slots{large_frame ? 10000u : 1u};
    ::llvm::AllocaInst* last{};
    for(unsigned index{}; index != slots; ++index)
    {
        last = ir.CreateAlloca(ir.getInt64Ty());
        ir.CreateStore(ir.getInt64(index), last)->setVolatile(true);
    }
    if(long_code)
    {
        for(unsigned index{}; index != 10000u; ++index)
        { ir.CreateStore(ir.getInt64(index), last)->setVolatile(true); }
    }
    ir.CreateLoad(ir.getInt64Ty(), last)->setVolatile(true);
    ir.CreateCondBr(ir.CreateICmpEQ(recursive->getArg(1u), ir.getInt32(0u)), leaf, recurse);
    ir.SetInsertPoint(leaf);
    ir.CreateInvoke(capture_type, recursive->getArg(2u), normal, unwind, {recursive->getArg(0u)});
    ir.SetInsertPoint(recurse);
    ir.CreateInvoke(recursive, normal, unwind, {recursive->getArg(0u),
        ir.CreateSub(recursive->getArg(1u), ir.getInt32(1u)), recursive->getArg(2u), recursive->getArg(3u)});
    ir.SetInsertPoint(normal);
    ir.CreateRetVoid();
    ir.SetInsertPoint(unwind);
    auto const record{ir.CreateLandingPad(::llvm::StructType::get(context, {pointer, ir.getInt32Ty()}), 0u)};
    record->setCleanup(true);
    ir.CreateCall(cleanup_type, recursive->getArg(3u), {recursive->getArg(0u), recursive->getArg(1u)})->setDoesNotThrow();
    ir.CreateResume(record);

    auto* const root_normal{::llvm::BasicBlock::Create(context, "normal", root)};
    auto* const root_unwind{::llvm::BasicBlock::Create(context, "cleanup", root)};
    ir.SetInsertPoint(::llvm::BasicBlock::Create(context, "entry", root, root_normal));
    ir.CreateInvoke(recursive, root_normal, root_unwind,
        {root->getArg(0u), ir.getInt32(2u), root->getArg(1u), root->getArg(2u)});
    ir.SetInsertPoint(root_normal);
    ir.CreateRetVoid();
    ir.SetInsertPoint(root_unwind);
    auto const root_record{ir.CreateLandingPad(::llvm::StructType::get(context, {pointer, ir.getInt32Ty()}), 0u)};
    root_record->setCleanup(true);
    ir.CreateCall(cleanup_type, root->getArg(2u), {root->getArg(0u), ir.getInt32(3u)})->setDoesNotThrow();
    ir.CreateResume(root_record);
    CHECK(!::llvm::verifyModule(*module));
    ::llvm::SmallVector<::std::string, 32u> attributes{};
    for(auto const& [name, enabled] : ::llvm::sys::getHostCPUFeatures())
    { attributes.emplace_back((enabled ? "+" : "-") + name.str()); }
    ::llvm::Triple const triple{::llvm::Triple::normalize(::llvm::sys::getDefaultTargetTriple())};
    CHECK(triple.isOSLinux() && (triple.isRISCV64() || triple.isX86_64()));
    support::llvm_jit_append_native_host_vector_features(triple, ::llvm::sys::getHostCPUName(), attributes);
    ::llvm::EngineBuilder selector{};
    selector.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
    support::llvm_jit_mcjit_configure_code_model(selector, triple);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{selector.selectTarget(triple, {},
        support::llvm_jit_abi_host_cpu_name(triple, ::llvm::sys::getHostCPUName()), attributes)}};
    CHECK(target);
    module->setTargetTriple(target->getTargetTriple());
    module->setDataLayout(target->createDataLayout());
    // Keep temporary labels in this private diagnostic object for inspection.
    // ROS no longer exposes the legacy conditional-label helper.
    target->Options.MCOptions.MCSaveTempLabels = true;
    auto memory{::std::make_unique<support::runtime_llvm_jit_section_memory_manager>()};
    auto* const observer{memory.get()};
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::move(memory)).create(target.release())}};
    CHECK(engine);
    engine->addGlobalMapping(personality, reinterpret_cast<void*>(&::__gxx_personality_v0));
    object_capture object{}; object.file = argv[2];
    engine->RegisterJITEventListener(&object);
    engine->finalizeObject();
    engine->UnregisterJITEventListener(&object);
    CHECK(!engine->hasError() && !observer->has_finalization_failure());
    CHECK(object.code_bytes != 0u && object.lsda_bytes != 0u && object.eh_bytes != 0u);
    trace_state state{};
    state.recursive = engine->getFunctionAddress("native_cleanup_recursive");
    state.root = engine->getFunctionAddress("native_cleanup_root");
    CHECK(state.recursive != 0u && state.root != 0u);
    using capture_type_t = void(*)(void*);
    using cleanup_type_t = void(*)(void*, unsigned) noexcept;
    using root_type_t = void(*)(void*, capture_type_t, cleanup_type_t);
    bool caught{};
    ::fast_io::io::println("diagnostic case=", choice, " code=", object.code_bytes,
        " lsda=", object.lsda_bytes, " eh=", object.eh_bytes);
    try { reinterpret_cast<root_type_t>(state.root)(&state, &capture, &cleanup); }
    catch(foreign_signal const&) { caught = true; }
    CHECK(caught && !state.bad_order && state.recursive_frames == 3u && state.root_frames == 1u && state.cleanups == 4u);
    engine.reset();
    ::fast_io::io::println("PASS private native diagnostic: three recursive frames, four actual cleanups, foreign signal caught");
}
