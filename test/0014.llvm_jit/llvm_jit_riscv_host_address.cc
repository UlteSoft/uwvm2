// Generate exact production RISC-V64 host-address lowering for cross-codegen.
// No arguments: verify IR and defensive rejection cases. HARNESS.c: also emit
// LLVM IR on stdout and a C runtime harness for the generated target object.
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/host_address_emit.h>
}
namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

int main(int argc, char** argv)
{
    if(argc > 2) { return 2; }
    if constexpr(sizeof(std::uintptr_t) != 8)
    { std::puts("SKIP: full-width RISC-V64 cross generator needs a 64-bit host"); return argc == 1 ? 0 : 77; }
    std::vector<std::uint64_t> values{1, 0x7ff, 0x800, 0xfff, 0x7fffffff,
        0x80000000, 0xffffffff, 0x100000000, 0x7fffffffffffffff,
        0x8000000000000000, 0xffffffffffffffff, 0x123456789abcdef0,
        0x00007ffff0000123, 0xffff800000000123};
    std::uint64_t state{0x7f4a7c159e3779b9};
    for(unsigned i{}; i != 256; ++i)
    {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        values.push_back(state);
    }
    llvm::LLVMContext context;
    llvm::Module module("riscv-host-address", context);
    module.setTargetTriple(llvm::Triple{"riscv64-unknown-linux-gnu"});
    module.setDataLayout("e-m:e-p:64:64-i64:64-i128:128-n32:64-S128");
    llvm::IRBuilder<> builder(context);
    auto* pointer_type{builder.getPtrTy()};
    if(d::get_llvm_riscv64_immediate_pointer_value(builder, 1, pointer_type) != nullptr) { return 3; }
    auto* signature{llvm::FunctionType::get(builder.getInt64Ty(), false)};
    std::ostringstream harness;
    harness << "#define _GNU_SOURCE\n#include <stdint.h>\n#include <stdio.h>\n#include <sys/mman.h>\n#include <sys/resource.h>\n#include <unistd.h>\n";
    for(unsigned i{}; i != values.size(); ++i)
    {
        auto name{"pointer_" + std::to_string(i)};
        auto* function{llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name, module)};
        function->addFnAttr(llvm::Attribute::NoUnwind);
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", function));
        if(d::get_llvm_riscv64_immediate_pointer_value(builder, 0, pointer_type) != nullptr ||
           d::get_llvm_riscv64_immediate_pointer_value(builder, 1, nullptr) != nullptr ||
           d::get_llvm_riscv64_immediate_pointer_value(builder, 1, builder.getInt64Ty()) != nullptr) { return 4; }
        auto* pointer{d::get_llvm_riscv64_immediate_pointer_value(builder, values[i], pointer_type)};
        if(pointer == nullptr) { return 5; }
        builder.CreateRet(builder.CreatePtrToInt(pointer, builder.getInt64Ty()));
        unsigned asm_count{};
        for(auto const& instruction: function->getEntryBlock())
        {
            if(llvm::isa<llvm::AllocaInst, llvm::LoadInst, llvm::StoreInst>(instruction)) { return 6; }
            if(auto* call{llvm::dyn_cast<llvm::CallInst>(&instruction)}; call != nullptr)
            {
                auto* assembly{llvm::dyn_cast<llvm::InlineAsm>(call->getCalledOperand())};
                if(assembly == nullptr || assembly->getAsmString() != "li $0, $1" ||
                   assembly->getConstraintString() != "=r,i" || !call->doesNotAccessMemory() ||
                   !call->doesNotThrow() || assembly->hasSideEffects()) { return 7; }
                ++asm_count;
            }
        }
        if(asm_count != 1) { return 8; }
        harness << "extern uint64_t " << name << "(void);\n";
    }
    constexpr std::uint64_t data_address{0x100100000}, code_address{0x100120000};
    auto* unary_signature{llvm::FunctionType::get(builder.getInt64Ty(), {builder.getInt64Ty()}, false)};
    for(unsigned operation{}; operation != 3; ++operation)
    {
        char const* name{operation == 0 ? "load_high" : operation == 1 ? "store_high" : "call_high"};
        auto* function{llvm::Function::Create(unary_signature, llvm::Function::ExternalLinkage, name, module)};
        function->addFnAttr(llvm::Attribute::NoUnwind);
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", function));
        auto* pointer{d::get_llvm_riscv64_immediate_pointer_value(builder,
            operation == 2 ? code_address : data_address, pointer_type)};
        if(pointer == nullptr) { return 9; }
        llvm::Value* result{function->getArg(0)};
        if(operation == 0) { result = builder.CreateLoad(builder.getInt64Ty(), pointer); }
        else if(operation == 1) { builder.CreateStore(result, pointer); }
        else { result = builder.CreateCall(unary_signature, pointer, {result}); }
        builder.CreateRet(result);
        harness << "extern uint64_t " << name << "(uint64_t);\n";
    }
    harness << "int main(void) {\n";
    harness << "struct rlimit no_core = {0, 0}; if (setrlimit(RLIMIT_CORE, &no_core) != 0) return 9;\n";
    for(unsigned i{}; i != values.size(); ++i)
    {
        harness << "if (pointer_" << i << "() != UINT64_C(" << values[i]
                << ")) { printf(\"FAIL address " << i << "\\n\"); return 1; }\n";
    }
    harness << "size_t page = (size_t)sysconf(_SC_PAGESIZE);\n"
            << "void *data = mmap((void*)UINT64_C(" << data_address << "), page, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0);\n"
            << "if (data == MAP_FAILED || (uintptr_t)data != UINT64_C(" << data_address << ")) { perror(\"mmap data\"); return 2; }\n"
            << "*(uint64_t*)data = UINT64_C(0xfedcba9876543210);\n"
            << "if (load_high(0) != UINT64_C(0xfedcba9876543210)) return 3;\n"
            << "if (store_high(UINT64_C(0x123456789abcdef0)) != UINT64_C(0x123456789abcdef0) || *(uint64_t*)data != UINT64_C(0x123456789abcdef0)) return 4;\n"
            << "void *code = mmap((void*)UINT64_C(" << code_address << "), page, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0);\n"
            << "if (code == MAP_FAILED || (uintptr_t)code != UINT64_C(" << code_address << ")) { perror(\"mmap code\"); return 5; }\n"
            << "((uint32_t*)code)[0] = UINT32_C(0x00150513); ((uint32_t*)code)[1] = UINT32_C(0x00008067);\n"
            << "__builtin___clear_cache((char*)code, (char*)code + 8);\n"
            << "if (mprotect(code, page, PROT_READ|PROT_EXEC) != 0) { perror(\"mprotect code\"); return 6; }\n"
            << "if (call_high(41) != 42) return 7;\n"
            << "if (munmap(data, page) != 0 || munmap(code, page) != 0) return 8;\n"
            << "puts(\"PASS 270 full-width pointers and >4 GiB load/store/call\"); return 0; }\n";
    if(llvm::verifyModule(module, &llvm::errs()) || !module.global_empty()) { return 10; }
    if(argc == 2)
    {
        std::ofstream output(argv[1]);
        output << harness.str();
        if(!output) { return 11; }
        module.print(llvm::outs(), nullptr);
    }
}
