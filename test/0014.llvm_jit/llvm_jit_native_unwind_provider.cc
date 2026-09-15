// Test the actual linked unwinder with the production calling-convention
// policy and section manager. Run with a matching LLVM/unwinder toolchain:
// preloading a different registration ABI over a libgcc-configured LLVM is a
// different (fallback) test. Synthetic IR here does not replace Wasm trap tests.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <array>
#include <cstdio>
#include <memory>

#if defined(_WIN32) || !__has_include(<unwind.h>)
int main() { return 77; }
#else
#include <unwind.h>
namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
using manager_type = uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager;

namespace
{
    // Optional first argument retains the first actual MCJIT object for CFI
    // and assembly inspection. This in-memory cache tests repeated relocation,
    // not authentication/security of the production persistent cache.
    struct diagnostic_object_cache final : llvm::ObjectCache
    {
        char const* path{};
        unsigned policy{};
        unsigned hits{};
        unsigned compilations{};
        std::array<std::unique_ptr<llvm::MemoryBuffer>, 10> cached{};
        unsigned slot(llvm::Module const& module) const
        { return policy * 2u + (module.getModuleIdentifier() == "native-provider-wrapper" ? 1u : 0u); }
        void notifyObjectCompiled(llvm::Module const* module, llvm::MemoryBufferRef object) override
        {
            ++compilations;
            cached[slot(*module)] = llvm::MemoryBuffer::getMemBufferCopy(object.getBuffer());
            if(path == nullptr) { return; }
            std::error_code error;
            llvm::raw_fd_ostream output{path, error, llvm::sys::fs::OF_None};
            if(error) { std::fprintf(stderr, "object dump: %s\n", error.message().c_str()); std::abort(); }
            output << object.getBuffer();
            path = nullptr;
        }
        std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const* module) override
        {
            auto const& object{cached[slot(*module)]};
            if(!object) { return nullptr; }
            ++hits;
            return llvm::MemoryBuffer::getMemBufferCopy(object->getBuffer());
        }
    };
    std::array<std::uintptr_t, 64> regions{};
    std::array<std::uintptr_t, 64> ips{};
    std::size_t region_count{};
    _Unwind_Reason_Code collect(_Unwind_Context* frame, void*) noexcept
    {
        if(region_count == regions.size()) { return _URC_END_OF_STACK; }
        ips[region_count] = static_cast<std::uintptr_t>(_Unwind_GetIP(frame));
        regions[region_count++] = static_cast<std::uintptr_t>(_Unwind_GetRegionStart(frame));
        return _URC_NO_REASON;
    }
}

extern "C" UWVM_NOINLINE void uwvm_test_capture_native_regions() noexcept
{
    region_count = 0;
    static_cast<void>(_Unwind_Backtrace(collect, nullptr));
}

