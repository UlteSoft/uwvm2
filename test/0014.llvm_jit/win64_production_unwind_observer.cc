// Test-only GNU Win64 component observer. Compile against an exact frozen source
// tree and its qualified LLVM closure. This is not a whole-VM/Wasm EH test.
// No alternate mapper, registration, relocation, personality stub or page
// permission is installed here: the final production manager owns all of them.
#define UWVM_RUNTIME_LLVM_JIT
#ifndef NOMINMAX
# define NOMINMAX
#endif
#include <fast_io.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h>
#include <uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#if defined(_WIN64) && defined(__SEH__) && defined(__GXX_ABI_VERSION) && \
    defined(__EXCEPTIONS) && (defined(__clang__) || defined(__GNUC__)) && \
    !defined(__CYGWIN__) && (defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64))
# include <windows.h>
# include <cxxabi.h>
# include <typeinfo>
#ifndef UWVM_TEST_OBSERVER_SOURCE_ID
# error "The test-only build must embed its exact frozen source ID"
#endif

// SDK-exact, nonthrowing observation APIs, directly linked to their real
// exports. Genuine __cxa_throw/rethrow and resume are never marked noexcept.
extern "C" __declspec(dllimport) SIZE_T WINAPI uwvm_observer_virtual_query(
    LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T) noexcept __asm__("VirtualQuery");
extern "C" EXCEPTION_DISPOSITION uwvm_observer_host_personality(
    PEXCEPTION_RECORD, void*, PCONTEXT, PDISPATCHER_CONTEXT) noexcept
    __asm__("__gxx_personality_seh0");

namespace symbols = ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;
namespace pads = ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad;
namespace detail = ::uwvm2::runtime::compiler::llvm_jit::details;
namespace
{
    using runtime_function = ::fast_io::win32::win_current_runtime_function;
    using unwind_address = ::fast_io::win32::win_current_unwind_address;
    static_assert(sizeof(runtime_function) == 12u && alignof(runtime_function) == 4u);
    static_assert(sizeof(runtime_function) == sizeof(RUNTIME_FUNCTION) && alignof(runtime_function) == alignof(RUNTIME_FUNCTION));
    static_assert(offsetof(runtime_function, BeginAddress) == offsetof(RUNTIME_FUNCTION, BeginAddress));
    static_assert(offsetof(runtime_function, EndAddress) == offsetof(RUNTIME_FUNCTION, EndAddress));
    static_assert(offsetof(runtime_function, UnwindData) == offsetof(RUNTIME_FUNCTION, UnwindData));
    static_assert(sizeof(unwind_address) == 8u && sizeof(::std::uintptr_t) == 8u);
    static_assert(noexcept(::fast_io::win32::nt::RtlLookupFunctionEntry(0, nullptr, nullptr)));
    static_assert(noexcept(uwvm_observer_virtual_query(nullptr, nullptr, 0)));

    void require(bool ok, char const* reason) noexcept
    {
        if(ok) { return; }
        ::fast_io::io::perrln("FAIL Win64 production observer: ", ::fast_io::mnp::os_c_str(reason));
        ::fast_io::fast_terminate();
    }
    bool add(::std::uintptr_t base, ::std::uintptr_t offset, ::std::uintptr_t& value) noexcept
    {
        if(offset > (::std::numeric_limits<::std::uintptr_t>::max)() - base) { return false; }
        value = base + offset;
        return true;
    }
    template<typename Function> ::std::uintptr_t function_address(Function pointer) noexcept
    {
        static_assert(sizeof(pointer) == sizeof(::std::uintptr_t));
        ::std::uintptr_t value{};
        ::std::memcpy(&value, &pointer, sizeof(value));
        return value;
    }

