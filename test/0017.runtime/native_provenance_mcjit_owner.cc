// Actual MCJIT loaded-object/lifetime COMPONENT. It creates/finalizes native
// code in the keeper's cgroup but never calls a generated function. Actual VM
// capture/trap credentials and product fresh-TU qualification are separate.
#define UWVM_RUNTIME_LLVM_JIT 1
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <uwvm2/runtime/lib/uwvm_runtime_pending_code_ranges.h>
#include <uwvm2/uwvm/debugger/native_disassembly_abi.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <string>
#include <vector>
#include <fast_io.h>

namespace metadata = ::uwvm2::runtime::compiler::llvm_jit::native_provenance;
namespace provenance = ::uwvm2::runtime::lib::details::native_loaded_provenance;
namespace abi = ::uwvm2::uwvm::debugger::native_disassembly_abi;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native provenance MCJIT owner failure line=", __LINE__); return 1; } } while(false)

#if defined(__GNUC__) || defined(__clang__)
# if defined(__APPLE__)
#  define PROBE_ASM_PRINTER_SYMBOL(name) __asm__("_" #name)
# else
#  define PROBE_ASM_PRINTER_SYMBOL(name) __asm__(#name)
# endif
# if defined(__x86_64__) || defined(_M_X64)
extern "C" LLVM_C_ABI void probe_asm_printer() noexcept PROBE_ASM_PRINTER_SYMBOL(LLVMInitializeX86AsmPrinter);
# elif defined(__aarch64__) || defined(_M_ARM64)
extern "C" LLVM_C_ABI void probe_asm_printer() noexcept PROBE_ASM_PRINTER_SYMBOL(LLVMInitializeAArch64AsmPrinter);
# endif
#endif

struct native_owner
{
    // Reverse destruction retires actual executable engine before context/rows.
    provenance::image rows{};
    ::std::unique_ptr<::llvm::LLVMContext> context{};
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{};
    ::std::uintptr_t begin{}, end{}, sample{};
};

static bool build(native_owner& owner, unsigned generation, bool debug)
{
    owner.context = ::std::make_unique<::llvm::LLVMContext>();
    auto module{::std::make_unique<::llvm::Module>("actual-loaded-native-provenance", *owner.context)};
    auto* integer{::llvm::Type::getInt32Ty(*owner.context)};
    auto* signature{::llvm::FunctionType::get(integer, {integer}, false)};
    auto* function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "native_provenance_probe", *module)};
    auto identity{::fast_io::concat("uwvm-m2-f3-g", ::fast_io::mnp::dec(generation), ".wasm-native-v1")};
    auto* scope{debug ? metadata::attach(*function, identity) : nullptr};
    if(debug && scope == nullptr) { return false; }
    auto* block{::llvm::BasicBlock::Create(*owner.context, "entry", function)};
    ::llvm::IRBuilder<> builder{block};
    if(debug && !metadata::location(builder, scope, 0u, 3u)) { return false; }
    auto* add{builder.CreateAdd(function->getArg(0u), builder.getInt32(9u))};
    if(debug && !metadata::location(builder, scope, 2u, 3u)) { return false; }
    auto* multiply{builder.CreateMul(add, builder.getInt32(7u))};
    if(debug) { metadata::unknown(builder, scope); }
    builder.CreateRet(multiply);
    if(::llvm::verifyModule(*module)) { return false; }
    ::std::string error{};
    owner.engine.reset(::llvm::EngineBuilder(::std::move(module)).setEngineKind(::llvm::EngineKind::JIT).setErrorStr(&error).create());
    if(!owner.engine) { return false; }
    ::uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges pending{*owner.engine, true};
    pending.configure_debug_full_capture(debug);
    owner.engine->finalizeObject();
    if(owner.engine->hasError()) { return false; }
    auto const address{owner.engine->getFunctionAddress("native_provenance_probe")};
    if(address == 0u || address > UINTPTR_MAX) { return false; }
    owner.begin = static_cast<::std::uintptr_t>(address);
    pending.commit([](::std::uintptr_t, ::std::uintptr_t) noexcept {},
        [&](::std::uintptr_t begin, ::std::uintptr_t size) noexcept
        {
            if(begin == owner.begin && size != 0u && size <= UINTPTR_MAX - begin)
            { owner.end = begin + size; }
        });
    owner.rows = pending.take_debug_full_capture(7u);
    if(owner.end <= owner.begin || owner.end - owner.begin > 65536u) { return false; }
    if(!debug) { return !owner.rows.valid() && owner.rows.row_count() == 0u; }
    if(!owner.rows.valid() || owner.rows.row_count() == 0u) { return false; }
    for(auto pc{owner.begin}; pc < owner.end; ++pc)
    {
        auto const point{owner.rows.lookup(pc, owner.begin, owner.end, identity, 3u, 7u)};
        if(point.state == provenance::status::exact)
        {
            if((point.wasm_offset != 0u && point.wasm_offset != 2u) || point.begin > pc || point.end <= pc) { return false; }
            owner.sample = pc; return true;
        }
    }
    return false;
}

int main()
{
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64))
# if defined(__x86_64__) || defined(_M_X64)
    abi::uwvm_LLVMInitializeX86TargetInfo(); abi::uwvm_LLVMInitializeX86Target(); abi::uwvm_LLVMInitializeX86TargetMC();
# else
    abi::uwvm_LLVMInitializeAArch64TargetInfo(); abi::uwvm_LLVMInitializeAArch64Target(); abi::uwvm_LLVMInitializeAArch64TargetMC();
# endif
    probe_asm_printer();
    native_owner original{}, replacement{}, ordinary{};
    CHECK(build(original, 4u, true));
    CHECK(build(replacement, 5u, true));
    CHECK(build(ordinary, 4u, false));
    CHECK(original.begin != replacement.begin && original.sample != 0u && replacement.sample != 0u);
    auto const old{original.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 7u)};
    auto const fresh{replacement.rows.lookup(replacement.sample, replacement.begin, replacement.end, "uwvm-m2-f3-g5.wasm-native-v1", 3u, 7u)};
    CHECK(old.state == provenance::status::exact && fresh.state == provenance::status::exact);
    CHECK(original.rows.lookup(replacement.sample, replacement.begin, replacement.end, "uwvm-m2-f3-g5.wasm-native-v1", 3u, 7u).state == provenance::status::unavailable);
    CHECK(replacement.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 7u).state == provenance::status::unavailable);
    CHECK(original.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 8u).state == provenance::status::unavailable);
    original.rows.invalidate_runtime_generation();
    CHECK(original.rows.lookup(original.sample, original.begin, original.end, "uwvm-m2-f3-g4.wasm-native-v1", 3u, 7u).state == provenance::status::unavailable);
    CHECK(old.state == provenance::status::exact); // Retained DATA grants no future live authority.
    ::fast_io::io::println("PASS actual MCJIT object callback, pending commit, independent engine/generation maps, ordinary opt-out, epoch revocation; no generated function executed");
#else
    ::fast_io::io::println("UNAVAILABLE native provenance MCJIT component: qualified Clang/GNU x86_64/AArch64 LLVM closure required");
    return 77;
#endif
}
#undef PROBE_ASM_PRINTER_SYMBOL
