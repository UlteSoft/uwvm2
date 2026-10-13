// Inventory only: no foreign machine code is emitted or executed here.
#include <fast_io.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Triple.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include "target_census_generated.h"

int main()
{
    initialize_census_target_infos();
    for(auto const& target : ::llvm::TargetRegistry::targets())
    {
        ::fast_io::println("registration\t", ::fast_io::mnp::os_c_str(target.getName()), "\t", target.hasJIT() ? 1 : 0);
    }
    for(auto const& row : census_profiles)
    {
        // Clang accepts arch-os-env shorthands; Triple's constructor alone
        // does not shift the components. Normalize before checking ABI gates.
        ::llvm::Triple const triple{::llvm::Triple::normalize(row.triple)};
        auto const arch{triple.getArch()};
        auto const format{triple.getObjectFormat()};
        ::fast_io::println("gate\t", ::fast_io::mnp::os_c_str(row.name), "\t", static_cast<unsigned>(arch), "\t",
            static_cast<unsigned>(format), "\t",
            ::uwvm2::runtime::compiler::llvm_jit::details::llvm_jit_mcjit_object_format_supported(triple) ? 1 : 0);
    }
}