    struct section
    {
        ::std::uintptr_t address{};
        ::std::uintptr_t size{};
        ::std::uint64_t dyld_address{};
        unsigned id{};
        unsigned alignment{};
        bool code{};
        bool read_only{};
        ::std::array<char, 96> name{};
        bool pdata() const noexcept
        { return ::std::strcmp(name.data(), ".pdata") == 0 || ::std::strncmp(name.data(), ".pdata$", 7u) == 0; }
        bool xdata() const noexcept
        { return ::std::strcmp(name.data(), ".xdata") == 0 || ::std::strncmp(name.data(), ".xdata$", 7u) == 0; }
        bool contains(::std::uintptr_t pointer, ::std::size_t bytes) const noexcept
        { return pointer >= address && pointer - address <= size && bytes <= size - (pointer - address); }
    };
    struct event
    {
        char const* name{};
        ::std::uintptr_t address{};
        ::std::uint64_t other{};
        ::std::uintptr_t size{};
        ::std::uintptr_t minimum{};
    };

    // Composition is necessary: the actual product manager is final. Forward
    // every layout/lookup/registration callback, especially reserveAllocationSpace;
    // substituting the RTDyld base defaults would change the very allocation
    // layout that this component is meant to observe.
    class observer final : public ::llvm::RTDyldMemoryManager
    {
        detail::runtime_llvm_jit_section_memory_manager actual_{};
        ::std::array<section, 128> sections_{};
        ::std::array<event, 256> events_{};
        ::std::size_t section_count_{};
        ::std::size_t event_count_{};
        bool overflow_{};

        void record(char const* name, ::std::uintptr_t address, ::std::uint64_t other, ::std::uintptr_t size) noexcept
        {
            if(event_count_ == events_.size()) { overflow_ = true; return; }
            events_[event_count_++] = {name, address, other, size, minimum()};
        }
        void remember(::std::uint8_t* pointer, ::std::uintptr_t size, unsigned alignment,
                      unsigned id, ::llvm::StringRef name, bool code, bool ro) noexcept
        {
            if(section_count_ == sections_.size() || name.size() >= sections_[0].name.size())
            { overflow_ = true; return; }
            auto& value{sections_[section_count_++]};
            value.address = reinterpret_cast<::std::uintptr_t>(pointer);
            if(size > (::std::numeric_limits<::std::uintptr_t>::max)() - value.address)
            { overflow_ = true; value.address = 0; value.size = 0; return; }
            value.size = size;
            value.id = id;
            value.alignment = alignment;
            value.code = code;
            value.read_only = ro;
            // [owned fixed-size telemetry name] [LLVM-owned input string]
            // [safe ] name.size()<96 proves the complete copy and trailing NUL.
            ::std::memcpy(value.name.data(), name.data(), name.size());
            value.name[name.size()] = '\0';
            record(code ? "allocate-code" : "allocate-data", value.address, id, size);
        }
    public:
        ::std::uintptr_t minimum() const noexcept
        {
            auto result{(::std::numeric_limits<::std::uintptr_t>::max)()};
            for(::std::size_t i{}; i != section_count_; ++i)
            { auto const& s{sections_[i]}; if(s.address && s.size && s.address < result) { result = s.address; } }
            return result;
        }
        section const* containing(::std::uintptr_t pointer, ::std::size_t bytes) const noexcept
        {
            for(::std::size_t i{}; i != section_count_; ++i)
            { if(sections_[i].address && sections_[i].contains(pointer, bytes)) { return &sections_[i]; } }
            return nullptr;
        }
        ::std::uint8_t* allocateCodeSection(::std::uintptr_t size, unsigned alignment, unsigned id,
                                           ::llvm::StringRef name) noexcept override
        {
            auto* value{actual_.allocateCodeSection(size, alignment, id, name)};
            remember(value, size, alignment, id, name, true, false);
            return value;
        }
        ::std::uint8_t* allocateDataSection(::std::uintptr_t size, unsigned alignment, unsigned id,
                                           ::llvm::StringRef name, bool ro) noexcept override
        {
            auto* value{actual_.allocateDataSection(size, alignment, id, name, ro)};
            remember(value, size, alignment, id, name, false, ro);
            return value;
        }
        TLSSection allocateTLSSection(::std::uintptr_t size, unsigned alignment, unsigned id, ::llvm::StringRef name) override
        { return actual_.allocateTLSSection(size, alignment, id, name); }
        bool needsToReserveAllocationSpace() override { return actual_.needsToReserveAllocationSpace(); }
        void reserveAllocationSpace(::std::uintptr_t code, ::llvm::Align code_align,
            ::std::uintptr_t ro, ::llvm::Align ro_align, ::std::uintptr_t rw, ::llvm::Align rw_align) override
        {
            record("reserve-space", code, ro, rw);
            actual_.reserveAllocationSpace(code, code_align, ro, ro_align, rw, rw_align);
        }
        bool allowStubAllocation() const override { return actual_.allowStubAllocation(); }
        void notifyObjectLoaded(::llvm::ExecutionEngine* engine, ::llvm::object::ObjectFile const& object) override
        { record("notify-engine-object", 0, 0, 0); actual_.notifyObjectLoaded(engine, object); }
        void notifyObjectLoaded(::llvm::RuntimeDyld& dyld, ::llvm::object::ObjectFile const& object) override
        {
            for(::std::size_t i{}; i != section_count_; ++i)
            { sections_[i].dyld_address = dyld.getSectionLoadAddress(sections_[i].id); }
            record("notify-dyld-object", 0, 0, 0);
            actual_.notifyObjectLoaded(dyld, object);
        }
        bool finalizeMemory(::std::string* error = nullptr) override
        {
            record("finalize-before", 0, 0, 0);
            auto const failed{actual_.finalizeMemory(error)};
            record("finalize-after", 0, failed, 0);
            return failed;
        }
        void registerEHFrames(::std::uint8_t* pointer, ::std::uint64_t load, ::std::size_t size) noexcept override
        {
            record("register-before", reinterpret_cast<::std::uintptr_t>(pointer), load, size);
            actual_.registerEHFrames(pointer, load, size);
            record("register-after", reinterpret_cast<::std::uintptr_t>(pointer), actual_.has_finalization_failure(), size);
        }
        void deregisterEHFrames() noexcept override { actual_.deregisterEHFrames(); }
        ::llvm::JITSymbol findSymbol(::std::string const& name) override { return actual_.findSymbol(name); }
        ::llvm::JITSymbol findSymbolInLogicalDylib(::std::string const& name) override { return actual_.findSymbolInLogicalDylib(name); }
        ::std::uint64_t getSymbolAddress(::std::string const& name) override { return actual_.getSymbolAddress(name); }
        ::std::uint64_t getSymbolAddressInLogicalDylib(::std::string const& name) override { return actual_.getSymbolAddressInLogicalDylib(name); }
        void* getPointerToNamedFunction(::std::string const& name, bool abort_on_failure = true) override
        { return actual_.getPointerToNamedFunction(name, abort_on_failure); }
        bool good() const noexcept { return !overflow_ && !actual_.has_finalization_failure(); }

