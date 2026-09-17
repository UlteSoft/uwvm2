// Verify production arithmetic emission after LLVM O3, including sNaN identity folds.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace detail = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
int main(int argc, char** argv)
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("wasm-nan-arithmetic", context);
    if(argc > 2) { module->setTargetTriple(llvm::Triple{argv[2]}); }
    llvm::IRBuilder<> builder{context};
    for(unsigned width : {32u, 64u})
    {
        for(unsigned operation{}; operation != 5u; ++operation)
        {
            auto integer = builder.getIntNTy(width);
            auto fp = width == 32u ? builder.getFloatTy() : builder.getDoubleTy();
            auto signature = llvm::FunctionType::get(integer, {integer}, false);
            auto function = llvm::Function::Create(signature, llvm::Function::ExternalLinkage,
                "nan_" + std::to_string(width) + "_" + std::to_string(operation), *module);
            if(module->getTargetTriple().isLoongArch()) { function->addFnAttr("target-features", "+f,+d"); }
            builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", function));
            auto input = builder.CreateBitCast(function->getArg(0), fp);
            llvm::Value* value{};
            {
                detail::llvm_wasm_arithmetic_scope scope{builder};
                switch(operation)
                {
                    case 0: value = detail::emit_llvm_float_binary(builder, input, llvm::ConstantFP::get(fp, 1.0), 3u); break;
                    case 1: value = detail::emit_llvm_float_binary(builder, input, llvm::ConstantFP::get(fp, 1.0), 2u); break;
                    case 2: value = detail::emit_llvm_float_binary(builder, input, llvm::ConstantFP::get(fp, 0.0), 1u); break;
                    case 3: value = detail::emit_llvm_float_binary(builder, input, llvm::ConstantFP::get(fp, -1.0), 3u); break;
                    default:
                        value = width == 32u ?
                            detail::emit_llvm_float_demote(builder, detail::emit_llvm_float_promote(builder, input)) :
                            detail::emit_llvm_float_promote(builder, detail::emit_llvm_float_demote(builder, input));
                }
            }
            builder.CreateRet(builder.CreateBitCast(value, integer));
        }
    }
    {
        auto signature = llvm::FunctionType::get(builder.getInt64Ty(), {builder.getInt32Ty()}, false);
        auto function = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, "nan_promote", *module);
        if(module->getTargetTriple().isLoongArch()) { function->addFnAttr("target-features", "+f,+d"); }
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", function));
        auto promoted = detail::emit_llvm_float_promote(builder, builder.CreateBitCast(function->getArg(0), builder.getFloatTy()));
        builder.CreateRet(builder.CreateBitCast(promoted, builder.getInt64Ty()));
    }
    llvm::LoopAnalysisManager loops;
    llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager cgscc;
    llvm::ModuleAnalysisManager modules;
    llvm::PassBuilder passes;
    passes.registerModuleAnalyses(modules);
    passes.registerCGSCCAnalyses(cgscc);
    passes.registerFunctionAnalyses(functions);
    passes.registerLoopAnalyses(loops);
    passes.crossRegisterProxies(loops, functions, cgscc, modules);
    auto pipeline = passes.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
    pipeline.run(*module, modules);
    if(llvm::verifyModule(*module, &llvm::errs())) { return 1; }
    if(argc > 1) { module->print(llvm::outs(), nullptr); return 0; }
    auto engine = std::unique_ptr<llvm::ExecutionEngine>{llvm::EngineBuilder(std::move(module)).create()};
    if(!engine) { return 2; }
    engine->finalizeObject();
    for(unsigned width : {32u, 64u}) for(unsigned op{}; op != 5u; ++op)
    {
        auto address = engine->getFunctionAddress("nan_" + std::to_string(width) + "_" + std::to_string(op));
        if(!address) { return 3; }
        if(width == 32u)
        {
            auto function = reinterpret_cast<std::uint32_t(*)(std::uint32_t)>(address);
            // Core numeric NaN propagation has two different obligations:
            // canonical-only NaN inputs require a canonical payload; arbitrary
            // NaN inputs permit any arithmetic payload, but never signaling.
            // Merely checking the quiet bit would miss a broken canonical case.
            for(auto bits : {0x7fa00001u, 0xffa00001u, 0x7fc12345u, 0xffc12345u, 0x7fc00000u, 0xffc00000u})
            {
                auto result{function(bits)};
                if((result & 0x7fc00000u) != 0x7fc00000u) { return 4; }
                if((bits & 0x7fffffffu) == 0x7fc00000u && (result & 0x7fffffffu) != 0x7fc00000u) { return 4; }
            }
            if(function(0x3f800000u) != (op == 3u ? 0xbf800000u : 0x3f800000u)) { return 5; }
        }
        else
        {
            auto function = reinterpret_cast<std::uint64_t(*)(std::uint64_t)>(address);
            for(auto bits : {0x7ff4000000000001ull, 0xfff4000000000001ull, 0x7ff8123456789abcull, 0xfff8123456789abcull,
                             0x7ff8000000000000ull, 0xfff8000000000000ull})
            {
                auto result{function(bits)};
                if((result & 0x7ff8000000000000ull) != 0x7ff8000000000000ull) { return 6; }
                if((bits & 0x7fffffffffffffffull) == 0x7ff8000000000000ull &&
                   (result & 0x7fffffffffffffffull) != 0x7ff8000000000000ull) { return 6; }
            }
            if(function(0x3ff0000000000000ull) != (op == 3u ? 0xbff0000000000000ull : 0x3ff0000000000000ull)) { return 7; }
        }
    }
    auto promote = reinterpret_cast<std::uint64_t(*)(std::uint32_t)>(engine->getFunctionAddress("nan_promote"));
    if(!promote) { return 8; }
    for(auto bits : {0x7fa00001u, 0xffa00001u, 0x7fc12345u, 0xffc12345u, 0x7fc00000u, 0xffc00000u})
    {
        auto result{promote(bits)};
        if((result & 0x7ff8000000000000ull) != 0x7ff8000000000000ull) { return 9; }
        if((bits & 0x7fffffffu) == 0x7fc00000u && (result & 0x7fffffffffffffffull) != 0x7ff8000000000000ull) { return 9; }
    }
    if(promote(0x3f800000u) != 0x3ff0000000000000ull) { return 10; }
    std::puts("NaN arithmetic: 77 optimized native cases passed (canonical and arithmetic payloads checked separately)");
}
