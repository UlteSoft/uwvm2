// Executes private-ABI IR through the actual target MCJIT and captures its object.
// A successful component run does not qualify the Wasm emitter or signed cache.
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdint>
#include <cxxabi.h>
#include <unwind.h>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" _Unwind_Reason_Code __gxx_personality_v0(int, _Unwind_Action, std::uint64_t,
                                                   _Unwind_Exception*, _Unwind_Context*);

namespace tailcc_fixture
{
    struct exception final { std::uint64_t value; };
    unsigned raises{}, unwind_frames{};
    _Unwind_Reason_Code observe_frame(_Unwind_Context* context, void*) noexcept
    {
        if(_Unwind_GetIP(context) != 0) { ++unwind_frames; }
        return unwind_frames < 64 ? _URC_NO_REASON : _URC_END_OF_STACK;
    }
    extern "C" [[noreturn]] void tailcc_native_raise(std::uint64_t value)
    {
        ++raises;
        _Unwind_Backtrace(observe_frame, nullptr);
        throw exception{value};
    }
    void const* exception_typeinfo()
    {
        // Obtain the exact typed descriptor without requiring RTTI. The
        // descriptor has static lifetime; no caught activation is retained.
        try { throw exception{}; }
        catch(exception const&) { return __cxxabiv1::__cxa_current_exception_type(); }
    }

    struct diagnostic_stream final : llvm::raw_ostream
    {
        fast_io::u8string bytes;
        diagnostic_stream() { SetUnbuffered(); }
        void write_impl(char const* p, std::size_t n) override
        { bytes.append(reinterpret_cast<char8_t const*>(p), n); }
        std::uint64_t current_pos() const override { return bytes.size(); }
    };

    struct capture final : llvm::ObjectCache
    {
        char const* path;
        unsigned compiled{};
        explicit capture(char const* value) : path{value} {}
        void notifyObjectCompiled(llvm::Module const*, llvm::MemoryBufferRef bytes) override
        {
            if(++compiled != 1) { fast_io::fast_terminate(); }
            fast_io::native_file file{fast_io::mnp::os_c_str(path),
                fast_io::open_mode::out | fast_io::open_mode::excl,
                static_cast<fast_io::perms>(0600)};
            auto first{reinterpret_cast<std::byte const*>(bytes.getBufferStart())};
            fast_io::operations::write_all_bytes(file, first, first + bytes.getBufferSize());
            file.close();
        }
        std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override { return {}; }
    };
}

int main(int argc, char** argv)
{
    if(argc != 6) { return 1; }
    unsigned level{};
    auto end{argv[3] + std::strlen(argv[3])};
    auto parsed{fast_io::parse_by_scan(argv[3], end, level)};
    if(parsed.code != fast_io::parse_code::ok || parsed.iter != end || level < 1 || level > 3) { return 2; }
    llvm::CodeModel::Model model;
    if(std::strcmp(argv[4], "small") == 0) { model = llvm::CodeModel::Small; }
    else if(std::strcmp(argv[4], "medium") == 0) { model = llvm::CodeModel::Medium; }
    else if(std::strcmp(argv[4], "large") == 0) { model = llvm::CodeModel::Large; }
    else { return 3; }
    std::vector<std::string> features;
    if(std::strcmp(argv[5], "scalar") == 0) { features = {"-lsx", "-lasx"}; }
    else if(std::strcmp(argv[5], "lsx") == 0) { features = {"+lsx", "-lasx"}; }
    else if(std::strcmp(argv[5], "lasx") == 0) { features = {"+lsx", "+lasx"}; }
    else { return 4; }
    fast_io::native_file_loader source{fast_io::mnp::os_c_str(argv[1])};
    if(source.size() > (8u << 20)) { return 5; }
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    auto module{llvm::parseAssemblyString({source.data(), source.size()}, diagnostic, context)};
    if(!module)
    {
        tailcc_fixture::diagnostic_stream stream;
        diagnostic.print("tailcc-native", stream);
        fast_io::io::print(fast_io::u8err(), stream.bytes);
        return 6;
    }
    tailcc_fixture::diagnostic_stream verification;
    if(llvm::verifyModule(*module, &verification))
    {
        fast_io::io::print(fast_io::u8err(), verification.bytes);
        return 7;
    }
    if(llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter() ||
       llvm::InitializeNativeTargetAsmParser()) { return 8; }
    bool const needs_exception_symbols{module->getNamedGlobal("tailcc_exception_typeinfo") != nullptr};
    bool const expects_exception{module->getNamedGlobal("tailcc_expects_raise") != nullptr};
    std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder{std::move(module)}
        .setEngineKind(llvm::EngineKind::JIT)
        .setOptLevel(level == 1 ? llvm::CodeGenOptLevel::Less :
                     level == 2 ? llvm::CodeGenOptLevel::Default : llvm::CodeGenOptLevel::Aggressive)
        .setCodeModel(model).setMCPU("la464").setMAttrs(features).create()};
    if(!engine) { return 9; }
    if(needs_exception_symbols)
    {
        engine->addGlobalMapping("tailcc_raise", reinterpret_cast<std::uintptr_t>(&tailcc_fixture::tailcc_native_raise));
        engine->addGlobalMapping("tailcc_exception_typeinfo",
            reinterpret_cast<std::uintptr_t>(tailcc_fixture::exception_typeinfo()));
        engine->addGlobalMapping("__gxx_personality_v0", reinterpret_cast<std::uintptr_t>(&__gxx_personality_v0));
        engine->addGlobalMapping("__cxa_begin_catch", reinterpret_cast<std::uintptr_t>(&__cxxabiv1::__cxa_begin_catch));
        engine->addGlobalMapping("__cxa_end_catch", reinterpret_cast<std::uintptr_t>(&__cxxabiv1::__cxa_end_catch));
    }
    tailcc_fixture::capture capture{argv[2]};
    engine->setObjectCache(&capture);
    engine->finalizeObject();
    auto address{engine->getFunctionAddress("entry")};
    if(address == 0) { return 10; }
    std::uintptr_t before{}, after{};
#if defined(__loongarch__)
    __asm__ __volatile__("or %0, $sp, $zero" : "=r"(before) :: "memory");
#else
#error This component qualifies the LoongArch native ABI only.
#endif
    auto result{reinterpret_cast<std::uint64_t(*)()>(static_cast<std::uintptr_t>(address))()};
    __asm__ __volatile__("or %0, $sp, $zero" : "=r"(after) :: "memory");
    engine->setObjectCache(nullptr);
    if(result != 0 || before != after || capture.compiled != 1 ||
       tailcc_fixture::raises != unsigned(expects_exception) ||
       (expects_exception && tailcc_fixture::unwind_frames < 4)) { return 11; }
    fast_io::io::println("tailcc-result=", result, " stack-restored=", before == after,
                        " compiled=", capture.compiled, " throws=", tailcc_fixture::raises,
                        " unwind-frames=", tailcc_fixture::unwind_frames, " codegen-O", level,
                        " model=", fast_io::mnp::os_c_str(argv[4]),
                        " features=", fast_io::mnp::os_c_str(argv[5]));
}
