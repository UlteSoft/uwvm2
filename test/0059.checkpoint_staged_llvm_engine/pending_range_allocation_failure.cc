// Standalone real MCJIT listener component. No VM publication, saved epoch,
// function ABI certificate or synthetic loaded-object metadata is constructed.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <fast_io.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <uwvm2/utils/macro/push_macros.h>
// This test is built with the real LLVM backend/C++ exception prerequisites.
// Do not manufacture derived runtime macros for a disabled backend build.
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_pending_code_ranges.h>

#if !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_CPP_EXCEPTIONS)
#error "Real LLVM backend and C++ exceptions are required for listener recovery"
#endif
namespace
{
    thread_local bool fail_allocation{};
    thread_local ::std::size_t successful_allocations{}, remaining_allocations{};
}
// Test-only replaceable allocation failure. Ordinary storage uses the bundled
// FastIO native allocator; the armed window is only the real pending callback.
void* operator new(::std::size_t size)
{
    if(fail_allocation)
    {
        if(remaining_allocations == 0u) { throw ::std::bad_alloc{}; }
        --remaining_allocations; ++successful_allocations;
    }
    return ::fast_io::native_global_allocator::allocate(size == 0u ? 1u : size);
}
void* operator new[](::std::size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { ::fast_io::native_global_allocator::deallocate(value); }
void operator delete[](void* value) noexcept { ::operator delete(value); }
void operator delete(void* value, ::std::size_t) noexcept { ::operator delete(value); }
void operator delete[](void* value, ::std::size_t) noexcept { ::operator delete(value); }

namespace
{
    using pending_t = ::uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges;
    struct callback_failure {};
    struct probe final : ::llvm::JITEventListener
    {
        pending_t* pending{};
        ::std::size_t fail_after{};
        bool inject{}, called{}, callback_caught{}, saw_loaded_text{};
        ::std::uintptr_t actual_function_begin{};
        void notifyObjectLoaded(ObjectKey key, ::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
        {
            called = true;
            for(auto const& section: object.sections())
            { if(section.isText() && loaded.getSectionLoadAddress(section) != 0u && section.getSize() != 0u) { saw_loaded_text = true; } }
            // Actual callback propagation control, using ONLY this notification's
            // real object and real relocation. No invented loaded range/section.
            try
            {
                ::uwvm2::runtime::lib::details::for_each_llvm_jit_loaded_function_range(object, loaded,
                    [&](::std::uintptr_t begin, ::std::uintptr_t)
                    { actual_function_begin = begin; throw callback_failure{}; });
            }
            catch(callback_failure const&) { callback_caught = true; }
            catch(...) { return; }
            if(!inject || pending == nullptr) { return; }
            remaining_allocations = fail_after; successful_allocations = 0u;
            struct disarm { ~disarm() { fail_allocation = false; } } reset{};
            fail_allocation = true;
            // Register this probe BEFORE pending so the manual notification
            // reaches an empty candidate. MCJIT subsequently invokes pending
            // again; a sticky failure must refuse the same object at that point.
            pending->notifyObjectLoaded(key, object, loaded);
        }
    };
    int run(bool inject, ::std::size_t fail_after)
    {
        ::llvm::LLVMContext context{};
        auto module{::std::make_unique<::llvm::Module>("pending-allocation-real-object", context)};
        auto* const type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context), false)};
        auto* const function{::llvm::Function::Create(type, ::llvm::Function::ExternalLinkage, "owned_pending_answer", *module)};
        auto* const entry{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> ir{entry}; ir.CreateRet(::llvm::ConstantInt::get(type->getReturnType(),42u));
        ::std::string error{};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder(::std::move(module))
            .setEngineKind(::llvm::EngineKind::JIT).setErrorStr(&error)
            .setMCJITMemoryManager(::std::make_unique<::llvm::SectionMemoryManager>()).create()};
        if(engine == nullptr) { return 1; }
        probe observer{}; observer.inject = inject; observer.fail_after = fail_after;
        engine->RegisterJITEventListener(&observer);
        struct detach_probe
        {
            ::llvm::ExecutionEngine& owner; probe& observer;
            ~detach_probe() { owner.UnregisterJITEventListener(&observer); }
        } detach{*engine,observer};
        pending_t pending{*engine,true}; observer.pending = &pending;
        engine->finalizeObject();
        if(engine->hasError() || !observer.called || !observer.saw_loaded_text || !observer.callback_caught ||
           observer.actual_function_begin == 0u || fail_allocation) { return 2; }
        if(inject)
        {
            // First failure and failure AFTER one successful collection both
            // discard the complete candidate, including any collected prefix.
            if(!pending.has_observation_failure() || !pending.empty() || successful_allocations != fail_after ||
               pending.owns_pending_loaded_function_entry(observer.actual_function_begin)) { return 3; }
            ::std::size_t text_publications{}, function_publications{};
            pending.commit([&](auto,auto) noexcept { ++text_publications; },
                [&](auto,auto) noexcept { ++function_publications; });
            auto rows{pending.take_debug_full_capture(1u)};
            if(text_publications != 0u || function_publications != 0u || rows.valid()) { return 4; }
        }
        else
        {
            if(pending.has_observation_failure() || pending.empty() ||
               !pending.owns_pending_loaded_function_entry(observer.actual_function_begin)) { return 5; }
            ::std::size_t text_publications{}, function_publications{};
            pending.commit([&](auto,auto) noexcept { ++text_publications; },
                [&](auto,auto) noexcept { ++function_publications; });
            if(text_publications == 0u || function_publications == 0u) { return 6; }
        }
        return 0;
    }
}
int main()
{
    if(::llvm::InitializeNativeTarget() || ::llvm::InitializeNativeTargetAsmPrinter()) { return 7; }
    if(auto const status{run(false,0u)}; status != 0) { return status; }
    if(auto const status{run(true,0u)}; status != 0) { return 10 + status; }
    if(auto const status{run(true,1u)}; status != 0) { return 20 + status; }
    return 0;
}
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