        bool executable(::std::uintptr_t address, char const* name) const noexcept
        {
            MEMORY_BASIC_INFORMATION info{};
            auto const bytes{uwvm_observer_virtual_query(reinterpret_cast<void const*>(address), &info, sizeof(info))};
            ::fast_io::io::println("PAGE name=", ::fast_io::mnp::os_c_str(name), " pc=0x", ::fast_io::mnp::hex(address),
                " bytes=", bytes, " base=0x", ::fast_io::mnp::hex(reinterpret_cast<::std::uintptr_t>(info.BaseAddress)),
                " size=", info.RegionSize, " state=0x", ::fast_io::mnp::hex(info.State), " protect=0x", ::fast_io::mnp::hex(info.Protect));
            auto const prot{info.Protect & 0xffu};
            auto const execute{prot == PAGE_EXECUTE || prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY};
            auto const* own{containing(address, 1u)};
            return bytes == sizeof(info) && info.State == MEM_COMMIT && !(info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                execute && own != nullptr && own->code;
        }
        void dump() const noexcept
        {
            ::fast_io::io::println("MANAGER source=", UWVM_TEST_OBSERVER_SOURCE_ID, " section_count=", section_count_,
                " event_count=", event_count_, " overflow=", overflow_, " finalization_failed=", actual_.has_finalization_failure(),
                " final_observed_minimum=0x", ::fast_io::mnp::hex(minimum()));
            for(::std::size_t i{}; i != event_count_; ++i)
            {
                auto const& e{events_[i]};
                ::fast_io::io::println("EVENT sequence=", i, " phase=", ::fast_io::mnp::os_c_str(e.name),
                    " address=0x", ::fast_io::mnp::hex(e.address), " other=0x", ::fast_io::mnp::hex(e.other),
                    " size=", e.size, " observed_minimum_at_callback=0x", ::fast_io::mnp::hex(e.minimum));
            }
            for(::std::size_t i{}; i != section_count_; ++i)
            {
                auto const& s{sections_[i]};
                ::fast_io::io::println("SECTION id=", s.id, " name=", ::fast_io::mnp::os_c_str(s.name.data()),
                    " address=0x", ::fast_io::mnp::hex(s.address), " dyld_address=0x", ::fast_io::mnp::hex(s.dyld_address),
                    " size=", s.size, " code=", s.code, " readonly=", s.read_only, " alignment=", s.alignment);
                if(!s.pdata() || !s.address) { continue; }
                if(s.size % sizeof(runtime_function) || s.size / sizeof(runtime_function) > 4096u)
                { ::fast_io::io::perrln("INVALID PDATA extent/capacity"); continue; }
                for(::std::size_t offset{}; offset != s.size; offset += sizeof(runtime_function))
                {
                    runtime_function f{};
                    // PDATA entry ... allocation_end
                    // [safe 12 bytes] unsafe (could be allocation_end)
                    // ^^ address+offset: size%12==0 and offset<size prove the copy.
                    ::std::memcpy(&f, reinterpret_cast<void const*>(s.address + offset), sizeof(f));
                    ::fast_io::io::println("PDATA section=", s.id, " offset=", offset, " begin_rva=0x", ::fast_io::mnp::hex(f.BeginAddress),
                        " end_rva=0x", ::fast_io::mnp::hex(f.EndAddress), " unwind_rva=0x", ::fast_io::mnp::hex(f.UnwindData));
                    // [safe copied entry] unsafe (could be allocation_end)
                    //                     ^^ offset advances by exactly one proved entry.
                }
            }
        }
        bool lookup(::std::uintptr_t pc, char const* name, ::std::uintptr_t personality) const noexcept
        {
            unwind_address image{};
            auto const* table{::fast_io::win32::nt::RtlLookupFunctionEntry(static_cast<unwind_address>(pc), &image, nullptr)};
            auto const table_address{reinterpret_cast<::std::uintptr_t>(table)};
            ::fast_io::io::println("LOOKUP name=", ::fast_io::mnp::os_c_str(name), " pc=0x", ::fast_io::mnp::hex(pc),
                " table=0x", ::fast_io::mnp::hex(table_address), " system_image_base=0x", ::fast_io::mnp::hex(image),
                " final_observed_minimum=0x", ::fast_io::mnp::hex(minimum()));
            auto const* table_section{containing(table_address, sizeof(runtime_function))};
            if(!table || !table_section || !table_section->pdata() ||
               (table_address - table_section->address) % sizeof(runtime_function)) { return false; }
            runtime_function f{};
            // [live product-owned .pdata allocation] [complete runtime entry]
            // [safe ] containing() proves full ownership before dereferencing OS output.
            ::std::memcpy(&f, table, sizeof(f));
            ::std::uintptr_t begin{}, end{}, unwind{};
            if(!add(image, f.BeginAddress, begin) || !add(image, f.EndAddress, end) ||
               !add(image, f.UnwindData, unwind) || begin >= end || pc < begin || pc >= end ||
               !containing(begin, end - begin) || !containing(begin, end - begin)->code) { return false; }
            auto const* xdata{containing(unwind, 4u)};
            if(!xdata || !xdata->xdata()) { return false; }
            ::std::array<::std::uint8_t, 4> header{};
            // [live product-owned .xdata allocation] [4-byte UNWIND_INFO prefix]
            // [safe ] no inferred RVA is read before the complete extent is proved.
            ::std::memcpy(header.data(), reinterpret_cast<void const*>(unwind), header.size());
            auto const version{header[0] & 7u};
            auto const flags{header[0] >> 3u};
            auto const codes{header[2]};
            auto const suffix{4u + ((static_cast<unsigned>(codes) + 1u) & ~1u) * 2u};
            ::fast_io::io::println("UNWIND name=", ::fast_io::mnp::os_c_str(name), " code_begin=0x", ::fast_io::mnp::hex(begin),
                " code_end=0x", ::fast_io::mnp::hex(end), " metadata=0x", ::fast_io::mnp::hex(unwind),
                " version=", version, " flags=", flags, " prolog=", static_cast<unsigned>(header[1]),
                " codes=", static_cast<unsigned>(codes), " frame=", static_cast<unsigned>(header[3]));
            if(version != 1u && version != 2u)
            { ::fast_io::io::perrln("UNSUPPORTED COMPONENT DECODER unwind_version=", version, "; no whole-product result"); return false; }
            if((flags & ~7u) || ((flags & 4u) && (flags & 3u))) { return false; }
            auto const trailer{(flags & 4u) ? sizeof(runtime_function) : ((flags & 3u) ? sizeof(::std::uint32_t) : 0u)};
            if(!xdata->contains(unwind, suffix + trailer)) { return false; }
            // [complete proved prefix/codes/trailer] [bounded diagnostic bytes]
            // [safe ] only <=256 bytes from the complete metadata extent are printed.
            auto const raw_size{(::std::min)(::std::size_t{suffix + trailer}, ::std::size_t{256})};
            for(::std::size_t i{}; i != raw_size; ++i)
            {
                ::std::uint8_t byte{};
                ::std::memcpy(&byte, reinterpret_cast<void const*>(unwind + i), sizeof(byte));
                ::fast_io::io::println("UNWIND_BYTE offset=", i, " value=0x", ::fast_io::mnp::hex(static_cast<unsigned>(byte)));
            }
            if(flags & 4u)
            {
                runtime_function chain{};
                ::std::memcpy(&chain, reinterpret_cast<void const*>(unwind + suffix), sizeof(chain));
                ::fast_io::io::println("CHAIN begin_rva=0x", ::fast_io::mnp::hex(chain.BeginAddress), " end_rva=0x",
                    ::fast_io::mnp::hex(chain.EndAddress), " unwind_rva=0x", ::fast_io::mnp::hex(chain.UnwindData));
                // Chained unwind interpretation is left to Windows/LLVM. Do not
                // recursively follow arbitrary trailers or claim opcode validation.
            }
            else if(flags & 3u)
            {
                ::std::uint32_t handler_rva{};
                ::std::memcpy(&handler_rva, reinterpret_cast<void const*>(unwind + suffix), sizeof(handler_rva));
                ::std::uintptr_t handler{};
                if(!add(image, handler_rva, handler)) { return false; }
                ::fast_io::io::println("HANDLER rva=0x", ::fast_io::mnp::hex(handler_rva), " actual=0x", ::fast_io::mnp::hex(handler),
                    " expected_local_production_personality=0x", ::fast_io::mnp::hex(personality));
                if(handler != personality || !executable(handler, "unwind-handler")) { return false; }
            }
            return executable(pc, name);
        }
    };

