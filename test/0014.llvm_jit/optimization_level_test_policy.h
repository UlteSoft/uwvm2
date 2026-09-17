#pragma once
#include <llvm/IR/Function.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <array>

namespace uwvm2test
{
    struct optimization_test_policy
    {
        llvm::OptimizationLevel level;
        unsigned size_level;
    };

    template <typename Level = llvm::OptimizationLevel>
    constexpr auto optimization_test_policies()
    {
        // LLVM 23.1 removed Os/Oz from OptimizationLevel. Its PassBuilder
        // diagnostic prescribes O2 plus optsize/minsize function attributes.
        // Probe the API, not LLVM_VERSION_MAJOR: early LLVM 23 git builds
        // still expose the old class. Keep all five actual policies tested.
        if constexpr(requires { Level::Os; Level::Oz; })
        {
            return std::array{optimization_test_policy{Level::O1, 0},
                optimization_test_policy{Level::O2, 0}, optimization_test_policy{Level::O3, 0},
                optimization_test_policy{Level::Os, 1}, optimization_test_policy{Level::Oz, 2}};
        }
        else
        {
            return std::array{optimization_test_policy{Level::O1, 0},
                optimization_test_policy{Level::O2, 0}, optimization_test_policy{Level::O3, 0},
                optimization_test_policy{Level::O2, 1}, optimization_test_policy{Level::O2, 2}};
        }
    }

    inline void apply_size_test_policy(llvm::Function& function, unsigned size_level)
    {
        // MinSize does not replace OptimizeForSize: apply both for the Oz
        // policy, and annotate caller/callee alike before running the pipeline.
        // Otherwise a nominal "size" test can silently test ordinary O2 only.
        if(size_level != 0) { function.addFnAttr(llvm::Attribute::OptimizeForSize); }
        if(size_level == 2) { function.addFnAttr(llvm::Attribute::MinSize); }
    }
}
