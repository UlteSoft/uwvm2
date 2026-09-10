#include <uwvm2/runtime/lib/uwvm_runtime_llvm_expanded_lane_unroll_policy.h>

#include <cstddef>
#include <cstdio>
#include <iterator>
#include <memory>

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>

namespace
{
    namespace policy = ::uwvm2::runtime::lib::details;

    enum class rejected_shape
    {
        none,
        mismatched_inner_step,
        inner_unknown_call,
        ancestor_unknown_call,
        mismatched_storage_width,
        non_bitcast_storage_conversion,
        storage_bitcast_multiple_uses,
        nonvolatile_storage,
        fast_math
    };

    struct fixture_options
    {
        bool constrained_fp{};
        bool integer_storage{};
        bool single_precision{};
        rejected_shape rejected{};
    };

    [[nodiscard]] ::std::unique_ptr<::llvm::Module> make_fixture(::llvm::LLVMContext& context, fixture_options const options)
    {
        auto module{::std::make_unique<::llvm::Module>("uwvm-expanded-lane-unroll-policy", context)};
        module->setDataLayout("e-p:64:64");

        auto* void_type{::llvm::Type::getVoidTy(context)};
        auto* i8_type{::llvm::Type::getInt8Ty(context)};
        auto* i32_type{::llvm::Type::getInt32Ty(context)};
        auto* i64_type{::llvm::Type::getInt64Ty(context)};
        auto* f32_type{::llvm::Type::getFloatTy(context)};
        auto* f64_type{::llvm::Type::getDoubleTy(context)};
        auto* element_type{options.single_precision ? f32_type : f64_type};
        auto* integer_storage_type{options.single_precision ? i32_type : i64_type};
        auto const element_size{options.single_precision ? 4u : 8u};
        auto* pointer_type{::llvm::PointerType::getUnqual(context)};
        auto* function_type{::llvm::FunctionType::get(
            void_type, {pointer_type, pointer_type, element_type, i64_type, i64_type, i64_type, i64_type}, false)};
        auto* function{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "wasm_lane_loop", *module)};
        if(options.constrained_fp) { function->addFnAttr(::llvm::Attribute::StrictFP); }

        auto argument{function->arg_begin()};
        auto* accumulator_base{::std::addressof(*argument++)};
        accumulator_base->setName("accumulator.base");
        auto* factor_base{::std::addressof(*argument++)};
        factor_base->setName("factor.base");
        auto* invariant_multiplier{::std::addressof(*argument++)};
        invariant_multiplier->setName("multiplier");
        auto* outer_trip_count{::std::addressof(*argument++)};
        outer_trip_count->setName("outer.trip.count");
        auto* middle_trip_count{::std::addressof(*argument++)};
        middle_trip_count->setName("middle.trip.count");
        auto* inner_parent_trip_count{::std::addressof(*argument++)};
        inner_parent_trip_count->setName("inner.parent.trip.count");
        auto* byte_limit{::std::addressof(*argument)};
        byte_limit->setName("byte.limit");

        auto* unknown_function_type{::llvm::FunctionType::get(void_type, false)};
        auto unknown_function{module->getOrInsertFunction("unknown_host_call", unknown_function_type)};

        auto* entry{::llvm::BasicBlock::Create(context, "entry", function)};
        auto* outer_header{::llvm::BasicBlock::Create(context, "outer.header", function)};
        auto* middle_header{::llvm::BasicBlock::Create(context, "middle.header", function)};
        auto* inner_parent_header{::llvm::BasicBlock::Create(context, "inner.parent.header", function)};
        auto* lane_loop{::llvm::BasicBlock::Create(context, "lane.loop", function)};
        auto* inner_parent_latch{::llvm::BasicBlock::Create(context, "inner.parent.latch", function)};
        auto* middle_latch{::llvm::BasicBlock::Create(context, "middle.latch", function)};
        auto* outer_latch{::llvm::BasicBlock::Create(context, "outer.latch", function)};
        auto* exit{::llvm::BasicBlock::Create(context, "exit", function)};

