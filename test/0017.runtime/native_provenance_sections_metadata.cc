// Independent two-text-section IR fixture for real llc ELF/COFF/Mach-O
// relocation qualification. This source is separate from the frozen r1
// single-function metadata fixture; no native execution is claimed here.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <fast_io.h>
#include <fast_io_dsal/string_view.h>
#include <string>

namespace provenance = ::uwvm2::runtime::compiler::llvm_jit::native_provenance;

int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
    auto const format{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[2])}};
    if(format != "elf" && format != "coff" && format != "macho") { return 2; }
    ::llvm::LLVMContext context{};
    ::llvm::Module module{"native-provenance-independent-sections", context};
    auto* const integer{::llvm::Type::getInt32Ty(context)};
    auto* const signature{::llvm::FunctionType::get(integer, {integer}, false)};
    auto const emit{[&](char const* name, char const* identity, char const* section,
                       ::std::size_t first_offset, ::std::size_t second_offset, ::std::size_t extent)
    {
        auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, name, module)};
        function->setSection(section);
        auto* const scope{provenance::attach(*function, identity)};
        if(scope == nullptr) { return false; }
        auto* const entry{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{entry};
        if(!provenance::location(builder, scope, first_offset, extent)) { return false; }
        auto* const add{builder.CreateAdd(function->getArg(0u), builder.getInt32(9u), "first_origin")};
        if(!provenance::location(builder, scope, second_offset, extent)) { return false; }
        auto* const multiply{builder.CreateMul(add, builder.getInt32(7u), "second_origin")};
        provenance::unknown(builder, scope);
        builder.CreateRet(multiply);
        return true;
    }};
    auto const padding{[&](char const* name, char const* section)
    {
        // An exported preceding function forces the mapped function away from
        // the section base. This exposes relocation loss of an inplace local
        // addend even when each section's loaded base is otherwise correct.
        auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, name, module)};
        function->setSection(section);
        auto* const entry{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{entry}; builder.CreateRet(builder.getInt32(13u));
    }};
    bool const macho{format == "macho"};
    padding("native_padding_first", macho ? "__TEXT,__prov_first" : ".text.prov_first");
    if(!emit("native_provenance_first", "uwvm-m2-f3-g4.wasm-native-v1",
             macho ? "__TEXT,__prov_first" : ".text.prov_first", 0u, 2u, 3u)) { return 1; }
    padding("native_padding_second", macho ? "__TEXT,__prov_second" : ".text.prov_second");
    if(!emit("native_provenance_second", "uwvm-m5-f7-g9.wasm-native-v1",
             macho ? "__TEXT,__prov_second" : ".text.prov_second", 1u, 4u, 5u) ||
       ::llvm::verifyModule(module)) { return 1; }
    ::std::string ir{};
    ::llvm::raw_string_ostream buffer{ir}; module.print(buffer, nullptr);
    ::fast_io::native_file output{argv[1], ::fast_io::open_mode::out};
    ::fast_io::io::print(output, ::fast_io::mnp::strvw(ir));
    ::fast_io::io::println("PASS real LLVM verifier, two independently named provenance functions/text sections; llc/object relocation remains a separate qualification");
}