    observer const* running_observer{}; // borrowed only while its engine lives
    ::std::uintptr_t running_personality{};
    unsigned native_cleanups{}, inner_cleanups{}, outer_exits{};
    bool live_return_lookup_ok{true};
    struct guest final { int value{}; };
    struct foreign final { int value{}; };
    struct native_guard final { ~native_guard() noexcept { ++native_cleanups; } };
    [[gnu::noinline]] int raise_or_return(int mode)
    {
        native_guard cleanup;
        // This is the actual compiler-produced return PC inside generated inner,
        // not an entry+guessed offset or a frame-pointer walk.
        auto const pc{reinterpret_cast<::std::uintptr_t>(__builtin_extract_return_addr(__builtin_return_address(0)))};
        if(running_observer) { live_return_lookup_ok &= running_observer->lookup(pc, "active-inner-return", running_personality); }
        if(mode == 1 || mode == 3) { throw guest{42}; }
        if(mode == 2) { throw foreign{77}; }
        return 17;
    }
    void inner_cleanup() noexcept { ++inner_cleanups; }
    void outer_exit() noexcept { ++outer_exits; }
    int payload(void* pointer) noexcept { return static_cast<guest const*>(pointer)->value; }
    ::std::type_info const* actual_guest_type()
    {
        // A real typed throw/catch obtains the immutable SDK type object when
        // building against LLVM's no-RTTI closure; no __cxa header is inspected.
        try { throw guest{}; }
        catch(guest const&) { return ::__cxxabiv1::__cxa_current_exception_type(); }
    }
    void save(char const* prefix, char const* suffix, char const* data, ::std::size_t size)
    {
        ::std::string name{prefix}; name += suffix;
        ::fast_io::native_file file{::fast_io::mnp::os_c_str(name.c_str()), ::fast_io::open_mode::out};
        // [caller-owned complete bytes] [live native file]
        // [safe ] size comes from the same complete LLVM buffer/string; end is one-past.
        ::fast_io::write(file, data, data + size);
    }
    struct object_copy final : ::llvm::ObjectCache
    {
        char const* prefix{};
        unsigned count{};
        explicit object_copy(char const* p) noexcept : prefix{p} {}
        void notifyObjectCompiled(::llvm::Module const*, ::llvm::MemoryBufferRef object) override
        {
            require(++count == 1u && object.getBufferSize() <= (1u << 20u), "object capture capacity/single-object contract");
            save(prefix, ".jit.obj", object.getBufferStart(), object.getBufferSize());
        }
        ::std::unique_ptr<::llvm::MemoryBuffer> getObject(::llvm::Module const*) override { return {}; }
    };

