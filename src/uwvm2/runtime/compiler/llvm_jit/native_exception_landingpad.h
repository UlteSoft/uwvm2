/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <utility>
# if defined(UWVM_RUNTIME_LLVM_JIT)
#  include <llvm/ADT/SmallVector.h>
#  include <llvm/IR/BasicBlock.h>
#  include <llvm/IR/IRBuilder.h>
#  include <llvm/IR/Instructions.h>
#  include <llvm/IR/Intrinsics.h>
#  include "native_exception_symbols.h"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad
{
    namespace symbols = ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols;

    struct catch_runtime
    {
        ::llvm::Function* begin{};
        ::llvm::Function* end{};
        ::llvm::Function* rethrow{};
        [[nodiscard]] explicit inline operator bool() const noexcept { return begin && end && rethrow; }
    };

    namespace details
    {
        [[nodiscard]] inline bool valid_runtime_function(::llvm::GlobalValue const* global, ::llvm::FunctionType* type) noexcept
        {
            if(global == nullptr) { return true; }
            auto const function{::llvm::dyn_cast<::llvm::Function>(global)};
            return function != nullptr && function->isDeclaration() && function->getFunctionType() == type &&
                function->hasExternalLinkage() && function->getCallingConv() == ::llvm::CallingConv::C &&
                function->getAddressSpace() == 0u && !function->hasDLLImportStorageClass() &&
                !function->hasDLLExportStorageClass();
        }
        [[nodiscard]] inline ::llvm::Function* declare_runtime_function(::llvm::Module& module,
            char const* name, ::llvm::FunctionType* type)
        {
            if(auto const existing{module.getFunction(name)}; existing != nullptr) { return existing; }
            // [module-owned function declaration] no host address is inserted into cached IR/code.
            // [safe                            ] Function::Create immediately transfers ownership.
            return ::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage, name, module);
        }
    }

    // Bind these declarations per engine, before finalization, to the same C++ ABI runtime that owns
    // the qualified guest typeinfo/personality. No process-global AddSymbol registration is required.
    [[nodiscard]] inline catch_runtime declare_catch_runtime(::llvm::Module& module)
    {
        auto& context{module.getContext()};
        auto const pointer{::llvm::PointerType::getUnqual(context)};
        auto const begin_type{::llvm::FunctionType::get(pointer, {pointer}, false)};
        auto const void_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), false)};
        if(!details::valid_runtime_function(module.getNamedValue("__cxa_begin_catch"), begin_type) ||
           !details::valid_runtime_function(module.getNamedValue("__cxa_end_catch"), void_type) ||
           !details::valid_runtime_function(module.getNamedValue("__cxa_rethrow"), void_type)) { return {}; }
        return {details::declare_runtime_function(module, "__cxa_begin_catch", begin_type),
                details::declare_runtime_function(module, "__cxa_end_catch", void_type),
                details::declare_runtime_function(module, "__cxa_rethrow", void_type)};
    }

    struct catch_entry
    {
        ::llvm::BasicBlock* landing{};
        ::llvm::BasicBlock* caught{};
        ::llvm::BasicBlock* foreign{};
        ::llvm::LandingPadInst* unwind{};
        ::llvm::Value* guest_object{};
        [[nodiscard]] explicit inline operator bool() const noexcept { return landing != nullptr; }
    };

    // Build the exceptional edge only; the caller emits an invoke targeting result.landing and its
    // ordinary continuation. No TLS handler push/pop, instruction trace entry, or memory guard is added
    // to that ordinary edge. Catch only the exact final guest_exception TYPE. Wasm tag matching remains
    // a separate dispatch inside result.caught; a native catch-all would incorrectly consume host traps.
    // That dispatch must search ALL lexical Wasm handlers protecting this call, inner to outer. A
    // mismatched inner try_table must not rethrow out of the native frame before trying an outer table
    // in the SAME Wasm function. emit_rethrow is only for no matching handler anywhere in this frame.
    //
    // emit_exit_cleanup emits only no-throw frame-exit cleanup (e.g. one instruction-stack pop). Use an
    // empty emitter for native-unwind mode: diagnostic unwinding must not maintain the instruction stack.
    // A caught Wasm handler continues in this function, so it MUST NOT run frame-exit cleanup yet.
    // The insertion point is left after begin_catch, ready for cold tag/payload dispatch.
    template<typename ExitCleanup>
    [[nodiscard]] inline catch_entry emit_typed_entry(::llvm::IRBuilder<>& builder, ::llvm::Function& function,
        symbols::declarations const& imports, catch_runtime const& runtime, ExitCleanup&& emit_exit_cleanup)
    {
        auto const module{function.getParent()}; // live owner of all declarations and blocks below
        if(module == nullptr || imports.status != symbols::error::ok || !runtime ||
           imports.type_info == nullptr || imports.personality == nullptr ||
           imports.type_info->getParent() != module || imports.personality->getParent() != module ||
           runtime.begin->getParent() != module || runtime.end->getParent() != module || runtime.rethrow->getParent() != module ||
           (function.hasPersonalityFn() && function.getPersonalityFn() != imports.personality)) { return {}; }
        function.setPersonalityFn(imports.personality);
        auto& context{function.getContext()};
        // [function-owned complete blocks] all returned pointers borrow the same live LLVM function.
        // [safe                         ] BasicBlock::Create publishes each block before builder use.
        auto const landing{::llvm::BasicBlock::Create(context, "guest.eh.landing", &function)};
        auto const caught{::llvm::BasicBlock::Create(context, "guest.eh.caught", &function)};
        auto const foreign{::llvm::BasicBlock::Create(context, "guest.eh.foreign", &function)};
        builder.SetInsertPoint(landing);
        auto const record_type{::llvm::StructType::get(context, {builder.getPtrTy(), builder.getInt32Ty()})};
        auto const record{builder.CreateLandingPad(record_type, 1u)};
        record->setCleanup(true);
        record->addClause(imports.type_info);
        // LLVM 23 makes eh.typeid.for pointer-overloaded (including address space); older supported
        // LLVM releases may use a fixed ptr signature. Supply its actual pointer type when required.
        ::llvm::SmallVector<::llvm::Type*, 1u> type_id_overloads{};
        if(::llvm::Intrinsic::isOverloaded(::llvm::Intrinsic::eh_typeid_for))
        { type_id_overloads.push_back(imports.type_info->getType()); }
        auto const type_id_function{::llvm::Intrinsic::getOrInsertDeclaration(module,
            ::llvm::Intrinsic::eh_typeid_for, type_id_overloads)};
        auto const type_id{builder.CreateCall(type_id_function, {imports.type_info})};
        auto const selector{builder.CreateExtractValue(record, 1u)};
        builder.CreateCondBr(builder.CreateICmpEQ(selector, type_id), caught, foreign);
        // [foreign complete block] builder cursor changes to its first insertion position.
        builder.SetInsertPoint(foreign);
        emit_exit_cleanup(builder);
        builder.CreateResume(record);
        // [caught complete block] selector checked before acquiring a typed native catch object.
        builder.SetInsertPoint(caught);
        auto const object{builder.CreateCall(runtime.begin, {builder.CreateExtractValue(record, 0u)})};
        object->setDoesNotThrow(); // Itanium __cxa_begin_catch does not throw.
        return {landing, caught, foreign, record, object};
    }

    inline void emit_end_catch(::llvm::IRBuilder<>& builder, catch_runtime const& runtime)
    {
        // Only guest_exception is caught here. Its noexcept destructor releases immutable owned values;
        // it cannot throw during end_catch. Do not put this attribute on unrelated C++ catch declarations.
        auto const call{builder.CreateCall(runtime.end)};
        call->setDoesNotThrow();
    }

    // Use this unwind destination for a throwing cold helper entered after begin_catch, and for
    // __cxa_rethrow when no Wasm handler in this frame matches. The NEW landingpad aggregate must be resumed: reusing the
    // original caught record would corrupt propagation when the helper threw a different exception.
    // Every path balances begin_catch once before retiring the current Wasm frame.
    template<typename ExitCleanup>
    [[nodiscard]] inline ::llvm::BasicBlock* emit_caught_exit_cleanup(::llvm::IRBuilder<>& builder,
        ::llvm::Function& function, catch_runtime const& runtime, ExitCleanup&& emit_exit_cleanup)
    {
        ::llvm::IRBuilder<>::InsertPointGuard saved{builder};
        auto& context{function.getContext()};
        // [function-owned complete cleanup block] builder borrows its live insertion cursor.
        auto const cleanup{::llvm::BasicBlock::Create(context, "guest.eh.caught.exit", &function)};
        builder.SetInsertPoint(cleanup);
        auto const record{builder.CreateLandingPad(::llvm::StructType::get(context,
            {builder.getPtrTy(), builder.getInt32Ty()}), 0u)};
        record->setCleanup(true);
        emit_end_catch(builder, runtime);
        emit_exit_cleanup(builder);
        builder.CreateResume(record);
        return cleanup;
    }

    inline void emit_rethrow(::llvm::IRBuilder<>& builder, catch_runtime const& runtime,
        ::llvm::BasicBlock* caught_exit_cleanup)
    {
        auto const function{builder.GetInsertBlock()->getParent()};
        // [function-owned unreachable successor] __cxa_rethrow has no normal return. Its invoke is
        // essential: a call followed by resume would skip end_catch and leak the active C++ catch.
        auto const never{::llvm::BasicBlock::Create(function->getContext(), "guest.eh.rethrow.never", function)};
        auto const invoke{builder.CreateInvoke(runtime.rethrow, never, caught_exit_cleanup, {})};
        invoke->setDoesNotReturn();
        builder.SetInsertPoint(never);
        builder.CreateUnreachable();
    }
}
#endif
