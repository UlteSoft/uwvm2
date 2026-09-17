// Exercise address decisions for memory32/ISA32, memory32/ISA64, and the future memory64/ISA64 helper.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Passes/PassBuilder.h>
#include <cstdio>
#include <string_view>
#include <vector>

namespace d = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
using protection = d::llvm_jit_memory_protection;
#if defined(UWVM_SUPPORT_MMAP)
constexpr auto guard_width{uwvm2::object::memory::linear::mmap_guard_max_access_size};
#else
constexpr std::size_t guard_width{};
#endif
struct test_case
{
    llvm::Function* function;
    unsigned address_bits;
    std::uint64_t offset;
    unsigned width;
    protection mode;
    std::uint64_t partial;
};

struct reference_address
{
    std::uint64_t offset;
    bool overflow;
};

// Independent two-limb arithmetic: each addition fits in 33 bits, including
// memory64 carries. Do not duplicate the emitter's wrapped 64-bit addition.
[[nodiscard]] reference_address mathematical_address(test_case const& test, std::uint64_t address)
{
    constexpr std::uint64_t mask{0xffffffffull};
    auto const dynamic{test.address_bits == 32u ? address & mask : address};
    auto const low{(dynamic & mask) + (test.offset & mask)};
    auto const high{(dynamic >> 32u) + (test.offset >> 32u) + (low >> 32u)};
    return {((high & mask) << 32u) | (low & mask),
            test.address_bits == 32u ? high != 0u : high > mask};
}

struct reference_outcome
{
    std::uint64_t offset;
    bool trapped;
};

[[nodiscard]] reference_outcome reference_result(test_case const& test, std::uint64_t address, std::uint64_t length)
{
    auto const effective{mathematical_address(test, address)};
    bool const checked{test.mode == protection::software ||
        (test.mode == protection::partial_guard && (effective.overflow || effective.offset >= test.partial))};
    if(checked && (effective.overflow || length < test.width || effective.offset > length - test.width)) { return {~0ull, true}; }
    return {effective.offset, false};
}

void optimize_module(llvm::Module& module)
{
    llvm::PassBuilder passes;
    llvm::LoopAnalysisManager loops; llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager calls; llvm::ModuleAnalysisManager modules;
    passes.registerModuleAnalyses(modules); passes.registerCGSCCAnalyses(calls);
    passes.registerFunctionAnalyses(functions); passes.registerLoopAnalyses(loops);
    passes.crossRegisterProxies(loops, functions, calls, modules);
    auto pipeline{passes.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3)};
    pipeline.run(module, modules);
}

