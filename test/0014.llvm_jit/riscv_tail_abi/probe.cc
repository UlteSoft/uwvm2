// Exact-provider MCJIT driver for the RISC-V tail ABI regression fixtures.
// This test has no guest inputs, process-global symbol registration or cache reuse.
#include <cstdint>
#include <cxxabi.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <unwind.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ObjectCache.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

extern "C" _Unwind_Reason_Code __gxx_personality_v0(int, _Unwind_Action, std::uint64_t,
                                                   _Unwind_Exception*, _Unwind_Context*);

namespace
{
struct probe_exception final { std::uint64_t value; };
void const* probe_exception_typeinfo()
{
    // C++ EH emits this exact type's descriptor even with -fno-rtti. Retain the
    // immutable descriptor, never the activation or caught object's address.
    static void const* const descriptor{[]() -> void const*
    {
        try { throw probe_exception{}; }
        catch(probe_exception const&) { return __cxxabiv1::__cxa_current_exception_type(); }
    }()};
    return descriptor;
}
std::uintptr_t minimum_sp{UINTPTR_MAX}, maximum_sp{};
std::uintptr_t selected_target{};
std::uint64_t observations{}, unwind_frames{};
extern "C" std::uintptr_t probe_get_target() noexcept { return selected_target; }
_Unwind_Reason_Code frame(_Unwind_Context* context, void*)
{
    if(_Unwind_GetIP(context) != 0) { ++unwind_frames; }
    return unwind_frames < 64 ? _URC_NO_REASON : _URC_END_OF_STACK;
}
extern "C" void probe_observe_stack() noexcept
{
    std::uintptr_t sp{};
#if defined(__riscv) && __riscv_xlen == 64
    asm volatile("mv %0, sp" : "=r"(sp));
#else
# error "This execution probe must run on the real RV64 target ABI (under QEMU is supported)."
#endif
    if(sp < minimum_sp) { minimum_sp = sp; }
    if(sp > maximum_sp) { maximum_sp = sp; }
    ++observations;
}
extern "C" [[noreturn]] void probe_raise(std::uint64_t value)
{
    probe_observe_stack();
    _Unwind_Backtrace(frame, nullptr);
    throw probe_exception{value};
}
class object_observer final : public llvm::ObjectCache
{
    std::string path;
public:
    explicit object_observer(std::string target) : path{std::move(target)} {}
    void notifyObjectCompiled(llvm::Module const*, llvm::MemoryBufferRef object) override
    {
        std::error_code error;
        llvm::raw_fd_ostream output(path, error, llvm::sys::fs::OF_None);
        if(error) { std::abort(); }
        auto const bytes{object.getBuffer()};
        // [LLVM-owned emitted buffer ...] The callback retains this full span.
        // [safe                         ] Only exactly its size is read, with no pointer adjustment.
        output.write(bytes.data(), bytes.size());
        output.flush();
        if(output.has_error()) { std::abort(); }
    }
    std::unique_ptr<llvm::MemoryBuffer> getObject(llvm::Module const*) override { return {}; }
};
}

int main(int argc, char** argv)
{
    if(argc != 7) { return 2; }
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    auto module{llvm::parseIRFile(argv[1], diagnostic, context)};
    if(!module) { diagnostic.print("tail-probe", llvm::errs()); return 3; }
    if(llvm::verifyModule(*module, &llvm::errs())) { return 4; }
    if(std::string{argv[2]} == "verify") { std::printf("PASS verifier LLVM %s\n", LLVM_VERSION_STRING); return 0; }
    if(llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter()) { return 5; }
    std::string error;
    llvm::EngineBuilder builder{std::move(module)};
    builder.setMCPU("generic-rv64").setMAttrs(std::vector<std::string>{"+m", "+a", "+f", "+d", "+c", "-v"});
    builder.setEngineKind(llvm::EngineKind::JIT).setErrorStr(&error)
        .setOptLevel(llvm::CodeGenOptLevel::Default)
        .setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
    object_observer observer{argv[2]};
    std::unique_ptr<llvm::ExecutionEngine> engine{builder.create()};
    if(!engine) { std::fprintf(stderr, "%s\n", error.c_str()); return 6; }
    engine->addGlobalMapping("probe_observe_stack", reinterpret_cast<std::uintptr_t>(&probe_observe_stack));
    engine->addGlobalMapping("probe_raise", reinterpret_cast<std::uintptr_t>(&probe_raise));
    engine->addGlobalMapping("probe_get_target", reinterpret_cast<std::uintptr_t>(&probe_get_target));
    engine->addGlobalMapping("probe_exception_typeinfo", reinterpret_cast<std::uintptr_t>(probe_exception_typeinfo()));
    engine->addGlobalMapping("__gxx_personality_v0", reinterpret_cast<std::uintptr_t>(&__gxx_personality_v0));
    engine->addGlobalMapping("__cxa_begin_catch", reinterpret_cast<std::uintptr_t>(&__cxxabiv1::__cxa_begin_catch));
    engine->addGlobalMapping("__cxa_end_catch", reinterpret_cast<std::uintptr_t>(&__cxxabiv1::__cxa_end_catch));
    engine->setObjectCache(&observer);
    engine->finalizeObject();
    // [engine-owned executable sink] Keep its complete address in host state;
    // [safe                        ] the engine remains alive for every callback.
    // This models the runtime table bridge without medlow static data addresses.
    selected_target = static_cast<std::uintptr_t>(engine->getFunctionAddress("sink"));
    auto const address{engine->getFunctionAddress("run_case")};
    if(address == 0 || engine->hasError()) { return 7; }
    // [engine-owned executable run_case] The validated fixture declares i64().
    // [safe                            ] Engine ownership outlives the native call and unwind.
    auto const entry{reinterpret_cast<std::uint64_t(*)()>(static_cast<std::uintptr_t>(address))};
    auto const expected{std::strtoull(argv[3], nullptr, 10)};
    bool const expect_throw{std::string{argv[4]} == "throw"};
    bool const expect_caught_in_jit{std::string{argv[4]} == "caught"};
    std::uint64_t value{};
    bool caught{};
    try { value = entry(); }
    catch(probe_exception const& exception) { caught = true; value = exception.value; }
    auto const span{observations ? maximum_sp - minimum_sp : 0u};
    auto const maximum_span{std::strtoull(argv[5], nullptr, 10)};
    auto const minimum_observations{std::strtoull(argv[6], nullptr, 10)};
    if(value != expected || caught != expect_throw || span > maximum_span || observations < minimum_observations ||
       ((expect_throw || expect_caught_in_jit) && unwind_frames < 4))
    {
        std::printf("FAIL value=%llu expected=%llu caught=%u observations=%llu span=%llu frames=%llu\n",
                    static_cast<unsigned long long>(value), expected, unsigned(caught),
                    static_cast<unsigned long long>(observations), static_cast<unsigned long long>(span),
                    static_cast<unsigned long long>(unwind_frames));
        return 8;
    }
    std::printf("PASS LLVM %s value=%llu caught=%u observations=%llu span=%llu frames=%llu\n", LLVM_VERSION_STRING,
                static_cast<unsigned long long>(value), unsigned(caught), static_cast<unsigned long long>(observations),
                static_cast<unsigned long long>(span), static_cast<unsigned long long>(unwind_frames));
}
