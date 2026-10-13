#pragma once

#include <utility>
#include <llvm/Config/llvm-config.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils/PromoteMemToReg.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

namespace uwvm2::runtime::compiler::llvm_jit::details
{
    // A compiler-private optimization hint, never native/root/restore authority.
    // The complete numeric copier admits small numeric functions by marking
    // their actual declared locals. In low GP pressure functions LLVM can
    // also promote internal entry temporaries; excluding them loses SSA gains.
    // LLVM proves that every promoted slot is nonvolatile and nonescaping.
    [[nodiscard]] inline unsigned promote_runtime_llvm_jit_debug_numeric_locals_impl(::llvm::Function& function)
    {
        if(function.isDeclaration()) { return 0u; }
        ::llvm::SmallVector<::llvm::AllocaInst*,8u> locals{};
        ::llvm::SmallVector<::llvm::AllocaInst*,8u> entry_slots{};
        ::llvm::SmallVector<::llvm::AllocaInst*,8u> dynamic_gp{};
        unsigned hot_gp_count{};
        bool admitted{};
        for(auto& instruction : function.getEntryBlock())
        {
            auto const local{::llvm::dyn_cast<::llvm::AllocaInst>(::std::addressof(instruction))};
            if(local == nullptr) { continue; }
            auto const type{local->getAllocatedType()};
            bool const gp{type->isIntegerTy(32u) || type->isIntegerTy(64u)};
            if(gp && local->getMetadata("uwvm.debug.numeric.mutable.gp") != nullptr) { ++hot_gp_count; }
            admitted = admitted || local->getMetadata("uwvm.debug.numeric.local") != nullptr;
            // The actual observer entry emitter keeps GP argument storage
            // when SSA would add per-call callee-saved register traffic.
            // This private storage hint creates no capture/root authority.
            if(gp && local->getMetadata("uwvm.debug.entry.stack.gp") != nullptr) { continue; }
            if(!::llvm::isAllocaPromotable(local)) { continue; }
            entry_slots.push_back(local);
            if(local->getMetadata("uwvm.debug.numeric.local") == nullptr) { continue; }
            locals.push_back(local);
            if(!gp) { continue; }
            for(auto const user : local->users())
            {
                auto const store{::llvm::dyn_cast<::llvm::StoreInst>(user)};
                if(store != nullptr && store->getPointerOperand() == local &&
                   !::llvm::isa<::llvm::Constant>(store->getValueOperand()))
                { dynamic_gp.push_back(local); break; }
            }
        }
        // Cold complete captures keep all local values live at every site.
        // Count actual parameters and nonconstant Wasm local.set/local.tee,
        // not cold ABI/checkpoint restores of otherwise unused locals. Those
        // restores still participate in LLVM's complete promotability proof.
        // Promoting dense mutable GP sets increases native register pressure.
        // Keep those slots on the stack; FP/vector and constant locals can
        // still benefit from their separate register bank/constant folding.
        if(!admitted) { return 0u; }
        if(hot_gp_count <= 2u) { locals = ::std::move(entry_slots); }
        else
        {
            ::llvm::SmallVector<::llvm::AllocaInst*,8u> selected{};
            for(auto const local : locals)
            {
                bool dynamic{};
                for(auto const gp : dynamic_gp) { if(local == gp) { dynamic = true; break; } }
                if(!dynamic) { selected.push_back(local); }
            }
            locals = ::std::move(selected);
        }
        if(locals.empty()) { return 0u; }
        auto const count{static_cast<unsigned>(locals.size())};
        ::llvm::DominatorTree dominators{function};
        ::llvm::PromoteMemToReg(locals,dominators);
        return count;
    }

    // Selected only by the existing observation pipeline on actual native
    // functions. Scalar combines can leave poison Invoke terminators behind
    // after deleting calls in a constant-dead block. Fold constant terminators
    // and remove complete unreachable blocks so safepoints and their paired
    // cancellation Invokes remain consistent. No inlining, loop motion or
    // general CFG merging is added. Opaque calls and escaping snapshot/root
    // slots keep LLVM memory semantics; optnone/other backends stay excluded.
    [[nodiscard]] inline bool simplify_runtime_llvm_jit_debug_scalar_ir(::llvm::Function& function)
    {
#if defined(__linux__) && defined(__x86_64__)
        auto const module{function.getParent()};
        if(module == nullptr || function.isDeclaration() ||
           !function.hasFnAttribute("uwvm.native.debug.pcrel") ||
           function.hasFnAttribute(::llvm::Attribute::OptimizeNone) ||
           module->getDataLayout().isDefault() || !module->getDataLayout().isLittleEndian() ||
           module->getDataLayout().getPointerSizeInBits() != 64u ||
#if LLVM_VERSION_MAJOR >= 21
           module->getTargetTriple().getArch() != ::llvm::Triple::x86_64 ||
           !module->getTargetTriple().isOSLinux()) { return false; }
#else
           ::llvm::Triple{module->getTargetTriple()}.getArch() != ::llvm::Triple::x86_64 ||
           !::llvm::Triple{module->getTargetTriple()}.isOSLinux()) { return false; }
#endif
        ::llvm::legacy::FunctionPassManager passes{module};
        passes.add(::llvm::createEarlyCSEPass());
        passes.add(::llvm::createInstructionCombiningPass());
        passes.doInitialization();
        auto const changed{passes.run(function)};
        passes.doFinalization();
        bool folded{};
        for(auto& block : function) { folded = ::llvm::ConstantFoldTerminator(::std::addressof(block), false) || folded; }
        auto const pruned{::llvm::EliminateUnreachableBlocks(function)};
        return changed || folded || pruned;
#else
        (void)function; return false;
#endif
    }

    [[nodiscard]] inline unsigned promote_runtime_llvm_jit_debug_numeric_locals(::llvm::Function& function)
    {
        auto const promoted{promote_runtime_llvm_jit_debug_numeric_locals_impl(function)};
        static_cast<void>(simplify_runtime_llvm_jit_debug_scalar_ir(function));
        return promoted;
    }
}
