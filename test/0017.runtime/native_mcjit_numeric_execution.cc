// Real native MCJIT numeric execution: the production relocation/code model
// must preserve constant-pool values across the function's host callback.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <bit>
#include <atomic>
#include <cstdint>
#include <string>
#include <memory>
#include <fast_io.h>
namespace support = ::uwvm2::runtime::compiler::llvm_jit::details;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native MCJIT numeric execution failure line=",__LINE__); return 1; } } while(false)
static void host_boundary() noexcept { ::std::atomic_signal_fence(::std::memory_order_seq_cst); }
struct object_capture final : ::llvm::JITEventListener
{
    char const* file{};
    void notifyObjectLoaded(ObjectKey,::llvm::object::ObjectFile const& object,
        ::llvm::RuntimeDyld::LoadedObjectInfo const&) override
    {
        if(file==nullptr) { return; }
        auto const buffer{object.getMemoryBufferRef().getBuffer()};
        ::fast_io::native_file output{::fast_io::mnp::os_c_str(file),::fast_io::open_mode::out};
        ::fast_io::io::print(output,::fast_io::mnp::strvw(::std::string_view{buffer.data(),buffer.size()}));
    }
};
int main(int argc,char** argv)
{
    CHECK(argc<=2);
    CHECK(!::llvm::InitializeNativeTarget());
    CHECK(!::llvm::InitializeNativeTargetAsmPrinter());
    auto context{::std::make_unique<::llvm::LLVMContext>()};
    auto module{::std::make_unique<::llvm::Module>("native-numeric-execution",*context)};
    ::llvm::IRBuilder<> ir{*context};
    auto* const callback_type{::llvm::FunctionType::get(ir.getVoidTy(),{},false)};
    auto* const pointer{ir.getPtrTy()};
    for(bool wide: {false,true})
    for(bool boundary: {false,true})
    {
        auto* const type{wide ? ir.getDoubleTy() : ir.getFloatTy()};
        auto* const signature{::llvm::FunctionType::get(type,{type,pointer},false)};
        auto* const function{::llvm::Function::Create(signature,::llvm::GlobalValue::ExternalLinkage,
            wide ? (boundary ? "native_f64" : "native_f64_direct") : (boundary ? "native_f32" : "native_f32_direct"),*module)};
        function->setUWTableKind(::llvm::UWTableKind::Async);
        ir.SetInsertPoint(::llvm::BasicBlock::Create(*context,"entry",function));
        auto* const result{ir.CreateFMul(function->getArg(0u),::llvm::ConstantFP::get(type,wide ? 2.25 : 1.5))};
        if(boundary)
        { auto* const call{ir.CreateCall(callback_type,function->getArg(1u),{})};call->setDoesNotThrow(); }
        ir.CreateRet(result);
    }
    CHECK(!::llvm::verifyModule(*module));
    ::llvm::Triple const triple{::llvm::sys::getProcessTriple()};
    ::llvm::SmallVector<::std::string,32u> attributes{};
    for(auto const& [name,enabled]: ::llvm::sys::getHostCPUFeatures())
    { attributes.emplace_back((enabled ? "+" : "-")+name.str()); }
    support::llvm_jit_append_native_host_vector_features(triple,::llvm::sys::getHostCPUName(),attributes);
    ::llvm::EngineBuilder target_builder{};
    target_builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
    support::llvm_jit_mcjit_configure_code_model(target_builder,triple);
    support::llvm_jit_mcjit_configure_host_unwind_abi(target_builder);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{target_builder.selectTarget(triple,{},
        support::llvm_jit_abi_host_cpu_name(triple,::llvm::sys::getHostCPUName()),attributes)}};
    CHECK(target);module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::make_unique<support::runtime_llvm_jit_section_memory_manager>())
        .create(target.release())}};
    CHECK(engine);object_capture capture{};if(argc==2) { capture.file=argv[1]; }
    engine->RegisterJITEventListener(&capture);engine->finalizeObject();engine->UnregisterJITEventListener(&capture);CHECK(!engine->hasError());
    auto const f32_direct{engine->getFunctionAddress("native_f32_direct")};
    auto const f64_direct{engine->getFunctionAddress("native_f64_direct")};CHECK(f32_direct && f64_direct);
    auto const f32{engine->getFunctionAddress("native_f32")};
    auto const f64{engine->getFunctionAddress("native_f64")};CHECK(f32 && f64);
    using callback = void(*)() noexcept;
    auto const single{reinterpret_cast<float(*)(float,callback)>(f32)(17.0f,&host_boundary)};
    auto const wide{reinterpret_cast<double(*)(double,callback)>(f64)(17.0,&host_boundary)};
    bool const single_direct_exact{::std::bit_cast<::std::uint32_t>(reinterpret_cast<float(*)(float,callback)>(f32_direct)(17.0f,&host_boundary))==0x41cc0000u};
    bool const wide_direct_exact{::std::bit_cast<::std::uint64_t>(reinterpret_cast<double(*)(double,callback)>(f64_direct)(17.0,&host_boundary))==0x4043200000000000ull};
    bool const single_exact{::std::bit_cast<::std::uint32_t>(single)==0x41cc0000u};
    bool const wide_exact{::std::bit_cast<::std::uint64_t>(wide)==0x4043200000000000ull};
    ::fast_io::io::println("production native constant-pool and host-call f32=",single_exact," f64=",wide_exact);
    ::fast_io::io::println("production direct f32=",single_direct_exact," f64=",wide_direct_exact);
    CHECK(single_exact && wide_exact && single_direct_exact && wide_direct_exact);
}
