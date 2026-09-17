/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#include "strict_float_bits.h"
#include <exception>
#include <string>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/TargetParser/Triple.h>

namespace uwvm2::runtime::compiler::shared::strict_float_jit
{
    // This bridge is part of the generated-object ABI. Cache hits must rebind its
    // symbol in the current process, just like new objects; persisting a native
    // function address or reusing an older FP-return object is not compatible.
    inline constexpr char symbol_name[]{"uwvm_strict_float_bits_v1"};
    inline void register_symbols(bool enabled = needs_lowering) noexcept
    {
        if(enabled) { ::llvm::sys::DynamicLibrary::AddSymbol(symbol_name, reinterpret_cast<void*>(&bridge)); }
    }

    // Software-only i386 has no libgcc comparison helpers in a normal hard-float
    // SDK. IEEE comparisons can be expressed in integer IR without changing any
    // operand bits (and without introducing an external floating ABI).
    inline ::llvm::Value* lower_compare(::llvm::IRBuilder<>& b, ::llvm::Value* lhs, ::llvm::Value* rhs,
                                        ::llvm::CmpInst::Predicate predicate)
    {
        auto scalar{lhs->getType()->getScalarType()};
        unsigned const width{scalar->isFloatTy() ? 32u : 64u};
        auto integer{::llvm::IntegerType::get(b.getContext(), width)};
        auto vector{::llvm::dyn_cast<::llvm::FixedVectorType>(lhs->getType())};
        ::llvm::Type* bits_type{vector ? static_cast<::llvm::Type*>(::llvm::FixedVectorType::get(integer, vector->getNumElements())) : integer};
        auto constant{[&](::std::uint64_t value) -> ::llvm::Constant*
        {
            auto c{::llvm::ConstantInt::get(integer, value)};
            return vector ? ::llvm::ConstantVector::getSplat(vector->getElementCount(), c) : c;
        }};
        auto x{b.CreateBitCast(lhs, bits_type)}, y{b.CreateBitCast(rhs, bits_type)};
        auto zero{constant(0u)}, sign{constant(::std::uint64_t{1u} << (width - 1u))};
        auto magnitude{constant(width == 32u ? 0x7fffffffull : 0x7fffffffffffffffull)};
        auto infinity{constant(width == 32u ? 0x7f800000ull : 0x7ff0000000000000ull)};
        auto ax{b.CreateAnd(x, magnitude)}, ay{b.CreateAnd(y, magnitude)};
        auto unordered{b.CreateOr(b.CreateICmpUGT(ax, infinity), b.CreateICmpUGT(ay, infinity))};
        auto both_zero{b.CreateICmpEQ(b.CreateOr(ax, ay), zero)};
        auto equal{b.CreateOr(b.CreateICmpEQ(x, y), both_zero)};
        auto negative_x{b.CreateICmpNE(b.CreateAnd(x, sign), zero)};
        auto negative_y{b.CreateICmpNE(b.CreateAnd(y, sign), zero)};
        auto less{b.CreateAnd(b.CreateNot(both_zero),
            b.CreateSelect(b.CreateXor(negative_x, negative_y), negative_x,
                b.CreateSelect(negative_x, b.CreateICmpUGT(x, y), b.CreateICmpULT(x, y))))};
        auto greater{b.CreateNot(b.CreateOr(less, equal))};
        auto ordered{b.CreateNot(unordered)};
        switch(predicate)
        {
            case ::llvm::CmpInst::FCMP_FALSE: return ::llvm::Constant::getNullValue(equal->getType());
            case ::llvm::CmpInst::FCMP_OEQ: return b.CreateAnd(ordered, equal);
            case ::llvm::CmpInst::FCMP_OGT: return b.CreateAnd(ordered, greater);
            case ::llvm::CmpInst::FCMP_OGE: return b.CreateAnd(ordered, b.CreateNot(less));
            case ::llvm::CmpInst::FCMP_OLT: return b.CreateAnd(ordered, less);
            case ::llvm::CmpInst::FCMP_OLE: return b.CreateAnd(ordered, b.CreateOr(less, equal));
            case ::llvm::CmpInst::FCMP_ONE: return b.CreateAnd(ordered, b.CreateNot(equal));
            case ::llvm::CmpInst::FCMP_ORD: return ordered;
            case ::llvm::CmpInst::FCMP_UNO: return unordered;
            case ::llvm::CmpInst::FCMP_UEQ: return b.CreateOr(unordered, equal);
            case ::llvm::CmpInst::FCMP_UGT: return b.CreateOr(unordered, greater);
            case ::llvm::CmpInst::FCMP_UGE: return b.CreateOr(unordered, b.CreateNot(less));
            case ::llvm::CmpInst::FCMP_ULT: return b.CreateOr(unordered, less);
            case ::llvm::CmpInst::FCMP_ULE: return b.CreateOr(unordered, b.CreateOr(less, equal));
            case ::llvm::CmpInst::FCMP_UNE: return b.CreateOr(unordered, b.CreateNot(equal));
            case ::llvm::CmpInst::FCMP_TRUE: return ::llvm::Constant::getAllOnesValue(equal->getType());
            default: ::std::terminate();
        }
    }

