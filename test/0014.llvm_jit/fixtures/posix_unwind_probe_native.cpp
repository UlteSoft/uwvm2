// Compile and execute the actual shared POSIX probe implementation fragment.
// Native-target selection is a small adapter here (its exact production bodies
// have separate full/lazy tests). Function attributes and section registration
// use the real translator/section-manager headers, not mocked unwind metadata.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <unwind.h>

namespace
{
    using llvm_module_owner_t = std::unique_ptr<llvm::Module>;
    using llvm_jit_memory_manager_owner_t = std::unique_ptr<llvm::RTDyldMemoryManager>;
    bool ensure_llvm_jit_native_target_initialized()
    { return !llvm::InitializeNativeTarget() && !llvm::InitializeNativeTargetAsmPrinter(); }
    auto const& get_llvm_jit_host_cpu_name_storage()
    {
        static uwvm2::utils::container::u8string const cpu{u8"generic"};
        return cpu;
    }
    auto const& get_llvm_jit_host_target_attribute_storage()
    {
        static uwvm2::utils::container::vector<uwvm2::utils::container::u8string> const features{};
        return features;
    }
    auto select_runtime_llvm_jit_target(llvm::EngineBuilder& builder,
        uwvm2::utils::container::u8string const&, uwvm2::utils::container::vector<uwvm2::utils::container::u8string> const&)
    { return uwvm2::utils::container::delete_owned_ptr<llvm::TargetMachine>{builder.selectTarget()}; }
    void set_llvm_module_target_triple_from_machine(llvm::Module& module, llvm::TargetMachine const& target)
    { module.setTargetTriple(target.getTargetTriple()); }
#undef UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE
#define UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE 1
#include <uwvm2/runtime/lib/uwvm_runtime_posix_unwind_probe.h>
}

int main()
{
    for(unsigned lifetime{}; lifetime != 20; ++lifetime)
    {
        if(!runtime_llvm_jit_posix_live_unwind_probe()) { return 1; }
    }
    std::puts("20 actual probe lifetimes: each recovered three recursive frames and one root");
}
