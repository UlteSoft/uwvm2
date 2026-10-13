// Exercise the product EH landingpad emitter with test-owned guest values, independently of guest VM exception enablement.
#define UWVM_RUNTIME_LLVM_JIT
#include <uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <cxxabi.h>
#include <unwind.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
namespace symbols = uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;
namespace pads = uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad;
extern "C" _Unwind_Reason_Code __gxx_personality_v0(int, _Unwind_Action, std::uint64_t, _Unwind_Exception*, _Unwind_Context*);
namespace
{
    unsigned checks{};
#define REQUIRE(condition) do { ++checks; if(!(condition)) { std::fprintf(stderr,"FAIL %u check %u: %s\n",__LINE__,checks,#condition); std::abort(); } } while(false)
    struct counters
    {
        unsigned guest_alive{}, guest_destroyed{}, foreign_alive{}, foreign_destroyed{};
        unsigned begins{}, ends{}, rethrows{}, pushes{}, pops{}, host_cleanups{}, caught_helpers{};
        unsigned depth{};
    } observed;
    struct guest_exception final
    {
        int tag{}, payload{};
        guest_exception(int t=0, int p=0) noexcept : tag{t}, payload{p} { ++observed.guest_alive; }
        guest_exception(guest_exception const& other) noexcept : guest_exception{other.tag, other.payload} {}
        ~guest_exception() noexcept { REQUIRE(observed.guest_alive != 0); --observed.guest_alive; ++observed.guest_destroyed; }
    };
    struct foreign_exception final
    {
        int payload{};
        explicit foreign_exception(int p) noexcept : payload{p} { ++observed.foreign_alive; }
        foreign_exception(foreign_exception const& other) noexcept : foreign_exception{other.payload} {}
        ~foreign_exception() noexcept { REQUIRE(observed.foreign_alive != 0); --observed.foreign_alive; ++observed.foreign_destroyed; }
    };
    struct host_cleanup { ~host_cleanup() noexcept { ++observed.host_cleanups; } };
    void raise_test(int mode)
    {
        host_cleanup cleanup;
        if(mode == 2) { throw foreign_exception{99}; }
        if(mode != 0) { throw guest_exception{mode == 3 ? 8 : 7, 41 + mode}; }
    }
    int tag_of(void* object) noexcept { return static_cast<guest_exception const*>(object)->tag; }
    int caught_helper(void* object, int mode)
    {
        ++observed.caught_helpers;
        host_cleanup cleanup;
        auto const& value = *static_cast<guest_exception const*>(object);
        REQUIRE(value.tag == 7);
        if(mode == 4) { throw guest_exception{9, 144}; }
        if(mode == 5) { throw foreign_exception{155}; }
        return value.payload;
    }
    void* counted_begin(void* exception) noexcept
    {
        ++observed.begins;
        return __cxxabiv1::__cxa_begin_catch(exception);
    }
    void counted_end() noexcept { ++observed.ends; __cxxabiv1::__cxa_end_catch(); }
    [[noreturn]] void counted_rethrow() { ++observed.rethrows; __cxxabiv1::__cxa_rethrow(); }
    void push_trace() noexcept { ++observed.pushes; ++observed.depth; }
    void pop_trace() noexcept { REQUIRE(observed.depth == 1); ++observed.pops; --observed.depth; }
    std::type_info const* guest_type()
    {
        try { throw guest_exception{}; }
        catch(guest_exception const&)
        {
            auto const type = __cxxabiv1::__cxa_current_exception_type();
            REQUIRE(type != nullptr);
            return type;
        }
    }
    template<typename Function> std::uintptr_t address(Function function)
    {
        static_assert(sizeof(function) <= sizeof(std::uintptr_t));
        std::uintptr_t result{};
        std::memcpy(&result, &function, sizeof(function));
        return result;
    }
    struct object_cache final : llvm::ObjectCache
    {
        std::string bytes;
        unsigned reads{}, writes{};
        void notifyObjectCompiled(llvm::Module const*, llvm::MemoryBufferRef object) override
        { bytes = object.getBuffer().str(); ++writes; }
        std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override
        {
            if(bytes.empty()) { return {}; }
            ++reads;
            return llvm::MemoryBuffer::getMemBufferCopy(bytes, "native-eh-landingpad");
        }
    };
    struct instance
    {
        std::unique_ptr<llvm::LLVMContext> context;
        std::unique_ptr<llvm::ExecutionEngine> engine;
        using entry_type = int(*)(int);
        entry_type entry{};
    };
    std::unique_ptr<llvm::TargetMachine> target_machine()
    {
        llvm::EngineBuilder builder;
#if defined(UWVM2TEST_EH_LARGE_CODE_MODEL)
        builder.setCodeModel(llvm::CodeModel::Large);
#elif !defined(UWVM2TEST_EH_JIT_DEFAULT_MODEL)
        builder.setRelocationModel(llvm::Reloc::PIC_);
#endif
        return std::unique_ptr<llvm::TargetMachine>{builder.selectTarget()};
    }
    std::unique_ptr<llvm::Module> new_module(llvm::LLVMContext& context, llvm::TargetMachine const& machine)
    {
        auto module = std::make_unique<llvm::Module>("native-eh-landingpad", context);
#if LLVM_VERSION_MAJOR >= 21
        module->setTargetTriple(machine.getTargetTriple());
#else
        module->setTargetTriple(machine.getTargetTriple().str());
#endif
        module->setDataLayout(machine.createDataLayout());
        return module;
    }
    instance build(object_cache& cache, bool instruction, std::string const& prefix)
    {
        std::fprintf(stderr,"BUILD %s begin\n",prefix.c_str());
        instance result;
        result.context = std::make_unique<llvm::LLVMContext>();
        auto target = target_machine();
        REQUIRE(target != nullptr && symbols::supports_itanium_dwarf_object(*target));
        auto module = new_module(*result.context, *target);
        auto imports = symbols::declare_itanium_dwarf_symbols(*module, *target);
        REQUIRE(imports.status == symbols::error::ok);
        auto runtime = pads::declare_catch_runtime(*module);
        REQUIRE(static_cast<bool>(runtime));
        auto repeated = pads::declare_catch_runtime(*module);
        REQUIRE(repeated.begin == runtime.begin && repeated.end == runtime.end && repeated.rethrow == runtime.rethrow);
        llvm::IRBuilder<> builder{*result.context};
        auto const i32 = builder.getInt32Ty();
        auto const pointer = builder.getPtrTy();
        auto declare = [&](char const* name, llvm::FunctionType* type)
        { return llvm::Function::Create(type, llvm::GlobalValue::ExternalLinkage, name, *module); };
        auto raise = declare("probe_raise", llvm::FunctionType::get(builder.getVoidTy(), {i32}, false));
        auto tag = declare("probe_tag", llvm::FunctionType::get(i32, {pointer}, false));
        auto helper = declare("probe_caught_helper", llvm::FunctionType::get(i32, {pointer, i32}, false));
        llvm::Function* push{};
        llvm::Function* pop{};
        if(instruction)
        {
            push = declare("probe_trace_push", llvm::FunctionType::get(builder.getVoidTy(), false));
            pop = declare("probe_trace_pop", llvm::FunctionType::get(builder.getVoidTy(), false));
        }
        auto function = declare("probe_entry", llvm::FunctionType::get(i32, {i32}, false));
        auto entry = llvm::BasicBlock::Create(*result.context, "entry", function);
        auto normal = llvm::BasicBlock::Create(*result.context, "normal", function);
        auto matched = llvm::BasicBlock::Create(*result.context, "matched", function);
        auto mismatch = llvm::BasicBlock::Create(*result.context, "mismatch", function);
        auto handled = llvm::BasicBlock::Create(*result.context, "handled", function);
        auto exit_cleanup = [&](llvm::IRBuilder<>& current)
        { if(instruction) { current.CreateCall(pop)->setDoesNotThrow(); } };
        // Every block/value below belongs to this live module. Product helpers own the actual landingpad construction.
        builder.SetInsertPoint(entry);
        auto caught = pads::emit_typed_entry(builder, *function, imports, runtime, exit_cleanup);
        REQUIRE(static_cast<bool>(caught));
        REQUIRE(builder.GetInsertBlock() == caught.caught);
        auto caught_exit = pads::emit_caught_exit_cleanup(builder, *function, runtime, exit_cleanup);
        REQUIRE(builder.GetInsertBlock() == caught.caught);
        auto read_tag = builder.CreateCall(tag, {caught.guest_object});
        read_tag->setDoesNotThrow();
        builder.CreateCondBr(builder.CreateICmpEQ(read_tag, builder.getInt32(7)), matched, mismatch);
        builder.SetInsertPoint(mismatch);
        pads::emit_rethrow(builder, runtime, caught_exit);
        builder.SetInsertPoint(matched);
        auto payload = builder.CreateInvoke(helper, handled, caught_exit, {caught.guest_object, function->getArg(0u)});
        builder.SetInsertPoint(handled);
        pads::emit_end_catch(builder, runtime);
        exit_cleanup(builder);
        builder.CreateRet(payload);
        builder.SetInsertPoint(entry);
        if(instruction) { builder.CreateCall(push)->setDoesNotThrow(); }
        builder.CreateInvoke(raise, normal, caught.landing, {function->getArg(0u)});
        builder.SetInsertPoint(normal);
        exit_cleanup(builder);
        builder.CreateRet(builder.getInt32(17));
        REQUIRE(!llvm::verifyModule(*module, &llvm::errs()));
        REQUIRE(instruction || (module->getFunction("probe_trace_push") == nullptr && module->getFunction("probe_trace_pop") == nullptr));
        std::error_code output_error;
        llvm::raw_fd_ostream ir_output{prefix + ".ll", output_error};
        REQUIRE(!output_error);
        module->print(ir_output, nullptr);
        ir_output.close();
        std::fprintf(stderr,"BUILD %s verified IR\n",prefix.c_str());
        std::string engine_error;
        llvm::EngineBuilder engine_builder{std::move(module)};
        engine_builder.setErrorStr(&engine_error).setEngineKind(llvm::EngineKind::JIT)
            .setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
        result.engine.reset(engine_builder.create(target.release()));
        if(!result.engine) { std::fprintf(stderr,"%s\n",engine_error.c_str()); }
        REQUIRE(result.engine != nullptr);
        result.engine->setObjectCache(&cache);
        REQUIRE(symbols::bind_itanium_dwarf_host_symbols(*result.engine, imports, guest_type(), address(&__gxx_personality_v0)) == symbols::error::ok);
        auto bind = [&](llvm::GlobalValue const* symbol, auto pointer_value)
        { result.engine->addGlobalMapping(symbol, reinterpret_cast<void*>(address(pointer_value))); };
        bind(raise, &raise_test);
        bind(tag, &tag_of);
        bind(helper, &caught_helper);
        bind(runtime.begin, &counted_begin);
        bind(runtime.end, &counted_end);
        bind(runtime.rethrow, &counted_rethrow);
        if(instruction) { bind(push, &push_trace); bind(pop, &pop_trace); }
        result.engine->finalizeObject();
        auto function_address = result.engine->getFunctionAddress("probe_entry");
        REQUIRE(function_address != 0);
        // Borrow the published JIT entry only while result.engine owns its executable code and registered unwind data.
        result.entry = reinterpret_cast<instance::entry_type>(static_cast<std::uintptr_t>(function_address));
        std::fprintf(stderr,"BUILD %s finalized\n",prefix.c_str());
        return result;
    }
    unsigned executions{};
    void exercise(instance const& generated, bool instruction)
    {
        for(unsigned repetition{}; repetition != 32; ++repetition)
        {
            for(int mode{}; mode != 6; ++mode)
            {
                observed = {};
                int caught_kind{};
                try
                {
                    int result = generated.entry(mode);
                    REQUIRE(mode == 0 || mode == 1);
                    REQUIRE(result == (mode == 0 ? 17 : 42));
                }
                catch(guest_exception const& value)
                {
                    caught_kind = 1;
                    REQUIRE(mode == 3 || mode == 4);
                    REQUIRE(value.tag == (mode == 3 ? 8 : 9));
                    REQUIRE(value.payload == (mode == 3 ? 44 : 144));
                    REQUIRE(observed.guest_alive == 1);
                    REQUIRE(observed.guest_destroyed == (mode == 3 ? 0u : 1u));
                }
                catch(foreign_exception const& value)
                {
                    caught_kind = 2;
                    REQUIRE(mode == 2 || mode == 5);
                    REQUIRE(value.payload == (mode == 2 ? 99 : 155));
                    REQUIRE(observed.foreign_alive == 1);
                    REQUIRE(observed.guest_alive == 0);
                }
                REQUIRE(caught_kind == (mode == 3 || mode == 4 ? 1 : mode == 2 || mode == 5 ? 2 : 0));
                REQUIRE(observed.guest_alive == 0 && observed.foreign_alive == 0);
                REQUIRE(observed.guest_destroyed == (mode == 0 || mode == 2 ? 0u : mode == 4 ? 2u : 1u));
                REQUIRE(observed.foreign_destroyed == (mode == 2 || mode == 5 ? 1u : 0u));
                REQUIRE(observed.begins == (mode == 1 || mode >= 3 ? 1u : 0u));
                REQUIRE(observed.ends == observed.begins);
                REQUIRE(observed.rethrows == (mode == 3 ? 1u : 0u));
                REQUIRE(observed.caught_helpers == (mode == 1 || mode >= 4 ? 1u : 0u));
                REQUIRE(observed.host_cleanups == 1 + observed.caught_helpers);
                REQUIRE(observed.pushes == unsigned(instruction) && observed.pops == unsigned(instruction) && observed.depth == 0);
                ++executions;
            }
        }
    }
    void reject_bad_runtime_declarations()
    {
        llvm::LLVMContext context;
        auto target = target_machine();
        auto module = new_module(context, *target);
        new llvm::GlobalVariable(*module, llvm::Type::getInt8Ty(context), true, llvm::GlobalValue::ExternalLinkage, nullptr, "__cxa_end_catch");
        REQUIRE(!pads::declare_catch_runtime(*module));
        REQUIRE(module->getNamedValue("__cxa_begin_catch") == nullptr && module->getNamedValue("__cxa_rethrow") == nullptr);
        auto second = new_module(context, *target);
        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context), false), llvm::GlobalValue::ExternalLinkage, "__cxa_begin_catch", *second);
        REQUIRE(!pads::declare_catch_runtime(*second));
        REQUIRE(second->getNamedValue("__cxa_end_catch") == nullptr && second->getNamedValue("__cxa_rethrow") == nullptr);
    }
}
int main(int argc, char** argv)
{
    REQUIRE(argc == 2);
    REQUIRE(!llvm::InitializeNativeTarget());
    REQUIRE(!llvm::InitializeNativeTargetAsmPrinter());
    reject_bad_runtime_declarations();
    for(bool instruction : {true, false})
    {
        auto prefix = std::string(argv[1]) + (instruction ? "/instruction" : "/unwind");
        object_cache cold_cache;
        auto cold = build(cold_cache, instruction, prefix + "-cold");
        REQUIRE(cold_cache.writes == 1 && cold_cache.reads == 0 && !cold_cache.bytes.empty());
        {
            std::ofstream output{prefix + ".o", std::ios::binary};
            output.write(cold_cache.bytes.data(), static_cast<std::streamsize>(cold_cache.bytes.size()));
        }
        exercise(cold, instruction);
        object_cache warm_cache;
        {
            std::ifstream input{prefix + ".o", std::ios::binary};
            warm_cache.bytes.assign(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
        }
        REQUIRE(warm_cache.bytes == cold_cache.bytes);
        auto warm = build(warm_cache, instruction, prefix + "-warm");
        REQUIRE(warm_cache.writes == 0 && warm_cache.reads == 1);
        exercise(warm, instruction);
        // Re-run the first still-live engine after warming the second: its symbol bindings and LSDA remain valid.
        exercise(cold, instruction);
    }
    std::printf("PASS product EH landingpads: %u checks, %u JIT executions, typed catch/foreign resume/rethrow/new-record cleanup, cold/warm instruction+unwind\n",checks,executions);
}