    int exercise(char const* prefix)
    {
        require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "native target initialization");
        ::llvm::EngineBuilder selector;
        auto target{::std::unique_ptr<::llvm::TargetMachine>{selector.selectTarget()}};
        require(target && target->getTargetTriple().isX86_64() && target->getTargetTriple().isWindowsGNUEnvironment(), "real GNU Win64 target");
        ::llvm::LLVMContext context;
        auto module{::std::make_unique<::llvm::Module>("win64-production-observer", context)};
        module->setTargetTriple(target->getTargetTriple());
        module->setDataLayout(target->createDataLayout());
        auto const imported{symbols::declare_itanium_dwarf_symbols(*module, *target)};
        require(imported.status == symbols::error::ok && imported.host_personality &&
            imported.personality && !imported.personality->isDeclaration() && imported.personality->getComdat(), "actual production local COMDAT personality");
        auto const runtime{pads::declare_catch_runtime(*module)};
        require(static_cast<bool>(runtime), "actual catch runtime declarations");
        ::llvm::IRBuilder<> builder{context};
        auto const entry_type{::llvm::FunctionType::get(builder.getInt32Ty(), {builder.getInt32Ty()}, false)};
        auto const noargs{::llvm::FunctionType::get(builder.getVoidTy(), false)};
        auto const raise{::llvm::Function::Create(entry_type, ::llvm::GlobalValue::ExternalLinkage, "observer_raise", *module)};
        auto const inner_drop{::llvm::Function::Create(noargs, ::llvm::GlobalValue::ExternalLinkage, "observer_inner_cleanup", *module)};
        auto const outer_drop{::llvm::Function::Create(noargs, ::llvm::GlobalValue::ExternalLinkage, "observer_outer_exit", *module)};
        auto const read_payload{::llvm::Function::Create(::llvm::FunctionType::get(builder.getInt32Ty(), {builder.getPtrTy()}, false),
            ::llvm::GlobalValue::ExternalLinkage, "observer_payload", *module)};
        inner_drop->setDoesNotThrow(); outer_drop->setDoesNotThrow(); read_payload->setDoesNotThrow();
        auto const inner{::llvm::Function::Create(entry_type, ::llvm::GlobalValue::LinkOnceODRLinkage, "observer_inner", *module)};
        inner->setComdat(module->getOrInsertComdat("observer_inner"));
        inner->addFnAttr(::llvm::Attribute::NoInline);
        inner->setUWTableKind(::llvm::UWTableKind::Async);
        inner->setPersonalityFn(imported.personality);
        auto const ie{::llvm::BasicBlock::Create(context, "entry", inner)};
        auto const in{::llvm::BasicBlock::Create(context, "normal", inner)};
        auto const il{::llvm::BasicBlock::Create(context, "cleanup", inner)};
        builder.SetInsertPoint(ie);
        auto const result{builder.CreateInvoke(raise, in, il, {inner->getArg(0)})};
        builder.SetInsertPoint(in); builder.CreateRet(result);
        builder.SetInsertPoint(il);
        auto const record{builder.CreateLandingPad(::llvm::StructType::get(context, {builder.getPtrTy(), builder.getInt32Ty()}), 0u)};
        record->setCleanup(true);
        builder.CreateCall(inner_drop)->setDoesNotThrow(); builder.CreateResume(record);

