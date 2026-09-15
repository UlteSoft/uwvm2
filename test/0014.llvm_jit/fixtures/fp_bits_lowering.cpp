// Cross-codegen regression: run with FEATURES OUTPUT.ll on an LLVM host, then
// compile for i386 and link fp_bits_runner.cpp with the native i686 SDK.
#include <uwvm2/runtime/compiler/shared/strict_float_jit.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <string>

int main(int argc, char** argv)
{
    if(argc != 3) { return 2; }
    llvm::LLVMContext context;
    llvm::Module module("wasm-fp-bits", context);
    module.setTargetTriple(llvm::Triple("i386-unknown-linux-gnu"));
    llvm::IRBuilder<> b(context);
    for(unsigned width: {32u, 64u})
    {
        for(unsigned op = 0; op != 25; ++op)
        {
            auto fp = width == 32u ? b.getFloatTy() : b.getDoubleTy();
            llvm::Type* arg_type = op == 14u                  ? (width == 32u ? b.getDoubleTy() : b.getFloatTy())
                                   : (op == 15u || op == 16u) ? static_cast<llvm::Type*>(b.getInt64Ty())
                                                              : fp;
            auto name = "bits_" + std::to_string(width) + "_" + std::to_string(op);
            llvm::Type* result_type = op < 17u ? fp : (op == 17u || op == 18u || op == 21u || op == 22u) ? b.getInt32Ty() : b.getInt64Ty();
            auto signature = llvm::FunctionType::get(result_type, {arg_type, arg_type}, false);
            auto typed = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name + "_typed", module);
            typed->setCallingConv(llvm::CallingConv::X86_FastCall);
            typed->addFnAttr(llvm::Attribute::NoInline);
            typed->addFnAttr("target-features", argv[1]);
            b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", typed));
            llvm::Value *x = typed->getArg(0), *y = typed->getArg(1), *result = x;
            auto intrinsic = [&](llvm::Intrinsic::ID id, bool binary = false) -> llvm::Value*
            {
                auto fn = llvm::Intrinsic::getOrInsertDeclaration(&module, id, {fp});
                if(binary) { return b.CreateCall(fn, {x, y}); }
                return b.CreateCall(fn, {x});
            };
            switch(op)
            {
                case 1: result = intrinsic(llvm::Intrinsic::fabs); break;
                case 2: result = b.CreateFNeg(x); break;
                case 3: result = intrinsic(llvm::Intrinsic::copysign, true); break;
                case 4: result = b.CreateSelect(b.CreateFCmpOLT(y, x), y, x); break;
                case 5: result = b.CreateFAdd(x, y); break;
                case 6: result = b.CreateFSub(x, y); break;
                case 7: result = b.CreateFMul(x, y); break;
                case 8: result = b.CreateFDiv(x, y); break;
                case 9: result = intrinsic(llvm::Intrinsic::sqrt); break;
                case 10: result = intrinsic(llvm::Intrinsic::ceil); break;
                case 11: result = intrinsic(llvm::Intrinsic::floor); break;
                case 12: result = intrinsic(llvm::Intrinsic::trunc); break;
                case 13: result = intrinsic(llvm::Intrinsic::roundeven); break;
                case 14: result = width == 32 ? b.CreateFPTrunc(x, fp) : b.CreateFPExt(x, fp); break;
                case 15: result = b.CreateSIToFP(x, fp); break;
                case 16: result = b.CreateUIToFP(x, fp); break;
                case 17:
                case 19: result = b.CreateFPToSI(x, result_type); break;
                case 18:
                case 20: result = b.CreateFPToUI(x, result_type); break;
                case 21:
                case 22:
                case 23:
                case 24:
                {
                    auto fn = llvm::Intrinsic::getOrInsertDeclaration(&module,
                                                                      (op & 1u) ? llvm::Intrinsic::fptosi_sat : llvm::Intrinsic::fptoui_sat,
                                                                      {result_type, fp});
                    result = b.CreateCall(fn, {x});
                    break;
                }
            }
            b.CreateRet(result);
            auto ptr = b.getPtrTy();
            auto raw_signature = llvm::FunctionType::get(b.getVoidTy(), {ptr, ptr, ptr}, false);
            auto raw = llvm::Function::Create(raw_signature, llvm::Function::ExternalLinkage, name, module);
            raw->addFnAttr("target-features", argv[1]);
            b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", raw));
            auto call = b.CreateCall(typed, {b.CreateLoad(arg_type, raw->getArg(0)), b.CreateLoad(arg_type, raw->getArg(1))});
            call->setCallingConv(llvm::CallingConv::X86_FastCall);
            b.CreateStore(call, raw->getArg(2));
            b.CreateRetVoid();
        }
    }
    // Exercise every scalar/vector and constrained comparison predicate.
    for(unsigned width: {32u, 64u})
    {
        auto scalar = width == 32u ? b.getFloatTy() : b.getDoubleTy();
        auto ptr = b.getPtrTy();
        auto fn = llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(), {ptr, ptr, ptr}, false),
                                         llvm::Function::ExternalLinkage,
                                         "compare_" + std::to_string(width),
                                         module);
        fn->addFnAttr("target-features", argv[1]);
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
        for(unsigned kind = 0; kind != 4; ++kind)
        {
            bool vector = kind & 1u, constrained = kind & 2u;
            llvm::Type* ty = vector ? llvm::FixedVectorType::get(scalar, 2) : scalar;
            auto x = b.CreateLoad(ty, fn->getArg(0)), y = b.CreateLoad(ty, fn->getArg(1));
            x->setAlignment(llvm::Align(1));
            y->setAlignment(llvm::Align(1));
            for(unsigned p = 0; p != 16; ++p)
            {
                auto predicate = static_cast<llvm::CmpInst::Predicate>(p);
                llvm::Value* result;
                if(constrained && p != 0u && p != 15u)
                {
                    auto intrinsic = llvm::Intrinsic::getOrInsertDeclaration(&module, llvm::Intrinsic::experimental_constrained_fcmp, {ty});
                    result = b.CreateCall(intrinsic,
                                          {x,
                                           y,
                                           llvm::MetadataAsValue::get(context, llvm::MDString::get(context, llvm::CmpInst::getPredicateName(predicate))),
                                           llvm::MetadataAsValue::get(context, llvm::MDString::get(context, "fpexcept.ignore"))});
                }
                else
                {
                    result = b.CreateFCmp(predicate, x, y);
                }
                for(unsigned lane = 0; lane != 2; ++lane)
                {
                    auto value = vector ? b.CreateExtractElement(result, b.getInt32(lane)) : result;
                    auto dest = b.CreateGEP(b.getInt32Ty(), fn->getArg(2), b.getInt32(kind * 32 + p * 2 + lane));
                    b.CreateStore(b.CreateZExt(value, b.getInt32Ty()), dest);
                }
            }
        }
        b.CreateRetVoid();
    }
    // Mixed multi-value returns must not reintroduce ST0 through aggregate
    // legalization. Exercise both direct and genuinely indirect fastcalls.
    {
        auto result_type = llvm::StructType::get(context, {b.getFloatTy(), b.getDoubleTy(), b.getInt64Ty(), b.getInt32Ty()});
        auto signature = llvm::FunctionType::get(result_type, {b.getFloatTy(), b.getDoubleTy(), b.getInt64Ty(), b.getInt32Ty()}, false);
        auto typed = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, "mixed_typed", module);
        typed->setCallingConv(llvm::CallingConv::X86_FastCall);
        typed->addFnAttr(llvm::Attribute::NoInline);
        typed->addFnAttr("target-features", argv[1]);
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", typed));
        llvm::Value* aggregate = llvm::PoisonValue::get(result_type);
        for(unsigned i{}; i != 4; ++i) { aggregate = b.CreateInsertValue(aggregate, typed->getArg(i), i); }
        b.CreateRet(aggregate);
        auto target = new llvm::GlobalVariable(module, b.getPtrTy(), false, llvm::GlobalValue::ExternalLinkage, typed, "mixed_target");
        for(unsigned indirect{}; indirect != 2; ++indirect)
        {
            auto raw = llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(), {b.getPtrTy(), b.getPtrTy()}, false),
                                              llvm::Function::ExternalLinkage,
                                              indirect ? "mixed_indirect" : "mixed_direct",
                                              module);
            raw->addFnAttr("target-features", argv[1]);
            b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", raw));
            llvm::SmallVector<llvm::Value*, 4> args;
            unsigned offsets[]{0, 4, 12, 20};
            for(unsigned i{}; i != 4; ++i)
            {
                auto load = b.CreateLoad(result_type->getElementType(i), b.CreateGEP(b.getInt8Ty(), raw->getArg(0), b.getInt32(offsets[i])));
                load->setAlignment(llvm::Align(1));
                args.push_back(load);
            }
            llvm::Value* callee = typed;
            if(indirect) { callee = b.CreateLoad(b.getPtrTy(), target, true); }
            auto call = b.CreateCall(signature, callee, args);
            call->setCallingConv(llvm::CallingConv::X86_FastCall);
            for(unsigned i{}; i != 4; ++i)
            {
                auto store = b.CreateStore(b.CreateExtractValue(call, i), b.CreateGEP(b.getInt8Ty(), raw->getArg(1), b.getInt32(offsets[i])));
                store->setAlignment(llvm::Align(1));
            }
            b.CreateRetVoid();
        }
    }
    if(llvm::verifyModule(module, &llvm::errs())) { return 5; }
    uwvm2::runtime::compiler::shared::strict_float_jit::lower(module, true, true);
    if(llvm::verifyModule(module, &llvm::errs())) { return 3; }
    std::error_code error;
    llvm::raw_fd_ostream out(argv[2], error);
    if(error) { return 4; }
    module.print(out, nullptr);
    return 0;
}
