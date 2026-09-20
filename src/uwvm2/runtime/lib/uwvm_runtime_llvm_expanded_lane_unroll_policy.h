/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Scalar/LoopPassManager.h>

namespace uwvm2::runtime::lib::details
{
    enum class runtime_llvm_jit_expanded_lane_fp_kind : ::std::uint_least8_t
    {
        ordinary,
        constrained
    };

    struct runtime_llvm_jit_expanded_lane_fp_operation
    {
        ::llvm::Instruction* instruction{};
        ::llvm::Value* lhs{};
        ::llvm::Value* rhs{};
        runtime_llvm_jit_expanded_lane_fp_kind kind{};
    };

    struct runtime_llvm_jit_expanded_lane_pattern
    {
        ::llvm::Type* element_type{};
        ::llvm::Value* invariant_multiplier{};
        ::llvm::LoadInst* accumulator_load{};
        ::llvm::LoadInst* factor_load{};
        ::llvm::StoreInst* accumulator_store{};
        ::llvm::SCEV const* accumulator_address{};
        ::llvm::SCEV const* factor_address{};
        ::llvm::BasicBlock* block{};
        runtime_llvm_jit_expanded_lane_fp_kind kind{};
    };

    struct runtime_llvm_jit_expanded_lane_storage_load
    {
        ::llvm::LoadInst* instruction{};
        ::llvm::BitCastInst* value_bitcast{};
    };

    [[nodiscard]] inline bool runtime_llvm_jit_instruction_belongs_directly_to_loop(::llvm::Instruction const& instruction,
                                                                                    ::llvm::Loop const& loop,
                                                                                    ::llvm::LoopInfo const& loop_info) noexcept
    {
        auto const* block{instruction.getParent()};
        return block != nullptr && loop_info.getLoopFor(block) == ::std::addressof(loop);
    }

    [[nodiscard]] inline ::std::optional<runtime_llvm_jit_expanded_lane_fp_operation>
        decode_runtime_llvm_jit_expanded_lane_fp_operation(::llvm::Value* value,
                                                           unsigned ordinary_opcode,
                                                           ::llvm::Intrinsic::ID constrained_intrinsic) noexcept
    {
        if(auto* binary_operator{::llvm::dyn_cast<::llvm::BinaryOperator>(value)};
           binary_operator != nullptr && binary_operator->getOpcode() == ordinary_opcode)
        {
            // This policy must never turn reassociable/contractable arithmetic into evidence for a strict Wasm lane.
            if(binary_operator->getFastMathFlags().any()) { return ::std::nullopt; }
            return runtime_llvm_jit_expanded_lane_fp_operation{binary_operator,
                                                               binary_operator->getOperand(0u),
                                                               binary_operator->getOperand(1u),
                                                               runtime_llvm_jit_expanded_lane_fp_kind::ordinary};
        }

        auto* constrained_operator{::llvm::dyn_cast<::llvm::ConstrainedFPIntrinsic>(value)};
        if(constrained_operator == nullptr || constrained_operator->getIntrinsicID() != constrained_intrinsic ||
           constrained_operator->getNonMetadataArgCount() != 2u)
        {
            return ::std::nullopt;
        }

        auto const rounding_mode{constrained_operator->getRoundingMode()};
        auto const exception_behavior{constrained_operator->getExceptionBehavior()};
        if(!rounding_mode || *rounding_mode != ::llvm::RoundingMode::NearestTiesToEven || !exception_behavior ||
           *exception_behavior != ::llvm::fp::ebStrict)
        {
            return ::std::nullopt;
        }

        return runtime_llvm_jit_expanded_lane_fp_operation{constrained_operator,
                                                           constrained_operator->getArgOperand(0u),
                                                           constrained_operator->getArgOperand(1u),
                                                           runtime_llvm_jit_expanded_lane_fp_kind::constrained};
    }

    [[nodiscard]] inline ::std::optional<::std::int_least64_t>
        get_runtime_llvm_jit_constant_byte_difference(::llvm::ScalarEvolution& scalar_evolution,
                                                      ::llvm::SCEV const* lhs,
                                                      ::llvm::SCEV const* rhs) noexcept
    {
        auto const difference{scalar_evolution.computeConstantDifference(lhs, rhs)};
        if(!difference || !difference->isSignedIntN(64u)) { return ::std::nullopt; }
        return difference->sextOrTrunc(64u).getSExtValue();
    }

