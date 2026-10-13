// Standalone LLVM/Itanium ABI probe. This test's cold RTTI discovery is NOT a product dependency.
// Compile with -fno-rtti and exceptions enabled inside the remote Linux cgroup only.
#define UWVM_RUNTIME_LLVM_JIT
#include <uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <cxxabi.h>
#include <unwind.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>
namespace eh = uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;
extern "C" _Unwind_Reason_Code __gxx_personality_v0(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
namespace
{
    unsigned checks{};
    void require(bool condition)
    {
        ++checks;
#if defined(UWVM2TEST_TRACE_EH_IMPORT)
        std::fprintf(stderr, "CHECK %u\n", checks);
#endif
        if(!condition) { std::fprintf(stderr, "FAIL native EH symbols check %u\n", checks); std::abort(); }
    }
    struct guest_a final { int value{}; };
    struct guest_b final { int value{}; };
    unsigned host_cleanups{};
    struct cleanup final { ~cleanup() { ++host_cleanups; } };
    template<typename T> std::type_info const* probe_type()
    {
        try { throw T{}; }
        catch(T const&)
        {
            // The ABI returns the static TYPE object; never retain the caught exception's address.
            auto const result{__cxxabiv1::__cxa_current_exception_type()};
            require(result != nullptr);
            return result;
        }
    }
    void raise(int mode)
    {
        cleanup scope;
        if(mode == 1) { throw guest_a{42}; }
        if(mode == 2) { throw guest_b{99}; }
    }
    int payload_a(void* object) noexcept { return static_cast<guest_a const*>(object)->value; }
    int payload_b(void* object) noexcept { return static_cast<guest_b const*>(object)->value; }
    template<typename Function> std::uintptr_t address(Function function)
    {
        static_assert(sizeof(function) <= sizeof(std::uintptr_t));
        std::uintptr_t result{};
        std::memcpy(&result, &function, sizeof(function));
        return result;
    }
    struct saved_object final: llvm::ObjectCache
    {
        std::string bytes;
        unsigned hits{};
        unsigned writes{};
        void notifyObjectCompiled(llvm::Module const*, llvm::MemoryBufferRef object) override
        { bytes = object.getBuffer().str(); ++writes; }
        std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override
        {
            if(bytes.empty()) { return {}; }
            ++hits;
            return llvm::MemoryBuffer::getMemBufferCopy(bytes, "native-eh-symbol-probe");
        }
    };
    std::unique_ptr<llvm::TargetMachine> machine()
    {
        llvm::EngineBuilder builder;
        // Match native runtime defaults unless explicitly qualifying a full-range EH code model.
#if defined(UWVM2TEST_EH_LARGE_CODE_MODEL)
        builder.setCodeModel(llvm::CodeModel::Large);
#endif
        auto result{std::unique_ptr<llvm::TargetMachine>{builder.selectTarget()}};
        return result;
    }
    std::unique_ptr<llvm::Module> new_module(llvm::LLVMContext& context, llvm::TargetMachine const& target)
    {
        auto result{std::make_unique<llvm::Module>("native-eh-symbol-probe", context)};
#if LLVM_VERSION_MAJOR >= 21
        result->setTargetTriple(target.getTargetTriple());
#else
        result->setTargetTriple(target.getTargetTriple().str());
#endif
        result->setDataLayout(target.createDataLayout());
        return result;
    }
    struct instance
    {
        std::unique_ptr<llvm::LLVMContext> context;
        std::unique_ptr<llvm::ExecutionEngine> engine;
        eh::declarations symbols;
        using entry_type = int(*)(int);
        entry_type entry{};
    };
    instance build(saved_object& cache, bool use_b, std::string const& prefix)
    {
        instance result;
        result.context = std::make_unique<llvm::LLVMContext>();
        auto target{machine()};
        require(target != nullptr && eh::supports_itanium_dwarf_object(*target));
        auto module{new_module(*result.context, *target)};
        result.symbols = eh::declare_itanium_dwarf_symbols(*module, *target);
        require(result.symbols.status == eh::error::ok);
        require(llvm::isa<llvm::GlobalVariable>(result.symbols.type_info));
        require(result.symbols.type_info->isDeclaration());
        auto repeated{eh::declare_itanium_dwarf_symbols(*module, *target)};
        require(repeated.type_info == result.symbols.type_info && repeated.personality == result.symbols.personality);
        auto& context{*result.context};
        llvm::IRBuilder<> builder{context};
        auto const i32{builder.getInt32Ty()};
        auto const pointer{builder.getPtrTy()};
        auto const raise_function{llvm::Function::Create(llvm::FunctionType::get(builder.getVoidTy(), {i32}, false), llvm::GlobalValue::ExternalLinkage, "probe_raise", *module)};
        auto const begin_catch{llvm::Function::Create(llvm::FunctionType::get(pointer, {pointer}, false), llvm::GlobalValue::ExternalLinkage, "__cxa_begin_catch", *module)};
        auto const end_catch{llvm::Function::Create(llvm::FunctionType::get(builder.getVoidTy(), false), llvm::GlobalValue::ExternalLinkage, "__cxa_end_catch", *module)};
        auto const read_payload{llvm::Function::Create(llvm::FunctionType::get(i32, {pointer}, false), llvm::GlobalValue::ExternalLinkage, "probe_payload", *module)};
        auto const function{llvm::Function::Create(llvm::FunctionType::get(i32, {i32}, false), llvm::GlobalValue::ExternalLinkage, "probe_entry", *module)};
        function->setPersonalityFn(result.symbols.personality);
        // All block/value pointers below are owned by the same still-live LLVM module/context.
        // Builder insertion points reference complete blocks, never byte-stream cursors.
        auto const entry{llvm::BasicBlock::Create(context, "entry", function)};
        auto const normal{llvm::BasicBlock::Create(context, "normal", function)};
        auto const landing{llvm::BasicBlock::Create(context, "landing", function)};
        auto const caught{llvm::BasicBlock::Create(context, "caught", function)};
        auto const propagate{llvm::BasicBlock::Create(context, "propagate", function)};
        builder.SetInsertPoint(entry);
        builder.CreateInvoke(raise_function, normal, landing, {function->getArg(0u)});
        builder.SetInsertPoint(normal);
        builder.CreateRet(builder.getInt32(17));
        builder.SetInsertPoint(landing);
        auto const exception{builder.CreateLandingPad(llvm::StructType::get(context, {pointer, i32}), 1u)};
        exception->setCleanup(true);
        exception->addClause(result.symbols.type_info);
        auto const selector{builder.CreateExtractValue(exception, 1u)};
        auto const typeid_function{llvm::Intrinsic::isOverloaded(llvm::Intrinsic::eh_typeid_for)
            ? llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::eh_typeid_for, {result.symbols.type_info->getType()})
            : llvm::Intrinsic::getOrInsertDeclaration(module.get(), llvm::Intrinsic::eh_typeid_for)};
        auto const typeid_value{builder.CreateCall(typeid_function, {result.symbols.type_info})};
        builder.CreateCondBr(builder.CreateICmpEQ(selector, typeid_value), caught, propagate);
        builder.SetInsertPoint(caught);
        auto const object{builder.CreateCall(begin_catch, {builder.CreateExtractValue(exception, 0u)})};
        auto const payload{builder.CreateCall(read_payload, {object})};
        builder.CreateCall(end_catch);
        builder.CreateRet(payload);
        builder.SetInsertPoint(propagate);
        builder.CreateResume(exception);
        require(!llvm::verifyModule(*module, &llvm::errs()));
        std::error_code output_error;
        llvm::raw_fd_ostream ir_output{prefix + ".ll", output_error};
        require(!output_error);
        module->print(ir_output, nullptr);
        ir_output.close();
        std::string engine_error;
        llvm::EngineBuilder engine_builder{std::move(module)};
        engine_builder.setErrorStr(&engine_error).setEngineKind(llvm::EngineKind::JIT)
            .setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
        result.engine.reset(engine_builder.create(target.release()));
        if(!result.engine) { std::fprintf(stderr, "%s\n", engine_error.c_str()); }
        require(result.engine != nullptr);
        result.engine->setObjectCache(&cache);
        auto const type_info{use_b ? probe_type<guest_b>() : probe_type<guest_a>()};
        auto const other_type{use_b ? probe_type<guest_a>() : probe_type<guest_b>()};
        auto const personality{address(&__gxx_personality_v0)};
        require(eh::bind_itanium_dwarf_host_symbols(*result.engine, result.symbols, nullptr, personality) == eh::error::invalid_binding);
        require(result.engine->getPointerToGlobalIfAvailable(result.symbols.type_info) == nullptr);
        require(eh::bind_itanium_dwarf_host_symbols(*result.engine, result.symbols, type_info, personality) == eh::error::ok);
        require(eh::bind_itanium_dwarf_host_symbols(*result.engine, result.symbols, type_info, personality) == eh::error::ok);
        require(eh::bind_itanium_dwarf_host_symbols(*result.engine, result.symbols, other_type, personality) == eh::error::conflicting_binding);
        require(result.engine->getPointerToGlobalIfAvailable(result.symbols.type_info) == type_info);
        // Parallel repetitions exercise the check-and-bind transaction under the engine's own lock.
        std::atomic<unsigned> valid_bindings{};
        std::vector<std::thread> threads;
        for(unsigned i{}; i != 8u; ++i)
        {
            threads.emplace_back([&] {
                for(unsigned n{}; n != 32u; ++n)
                    if(eh::bind_itanium_dwarf_host_symbols(*result.engine, result.symbols, type_info, personality) == eh::error::ok)
                        valid_bindings.fetch_add(1u, std::memory_order_relaxed);
            });
        }
        for(auto& thread: threads) { thread.join(); }
        require(valid_bindings.load() == 256u);
        result.engine->addGlobalMapping(raise_function, reinterpret_cast<void*>(address(&raise)));
        result.engine->addGlobalMapping(read_payload, reinterpret_cast<void*>(use_b ? address(&payload_b) : address(&payload_a)));
        result.engine->addGlobalMapping(begin_catch, reinterpret_cast<void*>(address(&__cxxabiv1::__cxa_begin_catch)));
        result.engine->addGlobalMapping(end_catch, reinterpret_cast<void*>(address(&__cxxabiv1::__cxa_end_catch)));
        result.engine->finalizeObject();
        auto const function_address{result.engine->getFunctionAddress("probe_entry")};
        require(function_address != 0u);
        // [live JIT allocation] pinned by result.engine; exact host signature matches the verified IR.
        // [safe               ] no guest-supplied address participates in this conversion.
        result.entry = reinterpret_cast<instance::entry_type>(static_cast<std::uintptr_t>(function_address));
        return result;
    }
    void reject_conflicting_declarations()
    {
        llvm::LLVMContext context;
        auto target{machine()};
        auto module{new_module(context, *target)};
        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context), false), llvm::GlobalValue::ExternalLinkage, eh::type_info_symbol, *module);
        require(eh::declare_itanium_dwarf_symbols(*module, *target).status == eh::error::incompatible_declaration);
        require(module->getNamedValue(eh::personality_symbol) == nullptr);
        auto second{new_module(context, *target)};
        new llvm::GlobalVariable(*second, llvm::Type::getInt8Ty(context), true, llvm::GlobalValue::ExternalLinkage, nullptr, eh::personality_symbol);
        require(eh::declare_itanium_dwarf_symbols(*second, *target).status == eh::error::incompatible_declaration);
        require(second->getNamedValue(eh::type_info_symbol) == nullptr);
    }
}
int main(int argc, char** argv)
{
    require(argc == 2);
    require(!llvm::InitializeNativeTarget());
    require(!llvm::InitializeNativeTargetAsmPrinter());
    reject_conflicting_declarations();
    saved_object cache;
    auto first{build(cache, false, std::string(argv[1]) + "/first")};
    require(cache.writes == 1u && cache.hits == 0u && !cache.bytes.empty());
    std::ofstream object_output{std::string(argv[1]) + "/native-eh.o", std::ios::binary};
    object_output.write(cache.bytes.data(), static_cast<std::streamsize>(cache.bytes.size()));
    object_output.close();
    require(first.entry(0) == 17);
    require(first.entry(1) == 42);
    try { first.entry(2); require(false); }
    catch(guest_b const& value) { require(value.value == 99); }
    auto second{build(cache, true, std::string(argv[1]) + "/second")};
    require(cache.hits == 1u && cache.writes == 1u);
    require(second.entry(0) == 17);
    require(second.entry(2) == 99);
    try { second.entry(1); require(false); }
    catch(guest_a const& value) { require(value.value == 42); }
    // Both engines remain live: the second binding must not redirect the first engine's handler.
    require(first.entry(1) == 42);
    require(host_cleanups == 7u);
    std::printf("PASS native EH symbol imports: %u checks, typed catch/resume, engine isolation, cached relocations\n", checks);
}
