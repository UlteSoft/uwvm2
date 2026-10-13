// Component-only N64EL TailCC oracle. It does not qualify Wasm lowering or cache.
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/Support/DynamicLibrary.h>
#include <unwind.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Triple.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if !defined(__linux__) || !defined(__mips__) || !defined(__mips64) || !defined(__MIPSEL__) || \
    __SIZEOF_POINTER__ != 8 || !defined(__mips_isa_rev) || __mips_isa_rev != 2 || \
    defined(__mips16) || defined(__mips_micromips)
#error This component requires a genuine Linux N64EL R2 process.
#endif

namespace mips_tailcc_fixture
{
    struct diagnostic_stream final : llvm::raw_ostream
    {
        fast_io::u8string bytes;
        diagnostic_stream() { SetUnbuffered(); }
        void write_impl(char const* p, std::size_t n) override
        { bytes.append(reinterpret_cast<char8_t const*>(p), n); }
        std::uint64_t current_pos() const override { return bytes.size(); }
    };

    struct code_range { std::uintptr_t begin, end; };
    inline std::vector<code_range> code_ranges;
    inline std::uint64_t unwind_checks{}, last_jit_frames{};
    struct measured_memory final : llvm::SectionMemoryManager
    {
        std::uint8_t* allocateCodeSection(std::uintptr_t size, unsigned align,
                                         unsigned id, llvm::StringRef name) override
        {
            auto p{llvm::SectionMemoryManager::allocateCodeSection(size, align, id, name)};
            auto begin{reinterpret_cast<std::uintptr_t>(p)};
            code_ranges.push_back({begin, begin + size});
            return p;
        }
    };
    struct walk_state
    {
        std::uintptr_t last_cfa{};
        std::uint64_t jit_frames{};
        bool invalid{};
    };
    _Unwind_Reason_Code visit_frame(_Unwind_Context* context, void* opaque) noexcept
    {
        auto& state{*static_cast<walk_state*>(opaque)};
        auto const cfa{static_cast<std::uintptr_t>(_Unwind_GetCFA(context))};
        // Each real caller must advance toward the caller's pre-call SP.
        if(cfa <= state.last_cfa)
        {
            state.invalid = true;
            return _URC_END_OF_STACK;
        }
        state.last_cfa = cfa;
        auto const ip{static_cast<std::uintptr_t>(_Unwind_GetIP(context))};
        for(auto const& range: code_ranges)
        {
            if(ip >= range.begin && ip < range.end) { ++state.jit_frames; break; }
        }
        return _URC_NO_REASON;
    }

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

// Test-only C callback; generated IR explicitly requests this observation.
extern "C" std::uint64_t uwvm_tailcc_check_unwind() noexcept
{
    mips_tailcc_fixture::walk_state walk;
    auto const reason{_Unwind_Backtrace(mips_tailcc_fixture::visit_frame, &walk)};
    ++mips_tailcc_fixture::unwind_checks;
    mips_tailcc_fixture::last_jit_frames = walk.jit_frames;
    return walk.invalid || walk.jit_frames != 2 || reason != _URC_END_OF_STACK;
}

int main(int argc, char** argv)
{
    if(argc != 6) { return 1; }
    if(std::strcmp(argv[4], "tail-calls") == 0)
    {
        char const* options[]{"mips-tailcc-native", "-mips-tail-calls"};
        llvm::cl::ParseCommandLineOptions(2, options);
    }
    else if(std::strcmp(argv[4], "default") != 0) { return 1; }
    bool const owned{std::strcmp(argv[5], "dso-local") == 0};
    if(!owned && std::strcmp(argv[5], "preemptible") != 0) { return 1; }
    unsigned level{};
    auto const end{argv[3] + std::strlen(argv[3])};
    auto const parsed{fast_io::parse_by_scan(argv[3], end, level)};
    if(parsed.code != fast_io::parse_code::ok || parsed.iter != end || level < 1 || level > 3) { return 2; }
    fast_io::native_file_loader source{fast_io::mnp::os_c_str(argv[1])};
    if(source.size() > (8u << 20)) { return 3; }
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    auto module{llvm::parseAssemblyString({source.data(), source.size()}, diagnostic, context)};
    if(!module)
    {
        mips_tailcc_fixture::diagnostic_stream stream;
        diagnostic.print("mips-tailcc-native", stream);
        fast_io::io::print(fast_io::u8err(), stream.bytes);
        return 4;
    }
    mips_tailcc_fixture::diagnostic_stream verification;
    if(llvm::verifyModule(*module, &verification))
    {
        fast_io::io::print(fast_io::u8err(), verification.bytes);
        return 5;
    }
    module->setTargetTriple(llvm::Triple{"mips64el-unknown-linux-gnuabi64"});
    // This is an explicit diagnostic control, never evidence that an
    // arbitrary external/imported symbol belongs to a Wasm publication.
    if(owned)
    {
        for(auto& function: *module)
        { if(!function.isDeclaration()) { function.setDSOLocal(true); } }
    }
    if(llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter() ||
       llvm::InitializeNativeTargetAsmParser()) { return 6; }
    llvm::sys::DynamicLibrary::AddSymbol("uwvm_tailcc_check_unwind",
        reinterpret_cast<void*>(&uwvm_tailcc_check_unwind));
    llvm::TargetOptions options{};
    options.EnableCFIFixup = true;
    std::vector<std::string> features{"+noabicalls", "+long-calls", "-mips16", "-micromips"};
    std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder{std::move(module)}
        .setEngineKind(llvm::EngineKind::JIT)
        .setOptLevel(level == 1 ? llvm::CodeGenOptLevel::Less :
                     level == 2 ? llvm::CodeGenOptLevel::Default : llvm::CodeGenOptLevel::Aggressive)
        .setTargetOptions(options)
        .setMCJITMemoryManager(std::make_unique<mips_tailcc_fixture::measured_memory>())
        .setMCPU("mips64r2")
        .setMAttrs(features)
        .create()};
    if(!engine) { return 7; }
    mips_tailcc_fixture::capture capture{argv[2]};
    engine->setObjectCache(&capture);
    engine->finalizeObject();
    auto const address{engine->getFunctionAddress("entry")};
    if(address == 0) { return 8; }
    std::uintptr_t before{}, after{};
    __asm__ __volatile__("move %0, $sp" : "=r"(before) :: "memory");
    auto const result{reinterpret_cast<std::uint64_t(*)()>(static_cast<std::uintptr_t>(address))()};
    __asm__ __volatile__("move %0, $sp" : "=r"(after) :: "memory");
    engine->setObjectCache(nullptr);
    if(result != 0 || before != after || capture.compiled != 1) { return 9; }
    fast_io::io::println("tailcc-native-result=", result, " stack-restored=", before == after,
                        " compiled=", capture.compiled, " codegen-O", level,
                        " backend-policy=", fast_io::mnp::os_c_str(argv[4]),
                        " ownership=", fast_io::mnp::os_c_str(argv[5]),
                        " unwind-checked=", mips_tailcc_fixture::unwind_checks,
                        " unwind-jit-frames=", mips_tailcc_fixture::last_jit_frames);
}