    // Canonicalizing an arithmetic NaN is allowed by Wasm even when preserving
    // its payload would also be allowed. Transport, reinterpret, sign operations,
    // and selection must NOT be fed through this repair pass. Normal and
    // constrained intrinsics and every vector lane need the same arithmetic rule.
    // Keep native arithmetic on noncanonical-NaN hosts, then normalize its result
    // using integer IR. Transport, sign operations and typed calls are excluded.
    inline void canonicalize_native_nan_results(::llvm::Module& module, bool rounding_only = false)
    {
        for(auto& function: module)
        {
            for(auto& block: function)
            {
                for(auto it{block.begin()}; it != block.end();)
                {
                    auto& instruction{*it++};
                    auto scalar{instruction.getType()->getScalarType()};
                    if((!scalar->isFloatTy() && !scalar->isDoubleTy()) || instruction.getMetadata("uwvm.wasm.nan.normalized")) { continue; }
                    if(rounding_only)
                    {
                        auto call{::llvm::dyn_cast<::llvm::CallInst>(&instruction)};
                        if(!call) { continue; }
                        switch(call->getIntrinsicID())
                        {
                            case ::llvm::Intrinsic::ceil: case ::llvm::Intrinsic::experimental_constrained_ceil:
                            case ::llvm::Intrinsic::floor: case ::llvm::Intrinsic::experimental_constrained_floor:
                            case ::llvm::Intrinsic::trunc: case ::llvm::Intrinsic::experimental_constrained_trunc:
                            case ::llvm::Intrinsic::roundeven: case ::llvm::Intrinsic::experimental_constrained_roundeven:
                            case ::llvm::Intrinsic::nearbyint: case ::llvm::Intrinsic::experimental_constrained_nearbyint:
                            case ::llvm::Intrinsic::rint: case ::llvm::Intrinsic::experimental_constrained_rint: break;
                            default: continue;
                        }
                    }
                    bool arithmetic{};
                    switch(instruction.getOpcode())
                    {
                        case ::llvm::Instruction::FAdd: case ::llvm::Instruction::FSub:
                        case ::llvm::Instruction::FMul: case ::llvm::Instruction::FDiv:
                        case ::llvm::Instruction::FPTrunc: case ::llvm::Instruction::FPExt: arithmetic = true; break;
                        default:
                            if(auto call{::llvm::dyn_cast<::llvm::CallInst>(&instruction)})
                            {
                                switch(call->getIntrinsicID())
                                {
                                    case ::llvm::Intrinsic::experimental_constrained_fadd:
                                    case ::llvm::Intrinsic::experimental_constrained_fsub:
                                    case ::llvm::Intrinsic::experimental_constrained_fmul:
                                    case ::llvm::Intrinsic::experimental_constrained_fdiv:
                                    case ::llvm::Intrinsic::experimental_constrained_fptrunc:
                                    case ::llvm::Intrinsic::experimental_constrained_fpext:
                                    case ::llvm::Intrinsic::sqrt: case ::llvm::Intrinsic::experimental_constrained_sqrt:
                                    case ::llvm::Intrinsic::ceil: case ::llvm::Intrinsic::experimental_constrained_ceil:
                                    case ::llvm::Intrinsic::floor: case ::llvm::Intrinsic::experimental_constrained_floor:
                                    case ::llvm::Intrinsic::trunc: case ::llvm::Intrinsic::experimental_constrained_trunc:
                                    case ::llvm::Intrinsic::roundeven: case ::llvm::Intrinsic::experimental_constrained_roundeven:
                                    case ::llvm::Intrinsic::nearbyint: case ::llvm::Intrinsic::experimental_constrained_nearbyint:
                                    case ::llvm::Intrinsic::rint: case ::llvm::Intrinsic::experimental_constrained_rint:
                                    case ::llvm::Intrinsic::minimum: case ::llvm::Intrinsic::maximum:
                                    case ::llvm::Intrinsic::minnum: case ::llvm::Intrinsic::maxnum: arithmetic = true; break;
                                    default: break;
                                }
                            }
                    }
                    if(!arithmetic) { continue; }
                    ::llvm::IRBuilder<> b{instruction.getNextNode()};
                    bool const wide{scalar->isDoubleTy()};
                    auto integer{b.getIntNTy(wide ? 64u : 32u)};
                    auto vector{::llvm::dyn_cast<::llvm::FixedVectorType>(instruction.getType())};
                    ::llvm::Type* bits_type{vector ? static_cast<::llvm::Type*>(::llvm::FixedVectorType::get(integer, vector->getNumElements())) : integer};
                    auto constant{[&](::std::uint64_t value) -> ::llvm::Constant*
                    {
                        auto c{::llvm::ConstantInt::get(integer, value)};
                        return vector ? ::llvm::ConstantVector::getSplat(vector->getElementCount(), c) : c;
                    }};
                    auto raw{b.CreateBitCast(&instruction, bits_type)};
                    auto classified{raw};
                    bool classified_wide{wide};
                    bool conversion{instruction.getOpcode() == ::llvm::Instruction::FPTrunc || instruction.getOpcode() == ::llvm::Instruction::FPExt};
                    if(auto call{::llvm::dyn_cast<::llvm::CallInst>(&instruction)})
                    { conversion = call->getIntrinsicID() == ::llvm::Intrinsic::experimental_constrained_fptrunc ||
                                   call->getIntrinsicID() == ::llvm::Intrinsic::experimental_constrained_fpext; }
                    // Classify the SOURCE for width conversions: legacy demotion can
                    // discard a tiny NaN payload and yield infinity; checking only
                    // the native result would then miss the NaN. Promotion also
                    // needs arithmetic quieting even when hardware only moves bits.
                    if(conversion)
                    {
                        auto source{instruction.getOperand(0u)};
                        classified_wide = source->getType()->getScalarType()->isDoubleTy();
                        auto source_integer{b.getIntNTy(classified_wide ? 64u : 32u)};
                        ::llvm::Type* source_bits{vector ? static_cast<::llvm::Type*>(::llvm::FixedVectorType::get(source_integer, vector->getNumElements())) : source_integer};
                        classified = b.CreateBitCast(source, source_bits);
                    }
                    auto classification_constant{[&](::std::uint64_t value)
                    { return ::llvm::ConstantInt::get(classified->getType(), value); }};
                    auto magnitude{b.CreateAnd(classified, classification_constant(classified_wide ? 0x7fffffffffffffffull : 0x7fffffffull))};
                    auto nan{b.CreateICmpUGT(magnitude, classification_constant(classified_wide ? 0x7ff0000000000000ull : 0x7f800000ull))};
                    auto bits{b.CreateSelect(nan, constant(wide ? 0x7ff8000000000000ull : 0x7fc00000ull), raw)};
                    auto result{b.CreateBitCast(bits, instruction.getType())};
                    // Keep the raw bitcast's use of the original instruction or the
                    // replacement would form a cycle. Metadata makes repeated lowering
                    // idempotent; wrappers/materialization can revisit the module.
                    instruction.replaceUsesWithIf(result, [&](::llvm::Use& use) { return use.getUser() != raw; });
                    instruction.setMetadata("uwvm.wasm.nan.normalized", ::llvm::MDNode::get(module.getContext(), {}));
                }
            }
        }
    }

