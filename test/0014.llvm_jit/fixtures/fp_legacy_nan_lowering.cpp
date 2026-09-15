// Exercise production lowering on target IR, including repeated application:
// metadata/use replacement must be idempotent and leave verifiable IR. Native
// rounding modes also cover ordinary/constrained scalar and vector intrinsics,
// including RISC-V when extended-precision bridge lowering is disabled.
#include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <string_view>

int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 2; }
    llvm::LLVMContext context;
    llvm::SMDiagnostic error;
    auto module = llvm::parseIRFile(argv[1], error, context);
    if(!module)
    {
        error.print(argv[0], llvm::errs());
        return 3;
    }
    std::string_view mode{argc == 4 ? argv[3] : "legacy"};
    bool const native{mode.starts_with("native")};
    if(native)
    {
        llvm::Intrinsic::ID operations[]{llvm::Intrinsic::ceil,
                                         llvm::Intrinsic::floor,
                                         llvm::Intrinsic::trunc,
                                         llvm::Intrinsic::nearbyint,
                                         llvm::Intrinsic::roundeven};
        for(unsigned width: {32u, 64u})
        {
            for(unsigned op{}; op != 5; ++op)
            {
                std::string name{"vector_round" + std::to_string(width) + "_" + std::to_string(op)};
                if(module->getFunction(name)) { continue; }
                auto scalar{width == 32 ? llvm::Type::getFloatTy(context) : llvm::Type::getDoubleTy(context)};
                auto vector{llvm::FixedVectorType::get(scalar, 128 / width)};
                auto pointer{llvm::PointerType::getUnqual(context)};
                auto type{llvm::FunctionType::get(llvm::Type::getVoidTy(context), {pointer, pointer}, false)};
                auto function{llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, *module)};
                for(auto attribute: {"target-features", "target-cpu", "target-abi"})
                {
                    auto value{module->getFunction("rounding32")->getFnAttribute(attribute)};
                    if(value.isValid()) { function->addFnAttr(value); }
                }
                llvm::IRBuilder<> builder{llvm::BasicBlock::Create(context, "entry", function)};
                auto input{builder.CreateAlignedLoad(vector, function->getArg(0), llvm::Align{1})};
                auto result{builder.CreateIntrinsic(operations[op], {vector}, {input})};
                builder.CreateAlignedStore(result, function->getArg(1), llvm::Align{1});
                builder.CreateRetVoid();
            }
        }
        if(mode == "native-constrained")
        {
            for(auto& function: *module)
            {
                for(auto& block: function)
                {
                    for(auto it{block.begin()}; it != block.end();)
                    {
                        auto call{llvm::dyn_cast<llvm::CallInst>(&*it++)};
                        if(!call) { continue; }
                        llvm::Intrinsic::ID id;
                        switch(call->getIntrinsicID())
                        {
                            case llvm::Intrinsic::ceil: id = llvm::Intrinsic::experimental_constrained_ceil; break;
                            case llvm::Intrinsic::floor: id = llvm::Intrinsic::experimental_constrained_floor; break;
                            case llvm::Intrinsic::trunc: id = llvm::Intrinsic::experimental_constrained_trunc; break;
                            case llvm::Intrinsic::nearbyint: id = llvm::Intrinsic::experimental_constrained_nearbyint; break;
                            case llvm::Intrinsic::roundeven: id = llvm::Intrinsic::experimental_constrained_roundeven; break;
                            default: continue;
                        }
                        function.addFnAttr(llvm::Attribute::StrictFP);
                        llvm::IRBuilder<> builder{call};
                        builder.setDefaultConstrainedRounding(llvm::RoundingMode::NearestTiesToEven);
                        builder.setDefaultConstrainedExcept(llvm::fp::ebIgnore);
                        auto intrinsic{llvm::Intrinsic::getOrInsertDeclaration(module.get(), id, {call->getType()})};
                        auto result{builder.CreateConstrainedFPCall(intrinsic, {call->getArgOperand(0)})};
                        call->replaceAllUsesWith(result);
                        call->eraseFromParent();
                    }
                }
            }
        }
    }
    auto count = [&]
    {
        unsigned n{};
        for(auto& f: *module)
        {
            for(auto& b: f) { n += b.size(); }
        }
        return n;
    };
    auto lower = [&]
    {
        if(mode == "native-before") { return; }
        if(native)
        {
            uwvm2::runtime::compiler::shared::strict_float_jit::lower(*module, false);
            return;
        }
        // Cross-host equivalent of m68k's needs_extended_rounding path: lower
        // arithmetic first, then normalize only remaining native instructions.
        if(argc == 4) { uwvm2::runtime::compiler::shared::strict_float_jit::lower(*module, true, false, false); }
        uwvm2::runtime::compiler::shared::strict_float_jit::lower(*module, true, false, true);
    };
    lower();
    auto first = count();
    lower();
    if(count() != first || llvm::verifyModule(*module, &llvm::errs())) { return 4; }
    std::error_code ec;
    llvm::raw_fd_ostream output(argv[2], ec);
    if(ec) { return 5; }
    module->print(output, nullptr);
}