int main(int argc, char** argv)
{
    bool const optimize{argc == 2 && std::string_view{argv[1]} == "--optimize-ir"};
    if(argc != 1 && !optimize)
    { std::fputs("usage: memory_direct_lowering [--optimize-ir]\n", stderr); return 64; }
    llvm::InitializeNativeTarget(); llvm::InitializeNativeTargetAsmPrinter();
    llvm::LLVMContext context;
    {
        // Pin the zero-overhead aligned-store guarantee: optimize the actual production preflight, then reject
        // any remaining probe, length load, helper or conditional branch. memarg.align alone is NOT this proof.
        llvm::Module aligned{"guard-aligned-store", context}; aligned.setDataLayout("e-p:64:64");
        llvm::IRBuilder<> b{context};
        auto ptr{llvm::PointerType::getUnqual(context)};
        auto vector{llvm::FixedVectorType::get(b.getInt8Ty(), 16u)};
        auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(), {ptr, b.getInt32Ty(), vector}, false),
            llvm::Function::ExternalLinkage, "aligned_store", aligned)};
        b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
        auto address{b.CreateZExt(b.CreateAnd(fn->getArg(1), b.getInt32(0xfffffff0u)), b.getInt64Ty())};
        auto pointer{b.CreateGEP(b.getInt8Ty(), fn->getArg(0), address)};
        d::emit_llvm_jit_guarded_store_preflight(b, pointer, address, 16u, 16u);
        static_cast<void>(d::finalize_llvm_jit_direct_memory_store(b.CreateStore(fn->getArg(2), pointer), llvm::Align{1}));
        b.CreateRetVoid();
        optimize_module(aligned);
        if(llvm::verifyModule(aligned, &llvm::errs()) || fn->size() != 1u) { return 7; }
        for(auto const& block: *fn) for(auto const& instruction: block)
        { if(llvm::isa<llvm::LoadInst>(instruction) || llvm::isa<llvm::CallInst>(instruction) || llvm::isa<llvm::ICmpInst>(instruction)) { return 8; } }
    }
    auto module{std::make_unique<llvm::Module>("direct-memory-address", context)};
    std::vector<test_case> cases;
    std::size_t full_guard_configurations{};
    for(unsigned pointer_bits: {32u, 64u}) for(unsigned address_bits: {32u, 64u})
    {
        if(address_bits > pointer_bits) { continue; }
        module->setDataLayout(pointer_bits == 32 ? "e-p:32:32" : "e-p:64:64");
        for(auto mode: {protection::software, protection::partial_guard, protection::full_wasm32_guard})
        for(unsigned width: {1u, 2u, 4u, 8u, 16u, 64u, 65u})
        for(std::uint64_t offset: {0ull, 1ull, 0xffffffffull, 0xfffffffffffffff0ull})
        {
            if(address_bits == 32 && offset > 0xffffffffull) { continue; }
            llvm::IRBuilder<> b{context};
            auto fn{llvm::Function::Create(llvm::FunctionType::get(b.getInt64Ty(),
                {b.getInt64Ty(), b.getInt64Ty(), llvm::PointerType::getUnqual(context)}, false),
                llvm::Function::ExternalLinkage, "address_" + std::to_string(cases.size()), *module)};
            b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", fn));
            unsigned length_reads{};
            std::uint64_t partial{pointer_bits == 32 ? 1ull << 28 : 1ull << 40};
            auto pointer{d::emit_llvm_jit_memory_address(b, llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context)),
                b.CreateZExtOrTrunc(fn->getArg(0), b.getIntNTy(address_bits)), offset, width, mode, partial,
                [&]() -> llvm::Value* { ++length_reads; return fn->getArg(1); })};
            auto gep{llvm::dyn_cast_or_null<llvm::GetElementPtrInst>(pointer)};
            if(!gep || gep->isInBounds()) { return 1; }
            b.CreateRet(b.CreateZExtOrTrunc(gep->getOperand(1), b.getInt64Ty()));
            auto actual{mode};
            if(width > guard_width || (mode == protection::full_wasm32_guard && (pointer_bits != 64 || address_bits != 32)))
            { actual = protection::software; }
            if(actual == protection::full_wasm32_guard)
            {
                ++full_guard_configurations;
                if(length_reads || fn->size() != 1) { return 2; }
                for(auto& block: *fn) for(auto& ins: block)
                { if(llvm::isa<llvm::ICmpInst>(ins) || llvm::isa<llvm::CallInst>(ins) || llvm::isa<llvm::LoadInst>(ins)) { return 3; } }
            }
            // Replace only the fatal-report blocks. A separate trap byte is essential: UINT64_MAX alone can also
            // be an effective-address value, so a sentinel-only comparison could miss an omitted bounds trap.
            // Production branch conditions remain untouched; no linear-memory pointer is dereferenced here.
            for(auto& block: *fn)
            {
                if(!llvm::isa<llvm::UnreachableInst>(block.getTerminator())) { continue; }
                while(!block.empty()) { block.back().eraseFromParent(); }
                b.SetInsertPoint(&block); b.CreateStore(b.getInt8(1), fn->getArg(2)); b.CreateRet(b.getInt64(~0ull));
            }
            cases.push_back({fn, address_bits, offset, width, actual, partial});
        }
    }
    if(llvm::verifyModule(*module, &llvm::errs())) { return 4; }
    // The probes return integers and only write the test-owned trap flag, never dereferencing a linear-memory pointer.
    // Use the native target for executing the already
    // emitted ISA32/ISA64 decision trees (their explicit widening/truncation is preserved).
    module->setDataLayout("");
    if(optimize)
    {
        optimize_module(*module);
        if(llvm::verifyModule(*module, &llvm::errs())) { return 9; }
    }
    std::string error;
    std::unique_ptr<llvm::ExecutionEngine> engine{llvm::EngineBuilder(std::move(module)).setErrorStr(&error)
        .setEngineKind(llvm::EngineKind::JIT).setOptLevel(llvm::CodeGenOptLevel::Aggressive).create()};
    if(!engine) { std::fprintf(stderr, "%s\n", error.c_str()); return 5; }
    engine->finalizeObject();
    std::size_t runs{};
    for(auto const& test: cases)
    {
        auto fn{reinterpret_cast<std::uint64_t (*)(std::uint64_t, std::uint64_t, std::uint8_t*)>(engine->getFunctionAddress(test.function->getName().str()))};
        auto const check{[&](std::uint64_t address, std::uint64_t length)
        {
            auto const expected{reference_result(test, address, length)};
            std::uint8_t trapped{};
            auto const actual{fn(address, length, &trapped)};
            if(actual != expected.offset || trapped != static_cast<std::uint8_t>(expected.trapped))
            {
                std::fprintf(stderr, "FAIL %s address=%llx length=%llx actual=%llx/%u expected=%llx/%u\n",
                    test.function->getName().str().c_str(), (unsigned long long)address, (unsigned long long)length,
                    (unsigned long long)actual, static_cast<unsigned>(trapped),
                    (unsigned long long)expected.offset, static_cast<unsigned>(expected.trapped));
                return false;
            }
            ++runs;
            return true;
        }};
        for(std::uint64_t address: {0ull, 1ull, 15ull, 16ull, 0xfffffffull, 0x10000000ull, 0xffffffefull, 0xfffffff0ull,
                                   0xffffffffull, 0x100000000ull, (1ull << 40) - 1, 1ull << 40, 0xffffffffffffffffull})
        for(std::uint64_t length: {0ull, 1ull, 15ull, 16ull, 64ull, 65536ull, 0xffffffffull, 0x100000000ull, 0xffffffffffffffffull})
        { if(!check(address, length)) { return 6; } }

        std::uint64_t seed{0x9e3779b97f4a7c15ull};
        auto const random{[&]()
        {
            seed ^= seed >> 12u; seed ^= seed << 25u; seed ^= seed >> 27u;
            return seed * 0x2545f4914f6cdd1dull;
        }};
        for(unsigned n{}; n != 2048u; ++n)
        {
            auto const address{random()};
            auto const effective{mathematical_address(test, address)};
            auto length{random()};
            switch(n & 7u)
            {
                case 0u: length = 0u; break;
                case 1u: length = test.width - 1u; break;
                case 2u: length = test.width; break;
                case 3u: length = effective.offset; break;
                case 4u:
                case 5u:
                    length = effective.offset <= ~0ull - test.width ? effective.offset + test.width : ~0ull;
                    if((n & 7u) == 5u) { --length; }
                    break;
                case 6u: length = 0xffffffffull; break;
                default: break;
            }
            if(!check(address, length)) { return 6; }
        }
    }
    std::printf("PASS memory address decisions: %zu configurations, %zu boundary/random cases; full-guard configurations=%zu; IR=%s\n",
        cases.size(), runs, full_guard_configurations, optimize ? "O3" : "baseline");
}
#include <uwvm2/utils/macro/pop_macros.h>
