#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <llvm/IR/Instructions.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>

#include <memory>

namespace
{
    namespace details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

    [[nodiscard]] int check_function_policy() noexcept
    {
        ::llvm::LLVMContext context{};
        ::llvm::Module module{"llvm-aot-noinline-policy", context};
        auto float_type{::llvm::Type::getFloatTy(context)};
        auto function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), {float_type, float_type}, false)};
        auto function{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "wasm_entry", module)};
        if(function == nullptr) { return 1; }

        details::apply_llvm_jit_wasm_calling_conv(*function);
        if(!function->hasFnAttribute(::llvm::Attribute::NoInline)) { return 2; }
        if(!function->hasFnAttribute(::llvm::Attribute::NoMerge)) { return 3; }
        if(!function->hasFnAttribute("nooutline")) { return 4; }
        if(function->hasFnAttribute(::llvm::Attribute::AlwaysInline)) { return 5; }
        if(function->hasFnAttribute(::llvm::Attribute::OptimizeNone)) { return 6; }

        auto const disable_tail_calls{function->getFnAttribute("disable-tail-calls")};
        if(!disable_tail_calls.isStringAttribute() || disable_tail_calls.getValueAsString() != "true") { return 7; }

        auto const denormal_f64{function->getFnAttribute("denormal-fp-math")};
        if(!denormal_f64.isStringAttribute() || denormal_f64.getValueAsString() != "ieee,ieee") { return 8; }
        auto const denormal_f32{function->getFnAttribute("denormal-fp-math-f32")};
        if(!denormal_f32.isStringAttribute() || denormal_f32.getValueAsString() != "ieee,ieee") { return 9; }

        auto block{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{block};
        auto add{::llvm::dyn_cast_or_null<::llvm::BinaryOperator>(builder.CreateFAdd(function->getArg(0), function->getArg(1)))};
        if(add == nullptr) { return 10; }
        if(add->getFastMathFlags().any()) { return 11; }
        builder.CreateRetVoid();
        return ::llvm::verifyModule(module, ::std::addressof(::llvm::errs())) ? 12 : 0;
    }

    [[nodiscard]] int check_optimized_wasm_call_boundary(::llvm::OptimizationLevel level) noexcept
    {
        ::llvm::LLVMContext context{};
        ::llvm::Module module{"llvm-aot-noinline-o3", context};
        auto float_type{::llvm::Type::getFloatTy(context)};
        auto function_type{::llvm::FunctionType::get(float_type, {float_type, float_type}, false)};

        auto callee{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "wasm_callee", module)};
        auto caller{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "wasm_caller", module)};
        if(callee == nullptr || caller == nullptr) { return 20; }
        details::apply_llvm_jit_wasm_calling_conv(*callee);
        details::apply_llvm_jit_wasm_calling_conv(*caller);

        auto callee_block{::llvm::BasicBlock::Create(context, "entry", callee)};
        ::llvm::IRBuilder<> callee_builder{callee_block};
        callee_builder.CreateRet(callee_builder.CreateFAdd(callee->getArg(0), callee->getArg(1)));

        auto caller_block{::llvm::BasicBlock::Create(context, "entry", caller)};
        ::llvm::IRBuilder<> caller_builder{caller_block};
        caller_builder.CreateRet(caller_builder.CreateCall(callee, {caller->getArg(0), caller->getArg(1)}));

        if(::llvm::verifyModule(module, ::std::addressof(::llvm::errs()))) { return 21; }

        ::llvm::LoopAnalysisManager loop_analysis_manager{};
        ::llvm::FunctionAnalysisManager function_analysis_manager{};
        ::llvm::CGSCCAnalysisManager cgscc_analysis_manager{};
        ::llvm::ModuleAnalysisManager module_analysis_manager{};
        ::llvm::PipelineTuningOptions pipeline_tuning_options{};
        pipeline_tuning_options.LoopUnrolling = true;
        pipeline_tuning_options.LoopInterleaving = true;
        pipeline_tuning_options.LoopVectorization = true;
        pipeline_tuning_options.SLPVectorization = true;
        ::llvm::PassBuilder pass_builder{nullptr, pipeline_tuning_options};
        pass_builder.registerModuleAnalyses(module_analysis_manager);
        pass_builder.registerCGSCCAnalyses(cgscc_analysis_manager);
        pass_builder.registerFunctionAnalyses(function_analysis_manager);
        pass_builder.registerLoopAnalyses(loop_analysis_manager);
        pass_builder.crossRegisterProxies(loop_analysis_manager, function_analysis_manager, cgscc_analysis_manager, module_analysis_manager);
        auto module_pass_manager{pass_builder.buildPerModuleDefaultPipeline(level)};
        module_pass_manager.run(module, module_analysis_manager);

        if(::llvm::verifyModule(module, ::std::addressof(::llvm::errs()))) { return 22; }
        callee = module.getFunction("wasm_callee");
        caller = module.getFunction("wasm_caller");
        if(callee == nullptr || caller == nullptr || callee->isDeclaration() || caller->isDeclaration()) { return 23; }

        for(auto const& block: *caller)
        {
            for(auto const& instruction: block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call != nullptr && call->getCalledFunction() == callee) { return 0; }
            }
        }
        return 24;
    }
}

int main()
{
    if(auto const status{check_function_policy()}; status != 0) { return status; }
    // Inlining decisions are not monotonic across speed/size pipelines. O3
    // alone cannot establish the mandatory boundary for O1/O2 or size-tuned
    // future policies. The unoptimized policy is checked above; each pipeline
    // here must retain the actual call, not merely the attribute's spelling.
    for(auto const level: {::llvm::OptimizationLevel::O1, ::llvm::OptimizationLevel::O2,
                          ::llvm::OptimizationLevel::O3, ::llvm::OptimizationLevel::Os, ::llvm::OptimizationLevel::Oz})
    {
        if(auto const status{check_optimized_wasm_call_boundary(level)}; status != 0) { return status; }
    }
    return 0;
}
