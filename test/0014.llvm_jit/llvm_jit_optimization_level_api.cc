// LLVM 22/early 23 use a class; stable 23.1 uses an enum. The full pipeline
// selects only these four levels. Keep the O2/O3 vectorization/unroll threshold
// independent of the removed getSpeedupLevel() member and enum representation.
#include <llvm/Passes/OptimizationLevel.h>
#include <cstdio>
#include <initializer_list>

int main()
{
    unsigned index{};
    for(auto level : {llvm::OptimizationLevel::O0, llvm::OptimizationLevel::O1,
                      llvm::OptimizationLevel::O2, llvm::OptimizationLevel::O3})
    {
        bool const speed{level == llvm::OptimizationLevel::O2 || level == llvm::OptimizationLevel::O3};
        if(speed != (index > 1u)) { return 1; }
        ++index;
    }
    std::puts("PASS: O0/O1 disabled; O2/O3 speed tuning enabled");
}