    // Direct Wasm floating-point memory operations deliberately use integer storage in the translator so every load and
    // store preserves the exact IEEE payload bits: f32 is carried by i32 and f64 by i64.  Accept only those two exact,
    // equal-width storage pairs here; numeric conversions, width changes, vectors, and constant-expression casts remain
    // outside this narrow repeated-lane policy.
    [[nodiscard]] inline bool runtime_llvm_jit_is_exact_fp_integer_storage_pair(::llvm::Type const* storage_type,
                                                                                ::llvm::Type const* element_type) noexcept
    {
        return (element_type->isFloatTy() && storage_type->isIntegerTy(32u)) ||
               (element_type->isDoubleTy() && storage_type->isIntegerTy(64u));
    }

    [[nodiscard]] inline ::std::optional<runtime_llvm_jit_expanded_lane_storage_load>
        decode_runtime_llvm_jit_expanded_lane_storage_load(::llvm::Value* value,
                                                           ::llvm::Type* element_type,
                                                           ::llvm::Loop& loop,
                                                           ::llvm::LoopInfo const& loop_info) noexcept
    {
        if(auto* direct_load{::llvm::dyn_cast<::llvm::LoadInst>(value)}; direct_load != nullptr)
        {
            if(direct_load->getType() != element_type) { return ::std::nullopt; }
            return runtime_llvm_jit_expanded_lane_storage_load{direct_load, nullptr};
        }

        auto* value_bitcast{::llvm::dyn_cast<::llvm::BitCastInst>(value)};
        if(value_bitcast == nullptr || !value_bitcast->hasOneUse() || value_bitcast->getType() != element_type ||
           !runtime_llvm_jit_is_exact_fp_integer_storage_pair(value_bitcast->getOperand(0u)->getType(), element_type) ||
           !runtime_llvm_jit_instruction_belongs_directly_to_loop(*value_bitcast, loop, loop_info))
        {
            return ::std::nullopt;
        }

        auto* storage_load{::llvm::dyn_cast<::llvm::LoadInst>(value_bitcast->getOperand(0u))};
        if(storage_load == nullptr) { return ::std::nullopt; }
        return runtime_llvm_jit_expanded_lane_storage_load{storage_load, value_bitcast};
    }

