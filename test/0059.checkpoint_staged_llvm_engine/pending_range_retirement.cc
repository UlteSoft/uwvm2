// Real MCJIT loaded-object ownership plus injected retirement notification.
// The injected event uses the synchronous real object key and borrowed loaded
// object; this component does not claim an actual VM restore or OS code unload.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <fast_io.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/lib/uwvm_runtime_pending_code_ranges.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_function_address.h>

#if !defined(UWVM_RUNTIME_LLVM_JIT) || !defined(UWVM_CPP_EXCEPTIONS)
#error "Real LLVM backend and C++ exceptions are required"
#endif
namespace
{
    using pending_t = ::uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges;
    constexpr ::std::array<char const*,4u> names{
        "owned_typed", "owned_raw", "owned_typed.checkpoint.resume.v2", "owned_typed.checkpoint.resume.v2.raw"};
    enum class event_order { ordinary, before_collection, after_collection };
    struct observed_range { ::std::uintptr_t begin{}, size{}; };
    struct probe final : ::llvm::JITEventListener
    {
        pending_t* pending{};
        event_order order{};
        ::std::vector<observed_range> functions{};
        bool called{}, failed{}, prefix_owned{}, replay_refused{};
        void notifyObjectLoaded(ObjectKey key, ::llvm::object::ObjectFile const& object,
            ::llvm::RuntimeDyld::LoadedObjectInfo const& loaded) noexcept override
        {
            try
            {
                called = true;
                ::uwvm2::runtime::lib::details::for_each_llvm_jit_loaded_function_range(object, loaded,
                    [&](::std::uintptr_t begin, ::std::uintptr_t size) { functions.push_back({begin,size}); });
                if(functions.empty() || pending == nullptr) { failed = true; return; }
                prefix_owned = pending->owns_pending_loaded_function_entry(functions.front().begin);
                if(order == event_order::ordinary) { return; }
                // Fault injection, not an invented range, key, or native entry.
                // The still-private engine remains alive for listener teardown.
                pending->notifyFreeingObject(key);
                // Redeliver this SAME real notification while its object and
                // LoadedObjectInfo are still alive. A revoked candidate must
                // never resurrect text/functions from a later notification.
                pending->notifyObjectLoaded(key,object,loaded);
                replay_refused = pending->has_observation_failure() && pending->empty() &&
                    !pending->owns_pending_loaded_function_entry(functions.front().begin);
            }
            catch(...) { failed = true; }
        }
    };
    struct actual_engine
    {
        ::llvm::LLVMContext context{};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{};
        ::std::unique_ptr<pending_t> pending{};
        probe observer{};
        bool attached{};
        explicit actual_engine(event_order order, unsigned base)
        {
            auto module{::std::make_unique<::llvm::Module>("real-private-entry-ranges",context)};
            auto* const type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context),false)};
            for(::std::size_t n{};n != names.size();++n)
            {
                auto* const function{::llvm::Function::Create(type,::llvm::Function::ExternalLinkage,names[n],*module)};
                auto* const entry{::llvm::BasicBlock::Create(context,"entry",function)};
                ::llvm::IRBuilder<> ir{entry};ir.CreateRet(::llvm::ConstantInt::get(type->getReturnType(),base+n));
            }
            ::std::string error{}; // Required LLVM EngineBuilder output parameter.
            engine.reset(::llvm::EngineBuilder(::std::move(module)).setEngineKind(::llvm::EngineKind::JIT)
                .setErrorStr(&error).setMCJITMemoryManager(::std::make_unique<::llvm::SectionMemoryManager>()).create());
            if(!engine) { return; }
            observer.order=order;
            if(order == event_order::before_collection)
            { engine->RegisterJITEventListener(&observer);attached=true; }
            pending=::std::make_unique<pending_t>(*engine,true);
            pending->configure_debug_full_capture(true);
            observer.pending=pending.get();
            if(!attached) { engine->RegisterJITEventListener(&observer);attached=true; }
            engine->finalizeObject();
        }
        ~actual_engine()
        { if(attached) { engine->UnregisterJITEventListener(&observer); } }
        actual_engine(actual_engine const&)=delete;
        [[nodiscard]] bool ready() const noexcept
        { return engine && pending && !engine->hasError() && observer.called && !observer.failed; }
        [[nodiscard]] ::std::uintptr_t entry(::std::size_t index)
        {
            if(index >= names.size()) { return 0u; }
            auto* const definition{engine->FindFunctionNamed(names[index])};
            if(definition == nullptr || definition->isDeclaration()) { return 0u; }
            auto const callable{reinterpret_cast<::std::uintptr_t>(engine->getPointerToFunction(definition))};
            return callable == 0u ? 0u : ::uwvm2::runtime::lib::details::native_function_code_address(callable);
        }
    };
    int check_ordinary()
    {
        actual_engine first{event_order::ordinary,37u},foreign{event_order::ordinary,42u};
        if(!first.ready() || !foreign.ready() || !first.observer.prefix_owned || first.pending->has_observation_failure()) { return 1; }
        for(::std::size_t n{};n != names.size();++n)
        {
            auto const own{first.entry(n)},other{foreign.entry(n)};
            if(own == 0u || other == 0u || own == other || !first.pending->owns_pending_loaded_function_entry(own) ||
                first.pending->owns_pending_loaded_function_entry(other) || !foreign.pending->owns_pending_loaded_function_entry(other)) { return 2; }
        }
        if(first.pending->owns_pending_loaded_function_entry(0u) || first.engine->FindFunctionNamed("missing_private_entry") != nullptr) { return 3; }
        for(auto const& function:first.observer.functions)
        {
            if(function.size > 1u && function.begin != UINTPTR_MAX && first.pending->owns_pending_loaded_function_entry(function.begin+1u))
            { return 4; }
        }
        ::std::size_t text{},functions{};
        first.pending->commit([&](auto,auto) noexcept { ++text; },[&](auto,auto) noexcept { ++functions; });
        if(text == 0u || functions < names.size() || first.pending->owns_pending_loaded_function_entry(first.entry(0u))) { return 5; }
        return 0;
    }
    int check_retirement(event_order order)
    {
        actual_engine actual{order,37u};
        if(!actual.ready() || actual.observer.prefix_owned != (order == event_order::after_collection)) { return 6; }
        ::std::size_t text{},functions{};
#if defined(UWVM_TEST_RETIRED_RANGE_BASELINE)
        if(actual.observer.replay_refused || actual.pending->has_observation_failure() || actual.pending->empty() ||
            !actual.pending->owns_pending_loaded_function_entry(actual.entry(0u))) { return 7; }
        actual.pending->commit([&](auto,auto) noexcept { ++text; },[&](auto,auto) noexcept { ++functions; });
        if(text == 0u || functions < names.size()) { return 8; }
#else
        if(!actual.observer.replay_refused || !actual.pending->has_observation_failure() || !actual.pending->empty()) { return 9; }
        for(::std::size_t n{};n != names.size();++n)
        { if(actual.pending->owns_pending_loaded_function_entry(actual.entry(n))) { return 10; } }
        actual.pending->commit([&](auto,auto) noexcept { ++text; },[&](auto,auto) noexcept { ++functions; });
        auto rows{actual.pending->take_debug_full_capture(1u)};
        if(text != 0u || functions != 0u || rows.valid()) { return 11; }
#endif
        return 0;
    }
}
int main()
{
    if(::llvm::InitializeNativeTarget() || ::llvm::InitializeNativeTargetAsmPrinter()) { return 12; }
    if(auto result{check_ordinary()};result != 0) { return result; }
    if(auto result{check_retirement(event_order::before_collection)};result != 0) { return 20+result; }
    if(auto result{check_retirement(event_order::after_collection)};result != 0) { return 40+result; }
#if defined(UWVM_TEST_RETIRED_RANGE_BASELINE)
    ::fast_io::io::println("BASELINE reproduced stale-range publication in both retirement orders; real MCJIT objects");
#else
    ::fast_io::io::println("PASS real MCJIT four-entry ownership, foreign/interior/zero refusal, whole-candidate retirement and replay refusal");
#endif
}
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <uwvm2/utils/macro/pop_macros.h>