    // RV32 has no FCVT.L.D; LLVM may lower f64 roundeven to the C23 libm symbol,
    // which musl does not provide. Do not substitute nearbyint/rint: their result
    // depends on FRM, unlike Wasm nearest. Use explicitly encoded RNE on RV32D
    // and integer IR otherwise, including when the target features are unknown.
    // Both cover every fixed-vector lane. With F enabled, ordinary f32 stays native. The exception-ignoring constrained f32 form
    // also needs rewriting to the equivalent ordinary intrinsic: LLVM 22
    // otherwise emits roundevenf even though FCVT.W.S supports this operation.
    // Other targets retain their existing instruction-selection paths.
    inline void lower_rv32_roundeven(::llvm::Module& module) noexcept
    {
        if(::llvm::Triple{module.getTargetTriple()}.getArch() != ::llvm::Triple::riscv32) { return; }
        for(auto& function: module)
        {
            for(auto& block: function)
            {
                for(auto it{block.begin()}; it != block.end();)
                {
                    auto call{::llvm::dyn_cast<::llvm::CallInst>(&*it++)};
                    if(!call || (!call->getType()->getScalarType()->isDoubleTy() && !call->getType()->getScalarType()->isFloatTy())) { continue; }
                    auto id{call->getIntrinsicID()};
                    if(id != ::llvm::Intrinsic::roundeven && id != ::llvm::Intrinsic::experimental_constrained_roundeven) { continue; }
                    if(id == ::llvm::Intrinsic::experimental_constrained_roundeven)
                    {
                        // Wasm ignores accrued exceptions. Do not silently erase
                        // a strict/maytrap exception contract in unrelated IR.
                        auto metadata{::llvm::dyn_cast<::llvm::MetadataAsValue>(call->getArgOperand(1u))};
                        auto text{metadata ? ::llvm::dyn_cast<::llvm::MDString>(metadata->getMetadata()) : nullptr};
                        if(!text || text->getString() != "fpexcept.ignore") { continue; }
                    }
                    auto vector{::llvm::dyn_cast<::llvm::FixedVectorType>(call->getType())};
                    if(call->getType()->isVectorTy() && !vector) { continue; }
                    ::llvm::IRBuilder<> b{call};
                    bool has_double_fpu{}, has_single_fpu{};
                    auto features{function.getFnAttribute("target-features")};
                    if(features.isValid())
                    {
                        ::llvm::SmallVector<::llvm::StringRef, 32> names{};
                        features.getValueAsString().split(names, ',');
                        for(auto name: names)
                        {
                            if(name == "+f") { has_single_fpu = true; }
                            else if(name == "-f") { has_single_fpu = false; }
                            else if(name == "+d") { has_double_fpu = true; }
                            else if(name == "-d") { has_double_fpu = false; }
                        }
                    }
                    if(call->getType()->getScalarType()->isFloatTy() && has_single_fpu)
                    {
                        if(id == ::llvm::Intrinsic::experimental_constrained_roundeven)
                        {
                            auto intrinsic{::llvm::Intrinsic::getOrInsertDeclaration(&module, ::llvm::Intrinsic::roundeven, {call->getType()})};
                            auto result{b.CreateCall(intrinsic, {call->getArgOperand(0u)})};
                            call->replaceAllUsesWith(result);
                            call->eraseFromParent();
                        }
                        continue;
                    }
                    bool const wide{call->getType()->getScalarType()->isDoubleTy()};
                    auto integer{b.getIntNTy(wide ? 64u : 32u)};
                    ::llvm::Type* bits_type{vector ? static_cast<::llvm::Type*>(::llvm::FixedVectorType::get(integer, vector->getNumElements())) : integer};
                    auto constant{[&](::std::uint64_t value) -> ::llvm::Constant*
                    {
                        auto c{::llvm::ConstantInt::get(integer, value)};
                        return vector ? ::llvm::ConstantVector::getSplat(vector->getElementCount(), c) : c;
                    }};
                    auto raw{b.CreateBitCast(call->getArgOperand(0u), bits_type)};
                    auto magnitude{b.CreateAnd(raw, constant(wide ? 0x7fffffffffffffffull : 0x7fffffffull))};
                    if(wide && has_double_fpu)
                    {
                        // For |x| < 2^52, RN-even (|x| + 2^52) - 2^52 is the
                        // exact integral result. Both instructions MUST encode
                        // RNE: LLVM 22 emitted dynamic rounding even for explicit
                        // tonearest constrained fadd/fsub in this regression.
                        // Inline asm keeps the fixed rounding in the instruction,
                        // avoids FRM save/restore and avoids emulated i64 shifts.
                        auto scalar_type{b.getDoubleTy()};
                        auto signature{::llvm::FunctionType::get(scalar_type, {scalar_type, scalar_type}, false)};
                        // Early-clobber prevents output from sharing the bias
                        // register, which is still read by the second instruction.
                        auto round{::llvm::InlineAsm::get(signature,
                            "fadd.d $0, $1, $2, rne\n\tfsub.d $0, $0, $2, rne", "=&f,f,f", false)};
                        auto bias{::llvm::ConstantFP::get(scalar_type, 4503599627370496.0)};
                        auto absolute{b.CreateBitCast(magnitude, call->getType())};
                        ::llvm::Value* rounded{};
                        if(vector)
                        {
                            rounded = ::llvm::PoisonValue::get(call->getType());
                            for(unsigned lane{}; lane != vector->getNumElements(); ++lane)
                            {
                                auto item{b.CreateCall(round, {b.CreateExtractElement(absolute, lane), bias})};
                                rounded = b.CreateInsertElement(rounded, item, lane);
                            }
                        }
                        else { rounded = b.CreateCall(round, {absolute, bias}); }
                        // Larger finite values are integral already; retain inf
                        // and quiet NaN bits explicitly. Exceptions are ignored
                        // by this Wasm lowering contract, including unused lanes.
                        auto result{b.CreateSelect(b.CreateICmpULT(magnitude, constant(0x4330000000000000ull)),
                                                  b.CreateBitCast(rounded, bits_type), magnitude)};
                        result = b.CreateSelect(b.CreateICmpUGT(magnitude, constant(0x7ff0000000000000ull)),
                                                b.CreateOr(magnitude, constant(0x0008000000000000ull)), result);
                        result = b.CreateOr(result, b.CreateAnd(raw, constant(0x8000000000000000ull)));
                        call->replaceAllUsesWith(b.CreateBitCast(result, call->getType()));
                        call->eraseFromParent();
                        continue;
                    }
                    // Unknown/software-only target features cannot authorize D
                    // instructions. Keep the fully integer f32/f64 fallback in that case.
                    unsigned const fraction{wide ? 52u : 23u}, bias{wide ? 1023u : 127u};
                    auto exponent{b.CreateLShr(magnitude, constant(fraction))};
                    auto in_range{b.CreateAnd(b.CreateICmpUGE(exponent, constant(bias)), b.CreateICmpULT(exponent, constant(bias + fraction)))};
                    // Clamp BEFORE shifting: selecting a different final result
                    // does not license an oversized/underflowed LLVM shift.
                    auto shift{b.CreateSelect(in_range, b.CreateSub(constant(bias + fraction), exponent), constant(1u))};
                    auto unit{b.CreateShl(constant(1u), shift)};
                    auto mask{b.CreateSub(unit, constant(1u))};
                    auto whole{b.CreateAnd(magnitude, b.CreateNot(mask))};
                    auto remainder{b.CreateAnd(magnitude, mask)};
                    auto half{b.CreateLShr(unit, constant(1u))};
                    auto odd{b.CreateICmpNE(b.CreateAnd(whole, unit), constant(0u))};
                    auto increment{b.CreateOr(b.CreateICmpUGT(remainder, half), b.CreateAnd(b.CreateICmpEQ(remainder, half), odd))};
                    auto rounded{b.CreateAdd(whole, b.CreateSelect(increment, unit, constant(0u)))};
                    // |x| <= 0.5 rounds to signed zero (tie to even); larger
                    // values below 1 round to signed one. Large finite/inf stay.
                    auto small{b.CreateSelect(b.CreateICmpUGT(magnitude, constant(wide ? 0x3fe0000000000000ull : 0x3f000000ull)),
                                              constant(wide ? 0x3ff0000000000000ull : 0x3f800000ull), constant(0u))};
                    auto result{b.CreateSelect(b.CreateICmpULT(exponent, constant(bias)), small, b.CreateSelect(in_range, rounded, magnitude))};
                    result = b.CreateSelect(b.CreateICmpUGT(magnitude, constant(wide ? 0x7ff0000000000000ull : 0x7f800000ull)),
                                            b.CreateOr(magnitude, constant(wide ? 0x0008000000000000ull : 0x00400000ull)), result);
                    result = b.CreateOr(result, b.CreateAnd(raw, constant(wide ? 0x8000000000000000ull : 0x80000000ull)));
                    call->replaceAllUsesWith(b.CreateBitCast(result, call->getType()));
                    call->eraseFromParent(); // No intrinsic remains; repeated lowering is idempotent.
                }
            }
        }
    }

