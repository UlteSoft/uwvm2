// Real CFI walks/EH while unrelated MCJIT engines register and retire FDEs.
#define UWVM_RUNTIME_LLVM_JIT
#include <uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h>
#include <uwvm2/runtime/lib/uwvm_runtime_pending_code_ranges.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <atomic>
#include <cxxabi.h>
#include <unwind.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
extern "C" _Unwind_Reason_Code __gxx_personality_v0(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
namespace symbols = uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;
namespace pads = uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad;
namespace
{
    void check(bool value) { if(!value) { std::fputs("FAIL concurrent native unwind\n", stderr); std::abort(); } }
    std::atomic<bool> observe_registry{}, finish{};
    std::atomic<unsigned> walks{}, registrations{}, retirements{}, active_executors{}, executions{};
    unsigned committed_ranges{}, failed_resolutions{};
    struct code_range { std::uintptr_t begin, end; };
    thread_local std::vector<code_range> const* executing_ranges{};
    struct guest { int payload; };
    struct foreign { int payload; };
    _Unwind_Reason_Code record_pc(_Unwind_Context* context, void* data)
    {
        auto const ip{static_cast<std::uintptr_t>(_Unwind_GetIP(context))};
        for(auto const range: *executing_ranges)
        { if(ip >= range.begin && ip < range.end) { *static_cast<bool*>(data) = true; } }
        return _URC_NO_REASON;
    }
    void raise_probe(unsigned mode)
    {
        // The real generated caller must appear BEFORE EH removes its activation.
        bool found{}; _Unwind_Backtrace(record_pc, &found); check(found);
        walks.fetch_add(1u, std::memory_order_release);
        if(mode == 1u) { throw guest{42}; }
        if(mode == 2u) { throw foreign{73}; }
    }
    std::type_info const* guest_type()
    {
        try { throw guest{0}; }
        catch(guest const&) { return __cxxabiv1::__cxa_current_exception_type(); }
    }
    struct memory final: llvm::SectionMemoryManager
    {
        std::vector<code_range> ranges;
        std::uint8_t* allocateCodeSection(std::uintptr_t size, unsigned alignment, unsigned id, llvm::StringRef name) override
        {
            auto* section{llvm::SectionMemoryManager::allocateCodeSection(size, alignment, id, name)};
            check(section != nullptr);
            auto const begin{reinterpret_cast<std::uintptr_t>(section)};
            ranges.push_back({begin, begin + size}); return section;
        }
        void overlap() noexcept
        {
            if(!observe_registry.load(std::memory_order_acquire)) { return; }
            auto const before{walks.load(std::memory_order_acquire)};
            // TEST-only handshake: execution remains active across each actual
            // registry operation, without serializing the unwinder and writer.
            while(walks.load(std::memory_order_acquire) < before + 4u) { std::this_thread::yield(); }
            check(active_executors.load(std::memory_order_acquire) != 0u);
        }
        void registerEHFrames(std::uint8_t* address, std::uint64_t load, std::size_t size) override
        {
            overlap(); llvm::SectionMemoryManager::registerEHFrames(address, load, size);
            if(observe_registry.load()) { registrations.fetch_add(1u); }
        }
        void deregisterEHFrames() override
        {
            overlap(); llvm::SectionMemoryManager::deregisterEHFrames();
            if(observe_registry.load()) { retirements.fetch_add(1u); }
        }
    };
    struct cache final: llvm::ObjectCache
    {
        std::string bytes; unsigned hits{}, writes{};
        void notifyObjectCompiled(llvm::Module const*, llvm::MemoryBufferRef object) override
        { bytes = object.getBuffer().str(); ++writes; }
        std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override
        {
            if(bytes.empty()) { return {}; }
            ++hits; return llvm::MemoryBuffer::getMemBufferCopy(bytes, "concurrent-unwind");
        }
    };
    struct instance
    {
        std::unique_ptr<llvm::LLVMContext> context;
        std::unique_ptr<llvm::ExecutionEngine> engine;
        std::vector<code_range> ranges;
        int (*entry)(unsigned){};
    };
    instance build(cache& objects, std::string const& prefix, bool fail_symbol = false)
    {
        instance result; result.context = std::make_unique<llvm::LLVMContext>();
        llvm::EngineBuilder selector;
        std::unique_ptr<llvm::TargetMachine> machine{selector.selectTarget()}; check(machine != nullptr);
        auto module{std::make_unique<llvm::Module>("concurrent-unwind", *result.context)};
#if LLVM_VERSION_MAJOR >= 21
        module->setTargetTriple(machine->getTargetTriple());
#else
        module->setTargetTriple(machine->getTargetTriple().str());
#endif
        module->setDataLayout(machine->createDataLayout());
        auto imports{symbols::declare_itanium_dwarf_symbols(*module, *machine)};
        auto runtime{pads::declare_catch_runtime(*module)};
        check(imports.status == symbols::error::ok && static_cast<bool>(runtime));
        llvm::IRBuilder<> builder{*result.context};
        auto* function{llvm::Function::Create(llvm::FunctionType::get(builder.getInt32Ty(), {builder.getInt32Ty()}, false),
            llvm::GlobalValue::ExternalLinkage, "entry", *module)};
        auto* host{llvm::Function::Create(llvm::FunctionType::get(builder.getVoidTy(), {builder.getInt32Ty()}, false),
            llvm::GlobalValue::ExternalLinkage, "raise", *module)};
        auto* entry{llvm::BasicBlock::Create(*result.context, "entry", function)};
        auto* normal{llvm::BasicBlock::Create(*result.context, "normal", function)};
        builder.SetInsertPoint(entry);
        auto caught{pads::emit_typed_entry(builder, *function, imports, runtime, [](auto&) {})};
        check(static_cast<bool>(caught));
        pads::emit_end_catch(builder, runtime); builder.CreateRet(builder.getInt32(42));
        builder.SetInsertPoint(entry); builder.CreateInvoke(host, normal, caught.landing, {function->getArg(0)});
        builder.SetInsertPoint(normal); builder.CreateRet(builder.getInt32(17));
        check(!llvm::verifyModule(*module, &llvm::errs()));
        std::error_code error; llvm::raw_fd_ostream output{prefix + ".ll", error};
        check(!error); module->print(output, nullptr); output.close();
        auto manager{std::make_unique<memory>()}; auto* observer{manager.get()};
        llvm::EngineBuilder engine_builder{std::move(module)};
        engine_builder.setEngineKind(llvm::EngineKind::JIT).setMCJITMemoryManager(std::move(manager));
        result.engine.reset(engine_builder.create(machine.release())); check(result.engine != nullptr);
        check(symbols::bind_itanium_dwarf_host_symbols(*result.engine, imports, guest_type(),
            reinterpret_cast<std::uintptr_t>(&__gxx_personality_v0)) == symbols::error::ok);
        result.engine->addGlobalMapping(host, reinterpret_cast<void*>(&raise_probe));
        result.engine->addGlobalMapping(runtime.begin, reinterpret_cast<void*>(&__cxxabiv1::__cxa_begin_catch));
        result.engine->addGlobalMapping(runtime.end, reinterpret_cast<void*>(&__cxxabiv1::__cxa_end_catch));
        result.engine->addGlobalMapping(runtime.rethrow, reinterpret_cast<void*>(&__cxxabiv1::__cxa_rethrow));
        result.engine->setObjectCache(&objects);
        uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges pending{*result.engine, true};
        result.engine->finalizeObject(); // Actual CFI registration precedes entry publication.
        check(!pending.empty());
        if(fail_symbol)
        {
            // The real engine has loaded executable ranges and registered CFI,
            // but a REQUIRED later symbol fails. Abort must publish no range.
            check(result.engine->getFunctionAddress("deliberately_missing_required_entry") == 0u);
            ++failed_resolutions;
            result.engine->setObjectCache(nullptr);
            return result; // Listener detaches before the engine can be destroyed.
        }
        result.entry = reinterpret_cast<int(*)(unsigned)>(result.engine->getFunctionAddress("entry"));
        check(result.entry != nullptr); result.ranges = observer->ranges;
        pending.commit([](std::uintptr_t begin, std::uintptr_t size) noexcept
        { check(begin != 0u && size != 0u); ++committed_ranges; });
        check(pending.empty());
        result.engine->setObjectCache(nullptr); return result;
    }
    void execute(instance const& current)
    {
        executing_ranges = &current.ranges;
        for(unsigned mode{}; mode != 3u; ++mode)
        {
            try { check(current.entry(mode) == (mode == 0u ? 17 : 42)); check(mode != 2u); }
            catch(foreign const& value) { check(mode == 2u && value.payload == 73); }
            executions.fetch_add(1u, std::memory_order_relaxed);
        }
        executing_ranges = nullptr;
    }
}
int main(int argc, char** argv)
{
    check(argc == 2); check(!llvm::InitializeNativeTarget()); check(!llvm::InitializeNativeTargetAsmPrinter());
    auto const prefix{std::string{argv[1]}};
    cache objects; auto stable{build(objects, prefix + "/stable")};
    check(objects.writes == 1u && objects.hits == 0u);
    { std::ofstream output{prefix + "/unwind.o", std::ios::binary}; output.write(objects.bytes.data(), objects.bytes.size()); }
    std::array<std::thread, 4> readers;
    for(auto& reader: readers)
    {
        reader = std::thread{[&]
        {
            active_executors.fetch_add(1u, std::memory_order_release);
            do { execute(stable); } while(!finish.load(std::memory_order_acquire));
            active_executors.fetch_sub(1u, std::memory_order_release);
        }};
    }
    while(active_executors.load(std::memory_order_acquire) != 4u) { std::this_thread::yield(); }
    observe_registry.store(true, std::memory_order_release);
    // Retire only unrelated engines; every reader's original engine/FDE remains
    // owned until its activation is gone and all four threads have joined.
    for(unsigned index{}; index != 48u; ++index)
    {
        cache next; if(index % 2u != 0u) { next.bytes = objects.bytes; }
        auto const before{committed_ranges};
        auto const fail_symbol{index % 6u == 0u};
        auto temporary{build(next, prefix + "/registration-" + std::to_string(index), fail_symbol)};
        check(index % 2u ? (next.hits == 1u && next.writes == 0u) : (next.hits == 0u && next.writes == 1u));
        if(fail_symbol) { check(temporary.entry == nullptr && committed_ranges == before); }
        else { check(committed_ranges > before); execute(temporary); }
    }
    observe_registry.store(false, std::memory_order_release); finish.store(true, std::memory_order_release);
    for(auto& reader: readers) { reader.join(); }
    check(registrations.load() >= 48u && retirements.load() >= 48u && failed_resolutions == 8u);
    std::printf("PASS concurrent CFI: %u registrations, %u retirements, %u native walks, %u JIT executions; 4 readers, 48 cold/warm engines, 8 failed-symbol rollbacks\n",
        registrations.load(), retirements.load(), walks.load(), executions.load());
}