    [[nodiscard]] inline ::std::optional<runtime_llvm_jit_expanded_lane_pattern>
        match_runtime_llvm_jit_expanded_lane(::llvm::StoreInst& accumulator_store,
                                             ::llvm::Loop& loop,
                                             ::llvm::LoopStandardAnalysisResults& analysis_results) noexcept
    {
        if(!accumulator_store.isVolatile() || accumulator_store.isAtomic()) { return ::std::nullopt; }

        auto* stored_value{accumulator_store.getValueOperand()};
        auto* storage_type{stored_value->getType()};
        auto* element_type{storage_type};
        ::llvm::BitCastInst* stored_value_bitcast{};
        if(!element_type->isFloatTy() && !element_type->isDoubleTy())
        {
            stored_value_bitcast = ::llvm::dyn_cast<::llvm::BitCastInst>(stored_value);
            if(stored_value_bitcast == nullptr || !stored_value_bitcast->hasOneUse() ||
               !runtime_llvm_jit_instruction_belongs_directly_to_loop(*stored_value_bitcast, loop, analysis_results.LI))
            {
                return ::std::nullopt;
            }
            stored_value = stored_value_bitcast->getOperand(0u);
            element_type = stored_value->getType();
            if(!runtime_llvm_jit_is_exact_fp_integer_storage_pair(storage_type, element_type)) { return ::std::nullopt; }
        }

        auto const add_operation{decode_runtime_llvm_jit_expanded_lane_fp_operation(
            stored_value, ::llvm::Instruction::FAdd, ::llvm::Intrinsic::experimental_constrained_fadd)};
        if(!add_operation || !add_operation->instruction->hasOneUse() ||
           !runtime_llvm_jit_instruction_belongs_directly_to_loop(*add_operation->instruction, loop, analysis_results.LI))
        {
            return ::std::nullopt;
        }

        ::std::optional<runtime_llvm_jit_expanded_lane_storage_load> accumulator_storage_load{};
        ::std::optional<runtime_llvm_jit_expanded_lane_fp_operation> multiply_operation{};
        if(auto candidate_load{decode_runtime_llvm_jit_expanded_lane_storage_load(
               add_operation->lhs, element_type, loop, analysis_results.LI)})
        {
            if(auto candidate_multiply{decode_runtime_llvm_jit_expanded_lane_fp_operation(
                   add_operation->rhs, ::llvm::Instruction::FMul, ::llvm::Intrinsic::experimental_constrained_fmul)})
            {
                accumulator_storage_load = *candidate_load;
                multiply_operation = *candidate_multiply;
            }
        }
        if(!multiply_operation)
        {
            if(auto candidate_load{decode_runtime_llvm_jit_expanded_lane_storage_load(
                   add_operation->rhs, element_type, loop, analysis_results.LI)})
            {
                if(auto candidate_multiply{decode_runtime_llvm_jit_expanded_lane_fp_operation(
                       add_operation->lhs, ::llvm::Instruction::FMul, ::llvm::Intrinsic::experimental_constrained_fmul)})
                {
                    accumulator_storage_load = *candidate_load;
                    multiply_operation = *candidate_multiply;
                }
            }
        }
        if(!accumulator_storage_load || !multiply_operation || multiply_operation->kind != add_operation->kind ||
           !multiply_operation->instruction->hasOneUse() ||
           !runtime_llvm_jit_instruction_belongs_directly_to_loop(*multiply_operation->instruction, loop, analysis_results.LI))
        {
            return ::std::nullopt;
        }

        ::std::optional<runtime_llvm_jit_expanded_lane_storage_load> factor_storage_load{};
        ::llvm::Value* invariant_multiplier{};
        if(auto candidate_load{decode_runtime_llvm_jit_expanded_lane_storage_load(
               multiply_operation->lhs, element_type, loop, analysis_results.LI)};
           candidate_load && loop.isLoopInvariant(multiply_operation->rhs))
        {
            factor_storage_load = *candidate_load;
            invariant_multiplier = multiply_operation->rhs;
        }
        else if(auto candidate_load{decode_runtime_llvm_jit_expanded_lane_storage_load(
                    multiply_operation->rhs, element_type, loop, analysis_results.LI)};
                candidate_load && loop.isLoopInvariant(multiply_operation->lhs))
        {
            factor_storage_load = *candidate_load;
            invariant_multiplier = multiply_operation->lhs;
        }
        auto* accumulator_load{accumulator_storage_load->instruction};
        auto* factor_load{factor_storage_load ? factor_storage_load->instruction : nullptr};
        if(factor_load == nullptr || accumulator_load == factor_load || !accumulator_load->isVolatile() || accumulator_load->isAtomic() ||
           !factor_load->isVolatile() || factor_load->isAtomic() || !accumulator_load->hasOneUse() || !factor_load->hasOneUse() ||
           accumulator_load->getType() != storage_type || factor_load->getType() != storage_type ||
           multiply_operation->instruction->getType() != element_type || invariant_multiplier->getType() != element_type ||
           !runtime_llvm_jit_instruction_belongs_directly_to_loop(*accumulator_load, loop, analysis_results.LI) ||
           !runtime_llvm_jit_instruction_belongs_directly_to_loop(*factor_load, loop, analysis_results.LI))
        {
            return ::std::nullopt;
        }

        auto* lane_block{accumulator_store.getParent()};
        if(accumulator_load->getParent() != lane_block || factor_load->getParent() != lane_block ||
           (stored_value_bitcast != nullptr && stored_value_bitcast->getParent() != lane_block) ||
           (accumulator_storage_load->value_bitcast != nullptr && accumulator_storage_load->value_bitcast->getParent() != lane_block) ||
           (factor_storage_load->value_bitcast != nullptr && factor_storage_load->value_bitcast->getParent() != lane_block) ||
           multiply_operation->instruction->getParent() != lane_block || add_operation->instruction->getParent() != lane_block)
        {
            return ::std::nullopt;
        }

        auto const* accumulator_address{analysis_results.SE.getSCEV(accumulator_load->getPointerOperand())};
        auto const* stored_accumulator_address{analysis_results.SE.getSCEV(accumulator_store.getPointerOperand())};
        auto const accumulator_store_difference{
            get_runtime_llvm_jit_constant_byte_difference(analysis_results.SE, stored_accumulator_address, accumulator_address)};
        if(!accumulator_store_difference || *accumulator_store_difference != 0) { return ::std::nullopt; }

        return runtime_llvm_jit_expanded_lane_pattern{element_type,
                                                      invariant_multiplier,
                                                      accumulator_load,
                                                      factor_load,
                                                      ::std::addressof(accumulator_store),
                                                      accumulator_address,
                                                      analysis_results.SE.getSCEV(factor_load->getPointerOperand()),
                                                      lane_block,
                                                      add_operation->kind};
    }

    struct runtime_llvm_jit_current_loop_add_rec_visitor
    {
        ::llvm::Loop const* loop{};
        ::llvm::SCEVAddRecExpr const* add_rec{};
        bool ambiguous{};

