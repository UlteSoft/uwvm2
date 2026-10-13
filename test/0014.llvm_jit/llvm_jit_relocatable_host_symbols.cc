// The production pointer carrier must rebind fresh process storage and bridges
// when the same object is loaded by a second process. This fixture's raw object
// is private test input; it is not a runtime persistent-cache format.
#include <uwvm2/utils/container/impl.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>
#include <cstring>

namespace fixture
{
    llvm::StringRef get_llvm_string_ref(uwvm2::utils::container::u8string const& value)
    { return {reinterpret_cast<char const*>(value.data()), value.size()}; }
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/relocatable_host_symbol_emit.h>

    struct text_stream final : llvm::raw_ostream
    {
        uwvm2::utils::container::u8string bytes;
        text_stream() { SetUnbuffered(); }
        void write_impl(char const* p, std::size_t n) override
        { bytes.append(reinterpret_cast<char8_t const*>(p), n); }
        std::uint64_t current_pos() const override { return bytes.size(); }
    };
    void write(char const* path, char const* data, std::size_t size)
    {
        fast_io::native_file file{fast_io::mnp::os_c_str(path),
            fast_io::open_mode::out | fast_io::open_mode::excl, static_cast<fast_io::perms>(0600)};
        if(size) { fast_io::operations::write_all_bytes(file,
            reinterpret_cast<std::byte const*>(data), reinterpret_cast<std::byte const*>(data) + size); }
        file.close();
    }
    std::uint64_t bridge(std::uint64_t value) { return value ^ 0x1234567812345678ull; }
    std::uint64_t other_bridge(std::uint64_t value) { return value ^ 0x8765432187654321ull; }
    struct cache final : llvm::ObjectCache
    {
        char const* path;
        bool reading;
        unsigned compiled{}, hits{};
        cache(char const* p, bool read) : path{p}, reading{read} {}
        void notifyObjectCompiled(llvm::Module const*, llvm::MemoryBufferRef buffer) override
        {
            ++compiled;
            if(reading) { fast_io::fast_terminate(); }
            write(path, buffer.getBufferStart(), buffer.getBufferSize());
        }
        std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override
        {
            if(!reading) { return {}; }
            fast_io::native_file_loader bytes{fast_io::mnp::os_c_str(path)};
            if(bytes.size() > (64u << 20)) { fast_io::fast_terminate(); }
            ++hits;
            return llvm::MemoryBuffer::getMemBufferCopy({bytes.data(), bytes.size()});
        }
    };
}

int main(int argc, char** argv)
{
    bool const emit{argc == 4 && std::strcmp(argv[1], "emit-ir") == 0};
    bool const read{argc == 4 && std::strcmp(argv[1], "read") == 0};
    if(argc != 4 || (!emit && !read && std::strcmp(argv[1], "write") != 0)) { return 1; }
    llvm::LLVMContext context;
    auto module{std::make_unique<llvm::Module>("relocatable_host_fixture", context)};
    if(emit) { module->setTargetTriple(llvm::Triple{llvm::Triple::normalize(argv[2])}); }
    auto const integer{llvm::Type::getInt64Ty(context)};
    auto const type{llvm::FunctionType::get(integer, {}, false)};
    auto const function{llvm::Function::Create(type, llvm::GlobalValue::ExternalLinkage, "run_binding", module.get())};
    llvm::IRBuilder<> builder{llvm::BasicBlock::Create(context, "entry", function)};
    auto const storage{new llvm::GlobalVariable{*module, integer, false,
        llvm::GlobalValue::ExternalLinkage, nullptr, "uwvm_fixture_storage"}};
    auto const bridge_type{llvm::FunctionType::get(integer, {integer}, false)};
    auto const bridge{llvm::Function::Create(bridge_type, llvm::GlobalValue::ExternalLinkage,
        "uwvm_fixture_bridge", module.get())};
    auto const bound_storage{fixture::get_llvm_relocatable_host_symbol_pointer(builder, storage)};
    auto const bound_bridge{fixture::get_llvm_relocatable_host_symbol_pointer(builder, bridge)};
    if(!bound_storage || !bound_bridge) { return 2; }
    auto const value{builder.CreateLoad(integer, bound_storage)};
    auto const called{builder.CreateCall(bridge_type, bound_bridge, {builder.getInt64(7)})};
    builder.CreateRet(builder.CreateAdd(value, called));
    if(llvm::verifyModule(*module)) { return 3; }
    if(emit)
    {
        fixture::text_stream stream;
        module->print(stream, nullptr);
        fixture::write(argv[3], reinterpret_cast<char const*>(stream.bytes.data()), stream.bytes.size());
        return 0;
    }
    std::uint64_t actual{};
    auto const end{argv[3] + std::strlen(argv[3])};
    auto const parsed{fast_io::parse_by_scan(argv[3], end, actual)};
    if(parsed.code != fast_io::parse_code::ok || parsed.iter != end) { return 8; }
    if(llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter() ||
       llvm::InitializeNativeTargetAsmParser()) { return 4; }
    auto const current_bridge{actual & 1u ? fixture::bridge : fixture::other_bridge};
    // Match production's named-symbol resolver. The next process deliberately
    // binds the same external name to a different bridge body and fresh data.
    llvm::sys::DynamicLibrary::AddSymbol("uwvm_fixture_storage", &actual);
    llvm::sys::DynamicLibrary::AddSymbol("uwvm_fixture_bridge", reinterpret_cast<void*>(current_bridge));
    std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder{std::move(module)}
        .setEngineKind(llvm::EngineKind::JIT).create()};
    if(!engine) { return 5; }
    fixture::cache cache{argv[2], read};
    engine->setObjectCache(&cache);
    engine->finalizeObject();
    auto const address{engine->getFunctionAddress("run_binding")};
    if(address == 0) { return 6; }
    auto const result{reinterpret_cast<std::uint64_t(*)()>(static_cast<std::uintptr_t>(address))()};
    engine->setObjectCache(nullptr);
    if(result != actual + current_bridge(7) || cache.hits != static_cast<unsigned>(read) ||
       cache.compiled != static_cast<unsigned>(!read)) { return 7; }
    fast_io::io::println("cache-hit=", cache.hits, " compiled=", cache.compiled,
        " storage=", fast_io::mnp::hex(reinterpret_cast<std::uintptr_t>(&actual)),
        " bridge=", fast_io::mnp::hex(reinterpret_cast<std::uintptr_t>(current_bridge)), " value=", actual);
}
