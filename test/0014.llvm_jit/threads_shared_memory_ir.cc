#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <string>
int main()
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace jit=uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    for(bool shared:{false,true}) for(bool unwind:{false,true})
    {
        module_builder builder{};builder.has_memory=builder.memory_has_max=true;
        builder.memory_shared=shared;builder.memory_min=1;builder.memory_max=5;
        func_body size{},grow{},load{};
        for(auto b:{0x3f,0,0x0b}){append_u8(size.code,b);}
        for(auto b:{0x41,1,0x40,0,0x0b}){append_u8(grow.code,b);}
        for(auto b:{0x41,0,0x28,2,0,0x0b}){append_u8(load.code,b);}
        builder.add_func({{}, {k_val_i32}},std::move(size));
        builder.add_func({{}, {k_val_i32}},std::move(grow));
        builder.add_func({{}, {k_val_i32}},std::move(load));
        auto wasm=builder.build();auto features=make_wasm1p1_feature_parameter();
        uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features).disable_threads=false;
        auto prepared=prepare_runtime_from_wasm(wasm,u8"shared-size",{},features);
        jit::compile_option options{};options.validator_feature_parameter=&features;options.verify_llvm_jit_ir=true;
        options.emit_call_stack_frames=!unwind;options.emit_unwind_call_stack_frames=unwind;
        uwvm2::validation::error::code_validation_error_impl error{};
        auto compiled=jit::compile_all_from_uwvm(*prepared.mod,options,error,0);
        UWVM2TEST_REQUIRE(compiled.llvm_jit_module.emitted);
        auto& module=*compiled.llvm_jit_module.llvm_module;UWVM2TEST_REQUIRE(!llvm::verifyModule(module,&llvm::errs()));
        unsigned sizes{};
        for(auto& function:module) for(auto& block:function) for(auto& instruction:block)
        {
            if(auto* length=llvm::dyn_cast<llvm::LoadInst>(&instruction);length!=nullptr && length->getName().starts_with("memory.length"))
            {
                ++sizes;
                UWVM2TEST_REQUIRE(length->getOrdering()==(shared ? llvm::AtomicOrdering::SequentiallyConsistent : llvm::AtomicOrdering::Acquire));
            }
        }
        UWVM2TEST_REQUIRE(sizes==2); // Plain mmap i32.load adds no byte-length load.
        if(auto dir=std::getenv("UWVM_SHARED_IR_DIR");dir!=nullptr)
        {
            std::string name=std::string(dir)+(shared ? "/shared-" : "/unshared-")+(unwind ? "unwind.ll" : "instruction.ll");
            std::error_code output_error{};llvm::raw_fd_ostream output{name,output_error};UWVM2TEST_REQUIRE(!output_error);
            module.print(output,nullptr);output.close();UWVM2TEST_REQUIRE(!output.has_error());
        }
    }
    std::puts("PASS shared size LLVM IR: seq_cst size/grow, acquire unshared size, branch-free ordinary mmap access, instruction/unwind");
}