        [[nodiscard]] bool follow(::llvm::SCEV const* expression) noexcept
        {
            if(auto const* candidate{::llvm::dyn_cast<::llvm::SCEVAddRecExpr>(expression)}; candidate != nullptr && candidate->getLoop() == loop)
            {
                if(add_rec != nullptr && add_rec != candidate) { ambiguous = true; }
                else { add_rec = candidate; }
            }
            return !ambiguous;
        }

        [[nodiscard]] bool isDone() const noexcept { return ambiguous; }
    };

    [[nodiscard]] inline ::llvm::SCEVAddRecExpr const*
        get_runtime_llvm_jit_unique_current_loop_add_rec(::llvm::SCEV const* expression, ::llvm::Loop const& loop) noexcept
    {
        if(::llvm::isa<::llvm::SCEVCouldNotCompute>(expression)) { return nullptr; }
        runtime_llvm_jit_current_loop_add_rec_visitor visitor{::std::addressof(loop)};
        ::llvm::visitAll(expression, visitor);
        return visitor.ambiguous ? nullptr : visitor.add_rec;
    }

    [[nodiscard]] inline bool runtime_llvm_jit_add_rec_has_exact_positive_step(::llvm::SCEVAddRecExpr const* add_rec,
                                                                               ::std::uint_least64_t expected_step,
                                                                               ::llvm::ScalarEvolution& scalar_evolution) noexcept
    {
        if(add_rec == nullptr || !add_rec->isAffine()) { return false; }
        auto const* step{::llvm::dyn_cast<::llvm::SCEVConstant>(add_rec->getStepRecurrence(scalar_evolution))};
        if(step == nullptr) { return false; }
        auto const& step_value{step->getAPInt()};
        return !step_value.isNegative() && step_value == ::llvm::APInt{step_value.getBitWidth(), expected_step};
    }