        ::llvm::IRBuilder<> entry_builder{entry};
        entry_builder.CreateBr(outer_header);

        ::llvm::IRBuilder<> outer_header_builder{outer_header};
        auto* outer_index{outer_header_builder.CreatePHI(i64_type, 2u, "outer.index")};
        outer_index->addIncoming(::llvm::ConstantInt::get(i64_type, 0u), entry);
        outer_header_builder.CreateBr(middle_header);

        ::llvm::IRBuilder<> middle_header_builder{middle_header};
        auto* middle_index{middle_header_builder.CreatePHI(i64_type, 2u, "middle.index")};
        middle_index->addIncoming(::llvm::ConstantInt::get(i64_type, 0u), outer_header);
        middle_header_builder.CreateBr(inner_parent_header);

        ::llvm::IRBuilder<> inner_parent_header_builder{inner_parent_header};
        auto* inner_parent_index{inner_parent_header_builder.CreatePHI(i64_type, 2u, "inner.parent.index")};
        inner_parent_index->addIncoming(::llvm::ConstantInt::get(i64_type, 0u), middle_header);
        if(options.rejected == rejected_shape::ancestor_unknown_call) { inner_parent_header_builder.CreateCall(unknown_function); }
        inner_parent_header_builder.CreateBr(lane_loop);

        ::llvm::IRBuilder<> lane_builder{lane_loop};
        auto* byte_offset{lane_builder.CreatePHI(i64_type, 2u, "byte.offset")};
        byte_offset->addIncoming(::llvm::ConstantInt::get(i64_type, 0u), inner_parent_header);
        if(options.rejected == rejected_shape::inner_unknown_call) { lane_builder.CreateCall(unknown_function); }
        if(options.constrained_fp)
        {
            lane_builder.setIsFPConstrained(true);
            lane_builder.setDefaultConstrainedRounding(::llvm::RoundingMode::NearestTiesToEven);
            lane_builder.setDefaultConstrainedExcept(::llvm::fp::ebStrict);
        }

        for(unsigned lane{}; lane != 3u; ++lane)
        {
            ::llvm::Value* lane_offset{byte_offset};
            if(lane != 0u)
            {
                lane_offset = lane_builder.CreateAdd(byte_offset, ::llvm::ConstantInt::get(i64_type, lane * element_size), "lane.offset");
            }

            auto* accumulator_pointer{lane_builder.CreateGEP(i8_type, accumulator_base, lane_offset, "accumulator.pointer")};
            auto* accumulator_storage_value{
                lane_builder.CreateLoad(options.integer_storage ? integer_storage_type : element_type,
                                        accumulator_pointer,
                                        "accumulator.storage.value")};
            accumulator_storage_value->setVolatile(true);
            ::llvm::Value* accumulator_value{accumulator_storage_value};
            if(options.integer_storage)
            {
                accumulator_value = lane_builder.CreateBitCast(accumulator_storage_value, element_type, "accumulator.value");
            }
            auto* factor_pointer{lane_builder.CreateGEP(i8_type, factor_base, lane_offset, "factor.pointer")};
            auto* factor_storage_value{
                lane_builder.CreateLoad(options.integer_storage ? integer_storage_type : element_type,
                                        factor_pointer,
                                        "factor.storage.value")};
            factor_storage_value->setVolatile(true);
            ::llvm::Value* factor_value{factor_storage_value};
            if(options.integer_storage) { factor_value = lane_builder.CreateBitCast(factor_storage_value, element_type, "factor.value"); }

            if(options.rejected == rejected_shape::storage_bitcast_multiple_uses)
            {
                auto* negated_accumulator{lane_builder.CreateFNeg(accumulator_value, "extra.accumulator.use")};
                auto* extra_stored_value{lane_builder.CreateBitCast(negated_accumulator, integer_storage_type, "extra.stored.bits")};
                auto* extra_store{lane_builder.CreateStore(extra_stored_value, accumulator_pointer)};
                extra_store->setVolatile(true);
            }

            auto* product{lane_builder.CreateFMul(factor_value, invariant_multiplier, "product")};
            auto* sum{lane_builder.CreateFAdd(accumulator_value, product, "sum")};
            if(options.rejected == rejected_shape::fast_math)
            {
                ::llvm::cast<::llvm::Instruction>(product)->setFast(true);
                ::llvm::cast<::llvm::Instruction>(sum)->setFast(true);
            }

            ::llvm::Value* stored_value{sum};
            if(options.integer_storage)
            {
                stored_value = lane_builder.CreateBitCast(sum, integer_storage_type, "stored.bits");
                if(options.rejected == rejected_shape::mismatched_storage_width)
                {
                    stored_value = lane_builder.CreateTrunc(stored_value, i32_type, "narrow.stored.bits");
                }
                else if(options.rejected == rejected_shape::non_bitcast_storage_conversion)
                {
                    stored_value = lane_builder.CreateFPToSI(sum, integer_storage_type, "converted.storage.value");
                }
            }
            auto* store{lane_builder.CreateStore(stored_value, accumulator_pointer)};
            if(options.rejected != rejected_shape::nonvolatile_storage) { store->setVolatile(true); }
        }