    inline void lower(::llvm::Module& module, bool enabled = needs_lowering, bool bit_preserving_abi = needs_bit_preserving_abi,
                      bool normalize_nan = fp::needs_nan_canonicalization) noexcept
    {
        lower_rv32_roundeven(module);
        // RISC-V's conversion-based rounding keeps large/NaN operands unchanged.
        // Normalize only rounding results; arithmetic, transport and sign operations
        // retain their native code. Check the generated target, including cross-JITs.
        if(::llvm::Triple{module.getTargetTriple()}.isRISCV()) { canonicalize_native_nan_results(module, true); }
        // The RISC-V rounding repair intentionally precedes this early return:
        // disabling extended-precision/ABI bridge lowering does not prove that
        // the generated target's native rounding quiets sNaNs. Use the module triple,
        // not the compiler host macros, for cross-generated RISC-V code.
        if(!enabled) { return; }
        if(normalize_nan && !fp::needs_extended_rounding && !bit_preserving_abi)
        {
            canonicalize_native_nan_results(module);
            return;
        }
        register_symbols(true);
        auto& context{module.getContext()};
        auto i64{::llvm::Type::getInt64Ty(context)};
        auto i32{::llvm::Type::getInt32Ty(context)};
        auto signature{::llvm::FunctionType::get(i64, {i64, i64, i32}, false)};
        // Cover scalar and vector IR before code generation. Opaque calls carry effects intentionally:
        // subsequent optimization must not move FP work across a callback that changes the controls.
        for(auto& function: module)
        {
            bool native_arithmetic{}, native_rounding{};
            bool no_sse_rounding_abi{};
            // x86_64 libc returns f32/f64 in XMM0 even if generated arithmetic
            // has SSE disabled. In that configuration ceil/floor/trunc/nearest
            // must cross the existing INTEGER bridge, not a floating libcall.
            // This is distinct from i386's private no-x87 result ABI. Determine
            // it from the destination IR, never from the cross-compiler host.
            if(::llvm::Triple{module.getTargetTriple()}.isX86())
            {
                auto attribute{function.getFnAttribute("target-features")};
                auto names{attribute.isStringAttribute() ? attribute.getValueAsString() : ::llvm::StringRef{}};
                while(!names.empty())
                {
                    auto item{names.split(',')};
                    if(item.first == "-sse" || item.first == "-sse2") { no_sse_rounding_abi = true; }
                    else if(item.first == "+sse2") { no_sse_rounding_abi = false; }
                    names = item.second;
                }
            }
#if (defined(__i386__) || defined(__x86_64__)) && !defined(__arm64ec__) && !defined(_M_ARM64EC)
            // A portable x87 interpreter may run on an SSE2-capable host. The JIT's actual function
            // features, not the interpreter's compile flags, then permit native arithmetic/demotion.
            // Keep integer conversions guarded: 32-bit codegen can still use x87 for i64 operands.
            auto const feature_attribute{function.getFnAttribute("target-features")};
            auto features{feature_attribute.isStringAttribute() ? feature_attribute.getValueAsString() : ::llvm::StringRef{}};
            while(!features.empty())
            {
                auto const item{features.split(',')};
                if(item.first == "+sse2") { native_arithmetic = true; }
                else if(item.first == "-sse2") { native_arithmetic = false; }
                else if(item.first == "+sse4.1") { native_rounding = true; }
                else if(item.first == "-sse4.1") { native_rounding = false; }
                features = item.second;
            }
#endif
            // No-SSE raw-buffer entries also need integer-only transport:
            // x87 FLD/FST quiet signaling NaNs even for splat/select/load/store.
            // Removing floating libcalls alone compiles successfully but is
            // semantically wrong. This does NOT invent an x86_64 scalar-FP C
            // return ABI: such native crossings still require the host ABI.
            bool const integer_fp{bit_preserving_abi || no_sse_rounding_abi};
            if(integer_fp)
            {
                // This is a PRIVATE generated-function ABI, not a change to native
                // C/C++ float returns. SSE2 only changes arithmetic selection; the
                // ordinary i386 FP result ABI still uses ST0. All generated callers,
                // callees and wrappers must opt in together, including indirect and
                // multi-result calls. Native crossings remain byte/integer bridges.
                // LLVM's no-x87 i386 ABI returns f32/f64 bits in integer registers. It also
                // prevents load/store/select/PHI legalization from quieting sNaNs in ST0.
                // Apply consistently to typed definitions, declarations, raw wrappers and
                // indirect callers. Native runtime bridges already use an integer/byte ABI.
                auto attribute{function.getFnAttribute("target-features")};
                auto value{attribute.isStringAttribute() ? attribute.getValueAsString().str() : ::std::string{}};
                if(!value.empty()) { value += ','; }
                value += "-x87";
                function.addFnAttr("target-features", value);
            }
            for(auto& block: function)
            {
                for(auto it{block.begin()}; it != block.end();)
                {
                    auto& instruction{*it++};
                    if(integer_fp)
                    {
                        ::llvm::CmpInst::Predicate predicate{::llvm::CmpInst::BAD_FCMP_PREDICATE};
                        unsigned conversion{};
                        bool saturating{};
                        if(auto compare{::llvm::dyn_cast<::llvm::FCmpInst>(&instruction)}) { predicate = compare->getPredicate(); }
                        else if(instruction.getOpcode() == ::llvm::Instruction::FPToSI) { conversion = 13u; }
                        else if(instruction.getOpcode() == ::llvm::Instruction::FPToUI) { conversion = 14u; }
                        else if(auto call{::llvm::dyn_cast<::llvm::CallInst>(&instruction)})
                        {
                            auto id{call->getIntrinsicID()};
                            if(id == ::llvm::Intrinsic::experimental_constrained_fcmp || id == ::llvm::Intrinsic::experimental_constrained_fcmps)
                            {
                                auto metadata{::llvm::cast<::llvm::MetadataAsValue>(call->getArgOperand(2u))};
                                auto name{::llvm::cast<::llvm::MDString>(metadata->getMetadata())->getString()};
                                for(unsigned p{::llvm::CmpInst::FCMP_FALSE}; p <= ::llvm::CmpInst::FCMP_TRUE; ++p)
                                {
                                    auto candidate{static_cast<::llvm::CmpInst::Predicate>(p)};
                                    if(name == ::llvm::CmpInst::getPredicateName(candidate)) { predicate = candidate; break; }
                                }
                            }
                            else if(id == ::llvm::Intrinsic::experimental_constrained_fptosi || id == ::llvm::Intrinsic::fptosi_sat)
                            { conversion = 13u; saturating = id == ::llvm::Intrinsic::fptosi_sat; }
                            else if(id == ::llvm::Intrinsic::experimental_constrained_fptoui || id == ::llvm::Intrinsic::fptoui_sat)
                            { conversion = 14u; saturating = id == ::llvm::Intrinsic::fptoui_sat; }
                        }
                        if(predicate != ::llvm::CmpInst::BAD_FCMP_PREDICATE && !native_arithmetic)
                        {
                            ::llvm::IRBuilder<> builder{&instruction};
                            auto result{lower_compare(builder, instruction.getOperand(0u), instruction.getOperand(1u), predicate)};
                            instruction.replaceAllUsesWith(result);
                            instruction.eraseFromParent();
                            continue;
                        }
                        // SSE2 can convert to i32 directly, but i64 legalization
                        // still requires x87 even on otherwise SSE-only targets.
                        if(conversion != 0u && (!native_arithmetic || instruction.getType()->getScalarType()->isIntegerTy(64u)))
                        {
                            ::llvm::IRBuilder<> builder{&instruction};
                            auto vector{::llvm::dyn_cast<::llvm::FixedVectorType>(instruction.getType())};
                            auto source{instruction.getOperand(0u)};
                            bool const wide{source->getType()->getScalarType()->isDoubleTy()};
                            auto result_type{instruction.getType()->getScalarType()};
                            auto code{conversion | (wide ? 16u : 0u) | (saturating ? 32u : 0u) | (result_type->isIntegerTy(64u) ? 64u : 0u)};
                            auto callee{module.getOrInsertFunction(symbol_name, signature)};
                            ::llvm::Value* result{vector ? ::llvm::PoisonValue::get(vector) : nullptr};
                            for(unsigned lane{}; lane != (vector ? vector->getNumElements() : 1u); ++lane)
                            {
                                auto value{vector ? builder.CreateExtractElement(source, builder.getInt32(lane)) : source};
                                auto bits{builder.CreateZExtOrTrunc(builder.CreateBitCast(value, wide ? i64 : i32), i64)};
                                auto converted{builder.CreateCall(callee, {bits, builder.getInt64(0u), builder.getInt32(code)})};
                                auto scalar_result{builder.CreateTruncOrBitCast(converted, result_type)};
                                result = vector ? builder.CreateInsertElement(result, scalar_result, builder.getInt32(lane)) : scalar_result;
                            }
                            instruction.replaceAllUsesWith(result);
                            instruction.eraseFromParent();
                            continue;
                        }
                    }
                    unsigned opcode{~0u};
                    switch(instruction.getOpcode())
                    {
                        case ::llvm::Instruction::FAdd: opcode = 0u; break;
                        case ::llvm::Instruction::FSub: opcode = 1u; break;
                        case ::llvm::Instruction::FMul: opcode = 2u; break;
                        case ::llvm::Instruction::FDiv: opcode = 3u; break;
                        case ::llvm::Instruction::SIToFP: opcode = 5u; break;
                        case ::llvm::Instruction::UIToFP: opcode = 6u; break;
                        case ::llvm::Instruction::FPTrunc: opcode = 7u; break;
                        case ::llvm::Instruction::FPExt: if(integer_fp) { opcode = 8u; } break;
                        default:
                            if(auto call{::llvm::dyn_cast<::llvm::CallInst>(&instruction)}; call != nullptr)
                            {
                                switch(call->getIntrinsicID())
                                {
                                    case ::llvm::Intrinsic::experimental_constrained_fadd: opcode = 0u; break;
                                    case ::llvm::Intrinsic::experimental_constrained_fsub: opcode = 1u; break;
                                    case ::llvm::Intrinsic::experimental_constrained_fmul: opcode = 2u; break;
                                    case ::llvm::Intrinsic::experimental_constrained_fdiv: opcode = 3u; break;
                                    case ::llvm::Intrinsic::sqrt:
                                    case ::llvm::Intrinsic::experimental_constrained_sqrt: opcode = 4u; break;
                                    case ::llvm::Intrinsic::experimental_constrained_sitofp: opcode = 5u; break;
                                    case ::llvm::Intrinsic::experimental_constrained_uitofp: opcode = 6u; break;
                                    case ::llvm::Intrinsic::experimental_constrained_fptrunc: opcode = 7u; break;
                                    case ::llvm::Intrinsic::experimental_constrained_fpext: if(integer_fp) { opcode = 8u; } break;
                                    case ::llvm::Intrinsic::ceil:
                                    case ::llvm::Intrinsic::experimental_constrained_ceil: if(bit_preserving_abi || no_sse_rounding_abi) { opcode = 9u; } break;
                                    case ::llvm::Intrinsic::floor:
                                    case ::llvm::Intrinsic::experimental_constrained_floor: if(bit_preserving_abi || no_sse_rounding_abi) { opcode = 10u; } break;
                                    case ::llvm::Intrinsic::trunc:
                                    case ::llvm::Intrinsic::experimental_constrained_trunc: if(bit_preserving_abi || no_sse_rounding_abi) { opcode = 11u; } break;
                                    case ::llvm::Intrinsic::roundeven:
                                    case ::llvm::Intrinsic::nearbyint:
                                    case ::llvm::Intrinsic::rint:
                                    case ::llvm::Intrinsic::experimental_constrained_roundeven:
                                    case ::llvm::Intrinsic::experimental_constrained_nearbyint:
                                    case ::llvm::Intrinsic::experimental_constrained_rint: if(bit_preserving_abi || no_sse_rounding_abi) { opcode = 12u; } break;
                                    default: break;
                                }
                            }
                    }
                    if(opcode == ~0u) { continue; }
                    if(native_arithmetic && (opcode < 5u || opcode == 7u || opcode == 8u)) { continue; }
                    if(native_rounding && opcode >= 9u) { continue; }
                    auto vector_type{::llvm::dyn_cast<::llvm::FixedVectorType>(instruction.getType())};
                    auto scalar_type{instruction.getType()->getScalarType()};
                    if(!scalar_type->isFloatTy() && !scalar_type->isDoubleTy()) { continue; }
                    ::llvm::IRBuilder<> builder{&instruction};
                    auto callee{module.getOrInsertFunction(symbol_name, signature)};
                    auto const count{vector_type == nullptr ? 1u : vector_type->getNumElements()};
                    ::llvm::Value* result{vector_type == nullptr ? nullptr : ::llvm::PoisonValue::get(vector_type)};
                    for(unsigned lane{}; lane != count; ++lane)
                    {
                        auto operand{[&](unsigned index) -> ::llvm::Value*
                        {
                            auto value{instruction.getOperand(index)};
                            if(vector_type != nullptr) { value = builder.CreateExtractElement(value, builder.getInt32(lane)); }
                            if(value->getType()->isFloatingPointTy())
                            { value = builder.CreateBitCast(value, value->getType()->isFloatTy() ? i32 : i64); }
                            return opcode == 5u ? builder.CreateSExtOrTrunc(value, i64) : builder.CreateZExtOrTrunc(value, i64);
                        }};
                        auto left{operand(0u)};
                        auto right{opcode < 4u ? operand(1u) : builder.getInt64(0u)};
                        auto code{builder.getInt32(opcode | (scalar_type->isDoubleTy() ? 16u : 0u))};
                        ::llvm::Value* bits{builder.CreateCall(callee, {left, right, code})};
                        if(scalar_type->isFloatTy()) { bits = builder.CreateTrunc(bits, i32); }
                        auto value{builder.CreateBitCast(bits, scalar_type)};
                        result = vector_type == nullptr ? value : builder.CreateInsertElement(result, value, builder.getInt32(lane));
                    }
                    instruction.replaceAllUsesWith(result);
                    instruction.eraseFromParent();
                }
            }
        }
        // The integer bridge already returns Wasm NaNs. Normalize only native
        // instructions left over after lowering, avoiding redundant vector work
        // on scalar-only targets such as m68k.
        if(normalize_nan) { canonicalize_native_nan_results(module); }
    }
}
