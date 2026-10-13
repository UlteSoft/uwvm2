// Source-only candidate derived from the existing real guest/ABI binder witness.
// Actual A64 Windows GNU provider, COFF registration, cache and cleanup are
// required at execution; this component is not full VM/IDE qualification.
#define UWVM_RUNTIME_LLVM_JIT
#if !defined(UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT) || UWVM2_ENABLE_NATIVE_EH_WINDOWS_ARM64_GNU_PRODUCT != 1
# error "ARM64 GNU EH component requires the exact-1 candidate gate"
#endif
#if !defined(_WIN32) || !defined(_WIN64) || !defined(__MINGW32__) || !defined(__SEH__) || \
    !defined(__GXX_ABI_VERSION) || !(defined(__cpp_exceptions) || defined(__EXCEPTIONS)) || \
    defined(_MSC_VER) || defined(__CYGWIN__) || defined(__USING_SJLJ_EXCEPTIONS__) || \
    defined(__arm64ec__) || defined(_M_ARM64EC) || !(defined(__aarch64__) || defined(_M_ARM64))
# error "This witness must be compiled for the actual GNU Windows ARM64 SEH provider"
#endif
#ifndef UWVM2_TEST_ARM64_GNU_EH_SOURCE_ID
# error "Embed the exact admitted component source identity; no generic old SDK/probe qualification"
#endif
#include <uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_exception_host.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <fast_io.h>
#include <uwvm2/runtime/exception/value.h>
#include <atomic>
#include <thread>
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
#include <cstring>
#include <memory>
#include <string>
#include <typeinfo>
namespace symbols = uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;
namespace pads = uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad;
namespace host = uwvm2::runtime::lib::details::native_exception_host;
namespace guest = uwvm2::runtime::exception;
// Exact native C++ ABI accessor: it only observes an active typed catch and
// cannot throw. Throw/rethrow functions are intentionally not marked noexcept.
extern "C++" ::std::type_info* uwvm_a64eh_current_exception_type_noexcept() noexcept
    __asm__("__cxa_current_exception_type");