        auto const byte_step{options.rejected == rejected_shape::mismatched_inner_step ? 4u * element_size : 3u * element_size};
        auto* next_byte_offset{lane_builder.CreateAdd(byte_offset, ::llvm::ConstantInt::get(i64_type, byte_step), "next.byte.offset")};
        byte_offset->addIncoming(next_byte_offset, lane_loop);
        auto* continue_lane_loop{lane_builder.CreateICmpULT(next_byte_offset, byte_limit, "continue.lane.loop")};
        lane_builder.CreateCondBr(continue_lane_loop, lane_loop, inner_parent_latch);

        ::llvm::IRBuilder<> inner_parent_latch_builder{inner_parent_latch};
        auto* next_inner_parent_index{
            inner_parent_latch_builder.CreateAdd(inner_parent_index, ::llvm::ConstantInt::get(i64_type, 1u), "next.inner.parent.index")};
        inner_parent_index->addIncoming(next_inner_parent_index, inner_parent_latch);
        auto* continue_inner_parent{
            inner_parent_latch_builder.CreateICmpULT(next_inner_parent_index, inner_parent_trip_count, "continue.inner.parent")};
        inner_parent_latch_builder.CreateCondBr(continue_inner_parent, inner_parent_header, middle_latch);

        ::llvm::IRBuilder<> middle_latch_builder{middle_latch};
        auto* next_middle_index{middle_latch_builder.CreateAdd(middle_index, ::llvm::ConstantInt::get(i64_type, 1u), "next.middle.index")};
        middle_index->addIncoming(next_middle_index, middle_latch);
        auto* continue_middle{middle_latch_builder.CreateICmpULT(next_middle_index, middle_trip_count, "continue.middle")};
        middle_latch_builder.CreateCondBr(continue_middle, middle_header, outer_latch);

        ::llvm::IRBuilder<> outer_latch_builder{outer_latch};
        auto* next_outer_index{outer_latch_builder.CreateAdd(outer_index, ::llvm::ConstantInt::get(i64_type, 1u), "next.outer.index")};
        outer_index->addIncoming(next_outer_index, outer_latch);
        auto* continue_outer{outer_latch_builder.CreateICmpULT(next_outer_index, outer_trip_count, "continue.outer")};
        outer_latch_builder.CreateCondBr(continue_outer, outer_header, exit);

