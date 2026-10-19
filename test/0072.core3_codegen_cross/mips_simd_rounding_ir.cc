// Compile against LLVM and the exact extracted production emitter struct.
// This component does not execute or inspect VM functions.
#include <fast_io.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Verifier.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>
#include <cstddef>
#include <cstdint>
#include "production-emitter.inc"

class output_stream final : public llvm::raw_ostream
{
    fast_io::native_file file;
    std::uint64_t position{};
    void write_impl(char const* p, std::size_t n) override
    {
        fast_io::io::print(file, fast_io::mnp::strvw(p, p + n));
        position += n;
    }
    std::uint64_t current_pos() const override { return position; }
public:
    explicit output_stream(char const* path) : llvm::raw_ostream(true),
        file(fast_io::mnp::os_c_str(path), fast_io::open_mode::out | fast_io::open_mode::excl) {}
};

int main()
{
    char const* const names[2][2][4]{
        {{"native32_ceil", "native32_floor", "native32_trunc", "native32_nearest"},
         {"native64_ceil", "native64_floor", "native64_trunc", "native64_nearest"}},
        {{"bits32_ceil", "bits32_floor", "bits32_trunc", "bits32_nearest"},
         {"bits64_ceil", "bits64_floor", "bits64_trunc", "bits64_nearest"}}};
    llvm::Intrinsic::ID const operations[]{llvm::Intrinsic::ceil, llvm::Intrinsic::floor,
                                         llvm::Intrinsic::trunc, llvm::Intrinsic::roundeven};
    for(bool little : {false, true})
    {
        llvm::LLVMContext context;
        llvm::Module module("exact-production-mips-rounding", context);
        module.setTargetTriple(llvm::Triple{little ? "mips64el-unknown-linux-gnuabi64" : "mips64-unknown-linux-gnuabi64"});
        module.setDataLayout(little ? "e-m:m-i64:64-n32:64-S128" : "E-m:m-i64:64-n32:64-S128");
        auto ptr{llvm::PointerType::get(context, 0)};
        auto bytes{llvm::FixedVectorType::get(llvm::Type::getInt8Ty(context), 16)};
        for(unsigned variant{}; variant != 2u; ++variant)
        for(unsigned width{}; width != 2u; ++width)
        for(unsigned op{}; op != 4u; ++op)
        {
            auto fn{llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(context), {ptr, ptr}, false),
                                           llvm::GlobalValue::ExternalLinkage, names[variant][width][op], module)};
            fn->addFnAttr("target-cpu", "mips64r2");
            fn->addFnAttr("target-features", "-msa,-soft-float,-single-float");
            llvm::IRBuilder<> builder{llvm::BasicBlock::Create(context, "entry", fn)};
            auto input{builder.CreateAlignedLoad(bytes, fn->getArg(0), llvm::Align{1})};
            simd_ir::emitter emitter{builder};
            auto result{variant == 0u ? emitter.round_mips64_scalar(input, width == 0u ? 32u : 64u, operations[op]) :
                                       emitter.round_bits(input, width == 0u ? 32u : 64u, operations[op])};
            builder.CreateAlignedStore(result, fn->getArg(1), llvm::Align{1});
            builder.CreateRetVoid();
        }
        if(llvm::verifyModule(module)) { return 1; }
        output_stream output{little ? "little.ll" : "big.ll"};
        module.print(output, nullptr);
    }
    fast_io::println("PASS exact production native/integer rounding IR verified");
}