int main(int argc, char** argv)
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::sys::DynamicLibrary::AddSymbol("uwvm_test_capture_native_regions",
        reinterpret_cast<void*>(&uwvm_test_capture_native_regions));
    unsigned checked{};
    diagnostic_object_cache objects;
    if(argc > 1) { objects.path = argv[1]; }
    for(unsigned lifetime{}; lifetime != 4u; ++lifetime)
    {
        for(auto level : {llvm::OptimizationLevel::O1, llvm::OptimizationLevel::O2,
                          llvm::OptimizationLevel::O3, llvm::OptimizationLevel::Os, llvm::OptimizationLevel::Oz})
        {
            llvm::LLVMContext context;
            auto module{std::make_unique<llvm::Module>("native-provider-recursion", context)};
            // As in the runtime, select the native object format and data
            // layout before optimizing. An empty triple can make the pass
            // pipeline apply assumptions for a different unwind/object ABI.
            llvm::EngineBuilder target_builder;
            auto target{std::unique_ptr<llvm::TargetMachine>{target_builder.selectTarget()}};
            if(!target) { return 7; }
#if LLVM_VERSION_MAJOR >= 21
            module->setTargetTriple(target->getTargetTriple());
#else
            module->setTargetTriple(target->getTargetTriple().str());
#endif
            module->setDataLayout(target->createDataLayout());
            llvm::IRBuilder<> b{context};
            auto signature{llvm::FunctionType::get(b.getVoidTy(), {b.getInt32Ty()}, false)};
            auto recursive{llvm::Function::Create(signature, llvm::Function::ExternalLinkage, "recursive", *module)};
            d::apply_llvm_jit_wasm_calling_conv(*recursive);
            d::apply_llvm_jit_unwind_call_stack_function_attrs(*recursive);
            auto entry{llvm::BasicBlock::Create(context, "entry", recursive)};
            auto leaf{llvm::BasicBlock::Create(context, "leaf", recursive)};
            auto again{llvm::BasicBlock::Create(context, "again", recursive)};
            b.SetInsertPoint(entry);
            b.CreateCondBr(b.CreateICmpEQ(recursive->getArg(0), b.getInt32(0u)), leaf, again);
            b.SetInsertPoint(leaf);
            auto capture{module->getOrInsertFunction("uwvm_test_capture_native_regions",
                llvm::FunctionType::get(b.getVoidTy(), false))};
            // The host callback uses C ABI; private Wasm recursion uses the
            // production callsite policy, including its notail requirement.
            b.CreateCall(capture)->setTailCallKind(llvm::CallInst::TCK_NoTail);
            b.CreateRetVoid();
            b.SetInsertPoint(again);
            d::apply_llvm_jit_wasm_calling_conv(b.CreateCall(recursive, {b.CreateSub(recursive->getArg(0), b.getInt32(1u))}));
            b.CreateRetVoid();

            auto wrapper{llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(), false),
                llvm::Function::ExternalLinkage, "host_entry", *module)};
            d::apply_llvm_jit_calling_conv(*wrapper, llvm::CallingConv::C);
            d::apply_llvm_jit_unwind_call_stack_function_attrs(*wrapper);
            b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", wrapper));
            d::apply_llvm_jit_wasm_calling_conv(b.CreateCall(recursive, {b.getInt32(3u)}));
            b.CreateRetVoid();

            llvm::LoopAnalysisManager loops;
            llvm::FunctionAnalysisManager functions;
            llvm::CGSCCAnalysisManager cgscc;
            llvm::ModuleAnalysisManager modules;
            llvm::PassBuilder passes{target.get()};
            passes.registerModuleAnalyses(modules);
            passes.registerCGSCCAnalyses(cgscc);
            passes.registerFunctionAnalyses(functions);
            passes.registerLoopAnalyses(loops);
            passes.crossRegisterProxies(loops, functions, cgscc, modules);
            auto pipeline{passes.buildPerModuleDefaultPipeline(level)};
            pipeline.run(*module, modules);
            if(llvm::verifyModule(*module, &llvm::errs())) { return 1; }

            // Match parallel/full and lazy loading: separate objects with a
            // cross-object recursive callee must each receive their own CFI
            // relocations, including when the object bytes come from a cache.
            llvm::ValueToValueMapTy values;
            auto wrapper_module{llvm::CloneModule(*module, values, [](llvm::GlobalValue const* global)
                { return global->getName() == "host_entry"; })};
            wrapper_module->setModuleIdentifier("native-provider-wrapper");
            module->getFunction("host_entry")->deleteBody();
            objects.policy = checked % 5u;
            auto manager{std::make_unique<manager_type>()};
            auto manager_observer{manager.get()};
            std::string error;
            llvm::EngineBuilder engine_builder{std::move(module)};
            engine_builder.setErrorStr(&error);
            engine_builder.setMCJITMemoryManager(std::move(manager));
            auto engine{std::unique_ptr<llvm::ExecutionEngine>{engine_builder.create(target.release())}};
            if(!engine) { std::fprintf(stderr, "%s\n", error.c_str()); return 2; }
            engine->setObjectCache(&objects);
            engine->addModule(std::move(wrapper_module));
            engine->finalizeObject();
            if(manager_observer->has_finalization_failure()) { return 3; }
            auto recursive_address{engine->getFunctionAddress("recursive")};
            auto wrapper_address{engine->getFunctionAddress("host_entry")};
            if(!recursive_address || !wrapper_address) { return 4; }
            reinterpret_cast<void(*)()>(wrapper_address)();
            unsigned frames{};
            for(std::size_t i{}; i != region_count; ++i)
            {
                auto address{regions[i]};
                if(address != recursive_address && address != wrapper_address) { continue; }
                if(address != (frames < 4u ? recursive_address : wrapper_address) || frames == 5u) { return 5; }
                ++frames;
            }
            if(frames != 5u)
            {
                std::fprintf(stderr, "native provider lost recursion: got %u/5 JIT frames\n", frames);
                std::fprintf(stderr, "recursive=%llx wrapper=%llx\n", static_cast<unsigned long long>(recursive_address),
                    static_cast<unsigned long long>(wrapper_address));
                for(std::size_t i{}; i != region_count; ++i)
                { std::fprintf(stderr, "frame[%zu] ip=%llx region=%llx\n", i, static_cast<unsigned long long>(ips[i]), static_cast<unsigned long long>(regions[i])); }
                return 6;
            }
            ++checked;
            // Destruction deregisters CFI before releasing its memory. Rebuild
            // repeatedly to exercise address reuse and registration ownership.
        }
    }
    if(objects.compilations != 10u || objects.hits != 30u) { return 8; }
    std::printf("PASS native unwinder: %u recursive chains, five exact JIT frames each; %u object compilations, %u cache replays\n",
                checked, objects.compilations, objects.hits);
}
#endif
