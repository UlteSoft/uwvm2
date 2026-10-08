// The keeper runs this against the actual product LLVM closure. Optional argv[1]
// saves verified IR through fast_io for official llc/dwarfdump qualification.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <string>
#include <fast_io.h>

namespace provenance = ::uwvm2::runtime::compiler::llvm_jit::native_provenance;

int main(int argc, char** argv)
{
    if(argc > 2) { return 1; }
    ::llvm::LLVMContext context{};
    ::llvm::Module module{"native-provenance-qualified-input", context};
    auto* const integer{::llvm::Type::getInt32Ty(context)};
    auto* const signature{::llvm::FunctionType::get(integer, {integer}, false)};
    auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "native_provenance_probe", module)};
    auto* const scope{provenance::attach(*function, "uwvm-m2-f3-g4.wasm-native-v1")};
    if(scope == nullptr || scope != function->getSubprogram() || scope->getUnit() == nullptr ||
       scope->getUnit()->getEmissionKind() != ::llvm::DICompileUnit::FullDebug ||
       scope->getFile()->getFilename() != "uwvm-m2-f3-g4.wasm-native-v1" ||
       scope->getFile()->getDirectory() != provenance::directory ||
       scope->getUnit()->getProducer() != provenance::producer) { return 2; }
    auto* const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    ::llvm::IRBuilder<> builder{entry};
    if(!provenance::location(builder, scope, 0u, 3u)) { return 3; }
    auto* const add{builder.CreateAdd(function->getArg(0u), builder.getInt32(9u), "wasm_offset_zero")};
    if(!provenance::location(builder, scope, 2u, 3u)) { return 4; }
    auto* const multiply{builder.CreateMul(add, builder.getInt32(7u), "wasm_offset_two")};
    provenance::unknown(builder, scope);
    auto* const returned{builder.CreateRet(multiply)};
    if(::llvm::cast<::llvm::Instruction>(add)->getDebugLoc().getLine() != 1u ||
       ::llvm::cast<::llvm::Instruction>(multiply)->getDebugLoc().getLine() != 3u ||
       returned->getDebugLoc().getLine() != 0u ||
       provenance::location(builder, scope, 3u, 3u) ||
       provenance::location(builder, nullptr, 0u, 3u) ||
       provenance::location(builder, scope, (::std::numeric_limits<::std::size_t>::max)(), 3u) ||
       provenance::attach(*function, "another-generation") != nullptr || ::llvm::verifyModule(module)) { return 5; }

    // Ordinary modules do not acquire metadata merely by including the helper.
    ::llvm::Module ordinary{"ordinary-full-no-debug-metadata", context};
    auto* const plain{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "plain", ordinary)};
    if(plain->getSubprogram() != nullptr || ordinary.getNamedMetadata("llvm.dbg.cu") != nullptr ||
       ordinary.getModuleFlag("uwvm.native.provenance.version") != nullptr) { return 6; }
    ::llvm::Module conflict{"existing-incompatible-dwarf", context};
    conflict.addModuleFlag(::llvm::Module::Error, "Dwarf Version", 4u);
    auto* const incompatible{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "conflict", conflict)};
    if(provenance::attach(*incompatible, "uwvm-m2-f3-g4.wasm-native-v1") != nullptr ||
       incompatible->getSubprogram() != nullptr || conflict.getNamedMetadata("llvm.dbg.cu") != nullptr) { return 7; }

    if(argc == 2)
    {
        ::std::string ir{};
        ::llvm::raw_string_ostream buffer{ir};
        module.print(buffer, nullptr);
        ::fast_io::native_file output{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::out};
        ::fast_io::io::print(output, ::fast_io::mnp::strvw(ir));
    }
    ::fast_io::io::println("PASS native provenance LLVM metadata, real IR verification, bounds, unknown scaffolding, conflicting-version refusal");
}