    [[nodiscard]] inline bool runtime_llvm_jit_loop_has_only_intrinsic_calls(::llvm::Loop& loop, ::llvm::LoopInfo const& loop_info) noexcept
    {
        for(auto* block: loop.blocks())
        {
            if(loop_info.getLoopFor(block) != ::std::addressof(loop)) { continue; }
            for(auto& instruction: *block)
            {
                auto const* call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr) { continue; }
                auto const* called_function{call->getCalledFunction()};
                if(called_function == nullptr || !called_function->isIntrinsic()) { return false; }
            }
        }
        return true;
    }

    [[nodiscard]] inline bool runtime_llvm_jit_has_safe_linear_ancestor_chain(::llvm::Loop& loop, ::llvm::LoopInfo const& loop_info) noexcept
    {
        for(auto* current{::std::addressof(loop)}; current != nullptr; current = current->getParentLoop())
        {
            auto* latch{current->getLoopLatch()};
            if(current->getLoopID() != nullptr || latch == nullptr || latch != current->getExitingBlock() ||
               !runtime_llvm_jit_loop_has_only_intrinsic_calls(*current, loop_info))
            {
                return false;
            }

            if(auto* parent{current->getParentLoop()}; parent != nullptr)
            {
                auto const& subloops{parent->getSubLoops()};
                if(subloops.size() != 1uz || subloops.front() != current) { return false; }
            }
        }
        return true;
    }

    struct runtime_llvm_jit_expanded_lane_with_offset
    {
        runtime_llvm_jit_expanded_lane_pattern const* lane{};
        ::std::int_least64_t offset{};
    };

    [[nodiscard]] inline bool match_runtime_llvm_jit_already_expanded_lane_loop(::llvm::Loop& loop,
                                                                               ::llvm::LoopStandardAnalysisResults& analysis_results) noexcept
    {
        if(!loop.isInnermost()) { return false; }
        if(!runtime_llvm_jit_has_safe_linear_ancestor_chain(loop, analysis_results.LI)) { return false; }

        ::llvm::SmallVector<runtime_llvm_jit_expanded_lane_pattern, 8u> candidates{};
        for(auto* block: loop.blocks())
        {
            if(analysis_results.LI.getLoopFor(block) != ::std::addressof(loop)) { continue; }
            for(auto& instruction: *block)
            {
                auto* store{::llvm::dyn_cast<::llvm::StoreInst>(::std::addressof(instruction))};
                if(store == nullptr) { continue; }
                if(auto lane{match_runtime_llvm_jit_expanded_lane(*store, loop, analysis_results)})
                {
                    // Keep matching cost bounded for adversarial modules; translated scalar lane groups are intentionally small.
                    if(candidates.size() == 64uz) { return false; }
                    candidates.push_back(*lane);
                }
            }
        }
        if(candidates.size() < 2uz) { return false; }

        for(auto const& first_lane: candidates)
        {
            ::llvm::SmallVector<runtime_llvm_jit_expanded_lane_with_offset, 8u> lanes{};
            for(auto const& candidate: candidates)
            {
                if(candidate.element_type != first_lane.element_type || candidate.invariant_multiplier != first_lane.invariant_multiplier ||
                   candidate.block != first_lane.block || candidate.kind != first_lane.kind)
                {
                    continue;
                }

                auto const accumulator_offset{get_runtime_llvm_jit_constant_byte_difference(
                    analysis_results.SE, candidate.accumulator_address, first_lane.accumulator_address)};
                auto const factor_offset{
                    get_runtime_llvm_jit_constant_byte_difference(analysis_results.SE, candidate.factor_address, first_lane.factor_address)};
                if(!accumulator_offset || !factor_offset || *accumulator_offset != *factor_offset) { continue; }
                lanes.push_back(runtime_llvm_jit_expanded_lane_with_offset{::std::addressof(candidate), *accumulator_offset});
            }
            if(lanes.size() < 2uz) { continue; }

            ::std::sort(lanes.begin(), lanes.end(), [](auto const& lhs, auto const& rhs) constexpr noexcept { return lhs.offset < rhs.offset; });
            auto const element_size{first_lane.element_type->isFloatTy() ? 4u : 8u};
            bool consecutive{true};
            for(::std::size_t lane_index{1uz}; lane_index != lanes.size(); ++lane_index)
            {
                auto const previous_offset{lanes[lane_index - 1uz].offset};
                if(previous_offset > ::std::numeric_limits<::std::int_least64_t>::max() - element_size ||
                   lanes[lane_index].offset != previous_offset + static_cast<::std::int_least64_t>(element_size) ||
                   !lanes[lane_index - 1uz].lane->accumulator_store->comesBefore(lanes[lane_index].lane->accumulator_store))
                {
                    consecutive = false;
                    break;
                }
            }
            if(!consecutive || lanes.size() > ::std::numeric_limits<::std::uint_least64_t>::max() / element_size) { continue; }

            auto const expected_step{static_cast<::std::uint_least64_t>(lanes.size()) * element_size};
            auto const* accumulator_add_rec{
                get_runtime_llvm_jit_unique_current_loop_add_rec(first_lane.accumulator_address, loop)};
            auto const* factor_add_rec{get_runtime_llvm_jit_unique_current_loop_add_rec(first_lane.factor_address, loop)};
            if(!runtime_llvm_jit_add_rec_has_exact_positive_step(accumulator_add_rec, expected_step, analysis_results.SE) ||
               !runtime_llvm_jit_add_rec_has_exact_positive_step(factor_add_rec, expected_step, analysis_results.SE))
            {
                continue;
            }

            return true;
        }
        return false;
    }

    struct runtime_llvm_jit_mark_expanded_lane_loop_pass : ::llvm::PassInfoMixin<runtime_llvm_jit_mark_expanded_lane_loop_pass>
    {
        [[nodiscard]] ::llvm::PreservedAnalyses run(::llvm::Loop& loop,
                                                    ::llvm::LoopAnalysisManager&,
                                                    ::llvm::LoopStandardAnalysisResults& analysis_results,
                                                    ::llvm::LPMUpdater&) const noexcept
        {
            if(!match_runtime_llvm_jit_already_expanded_lane_loop(loop, analysis_results)) { return ::llvm::PreservedAnalyses::all(); }

            // A later unroller may move the second expansion opportunity from the matched inner loop to a containing loop. Mark the
            // whole ancestor chain so the translator's existing adjacent lanes remain the only expansion. The matcher admits only a
            // strictly linear chain, therefore this cannot suppress unrolling of a sibling loop.
            for(auto* current{::std::addressof(loop)}; current != nullptr; current = current->getParentLoop())
            {
                current->setLoopAlreadyUnrolled();
            }
            return ::llvm::PreservedAnalyses::all();
        }
    };

    inline void register_runtime_llvm_jit_expanded_lane_unroll_policy(::llvm::PassBuilder& pass_builder)
    {
        pass_builder.registerLoopOptimizerEndEPCallback(
            [](::llvm::LoopPassManager& loop_pass_manager, ::llvm::OptimizationLevel optimization_level)
            {
                // O0/O1 and size pipelines do not participate: this is a narrow correction to the O2/O3 follow-up unroller.
                if(optimization_level == ::llvm::OptimizationLevel::O2 || optimization_level == ::llvm::OptimizationLevel::O3)
                {
                    loop_pass_manager.addPass(runtime_llvm_jit_mark_expanded_lane_loop_pass{});
                }
            });
    }
}
