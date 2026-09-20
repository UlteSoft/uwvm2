#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <memory>

namespace
{
    namespace details = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;

    static_assert(details::get_llvm_wasm_nearest_intrinsic_id() == ::llvm::Intrinsic::roundeven);

    [[nodiscard]] int check_roundeven_call(bool use_f64) noexcept
    {
        ::llvm::LLVMContext context{};
        ::llvm::Module module{use_f64 ? "wasm-f64-nearest" : "wasm-f32-nearest", context};
        auto value_type{use_f64 ? ::llvm::Type::getDoubleTy(context) : ::llvm::Type::getFloatTy(context)};
        auto function_type{::llvm::FunctionType::get(value_type, {value_type}, false)};
        auto function{::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, "nearest", module)};
        if(function == nullptr) { return 1; }

        auto block{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{block};
        ::llvm::Type* overloaded_types[]{value_type};
        ::llvm::Value* arguments[]{function->getArg(0)};
        auto rounded{details::call_llvm_intrinsic(
            module, builder, details::get_llvm_wasm_nearest_intrinsic_id(), overloaded_types, arguments)};
        auto call{::llvm::dyn_cast_or_null<::llvm::CallInst>(rounded)};
        auto callee{call == nullptr ? nullptr : call->getCalledFunction()};
        if(callee == nullptr || callee->getIntrinsicID() != ::llvm::Intrinsic::roundeven ||
           callee->getIntrinsicID() == ::llvm::Intrinsic::rint)
        {
            return 2;
        }

        builder.CreateRet(rounded);
        return ::llvm::verifyModule(module, ::std::addressof(::llvm::errs())) ? 3 : 0;
    }
}

int main()
{
    auto const f32_result{check_roundeven_call(false)};
    if(f32_result != 0) { return f32_result; }
    auto const f64_result{check_roundeven_call(true)};
    return f64_result == 0 ? 0 : f64_result + 3;
}