        ::llvm::IRBuilder<> exit_builder{exit};
        exit_builder.CreateRetVoid();
        return module;
    }

    [[nodiscard]] bool has_unroll_disable(::llvm::Loop const& loop) noexcept
    {
        auto const* loop_id{loop.getLoopID()};
        if(loop_id == nullptr) { return false; }
        for(unsigned operand_index{1u}; operand_index != loop_id->getNumOperands(); ++operand_index)
        {
            auto const* property{::llvm::dyn_cast_or_null<::llvm::MDNode>(loop_id->getOperand(operand_index).get())};
            if(property == nullptr || property->getNumOperands() == 0u) { continue; }
            auto const* name{::llvm::dyn_cast_or_null<::llvm::MDString>(property->getOperand(0u).get())};
            if(name != nullptr && name->getString() == "llvm.loop.unroll.disable") { return true; }
        }
        return false;
    }

    [[nodiscard]] ::std::size_t count_unroll_disable_markers(::llvm::Module const& module) noexcept
    {
        ::std::size_t count{};
        for(auto const& function: module)
        {
            for(auto const& block: function)
            {
                auto const* loop_id{block.getTerminator()->getMetadata(::llvm::LLVMContext::MD_loop)};
                if(loop_id == nullptr) { continue; }
                for(unsigned operand_index{1u}; operand_index != loop_id->getNumOperands(); ++operand_index)
                {
                    auto const* property{::llvm::dyn_cast_or_null<::llvm::MDNode>(loop_id->getOperand(operand_index).get())};
                    if(property == nullptr || property->getNumOperands() == 0u) { continue; }
                    auto const* name{::llvm::dyn_cast_or_null<::llvm::MDString>(property->getOperand(0u).get())};
                    if(name != nullptr && name->getString() == "llvm.loop.unroll.disable")
                    {
                        ++count;
                        break;
                    }
                }
            }
        }
        return count;
    }

    [[nodiscard]] bool has_complete_marked_ancestor_chain(::llvm::LoopInfo const& loop_info) noexcept
    {
        if(::std::distance(loop_info.begin(), loop_info.end()) != 1) { return false; }
        auto const* current{*loop_info.begin()};
        for(unsigned depth{}; depth != 4u; ++depth)
        {
            if(current == nullptr || !has_unroll_disable(*current)) { return false; }
            auto const& subloops{current->getSubLoops()};
            if(depth == 3u) { return subloops.empty(); }
            if(subloops.size() != 1uz) { return false; }
            current = subloops.front();
        }
        return false;
    }

    [[nodiscard]] int run_case(char const* name,
                               fixture_options const options,
                               ::llvm::OptimizationLevel const optimization_level,
                               bool expect_marked_chain) noexcept
    {
        ::llvm::LLVMContext context{};
        auto module{make_fixture(context, options)};
        if(::llvm::verifyModule(*module, ::std::addressof(::llvm::errs())))
        {
            ::std::fprintf(stderr, "%s: input verification failed\n", name);
            return 1;
        }

        ::llvm::LoopAnalysisManager loop_analysis_manager{};
        ::llvm::FunctionAnalysisManager function_analysis_manager{};
        ::llvm::CGSCCAnalysisManager cgscc_analysis_manager{};
        ::llvm::ModuleAnalysisManager module_analysis_manager{};
        ::llvm::PipelineTuningOptions pipeline_tuning_options{};
        pipeline_tuning_options.LoopUnrolling = optimization_level == ::llvm::OptimizationLevel::O2 ||
                                                optimization_level == ::llvm::OptimizationLevel::O3;
        pipeline_tuning_options.LoopInterleaving = pipeline_tuning_options.LoopUnrolling;
        pipeline_tuning_options.LoopVectorization = pipeline_tuning_options.LoopUnrolling;
        pipeline_tuning_options.SLPVectorization = pipeline_tuning_options.LoopUnrolling;
        ::llvm::PassBuilder pass_builder{nullptr, pipeline_tuning_options};
        policy::register_runtime_llvm_jit_expanded_lane_unroll_policy(pass_builder);
        pass_builder.registerModuleAnalyses(module_analysis_manager);
        pass_builder.registerCGSCCAnalyses(cgscc_analysis_manager);
        pass_builder.registerFunctionAnalyses(function_analysis_manager);
        pass_builder.registerLoopAnalyses(loop_analysis_manager);
        pass_builder.crossRegisterProxies(
            loop_analysis_manager, function_analysis_manager, cgscc_analysis_manager, module_analysis_manager);
        auto module_pass_manager{pass_builder.buildPerModuleDefaultPipeline(optimization_level)};
        module_pass_manager.run(*module, module_analysis_manager);

        if(::llvm::verifyModule(*module, ::std::addressof(::llvm::errs())))
        {
            ::std::fprintf(stderr, "%s: optimized verification failed\n", name);
            return 2;
        }

        auto const marker_count{count_unroll_disable_markers(*module)};
        if(!expect_marked_chain)
        {
            if(marker_count != 0uz)
            {
                ::std::fprintf(stderr, "%s: unexpectedly found %zu unroll-disable markers\n", name, marker_count);
                return 3;
            }
            return 0;
        }

        auto* function{module->getFunction("wasm_lane_loop")};
        if(function == nullptr)
        {
            ::std::fprintf(stderr, "%s: optimized function disappeared\n", name);
            return 4;
        }
        auto& loop_info{function_analysis_manager.getResult<::llvm::LoopAnalysis>(*function)};
        if(marker_count != 4uz || !has_complete_marked_ancestor_chain(loop_info))
        {
            ::std::fprintf(stderr, "%s: expected one completely marked four-loop ancestor chain, markers=%zu\n", name, marker_count);
            return 5;
        }
        return 0;
    }
}

