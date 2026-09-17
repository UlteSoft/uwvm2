// Exercise destination-specific emitters even on a host with a working vector
// ISA. Native-only differential tests cannot cover these backend/ABI regressions.
// Object generation and QEMU execution remain necessary in addition to IR tests.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/shared/strict_float_jit.h>

namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
using code = d::llvm_jit_simd_code;

int main()
{
    // AArch64 scalar legalization of bitcast <16 x i1> to i16 lost lanes 8-15.
    // The no-NEON emitter must extract every input lane before reducing it.
    {
        llvm::LLVMContext context;
        llvm::Module module{"aarch64-no-neon-bitmask", context};
#if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(llvm::Triple{"aarch64-linux-gnu"});
#else
        module.setTargetTriple("aarch64-linux-gnu");
#endif
        llvm::IRBuilder<> b{context};
        auto vector{llvm::FixedVectorType::get(b.getInt8Ty(), 16u)};
        auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(), {vector}, false),
            llvm::Function::ExternalLinkage, "mask", module)};
        fn->addFnAttr("target-features", "-neon,-sve");
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
        b.CreateRet(d::simd_ir::emit_value(b, code::i8x16_bitmask, fn->getArg(0)));
        unsigned seen{};
        for(auto& block : *fn) for(auto& instruction : block)
        {
            if(auto extract{llvm::dyn_cast<llvm::ExtractElementInst>(&instruction)})
            {
                auto lane{llvm::dyn_cast<llvm::ConstantInt>(extract->getIndexOperand())};
                if(lane && lane->getZExtValue() < 16u) { seen |= 1u << lane->getZExtValue(); }
            }
        }
        if(seen != 0xffffu || llvm::verifyModule(module, &llvm::errs())) { return 1; }
    }
    // LLVM minimum/maximum can become incompatible ST0-return libcalls on
    // i386's private no-x87 ABI, XMM0-return libcalls on no-SSE x86_64,
    // or an illegal 64-bit GPR load on PPC32/G4.
    for(auto triple : {"i686-linux-gnu", "x86_64-linux-gnu", "powerpc-linux-gnu"})
    {
        bool const ppc{llvm::StringRef{triple}.starts_with("powerpc")};
        bool const x64{llvm::StringRef{triple}.starts_with("x86_64")};
        llvm::LLVMContext context;
        llvm::Module module{"scalar-minmax", context};
#if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(llvm::Triple{triple});
#else
        module.setTargetTriple(triple);
#endif
        module.setDataLayout(ppc ? "E-p:32:32" : x64 ? "e-p:64:64" : "e-p:32:32");
        llvm::IRBuilder<> b{context};
        auto vector{llvm::FixedVectorType::get(b.getInt8Ty(), 16u)};
        for(auto operation : {code::f32x4_min, code::f32x4_max, code::f64x2_min, code::f64x2_max})
        {
            if(ppc && (operation == code::f32x4_min || operation == code::f32x4_max)) { continue; }
            auto fn{llvm::Function::Create(llvm::FunctionType::get(vector, {vector, vector}, false),
                llvm::Function::ExternalLinkage, "minmax", module)};
            fn->addFnAttr("target-features", ppc ? "+altivec,-vsx" : "-sse,-sse2");
            b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
            b.CreateRet(d::simd_ir::emit_value(b, operation, fn->getArg(0), fn->getArg(1)));
        }
        if(!ppc) { uwvm2::runtime::compiler::shared::strict_float_jit::lower(module, true, !x64, false); }
        for(auto& fn : module) for(auto& block : fn) for(auto& instruction : block)
        {
            if(auto call{llvm::dyn_cast<llvm::CallBase>(&instruction)})
            {
                if(call->getIntrinsicID() == llvm::Intrinsic::minimum || call->getIntrinsicID() == llvm::Intrinsic::maximum) { return 2; }
            }
            if(!ppc && llvm::isa<llvm::FCmpInst>(instruction)) { return 3; }
        }
        if(llvm::verifyModule(module, &llvm::errs())) { return 4; }
    }
    // Rounding must not leave a floating-return libcall on x86_64 with SSE
    // disabled. Check the integer bridge and transport feature independently
    // of the cross-codegen/QEMU oracle. This does not test a no-SSE C++ ABI.
    {
        llvm::LLVMContext context;
        llvm::Module module{"x64-no-sse-rounding", context};
#if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(llvm::Triple{"x86_64-linux-gnu"});
#else
        module.setTargetTriple("x86_64-linux-gnu");
#endif
        llvm::IRBuilder<> b{context};
        for(bool wide : {false, true})
        for(auto operation : {llvm::Intrinsic::ceil, llvm::Intrinsic::floor, llvm::Intrinsic::trunc, llvm::Intrinsic::roundeven})
        {
            auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getInt64Ty(), {b.getInt64Ty()}, false),
                llvm::Function::ExternalLinkage, "round-bits", module)};
            fn->addFnAttr("target-features", "-sse,-sse2,+x87");
            b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
            auto bits{wide ? static_cast<llvm::Value*>(fn->getArg(0)) : b.CreateTrunc(fn->getArg(0), b.getInt32Ty())};
            auto value{b.CreateBitCast(bits, wide ? b.getDoubleTy() : b.getFloatTy())};
            auto rounded{b.CreateBitCast(b.CreateUnaryIntrinsic(operation, value), bits->getType())};
            b.CreateRet(wide ? rounded : b.CreateZExt(rounded, b.getInt64Ty()));
        }
        uwvm2::runtime::compiler::shared::strict_float_jit::lower(module, true, false, false);
        unsigned bridges{};
        for(auto& fn : module)
        {
            if(fn.isDeclaration()) { continue; }
            if(!fn.getFnAttribute("target-features").getValueAsString().ends_with(",-x87")) { return 8; }
            for(auto& block : fn) for(auto& instruction : block)
            {
                if(auto call{llvm::dyn_cast<llvm::CallBase>(&instruction)})
                {
                    if(call->getIntrinsicID() != llvm::Intrinsic::not_intrinsic || !call->getType()->isIntegerTy(64) ||
                       call->arg_size() != 3u || !call->getArgOperand(0)->getType()->isIntegerTy(64) ||
                       !call->getArgOperand(1)->getType()->isIntegerTy(64) || !call->getArgOperand(2)->getType()->isIntegerTy(32)) { return 9; }
                    ++bridges;
                }
            }
        }
        if(bridges != 8u || llvm::verifyModule(module, &llvm::errs())) { return 10; }
    }
    // SPARC hard-float libgcc has no __truncdfsf2. Keep native fptrunc plus
    // explicit input-bit NaN handling, without relaxing the surrounding scope.
    for(auto triple : {"sparc-linux-gnu", "sparc64-linux-gnu"})
    {
        llvm::LLVMContext context;
        llvm::Module module{"sparc-demote", context};
#if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(llvm::Triple{triple});
#else
        module.setTargetTriple(triple);
#endif
        llvm::IRBuilder<> b{context};
        auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getFloatTy(), {b.getDoubleTy()}, false),
            llvm::Function::ExternalLinkage, "demote", module)};
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
        b.setIsFPConstrained(true);
        b.CreateRet(d::emit_llvm_float_demote(b, fn->getArg(0)));
        if(!b.getIsFPConstrained()) { return 5; }
        unsigned truncations{}, selects{};
        for(auto& block : *fn) for(auto& instruction : block)
        {
            if(llvm::isa<llvm::FPTruncInst>(instruction)) { ++truncations; }
            if(llvm::isa<llvm::SelectInst>(instruction)) { ++selects; }
            if(llvm::isa<llvm::CallBase>(instruction)) { return 6; }
        }
        if(truncations != 1u || selects != 1u || llvm::verifyModule(module, &llvm::errs())) { return 7; }
    }
}
