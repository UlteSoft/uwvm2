// Generate the production SIMD IR and an independent byte-buffer oracle for cross execution.
// This checks code generation, not an entire JIT runtime running under QEMU.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <fstream>
#include "relaxed_simd_vectors.h"

int main(int argc, char** argv)
{
    if(argc != 5) { return 2; }
    llvm::InitializeAllTargetInfos(); llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs(); llvm::InitializeAllAsmPrinters();
    llvm::Triple triple{llvm::Triple::normalize(argv[1])};
    std::string error;
    auto target{llvm::TargetRegistry::lookupTarget("", triple, error)};
    if(!target) { llvm::errs() << error; return 3; }
    std::unique_ptr<llvm::TargetMachine> machine{target->createTargetMachine(triple, argv[2], argv[3], llvm::TargetOptions{}, llvm::Reloc::PIC_)};
    if(!machine) { return 4; }
    llvm::LLVMContext context;
    llvm::Module module{"wasm3-relaxed-simd", context};
    module.setTargetTriple(triple); module.setDataLayout(machine->createDataLayout());
    llvm::IRBuilder<> builder{context};
    namespace shared = uwvm2::runtime::compiler::shared;
    namespace compiler = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
    using code = compiler::llvm_jit_simd_code;
    for(unsigned opcode{0x100}; opcode != 0x114; ++opcode)
    {
        auto ptr{llvm::PointerType::getUnqual(context)};
        auto function{llvm::Function::Create(llvm::FunctionType::get(builder.getVoidTy(), {ptr, ptr}, false),
            llvm::Function::ExternalLinkage, "relaxed_" + std::to_string(opcode), module)};
        function->addFnAttr("target-cpu", argv[2]); function->addFnAttr("target-features", argv[3]);
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", function));
        compiler::simd_ir::emitter emitter{builder};
        llvm::Value* arguments[3]{};
        auto count{uwvm2::validation::standard::wasm3::relaxed_simd_operand_count(opcode)};
        for(unsigned i{}; i != count; ++i)
        {
            auto load{builder.CreateLoad(emitter.integer(8u), builder.CreateGEP(builder.getInt8Ty(), function->getArg(1), builder.getInt32(i * 16u)))};
            load->setAlignment(llvm::Align{1}); arguments[i] = load;
        }
        auto found{shared::visit_wasm1p1_simd_instruction(static_cast<code>(opcode),
            [&]<code Op, shared::wasm1p1_simd_instruction_kind, shared::wasm1p1_simd_scalar_kind, std::size_t, std::uint_least32_t>()
            {
                auto result{compiler::simd_ir::emit_value(builder, Op, arguments[0], arguments[1], arguments[2], 0, nullptr)};
                if(!result) { return false; }
                builder.CreateStore(result, function->getArg(0))->setAlignment(llvm::Align{1});
                builder.CreateRetVoid(); return true;
            })};
        if(!found) { return 5; }
    }
    if(llvm::verifyModule(module, &llvm::errs())) { return 6; }
    std::error_code file_error;
    llvm::raw_fd_ostream ir{std::string{argv[4]} + ".ll", file_error};
    if(file_error) { return 7; }
    module.print(ir, nullptr); ir.close(); if(ir.has_error()) { return 7; }
    std::ofstream c{std::string{argv[4]} + ".c"};
    c << "#include <stdio.h>\n";
    for(unsigned opcode{256}; opcode != 276; ++opcode) { c << "extern void relaxed_" << opcode << "(unsigned char*, const unsigned char*);\n"; }
    auto bytes{[&](auto const& values) { c << "{"; for(auto v: values) { c << unsigned(v) << ','; } c << "}"; }};
    c << "struct test {unsigned op; unsigned char args[48], expected[16], mask[16];};\nstatic const struct test cases[]={\n";
    for(auto const& item: relaxed_cases)
    {
        c << '{' << item.opcode << ",{ "; for(auto const& operand: item.args) for(auto byte: operand) { c << unsigned(byte) << ','; }
        c << "},"; bytes(item.expected); c << ','; bytes(item.mask); c << "},\n";
    }
    c << "};\nint main(void){for(unsigned n=0;n<sizeof(cases)/sizeof(cases[0]);++n){unsigned char out[16];switch(cases[n].op){\n";
    for(unsigned opcode{256}; opcode != 276; ++opcode) { c << "case " << opcode << ":relaxed_" << opcode << "(out,cases[n].args);break;\n"; }
    c << "default:return 3;}for(unsigned i=0;i<16;++i)if(((out[i]^cases[n].expected[i])&cases[n].mask[i])!=0){printf(\"FAIL case=%u op=%x byte=%u actual=%x expected=%x\\n\",n,cases[n].op,i,out[i],cases[n].expected[i]);return 1;}}puts(\"PASS 25 relaxed SIMD vectors\");return 0;}\n";
    return c ? 0 : 8;
}
