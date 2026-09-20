// Core float_exprs.wast: trunc_s then convert_u must reinterpret the integer
// bits, not retain its signed value. Exercise production emission after O3;
// the original LLVM 23 X86 combine fails both i32 mixed-signedness cases.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

namespace detail = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static unsigned checks{};

template<typename Float, typename Bits>
bool check(llvm::ExecutionEngine& engine, unsigned integer_width, bool trunc_signed, bool convert_signed, std::string const& name)
{
    auto address = engine.getFunctionAddress(name);
    if(!address) { return false; }
    auto function = reinterpret_cast<Bits(*)(Bits)>(address);
    for(Float input : {Float{-1.5}, Float{-0.5}, Float{-0.0}, Float{0.0}, Float{0.5}, Float{1.5}, Float{2147483520.0},
                       Float{2147483648.0}, Float{4294967040.0}, Float{9223372036854775808.0}, Float{18446744073709549568.0}})
    {
        if(!trunc_signed && input <= Float{-1.0}) { continue; } // outside LLVM fptoui's domain
        if(input >= std::ldexp(Float{1.0}, integer_width - static_cast<unsigned>(trunc_signed))) { continue; }
        // Check the source conversion's domain BEFORE any host cast; high
        // unsigned inputs cannot pass through int64_t without undefined behavior.
        auto truncated = trunc_signed ? static_cast<std::uint64_t>(static_cast<std::int64_t>(input)) : static_cast<std::uint64_t>(input);
        Float expected;
        if(integer_width == 32u)
        {
            auto raw = static_cast<std::uint32_t>(truncated);
            expected = convert_signed ? static_cast<Float>(std::bit_cast<std::int32_t>(raw)) : static_cast<Float>(raw);
        }
        else
        {
            auto raw = static_cast<std::uint64_t>(truncated);
            expected = convert_signed ? static_cast<Float>(std::bit_cast<std::int64_t>(raw)) : static_cast<Float>(raw);
        }
        auto actual = function(std::bit_cast<Bits>(input));
        ++checks;
        if(actual != std::bit_cast<Bits>(expected))
        {
            std::fprintf(stderr, "%s input=%g expected=%llx actual=%llx\n", name.c_str(), static_cast<double>(input),
                         static_cast<unsigned long long>(std::bit_cast<Bits>(expected)), static_cast<unsigned long long>(actual));
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv)
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::LLVMContext context;
    auto module = std::make_unique<llvm::Module>("mixed-signedness-conversion", context);
    module->setTargetTriple(llvm::Triple{argc > 2 ? argv[2] : llvm::sys::getDefaultTargetTriple()});
    llvm::IRBuilder<> builder{context};
    for(unsigned fp_width : {32u, 64u}) for(unsigned int_width : {32u, 64u})
    for(bool trunc_signed : {false, true}) for(bool convert_signed : {false, true})
    {
        auto name = "roundtrip_" + std::to_string(fp_width) + "_" + std::to_string(int_width) +
                    (trunc_signed ? "_s" : "_u") + (convert_signed ? "_s" : "_u");
        auto bits = builder.getIntNTy(fp_width);
        auto fp = fp_width == 32u ? builder.getFloatTy() : builder.getDoubleTy();
        auto function = llvm::Function::Create(llvm::FunctionType::get(bits, {bits}, false), llvm::Function::ExternalLinkage, name, *module);
        if(argc > 3) { function->addFnAttr("target-features", argv[3]); }
        builder.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", function));
        auto input = builder.CreateBitCast(function->getArg(0), fp);
        auto integer = trunc_signed ? builder.CreateFPToSI(input, builder.getIntNTy(int_width)) : builder.CreateFPToUI(input, builder.getIntNTy(int_width));
        // The raw mode validates a replacement LLVM's combine itself before
        // removing a production workaround. Production mode remains default.
        bool const raw{argc > 1 && std::string{argv[1]} == "emit-raw"};
        auto result = raw ? (convert_signed ? builder.CreateSIToFP(integer, fp) : builder.CreateUIToFP(integer, fp))
                          : detail::emit_llvm_int_to_float(builder, integer, fp, convert_signed);
        builder.CreateRet(builder.CreateBitCast(result, bits));
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
    auto engine = std::unique_ptr<llvm::ExecutionEngine>{llvm::EngineBuilder(std::move(module)).setMCPU(llvm::sys::getHostCPUName()).create()};
    if(!engine) { return 2; }
    engine->finalizeObject();
    for(unsigned fp_width : {32u, 64u}) for(unsigned int_width : {32u, 64u})
    for(bool trunc_signed : {false, true}) for(bool convert_signed : {false, true})
    {
        auto name = "roundtrip_" + std::to_string(fp_width) + "_" + std::to_string(int_width) +
                    (trunc_signed ? "_s" : "_u") + (convert_signed ? "_s" : "_u");
        bool ok = fp_width == 32u ? check<float, std::uint32_t>(*engine, int_width, trunc_signed, convert_signed, name) :
                                   check<double, std::uint64_t>(*engine, int_width, trunc_signed, convert_signed, name);
        if(!ok) { return 3; }
    }
    std::printf("PASS: 16 conversion families, %u exact-bit results after LLVM O3\n", checks);
}