int main()
{
    struct test_case
    {
        char const* name{};
        fixture_options options{};
        ::llvm::OptimizationLevel optimization_level{};
        bool expect_marked_chain{};
    };

    test_case const cases[]{
        {"ordinary-direct-o3", {false, false, false, rejected_shape::none}, ::llvm::OptimizationLevel::O3, true},
        {"constrained-direct-o2", {true, false, false, rejected_shape::none}, ::llvm::OptimizationLevel::O2, true},
        {"ordinary-f64-integer-storage-o3", {false, true, false, rejected_shape::none}, ::llvm::OptimizationLevel::O3, true},
        {"constrained-f64-integer-storage-o2", {true, true, false, rejected_shape::none}, ::llvm::OptimizationLevel::O2, true},
        {"ordinary-f32-integer-storage-o3", {false, true, true, rejected_shape::none}, ::llvm::OptimizationLevel::O3, true},
        {"mismatched-step", {false, true, false, rejected_shape::mismatched_inner_step}, ::llvm::OptimizationLevel::O3, false},
        {"inner-unknown-call", {false, true, false, rejected_shape::inner_unknown_call}, ::llvm::OptimizationLevel::O3, false},
        {"ancestor-unknown-call", {true, true, false, rejected_shape::ancestor_unknown_call}, ::llvm::OptimizationLevel::O3, false},
        {"mismatched-storage-width", {false, true, false, rejected_shape::mismatched_storage_width}, ::llvm::OptimizationLevel::O3, false},
        {"non-bitcast-storage", {false, true, false, rejected_shape::non_bitcast_storage_conversion}, ::llvm::OptimizationLevel::O3, false},
        {"storage-bitcast-multiple-uses", {false, true, false, rejected_shape::storage_bitcast_multiple_uses}, ::llvm::OptimizationLevel::O3, false},
        {"nonvolatile-storage", {false, true, false, rejected_shape::nonvolatile_storage}, ::llvm::OptimizationLevel::O3, false},
        {"fast-math", {false, true, false, rejected_shape::fast_math}, ::llvm::OptimizationLevel::O3, false},
        {"ordinary-o1", {false, true, false, rejected_shape::none}, ::llvm::OptimizationLevel::O1, false}};

    for(auto const& test: cases)
    {
        if(auto const status{run_case(test.name, test.options, test.optimization_level, test.expect_marked_chain)}; status != 0)
        {
            return status;
        }
    }
    return 0;
}
