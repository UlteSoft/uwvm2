// Inspect actual LLVM ARM target construction. This does not claim that the
// Linux EHABI sysroot supplies an ARM DWARF unwinder or that a full ARM JIT ran.
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Triple.h>
#include <memory>
#include <string>
#include <type_traits>

// Model the host compile-time ABI only after platform/system headers. The
// generated target below is a real ARM TargetMachine in either test variant.
#ifdef UWVM_TEST_ARM_DWARF
# define __arm__ 1
# define __ARM_DWARF_EH__ 1
#endif
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#ifdef UWVM_TEST_ARM_DWARF
# undef __ARM_DWARF_EH__
# undef __arm__
#endif

template<typename info_type>
auto exception_model(info_type&& info)
{
    if constexpr(std::is_pointer_v<std::remove_cvref_t<info_type>>) { return info->getExceptionHandlingType(); }
    else { return info.getExceptionHandlingType(); }
}

int main()
{
    LLVMInitializeARMTargetInfo();
    LLVMInitializeARMTarget();
    LLVMInitializeARMTargetMC();
    llvm::EngineBuilder builder{};
    uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_configure_host_unwind_abi(builder);
    llvm::SmallVector<std::string, 1> attributes{};
    std::unique_ptr<llvm::TargetMachine> target{builder.selectTarget(
        llvm::Triple{"armv7-unknown-linux-gnueabihf"}, "", "cortex-a15", attributes)};
    if(!target) { return 1; }
#ifdef UWVM_TEST_ARM_DWARF
    return exception_model(target->getMCAsmInfo()) != llvm::ExceptionHandling::DwarfCFI;
#else
    return exception_model(target->getMCAsmInfo()) != llvm::ExceptionHandling::ARM;
#endif
}
