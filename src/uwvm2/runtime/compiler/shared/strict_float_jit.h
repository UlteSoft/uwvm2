/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#include "strict_float_bits.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/DynamicLibrary.h>

namespace uwvm2::runtime::compiler::shared::strict_float_jit
{
    inline constexpr char symbol_name[]{"uwvm_strict_float_bits_v1"};
    inline void register_symbols(bool enabled = fp::needs_extended_rounding) noexcept
    {
        if(enabled) { ::llvm::sys::DynamicLibrary::AddSymbol(symbol_name, reinterpret_cast<void*>(&bridge)); }
    }

    inline void lower(::llvm::Module& module, bool enabled = fp::needs_extended_rounding) noexcept
    {
        if(!enabled) { return; }
        register_symbols(true);
        auto& context{module.getContext()};
        auto i64{::llvm::Type::getInt64Ty(context)};
        auto i32{::llvm::Type::getInt32Ty(context)};
        auto signature{::llvm::FunctionType::get(i64, {i64, i64, i32}, false)};
        // Cover scalar and vector IR before code generation. Opaque calls carry effects intentionally:
        // subsequent optimization must not move FP work across a callback that changes the controls.
        for(auto& function: module)
        {
            bool native_arithmetic{};
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
                features = item.second;
            }
#endif
            for(auto& block: function)
            {
                for(auto it{block.begin()}; it != block.end();)
                {
                    auto& instruction{*it++};
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
                        default:
                            if(auto call{::llvm::dyn_cast<::llvm::CallInst>(&instruction)};
                               call != nullptr && call->getIntrinsicID() == ::llvm::Intrinsic::sqrt) { opcode = 4u; }
                    }
                    if(opcode == ~0u) { continue; }
                    if(native_arithmetic && (opcode < 5u || opcode == 7u)) { continue; }
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
    }
}