using product_manager = ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager;
static_assert(sizeof(::fast_io::win32::win_current_runtime_function) == 8u);
static_assert(sizeof(::fast_io::win32::win_current_runtime_function) == sizeof(RUNTIME_FUNCTION));
static_assert(alignof(::fast_io::win32::win_current_runtime_function) == alignof(RUNTIME_FUNCTION));
static_assert(noexcept(::fast_io::win32::nt::RtlLookupFunctionEntry(0, nullptr, nullptr)));
namespace
{
    unsigned checks{};
#define REQUIRE(condition) do { ++checks; if(!(condition)) { ::fast_io::io::perrln("FAIL A64 GNU EH line ", __LINE__, " check ", checks, ": ", #condition); ::fast_io::fast_terminate(); } } while(false)
    struct counters
    {
        unsigned guest_alive{}, guest_destroyed{}, foreign_alive{}, foreign_destroyed{};
        unsigned begins{}, ends{}, rethrows{}, pushes{}, pops{}, host_cleanups{}, caught_helpers{};
        unsigned depth{};
    } observed;
    using guest_exception = guest::guest_exception;
    std::shared_ptr<unsigned char const> tag_roots[]{
        std::make_shared<unsigned char const>(7), std::make_shared<unsigned char const>(8), std::make_shared<unsigned char const>(9)};
    guest_exception make_guest(int tag, int payload)
    {
        REQUIRE(tag >= 7 && tag <= 9); // Complete immutable tag_roots extent before index formation.
        std::array<std::byte, sizeof(payload)> bits{};
        std::memcpy(bits.data(), &payload, sizeof(payload));
        auto field = guest::payload_field::numeric(guest::payload_kind::i32, bits);
        if(!field) { ::fast_io::fast_terminate(); }
        return guest_exception{guest::value::make(tag_roots[tag - 7], std::span{&*field, 1u})};
    }
    int guest_tag(guest_exception const& value) noexcept
    {
        for(unsigned i{}; i != 3; ++i) { if(value.instance()->tag_identity() == tag_roots[i].get()) { return int(i + 7); } }
        ::fast_io::fast_terminate();
    }
    int guest_payload(guest_exception const& value) noexcept
    {
        int payload{};
        std::memcpy(&payload, value.instance()->fields()[0].bits().data(), sizeof(payload));
        return payload;
    }
    std::atomic<unsigned> cold_factories{};
    guest_exception make_probe_first() { ++cold_factories; return make_guest(7, 0); }
    guest_exception make_probe_second() { ++cold_factories; return make_guest(8, 0); }
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
        if(mode != 0) { throw make_guest(mode == 3 ? 8 : 7, 41 + mode); }
    }
    int tag_of(void* object) noexcept { return guest_tag(*static_cast<guest_exception const*>(object)); }
    int caught_helper(void* object, int mode)
    {
        ++observed.caught_helpers;
        host_cleanup cleanup;
        auto const& value = *static_cast<guest_exception const*>(object);
        REQUIRE(guest_tag(value) == 7);
        if(mode == 4) { throw make_guest(9, 144); }
        if(mode == 5) { throw foreign_exception{155}; }
        return guest_payload(value);
    }
    void push_trace() noexcept { ++observed.pushes; ++observed.depth; }
    void pop_trace() noexcept { REQUIRE(observed.depth == 1); ++observed.pops; --observed.depth; }
    std::type_info const* guest_type()
    {
        try { throw make_guest(7, 0); }
        catch(guest_exception const&)
        {
            auto const type = uwvm_a64eh_current_exception_type_noexcept();
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
    void save_binary(std::string const& path, char const* data, std::size_t size)
    {
        REQUIRE(size <= (1u << 20u) && (data != nullptr || size == 0u));
        ::fast_io::native_file file{::fast_io::mnp::os_c_str(path.c_str()), ::fast_io::open_mode::out};
        // [complete caller-owned LLVM/string bytes] end
        // [safe ] every caller passes data()/size() from the same bounded owner;
        //         forming data+size may reach one-past, write does not dereference it.
        if(size != 0u) { ::fast_io::write(file, data, data + size); }
    }
    std::string load_owned_object(std::string const& path)
    {
        ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(path.c_str()), ::fast_io::open_mode::in};
        REQUIRE(file.size() != 0u && file.size() <= (1u << 20u));
        // [complete live file-loader bytes] independent std::string copy
        // [safe ] char aliases the complete loader allocation; no pointer survives file.
        auto const* data{reinterpret_cast<char const*>(file.data())};
        return std::string{data, file.size()};
    }
    bool registered_entry(std::uintptr_t pc) noexcept
    {
        ::fast_io::win32::win_current_unwind_address image{};
        // [real native PC from this live engine] [kernel registered table borrow]
        // [safe ] inspect only nullable identity; never dereference OS table/code here.
        auto const* table{::fast_io::win32::nt::RtlLookupFunctionEntry(pc, &image, nullptr)};
        return pc != 0u && table != nullptr && image != 0u && pc >= image;
    }
    struct object_cache final : llvm::ObjectCache
    {
        std::string bytes;
        unsigned reads{}, writes{};
        void notifyObjectCompiled(llvm::Module const*, llvm::MemoryBufferRef object) override
        {
            REQUIRE(object.getBufferSize() != 0u && object.getBufferSize() <= (1u << 20u));
            // [live LLVM object buffer] bounded independently owned cache copy
            // [safe ] object.getBuffer().str() copies all bytes before the callback returns.
            bytes = object.getBuffer().str(); ++writes;
        }
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
        // Borrowed only while engine owns this manager; invalidated before teardown.
        product_manager* memory{};
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
        REQUIRE(symbols::supports_itanium_dwarf_object(machine));
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
        ::fast_io::io::perrln("BUILD ", ::fast_io::mnp::os_c_str(prefix.c_str()), " begin");
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
        std::string ir;
        llvm::raw_string_ostream ir_output{ir};
        module->print(ir_output, nullptr); ir_output.flush();
        save_binary(prefix + ".ll", ir.data(), ir.size());
        ::fast_io::io::perrln("BUILD ", ::fast_io::mnp::os_c_str(prefix.c_str()), " verified IR");
        std::string engine_error;
        auto memory{std::make_unique<product_manager>()};
        // [pending unique memory-manager owner] then [owning MCJIT engine]
        // [safe ] the identity is borrowed before transfer and inspected only
        //         after create succeeds; the engine retains it through all code use.
        result.memory = memory.get();
        llvm::EngineBuilder engine_builder{std::move(module)};
        engine_builder.setErrorStr(&engine_error).setEngineKind(llvm::EngineKind::JIT)
            .setMCJITMemoryManager(std::move(memory));
        result.engine.reset(engine_builder.create(target.release()));
        if(!result.engine) { ::fast_io::io::perrln("ENGINE_ERROR ", ::fast_io::mnp::os_c_str(engine_error.c_str())); }
        REQUIRE(result.engine != nullptr);
        result.engine->setObjectCache(&cache);
        REQUIRE(host::bind<guest_exception>(*result.engine, imports, runtime, &make_probe_first));
        REQUIRE(host::bind<guest_exception>(*result.engine, imports, runtime, &make_probe_second));
        REQUIRE(result.engine->getPointerToGlobalIfAvailable(imports.type_info) == guest_type());
        REQUIRE(cold_factories == 1);
        auto bind = [&](llvm::GlobalValue const* symbol, auto pointer_value)
        { result.engine->addGlobalMapping(symbol, reinterpret_cast<void*>(address(pointer_value))); };
        bind(raise, &raise_test);
        bind(tag, &tag_of);
        bind(helper, &caught_helper);
        if(instruction) { bind(push, &push_trace); bind(pop, &pop_trace); }
        result.engine->finalizeObject();
        REQUIRE(result.memory != nullptr && !result.memory->has_finalization_failure());
        auto function_address = result.engine->getFunctionAddress("probe_entry");
        REQUIRE(function_address != 0 && !result.memory->has_finalization_failure());
        REQUIRE(registered_entry(static_cast<std::uintptr_t>(function_address)));
        // Borrow the published JIT entry only while result.engine owns its executable code and registered unwind data.
        result.entry = reinterpret_cast<instance::entry_type>(static_cast<std::uintptr_t>(function_address));
        ::fast_io::io::perrln("BUILD ", ::fast_io::mnp::os_c_str(prefix.c_str()), " finalized with actual registered A64 entry");
        return result;
    }
    instance build_object_only(object_cache const& cache, bool instruction)
    {
        instance result;
        result.context = std::make_unique<llvm::LLVMContext>();
        auto target = target_machine();
        REQUIRE(target != nullptr && symbols::supports_itanium_dwarf_object(*target));
        auto module = new_module(*result.context, *target);
        auto imports = symbols::declare_itanium_dwarf_symbols(*module, *target);
        auto runtime = pads::declare_catch_runtime(*module);
        llvm::IRBuilder<> builder{*result.context};
        auto declare = [&](char const* name, llvm::FunctionType* type)
        { return llvm::Function::Create(type, llvm::GlobalValue::ExternalLinkage, name, *module); };
        auto i32 = builder.getInt32Ty();
        auto raise = declare("probe_raise", llvm::FunctionType::get(builder.getVoidTy(), {i32}, false));
        auto tag = declare("probe_tag", llvm::FunctionType::get(i32, {builder.getPtrTy()}, false));
        auto helper = declare("probe_caught_helper", llvm::FunctionType::get(i32, {builder.getPtrTy(), i32}, false));
        llvm::Function* push{};
        llvm::Function* pop{};
        if(instruction)
        {
            push = declare("probe_trace_push", llvm::FunctionType::get(builder.getVoidTy(), false));
            pop = declare("probe_trace_pop", llvm::FunctionType::get(builder.getVoidTy(), false));
        }
        // The module has imported helpers and its real local personality wrapper;
        // the independently cached object owns its real COFF .pdata/.xdata sections.
        auto memory{std::make_unique<product_manager>()};
        // [pending unique manager] ownership transfers to EngineBuilder below;
        // [safe ] the identity borrow is usable only after the owning engine exists.
        result.memory = memory.get();
        llvm::EngineBuilder engine_builder{std::move(module)};
        engine_builder.setEngineKind(llvm::EngineKind::JIT).setMCJITMemoryManager(std::move(memory));
        result.engine.reset(engine_builder.create(target.release()));
        REQUIRE(result.engine != nullptr);
        REQUIRE(host::bind<guest_exception>(*result.engine, imports, runtime, &make_probe_second));
        auto bind = [&](llvm::GlobalValue const* symbol, auto pointer_value)
        { result.engine->addGlobalMapping(symbol, reinterpret_cast<void*>(address(pointer_value))); };
        bind(raise, &raise_test);
        bind(tag, &tag_of);
        bind(helper, &caught_helper);
        if(instruction) { bind(push, &push_trace); bind(pop, &pop_trace); }
        auto buffer = llvm::MemoryBuffer::getMemBufferCopy(cache.bytes, "native-eh-object-only");
        auto object = llvm::object::ObjectFile::createObjectFile(buffer->getMemBufferRef());
        REQUIRE(static_cast<bool>(object));
        result.engine->addObjectFile(llvm::object::OwningBinary<llvm::object::ObjectFile>{std::move(*object), std::move(buffer)});
        REQUIRE(!result.engine->hasError());
        result.engine->finalizeObject();
        REQUIRE(result.memory != nullptr && !result.memory->has_finalization_failure());
        auto entry = result.engine->getFunctionAddress("probe_entry");
        REQUIRE(entry != 0 && !result.memory->has_finalization_failure());
        REQUIRE(registered_entry(static_cast<std::uintptr_t>(entry)));
        // [engine-owned executable allocation] entry is borrowed only while this instance keeps its engine alive.
        result.entry = reinterpret_cast<instance::entry_type>(static_cast<std::uintptr_t>(entry));
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
                    REQUIRE(guest_tag(value) == (mode == 3 ? 8 : 9));
                    REQUIRE(guest_payload(value) == (mode == 3 ? 44 : 144));
                }
                catch(foreign_exception const& value)
                {
                    caught_kind = 2;
                    REQUIRE(mode == 2 || mode == 5);
                    REQUIRE(value.payload == (mode == 2 ? 99 : 155));
                    REQUIRE(observed.foreign_alive == 1);
                }
                REQUIRE(caught_kind == (mode == 3 || mode == 4 ? 1 : mode == 2 || mode == 5 ? 2 : 0));
                REQUIRE(observed.foreign_destroyed == (mode == 2 || mode == 5 ? 1u : 0u));
                REQUIRE(observed.caught_helpers == (mode == 1 || mode >= 4 ? 1u : 0u));
                REQUIRE(observed.host_cleanups == 1 + observed.caught_helpers);
                REQUIRE(observed.pushes == unsigned(instruction) && observed.pops == unsigned(instruction) && observed.depth == 0);
                REQUIRE(uwvm_a64eh_current_exception_type_noexcept() == nullptr);
                ++executions;
            }
        }
    }
    void reject_partial_binding()
    {
        llvm::LLVMContext context;
        auto target = target_machine();
        REQUIRE(target != nullptr && symbols::supports_itanium_dwarf_object(*target));
        auto module = new_module(context, *target);
        auto imports = symbols::declare_itanium_dwarf_symbols(*module, *target);
        auto runtime = pads::declare_catch_runtime(*module);
        llvm::EngineBuilder engine_builder{std::move(module)};
        engine_builder.setEngineKind(llvm::EngineKind::JIT)
            .setMCJITMemoryManager(std::make_unique<product_manager>());
        std::unique_ptr<llvm::ExecutionEngine> engine{engine_builder.create(target.release())};
        REQUIRE(engine != nullptr);
        // Conflict in the last checked ABI symbol must not publish any earlier mapping.
        engine->addGlobalMapping(runtime.rethrow, reinterpret_cast<void*>(address(&raise_test)));
        REQUIRE(!host::bind<guest_exception>(*engine, imports, runtime, &make_probe_first));
        REQUIRE(engine->getPointerToGlobalIfAvailable(imports.type_info) == nullptr);
        REQUIRE(engine->getPointerToGlobalIfAvailable(imports.personality) == nullptr);
        REQUIRE(engine->getPointerToGlobalIfAvailable(runtime.begin) == nullptr);
        REQUIRE(engine->getPointerToGlobalIfAvailable(runtime.end) == nullptr);
        engine->updateGlobalMapping(runtime.rethrow, 0);
        REQUIRE(host::bind<guest_exception>(*engine, imports, runtime, &make_probe_second));
        REQUIRE(host::bind<guest_exception>(*engine, imports, runtime, &make_probe_first));
    }
    void retire(instance& generated)
    {
        REQUIRE(generated.engine != nullptr && generated.memory != nullptr && generated.entry != nullptr);
        auto const pc{address(generated.entry)}; REQUIRE(registered_entry(pc));
        // [live unique engine owns code/manager] [no executing threads at this boundary]
        // [safe ] retire callable/manager borrows first; engine teardown deregisters
        //         tables before freeing code. Retain only an integer PC for OS lookup.
        generated.entry = nullptr;
        generated.memory = nullptr;
        generated.engine.reset();
        ::fast_io::win32::win_current_unwind_address image{};
        REQUIRE(::fast_io::win32::nt::RtlLookupFunctionEntry(pc, &image, nullptr) == nullptr);
    }
    void concurrent_cold_capture()
    {
        std::array<void const*, 8> types{};
        std::array<std::thread, 8> threads;
        for(unsigned i{}; i != threads.size(); ++i)
        {
            threads[i] = std::thread{[&, i]
            {
                types[i] = host::details::guest_type_info<guest_exception>(i % 2 ? &make_probe_first : &make_probe_second);
            }};
        }
        for(auto& thread : threads) { thread.join(); }
        REQUIRE(cold_factories == 1);
        for(auto pointer : types) { REQUIRE(pointer == guest_type()); }
    }
    void reject_bad_runtime_declarations()
    {
        llvm::LLVMContext context;
        auto target = target_machine();
        REQUIRE(target != nullptr && symbols::supports_itanium_dwarf_object(*target));
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
    REQUIRE(host::available);
    auto actual_target{target_machine()}; REQUIRE(actual_target != nullptr);
    REQUIRE(actual_target->getTargetTriple().isWindowsGNUEnvironment() &&
        actual_target->getTargetTriple().isOSBinFormatCOFF() &&
        actual_target->getTargetTriple().getArch() == llvm::Triple::aarch64 &&
        !actual_target->getTargetTriple().isWindowsArm64EC() &&
        symbols::supports_itanium_dwarf_object(*actual_target));
    ::fast_io::io::perrln("SOURCE ", UWVM2_TEST_ARM64_GNU_EH_SOURCE_ID);
    concurrent_cold_capture();
    reject_partial_binding();
    reject_bad_runtime_declarations();
    for(bool instruction : {true, false})
    {
        auto prefix = std::string(argv[1]) + (instruction ? "/instruction" : "/unwind");
        object_cache cold_cache;
        auto cold = build(cold_cache, instruction, prefix + "-cold");
        REQUIRE(cold_cache.writes == 1 && cold_cache.reads == 0 && !cold_cache.bytes.empty());
        save_binary(prefix + ".o", cold_cache.bytes.data(), cold_cache.bytes.size());
        exercise(cold, instruction);
        object_cache warm_cache;
        warm_cache.bytes = load_owned_object(prefix + ".o");
        REQUIRE(warm_cache.bytes == cold_cache.bytes);
        auto warm = build(warm_cache, instruction, prefix + "-warm");
        REQUIRE(warm_cache.writes == 0 && warm_cache.reads == 1);
        exercise(warm, instruction);
        auto loaded = build_object_only(warm_cache, instruction);
        exercise(loaded, instruction);
        // Re-run the first still-live engine after warming the second: its symbol bindings and LSDA remain valid.
        exercise(cold, instruction);
        retire(loaded); exercise(cold, instruction);
        retire(warm); exercise(cold, instruction);
        retire(cold);
    }
    ::fast_io::io::println("PASS A64 GNU product-manager guest EH host bindings: ", checks,
        " checks, ", executions,
        " JIT executions, actual COFF registration/retirement, typed catch/foreign resume/rethrow/new-record cleanup, cold/warm instruction+unwind");
}
