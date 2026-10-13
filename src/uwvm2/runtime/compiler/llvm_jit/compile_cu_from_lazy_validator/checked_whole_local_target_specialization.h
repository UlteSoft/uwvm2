#pragma once
// Private compiler implementation, not a source/native/CFI capability. The
// sealed plan supplies dense ALL-local bindings from its actual same-walk
// records; this transform only operates on an unpublished owned LLVM module.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <vector>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Verifier.h>
#include <llvm/TargetParser/Triple.h>

namespace uwvm2::runtime::compiler::llvm_jit::checked_whole_local_targets
{
    inline constexpr char identity_name[]{"uwvm.checked.tiered.local.typed.target.v2"};
    struct function_binding
    {
        ::llvm::Function* typed{};
        ::llvm::Function* core{}; // Actual same owner hidden core, or null.
        ::std::uint64_t module_index{};
        ::std::uint64_t public_index{};
    };
    struct slot_binding
    {
        ::std::uint64_t address{}; // DATA; never dereferenced by this transform.
        ::llvm::StringRef symbol{}; // Exact original compiler-generated symbol.
        bool atomic{};
    };
    enum class status : unsigned { unchanged, transformed, rejected_before_mutation, rejected_after_verification };
    struct result { status state{status::rejected_before_mutation}; ::std::size_t replaced{}; };
    namespace details
    {
        [[nodiscard]] inline bool integer(::llvm::Metadata const* value, ::std::uint64_t& output) noexcept
        {
            auto const wrapper{::llvm::dyn_cast_or_null<::llvm::ConstantAsMetadata>(value)};
            auto const number{wrapper == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::ConstantInt>(wrapper->getValue())};
            if(number == nullptr || number->getBitWidth() != 64u) { return false; }
            output = number->getZExtValue(); return true;
        }
        [[nodiscard]] inline ::llvm::Function* function(::llvm::Metadata* value) noexcept
        {
            auto const wrapper{::llvm::dyn_cast_or_null<::llvm::ConstantAsMetadata>(value)};
            return wrapper == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::Function>(wrapper->getValue());
        }
        [[nodiscard]] inline bool exact_number(::llvm::Value const* value, unsigned bits, ::std::uint64_t expected) noexcept
        {
            auto const number{::llvm::dyn_cast_or_null<::llvm::ConstantInt>(value)};
            return number != nullptr && number->getBitWidth() == bits &&
                number->getValue() == ::llvm::APInt{bits, expected};
        }
        // Recognize actual existing source producers, not arbitrary host address
        // SSA. Unknown producers are not rewritten. No memory/slot is read here.
        [[nodiscard]] inline bool relocatable_cell_address(::llvm::CallInst const& call,
            ::llvm::GlobalVariable const& carrier, unsigned bits) noexcept
        {
            auto const assembly{::llvm::dyn_cast<::llvm::InlineAsm>(call.getCalledOperand())};
            auto const module{carrier.getParent()};
            auto const arch{module == nullptr ? ::llvm::Triple::UnknownArch :
                ::llvm::Triple{module->getTargetTriple()}.getArch()};
            return module != nullptr && call.getModule() == module && assembly != nullptr &&
                ((bits == 32u && arch == ::llvm::Triple::riscv32) || (bits == 64u && arch == ::llvm::Triple::riscv64)) &&
                call.arg_size() == 1u && call.getArgOperand(0u) == ::std::addressof(carrier) &&
                call.getType() == carrier.getType() && call.doesNotAccessMemory() && call.doesNotThrow() &&
                !call.isTailCall() && call.getCallingConv() == ::llvm::CallingConv::C &&
                !call.isConvergent() && !call.cannotDuplicate() &&
                !call.hasFnAttr(::llvm::Attribute::NoReturn) && !call.hasFnAttr(::llvm::Attribute::ReturnsTwice) &&
                !assembly->hasSideEffects() && !assembly->canThrow() && !assembly->isAlignStack() &&
                assembly->getDialect() == ::llvm::InlineAsm::AD_ATT && assembly->getAsmString() == "lla $0, $1" &&
                assembly->getConstraintString() == "=r,i" && assembly->getFunctionType() == call.getFunctionType() &&
                !assembly->getFunctionType()->isVarArg() && assembly->getFunctionType()->getNumParams() == 1u &&
                assembly->getFunctionType()->getParamType(0u) == carrier.getType();
        }
        [[nodiscard]] inline bool readonly_pointer_cell_load(::llvm::User const& user,
            ::llvm::Value const& location, ::llvm::GlobalVariable const& carrier, unsigned bits) noexcept
        {
            auto const reader{::llvm::dyn_cast<::llvm::LoadInst>(::std::addressof(user))};
            return reader != nullptr && reader->getPointerOperand() == ::std::addressof(location) &&
                reader->getModule() == carrier.getParent() && reader->getType() == carrier.getValueType() &&
                reader->isVolatile() && !reader->isAtomic() && reader->getAlign().value() >= bits / 8u;
        }
        [[nodiscard]] inline bool relocatable_base_matches(::llvm::LoadInst& loaded, ::llvm::Function& caller,
            slot_binding const& binding, unsigned bits, ::std::vector<::llvm::Instruction*>& auxiliaries,
            ::llvm::SmallPtrSetImpl<::llvm::GlobalVariable*>& checked_carriers)
        {
            auto const module{caller.getParent()};
            auto const location{loaded.getPointerOperand()};
            auto carrier{::llvm::dyn_cast<::llvm::GlobalVariable>(location)};
            auto const address{::llvm::dyn_cast<::llvm::CallInst>(location)};
            if(carrier == nullptr && address != nullptr && address->arg_size() == 1u)
            { carrier = ::llvm::dyn_cast<::llvm::GlobalVariable>(address->getArgOperand(0u)); }
            if(module == nullptr || loaded.getFunction() != ::std::addressof(caller) || carrier == nullptr ||
               carrier->getParent() != module || !carrier->hasPrivateLinkage() || !carrier->hasInitializer() ||
               carrier->isConstant() || carrier->isThreadLocal() || carrier->isExternallyInitialized() ||
               carrier->getAddressSpace() != 0u || carrier->getAlign().valueOrOne().value() < bits / 8u ||
               !carrier->getValueType()->isPointerTy() || carrier->getValueType()->getPointerAddressSpace() != 0u ||
               !readonly_pointer_cell_load(loaded, *location, *carrier, bits)) { return false; }
            auto const symbol{::llvm::dyn_cast<::llvm::GlobalVariable>(carrier->getInitializer())};
            if(symbol == nullptr || symbol->getParent() != module || !symbol->isDeclaration() ||
               !symbol->hasExternalLinkage() || symbol->isThreadLocal() || symbol->getAddressSpace() != 0u ||
               symbol->getName() != binding.symbol || !symbol->getValueType()->isIntegerTy(bits)) { return false; }
            if(address != nullptr && !relocatable_cell_address(*address, *carrier, bits)) { return false; }
            // The external initializer identifies the original table. It never
            // grants immutability: check every direct reader and every use of
            // the exact RISC-V address producer before replacing a tagged slot.
            // Linker-renamed private cells are safe only through this same
            // declaration and complete use proof, not through their name.
            if(checked_carriers.insert(carrier).second)
            {
                for(auto const& use: carrier->uses())
                {
                    if(readonly_pointer_cell_load(*use.getUser(), *carrier, *carrier, bits)) { continue; }
                    auto const producer{::llvm::dyn_cast<::llvm::CallInst>(use.getUser())};
                    if(producer == nullptr || !relocatable_cell_address(*producer, *carrier, bits)) { return false; }
                    for(auto const& address_use: producer->uses())
                    {
                        if(!readonly_pointer_cell_load(*address_use.getUser(), *producer, *carrier, bits)) { return false; }
                    }
                }
            }
            auxiliaries.push_back(::std::addressof(loaded));
            if(address != nullptr) { auxiliaries.push_back(address); }
            return true;
        }
        [[nodiscard]] inline bool base_matches(::llvm::Value* base, ::llvm::Function& caller,
            slot_binding const& binding, unsigned bits, ::std::vector<::llvm::Instruction*>& auxiliaries,
            ::llvm::SmallPtrSetImpl<::llvm::GlobalVariable*>& checked_carriers)
        {
            auto const module{caller.getParent()};
            if(base == nullptr || !base->getType()->isPointerTy() || module == nullptr ||
               module->getDataLayout().getPointerSizeInBits(base->getType()->getPointerAddressSpace()) != bits ||
               module->getDataLayout().isNonIntegralPointerType(base->getType())) { return false; }
            if(auto const loaded{::llvm::dyn_cast<::llvm::LoadInst>(base)}; loaded != nullptr)
            { return relocatable_base_matches(*loaded, caller, binding, bits, auxiliaries, checked_carriers); }
            if(auto const global{::llvm::dyn_cast<::llvm::GlobalVariable>(base)}; global != nullptr)
            {
                return global->getParent() == module && global->isDeclaration() &&
                    global->getName() == binding.symbol && global->getValueType()->isIntegerTy(bits) &&
                    global->getLinkage() == ::llvm::GlobalValue::ExternalLinkage;
            }
            if(auto const constant{::llvm::dyn_cast<::llvm::ConstantExpr>(base)}; constant != nullptr)
            {
                return constant->getOpcode() == ::llvm::Instruction::IntToPtr &&
                    exact_number(constant->getOperand(0u), bits, binding.address);
            }
            auto const pointer{::llvm::dyn_cast<::llvm::IntToPtrInst>(base)};
            if(pointer == nullptr || pointer->getFunction() != ::std::addressof(caller)) { return false; }
            if(auto const loaded{::llvm::dyn_cast<::llvm::LoadInst>(pointer->getOperand(0u))}; loaded != nullptr)
            {
                auto const global{::llvm::dyn_cast<::llvm::GlobalVariable>(loaded->getPointerOperand())};
                if(loaded->getFunction() != ::std::addressof(caller) || !loaded->getType()->isIntegerTy(bits) ||
                   !loaded->isVolatile() || loaded->isAtomic() || global == nullptr || global->getParent() != module ||
                   !global->hasPrivateLinkage() || !global->hasInitializer() ||
                   global->getUnnamedAddr() != ::llvm::GlobalValue::UnnamedAddr::Global ||
                   !global->getValueType()->isIntegerTy(bits) || !exact_number(global->getInitializer(), bits, binding.address) ||
                   loaded->getAlign().value() < bits / 8u) { return false; }
                // A private initializer alone is not immutability. Prove every
                // carrier use is an original readonly load; no store, alias,
                // address escape or indirect use can change its target bytes.
                // The dense set scans each shared carrier only once, avoiding
                // quadratic repeated validation on large admitted modules.
                if(checked_carriers.insert(global).second)
                {
                    for(auto const& use: global->uses())
                    {
                        auto const reader{::llvm::dyn_cast<::llvm::LoadInst>(use.getUser())};
                        if(reader == nullptr || reader->getPointerOperand() != global ||
                           reader->getModule() != module || !reader->getType()->isIntegerTy(bits) ||
                           !reader->isVolatile() || reader->isAtomic()) { return false; }
                    }
                }
                auxiliaries.push_back(pointer); auxiliaries.push_back(loaded); return true;
            }
            auto const call{::llvm::dyn_cast<::llvm::CallInst>(pointer->getOperand(0u))};
            auto const assembly{call == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::InlineAsm>(call->getCalledOperand())};
            if(call == nullptr || assembly == nullptr || bits != 64u ||
               ::llvm::Triple{module->getTargetTriple()}.getArch() != ::llvm::Triple::riscv64 ||
               call->getFunction() != ::std::addressof(caller) || call->arg_size() != 1u ||
               !call->getType()->isIntegerTy(64u) || !exact_number(call->getArgOperand(0u), 64u, binding.address) ||
               !call->doesNotAccessMemory() || !call->doesNotThrow() || call->isTailCall() ||
               call->getCallingConv() != ::llvm::CallingConv::C || call->isConvergent() || call->cannotDuplicate() ||
               call->hasFnAttr(::llvm::Attribute::NoReturn) || call->hasFnAttr(::llvm::Attribute::ReturnsTwice) ||
               assembly->hasSideEffects() || assembly->canThrow() || assembly->isAlignStack() ||
               assembly->getDialect() != ::llvm::InlineAsm::AD_ATT || assembly->getAsmString() != "li $0, $1" ||
               assembly->getConstraintString() != "=r,i" || assembly->getFunctionType() != call->getFunctionType() ||
               assembly->getFunctionType()->isVarArg() || assembly->getFunctionType()->getNumParams() != 1u ||
               !assembly->getFunctionType()->getParamType(0u)->isIntegerTy(64u)) { return false; }
            auxiliaries.push_back(pointer); auxiliaries.push_back(call); return true;
        }
        [[nodiscard]] inline bool typed_calls(::llvm::IntToPtrInst const& pointer, ::llvm::Function const& target,
            ::llvm::BasicBlock const& fast) noexcept
        {
            if(pointer.getType() != target.getType() || pointer.getParent() != ::std::addressof(fast) || pointer.use_empty()) { return false; }
            for(auto const& use: pointer.uses())
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(use.getUser())};
                if(call == nullptr || !call->isCallee(::std::addressof(use)) || call->getParent() != ::std::addressof(fast) ||
                   call->getFunctionType() != target.getFunctionType() || call->getCallingConv() != target.getCallingConv() ||
                   call->getAttributes().getRetAttrs() != target.getAttributes().getRetAttrs() ||
                   call->arg_size() != target.arg_size()) { return false; }
                for(unsigned index{}; index != call->arg_size(); ++index)
                {
                    // Bounds index<actual arguments precede each ABI attribute access.
                    if(call->getAttributes().getParamAttrs(index) != target.getAttributes().getParamAttrs(index)) { return false; }
                }
            }
            return true;
        }
        [[nodiscard]] inline bool complete_dispatch_uses(::llvm::LoadInst const& load, ::llvm::Function const& target) noexcept
        {
            ::llvm::ICmpInst const* guard{};
            for(auto const& use: load.uses())
            {
                auto const candidate{::llvm::dyn_cast<::llvm::ICmpInst>(use.getUser())};
                if(candidate == nullptr) { continue; }
                if(guard != nullptr || candidate->getParent() != load.getParent() ||
                   (candidate->getPredicate() != ::llvm::CmpInst::ICMP_NE && candidate->getPredicate() != ::llvm::CmpInst::ICMP_EQ) ||
                   !candidate->hasOneUse()) { return false; }
                auto const other{candidate->getOperand(0u) == ::std::addressof(load) ? candidate->getOperand(1u) : candidate->getOperand(0u)};
                auto const zero{::llvm::dyn_cast<::llvm::ConstantInt>(other)};
                if(zero == nullptr || !zero->isZero() || zero->getType() != load.getType()) { return false; }
                guard = candidate;
            }
            if(guard == nullptr) { return false; }
            auto const branch{::llvm::dyn_cast<::llvm::BranchInst>(*guard->user_begin())};
            if(branch == nullptr || !branch->isConditional() || branch->getCondition() != guard ||
               branch->getParent() != load.getParent()) { return false; }
            auto const fast{branch->getSuccessor(guard->getPredicate() == ::llvm::CmpInst::ICMP_NE ? 0u : 1u)};
            if(fast == branch->getSuccessor(guard->getPredicate() == ::llvm::CmpInst::ICMP_NE ? 1u : 0u) ||
               fast->getParent() != load.getFunction()) { return false; }
            bool target_seen{};
            for(auto const& use: load.uses())
            {
                if(use.getUser() == guard) { continue; }
                auto const pointer{::llvm::dyn_cast<::llvm::IntToPtrInst>(use.getUser())};
                if(pointer == nullptr || !typed_calls(*pointer, target, *fast)) { return false; }
                target_seen = true;
            }
            return target_seen; // Unknown escapes/PHIs/tail resolver paths are refused.
        }
    }
    // ALL preflight is read-only. Rejection-before leaves the module unchanged;
    // rejection-after requires discarding the unpublished derivative. This does
    // not authorize a native address, mutable ref/table slot, or foreign import.
    [[nodiscard]] inline result specialize(::llvm::Module& module,
        ::llvm::ArrayRef<function_binding> functions, slot_binding const& slots)
    {
        constexpr ::std::size_t maximum_functions{65536uz}, maximum_replacements{262144uz};
        auto const& layout{module.getDataLayout()};
        if(functions.size() > maximum_functions || layout.isDefault() || slots.address == 0u || slots.symbol.empty() ||
           ::llvm::verifyModule(module)) { return {}; }
        auto const bits{layout.getPointerSizeInBits(0u)};
        if((bits != 32u && bits != 64u) || (bits == 32u && slots.address > (::std::numeric_limits<::std::uint32_t>::max)())) { return {}; }
        if(!functions.empty() && functions[0uz].public_index >
           (::std::numeric_limits<::std::uint64_t>::max)() - (functions.size() - 1uz)) { return {}; }
        for(::std::size_t index{}; index != functions.size(); ++index)
        {
            // [factory ALL binding0 ... index ... count] end
            // [safe] index<count BEFORE dense identity/definition access.
            auto const& binding{functions[index]}; auto const target{binding.typed};
            if(target == nullptr || target->getParent() != ::std::addressof(module) || target->isDeclaration() || target->isIntrinsic() ||
               target->hasExternalWeakLinkage() || target->hasAvailableExternallyLinkage() ||
               target->getAddressSpace() != 0u || layout.isNonIntegralPointerType(target->getType()) ||
               binding.module_index != functions[0uz].module_index || binding.public_index != functions[0uz].public_index + index ||
               (binding.core != nullptr && (binding.core->getParent() != ::std::addressof(module) || binding.core->isDeclaration() ||
                    !binding.core->hasInternalLinkage() || binding.core->isIntrinsic()))) { return {}; }
        }
        struct replacement { ::llvm::LoadInst* load{}; ::llvm::Constant* target{}; };
        ::std::vector<replacement> pending{};
        ::std::vector<::llvm::Instruction*> auxiliaries{};
        ::llvm::SmallPtrSet<::llvm::GlobalVariable*, 16u> checked_carriers{};
        for(auto& function: module) for(auto& block: function) for(auto& instruction: block)
        {
            auto const identity{instruction.getMetadata(identity_name)};
            if(identity == nullptr) { continue; }
            auto const load{::llvm::dyn_cast<::llvm::LoadInst>(::std::addressof(instruction))};
            ::std::uint64_t module_id{}, caller_index{}, target_index{}, local_index{}, address{};
            if(load == nullptr || pending.size() == maximum_replacements || identity->getNumOperands() != 7u ||
               !load->getType()->isIntegerTy(bits) || !load->isVolatile() || load->isAtomic() != slots.atomic ||
               (slots.atomic && load->getOrdering() != ::llvm::AtomicOrdering::Acquire) ||
               !details::integer(identity->getOperand(0u).get(), module_id) || functions.empty() || module_id != functions[0uz].module_index ||
               !details::integer(identity->getOperand(1u).get(), caller_index) || caller_index < functions[0uz].public_index ||
               caller_index - functions[0uz].public_index >= functions.size() ||
               !details::integer(identity->getOperand(2u).get(), target_index) ||
               !details::integer(identity->getOperand(3u).get(), local_index) || local_index >= functions.size() ||
               target_index != functions[0uz].public_index + local_index ||
               !details::integer(identity->getOperand(4u).get(), address) || address != slots.address) { return {}; }
            // [dense ALL owned bindings0 ... count] end
            // [safe] both u64 indices were bounded BEFORE narrowing/indexing.
            auto const& caller{functions[static_cast<::std::size_t>(caller_index - functions[0uz].public_index)]};
            auto const& target{functions[static_cast<::std::size_t>(local_index)]};
            if((::std::addressof(function) != caller.typed && ::std::addressof(function) != caller.core) ||
               details::function(identity->getOperand(5u).get()) != target.typed ||
               details::function(identity->getOperand(6u).get()) != ::std::addressof(function) ||
               load->getAlign().value() < layout.getPointerABIAlignment(0u).value() ||
               !details::complete_dispatch_uses(*load, *target.typed)) { return {}; }
            auto pointer{load->getPointerOperand()};
            if(auto const gep{::llvm::dyn_cast<::llvm::GEPOperator>(pointer)}; gep != nullptr)
            {
                if(!gep->isInBounds() || gep->getNumIndices() != 1u || gep->getSourceElementType() != load->getType() ||
                   !details::exact_number(gep->getOperand(1u), bits, local_index)) { return {}; }
                if(auto const actual{::llvm::dyn_cast<::llvm::Instruction>(pointer)}; actual != nullptr)
                { if(actual->getFunction() != ::std::addressof(function)) { return {}; } auxiliaries.push_back(actual); }
                pointer = gep->getPointerOperand(); // Checked exact one-index GEP BEFORE advancing to its base.
            }
            else if(local_index != 0u) { return {}; } // IRBuilder may fold a zero-index GEP to the base.
            if(!details::base_matches(pointer, function, slots, bits, auxiliaries, checked_carriers)) { return {}; }
            auto const address_value{::llvm::ConstantExpr::getPtrToInt(target.typed, load->getType())};
            if(address_value == nullptr || address_value->getType() != load->getType()) { return {}; }
            pending.push_back({load, address_value});
        }
        if(pending.empty()) { return {status::unchanged, 0uz}; }
        // Deduplicate while all objects remain alive. Never compare dangling
        // pointers or erase an auxiliary shared by another original dispatch.
        ::std::sort(auxiliaries.begin(), auxiliaries.end(), ::std::less<::llvm::Instruction*>{});
        auxiliaries.erase(::std::unique(auxiliaries.begin(), auxiliaries.end()), auxiliaries.end());
        for(auto const replacement: pending)
        {
            replacement.load->replaceAllUsesWith(replacement.target);
            replacement.load->eraseFromParent();
        }
        // At most three layers (GEP -> pointer -> original address producer).
        // use_empty preserves any auxiliary still used by another dispatch.
        for(unsigned pass{}; pass != 3u; ++pass) for(auto& auxiliary: auxiliaries)
        {
            if(auxiliary != nullptr && auxiliary->use_empty()) { auxiliary->eraseFromParent(); auxiliary = nullptr; }
        }
        if(::llvm::verifyModule(module)) { return {status::rejected_after_verification, pending.size()}; }
        return {status::transformed, pending.size()};
    }
}