        auto const outer{::llvm::Function::Create(entry_type, ::llvm::GlobalValue::ExternalLinkage, "observer_outer", *module)};
        outer->addFnAttr(::llvm::Attribute::NoInline); outer->setUWTableKind(::llvm::UWTableKind::Async);
        auto const oe{::llvm::BasicBlock::Create(context, "entry", outer)};
        auto const on{::llvm::BasicBlock::Create(context, "normal", outer)};
        auto const matched{::llvm::BasicBlock::Create(context, "matched", outer)};
        auto const rethrow{::llvm::BasicBlock::Create(context, "rethrow", outer)};
        auto exit{[&](::llvm::IRBuilder<>& b) { b.CreateCall(outer_drop)->setDoesNotThrow(); }};
        auto const caught{pads::emit_typed_entry(builder, *outer, imported, runtime, exit)};
        require(static_cast<bool>(caught), "actual typed landingpad emission");
        auto const caught_exit{pads::emit_caught_exit_cleanup(builder, *outer, runtime, exit)};
        builder.CreateCondBr(builder.CreateICmpEQ(outer->getArg(0), builder.getInt32(3)), rethrow, matched);
        builder.SetInsertPoint(rethrow); pads::emit_rethrow(builder, runtime, caught_exit);
        builder.SetInsertPoint(matched);
        auto const value{builder.CreateCall(read_payload, {caught.guest_object})}; value->setDoesNotThrow();
        pads::emit_end_catch(builder, runtime); exit(builder); builder.CreateRet(value);
        builder.SetInsertPoint(oe);
        auto const call{builder.CreateInvoke(inner, on, caught.landing, {outer->getArg(0)})};
        builder.SetInsertPoint(on); exit(builder); builder.CreateRet(call);
        require(!::llvm::verifyModule(*module, &::llvm::errs()), "real LLVM verifier");
        ::std::string ir;
        ::llvm::raw_string_ostream ir_stream{ir}; module->print(ir_stream, nullptr);
        ir_stream.flush();
        save(prefix, ".ll", ir.data(), ir.size());
        auto memory{::std::make_unique<observer>()}; auto const* observed{memory.get()};
        ::std::string error;
        // The cache outlives the engine even on the diagnostic preflight return.
        object_copy cache{prefix};
        ::llvm::EngineBuilder create{::std::move(module)};
        create.setEngineKind(::llvm::EngineKind::JIT).setErrorStr(&error).setMCJITMemoryManager(::std::move(memory));
        auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{create.create(target.release())}};
        if(!engine) { ::fast_io::io::perrln("ENGINE_ERROR ", ::fast_io::mnp::os_c_str(error.c_str())); }
        require(engine != nullptr, "actual MCJIT engine");
        engine->setObjectCache(&cache);
        require(symbols::bind_itanium_dwarf_host_symbols(*engine, imported, actual_guest_type(),
            function_address(&uwvm_observer_host_personality)) == symbols::error::ok, "real host type/personality binding");
        auto bind{[&](::llvm::GlobalValue const* symbol, auto pointer)
            { engine->addGlobalMapping(symbol, reinterpret_cast<void*>(function_address(pointer))); }};
        bind(raise, &raise_or_return); bind(inner_drop, &inner_cleanup); bind(outer_drop, &outer_exit); bind(read_payload, &payload);
        bind(runtime.begin, &::__cxxabiv1::__cxa_begin_catch); bind(runtime.end, &::__cxxabiv1::__cxa_end_catch); bind(runtime.rethrow, &::__cxxabiv1::__cxa_rethrow);
        engine->finalizeObject(); observed->dump();
        require(observed->good() && cache.count == 1u, "actual finalize/object capture");
        auto const outer_pc{static_cast<::std::uintptr_t>(engine->getFunctionAddress("observer_outer"))};
        auto const inner_pc{static_cast<::std::uintptr_t>(engine->getFunctionAddress("observer_inner"))};
        auto const personality_pc{static_cast<::std::uintptr_t>(engine->getFunctionAddress(symbols::personality_symbol))};
        bool const metadata{outer_pc && inner_pc && personality_pc &&
            observed->lookup(outer_pc, "outer-entry", personality_pc) && observed->lookup(inner_pc, "inner-entry", personality_pc) &&
            observed->executable(personality_pc, "actual-local-personality")};
        if(!metadata) { ::fast_io::io::perrln("FAIL component dynamic RVA/permission preflight; no generated code executed"); return 2; }
        running_observer = observed; running_personality = personality_pc;
        using entry = int(*)(int);
        // [live engine-owned executable allocation] [exact verified host signature]
        // [safe ] only the actual published symbol is converted, never a guessed PC.
        auto const function{reinterpret_cast<entry>(outer_pc)};
        for(int mode{}; mode != 4; ++mode)
        {
            int kind{}, answer{};
            ::fast_io::io::println("EXECUTE mode=", mode, " source=", UWVM_TEST_OBSERVER_SOURCE_ID);
            try { answer = function(mode); }
            catch(guest const& e) { kind = 1; answer = e.value; }
            catch(foreign const& e) { kind = 2; answer = e.value; }
            ::fast_io::io::println("RESULT mode=", mode, " kind=", kind, " answer=", answer, " native_cleanup=", native_cleanups,
                " inner_cleanup=", inner_cleanups, " outer_exit=", outer_exits, " live_return_lookup_ok=", live_return_lookup_ok);
            require(kind == (mode == 2 ? 2 : mode == 3 ? 1 : 0) && answer == (mode == 0 ? 17 : mode == 2 ? 77 : 42), "real catch/resume/rethrow result");
            require(native_cleanups == static_cast<unsigned>(mode + 1) && inner_cleanups == static_cast<unsigned>(mode) &&
                outer_exits == static_cast<unsigned>(mode + 1) && live_return_lookup_ok, "exactly-once native/generated cleanup and live return lookup");
        }
        running_observer = nullptr; running_personality = 0;
        engine->setObjectCache(nullptr); engine.reset();
        unwind_address image{};
        require(::fast_io::win32::nt::RtlLookupFunctionEntry(outer_pc, &image, nullptr) == nullptr &&
            ::fast_io::win32::nt::RtlLookupFunctionEntry(inner_pc, &image, nullptr) == nullptr, "actual tables retired before freed JIT storage");
        ::fast_io::io::println("PASS Win64 production SectionMemoryManager/native_exception_symbols component: real tables, local personality, catch/resume/rethrow/retirement; NOT whole-VM Wasm qualification");
        return 0;
    }
}
int main(int argc, char** argv)
{
    if(argc != 2) { ::fast_io::io::perrln("usage: observer.exe output-prefix"); return 64; }
    return exercise(argv[1]);
}
#else
int main()
{
    ::fast_io::io::perrln("UNSUPPORTED Win64 production unwind observer: actual GNU/Clang x64 SEH with C++ exceptions required; no qualification");
    return 78;
}
#endif
