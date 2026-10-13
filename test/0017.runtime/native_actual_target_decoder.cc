// Actual finalized LLVM engine -> owned target DATA -> two actual MC decoders.
// Supplied instruction bytes are owned DATA; no JIT address is dereferenced.
// This component is not a genuine native trap/OS stepping qualification.
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/uwvm/debugger/native_owned_instruction_semantics.h>
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/MC/MCAsmInfo.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <fast_io.h>
namespace lib = ::uwvm2::runtime::lib;
namespace dbg = ::uwvm2::uwvm::debugger;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("actual target MC failure line=", __LINE__); return 1; } } while(false)
template<typename Out, typename In>
static bool copy(Out& out, ::std::size_t& size, In const& actual)
{
    if(actual.size() >= out.size()) { return false; }
    for(::std::size_t i{}; i != actual.size(); ++i)
    {
        // [actual engine-owned input ... complete size][owned output capacity]
        // [safe                                     ] both bounds were checked;
        //  ^^ one indexed byte copy; no supplied address or pointer advance.
        if(actual[i] == '\0') { return false; }
        out[i] = actual[i];
    }
    size = actual.size(); out[size] = '\0'; return true;
}
template<typename Value>
static auto const* sdk_address(Value const& actual)
{
    if constexpr(::std::is_pointer_v<Value>) { return actual; }
    else { return ::std::addressof(actual); }
}
template<typename Asm, typename STI>
static auto maximum(Asm const& a, STI const& s)
{
    if constexpr(requires { a.getMaxInstLength(::std::addressof(s)); })
    { return a.getMaxInstLength(::std::addressof(s)); }
    else { return a.getMaxInstLength(); }
}
int main()
{
    CHECK(dbg::native_disassembly::details::initialize_native());
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("actual-target-metadata-test", context)};
    auto const type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context), false)};
    auto function{::llvm::Function::Create(type, ::llvm::Function::ExternalLinkage, "constant42", module.get())};
    auto block{::llvm::BasicBlock::Create(context, "entry", function)};
    ::llvm::IRBuilder<> builder{block};
    builder.CreateRet(::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(context), 42u));
    CHECK(!::llvm::verifyModule(*module, ::std::addressof(::llvm::errs())));
    ::std::string error{};
    ::llvm::EngineBuilder owner{::std::move(module)};
    owner.setEngineKind(::llvm::EngineKind::JIT);
    owner.setErrorStr(::std::addressof(error));
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{owner.create()};
    CHECK(engine);
    engine->finalizeObject();
    CHECK(engine->getFunctionAddress("constant42") != 0u);
    auto const* machine{engine->getTargetMachine()};
    CHECK(machine != nullptr);
    auto const* assembly{sdk_address(machine->getMCAsmInfo())};
    auto const* subtarget{sdk_address(machine->getMCSubtargetInfo())};
    CHECK(assembly != nullptr && subtarget != nullptr);
    lib::llvm_jit_debug_native_target target{};
    CHECK(copy(target.triple, target.triple_size, machine->getTargetTriple().str()));
    CHECK(copy(target.cpu, target.cpu_size, machine->getTargetCPU()));
    CHECK(copy(target.features, target.features_size, machine->getTargetFeatureString()));
    target.description_version = 1u; target.available = true;
    target.pointer_bits = engine->getDataLayout().getPointerSizeInBits();
    target.little_endian = engine->getDataLayout().isLittleEndian();
    target.maximum_instruction_bytes = maximum(*assembly, *subtarget);
    target.minimum_instruction_alignment = assembly->getMinInstAlignment();
    CHECK(target.pointer_bits == sizeof(::std::uintptr_t) * 8u && dbg::native_target_metadata::valid(target));
    dbg::native_disassembly::decoder display{target};
    dbg::native_owned_instruction_semantics::decoder semantics{target};
    CHECK(display && semantics);
    ::std::array<::std::uint8_t, 4u> bytes{};
    ::std::size_t size{};
    auto const& triple{machine->getTargetTriple()};
    if(triple.isX86()) { bytes[0u] = 0x31u; bytes[1u] = 0xc0u; size = 2u; }
    else if(triple.isAArch64())
    {
        // A64 instruction encodings are little-endian, including BE8 data targets.
        bytes = {0x00u,0x04u,0x00u,0x91u}; size = 4u; // add x0,x0,#1
    }
    else if(triple.isRISCV()) { bytes = {0x13u,0u,0u,0u}; size = 4u; }
    else
    {
        ::fast_io::io::println("actual target MC: explicit first-corpus missing for this target; no native qualification");
        return 77;
    }
    auto const shown{display.decode(0x1000u, {bytes.data(), size})};
    auto const decoded{semantics.decode(0x1000u, {bytes.data(), size})};
    CHECK(shown && decoded && shown.size == size && decoded.semantics().size == size);
    CHECK(decoded.safe_for_single_instruction()); // DATA policy, not execution permission.
    CHECK(!display.decode(0x1000u, {bytes.data(), size - 1u}));
    CHECK(!semantics.decode(0x1000u, {bytes.data(), size - 1u}));
    CHECK(!display.decode(UINTPTR_MAX, {bytes.data(), size}));
    if(target.minimum_instruction_alignment != 1u)
    {
        CHECK(!display.decode(0x1001u, {bytes.data(), size}));
        CHECK(!semantics.decode(0x1001u, {bytes.data(), size}));
    }
    auto malformed{target}; malformed.triple_size = malformed.triple.size();
    CHECK(!dbg::native_disassembly::decoder{malformed} && !dbg::native_owned_instruction_semantics::decoder{malformed});
    malformed = target; malformed.features[0u] = '\0'; malformed.features_size = 1u;
    CHECK(!dbg::native_disassembly::decoder{malformed} && !dbg::native_owned_instruction_semantics::decoder{malformed});
    malformed = target; malformed.little_endian = !target.little_endian;
    CHECK(!dbg::native_owned_instruction_semantics::decoder{malformed});
    malformed = target; malformed.maximum_instruction_bytes = target.maximum_instruction_bytes == 32u ? 31u : target.maximum_instruction_bytes + 1u;
    CHECK(!dbg::native_owned_instruction_semantics::decoder{malformed});
    malformed = target; malformed.description_version = 2u;
    CHECK(!dbg::native_disassembly::decoder{malformed} && !dbg::native_owned_instruction_semantics::decoder{malformed});
    ::fast_io::io::println("actual target MC: PASS target=",
        ::fast_io::basic_io_scatter_t<char>{target.triple.data(), target.triple_size},
        " same-finalized-engine=yes decoder-policy-DATA-only=yes native-execution-qualified=no");
}
