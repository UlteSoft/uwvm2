// Compile-time IR regression: genuine debug numeric identities must preserve
// sibling/merge dominance and ordinary opt-out. No generated code is executed.
#define UWVM_RUNTIME_LLVM_JIT 1
#define UWVM_USE_LLVM_JIT 1
#include <uwvm2/runtime/compiler/llvm_jit/native_provenance.h>
#include <llvm/IR/Verifier.h>
#include <fast_io.h>

namespace metadata = ::uwvm2::runtime::compiler::llvm_jit::native_provenance;
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("numeric rebind failure line=", __LINE__); return 1; } } while(false)

static ::llvm::Instruction* identity(::llvm::IRBuilder<>& builder, ::llvm::Value* input,
    ::llvm::SmallVectorImpl<metadata::numeric_identity>& identities)
{
    ::llvm::Instruction* witness{};
    auto* const value{metadata::materialize_numeric_register(builder, input, &witness)};
    if(value == nullptr || value == input) { return nullptr; }
    auto* const same_type{::llvm::dyn_cast<::llvm::Instruction>(builder.CreateBitCast(value, input->getType()))};
    if(same_type != nullptr)
    { identities.push_back({input, same_type}); }
    return same_type;
}

int main()
{
#if !defined(__linux__) || !(defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__ALTIVEC__) || defined(__ARM_NEON) || defined(__mips_msa) || defined(__loongarch_sx))
    ::fast_io::io::println("SKIP no whole-vector register class on this compile target");
    return 0;
#else
    ::llvm::LLVMContext context{};
    ::llvm::Module module{"debug-numeric-dominance", context};
    auto* const vector{::llvm::FixedVectorType::get(::llvm::Type::getInt8Ty(context), 16u)};
    auto* const signature{::llvm::FunctionType::get(vector, {vector, ::llvm::Type::getInt1Ty(context)}, false)};
    for(bool phi_only: {false, true})
    {
        auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage,
            phi_only ? "phi_edges" : "sibling_uses", module)};
        auto* const scope{metadata::attach(*function, "uwvm-m2-f3-g4.wasm-native-v1")};
        CHECK(scope != nullptr);
        auto* const entry{::llvm::BasicBlock::Create(context, "entry", function)};
        auto* const left{::llvm::BasicBlock::Create(context, "left", function)};
        auto* const right{::llvm::BasicBlock::Create(context, "right", function)};
        auto* const merge{::llvm::BasicBlock::Create(context, "merge", function)};
        ::llvm::IRBuilder<> builder{entry};
        ::llvm::SmallVector<metadata::numeric_identity, 0u> identities{};
        auto* const input{function->getArg(0u)};
        builder.CreateCondBr(function->getArg(1u), left, right);
        builder.SetInsertPoint(left);
        CHECK(metadata::location(builder, scope, 2u, 3u));
        auto* const rebound{identity(builder, input, identities)}; CHECK(rebound != nullptr);
        auto* const one{::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u), builder.getInt8(1u))};
        auto* const left_use{phi_only ? nullptr : ::llvm::cast<::llvm::Instruction>(builder.CreateAdd(input, one))};
        builder.CreateBr(merge);
        builder.SetInsertPoint(right);
        auto* const right_use{phi_only ? nullptr : ::llvm::cast<::llvm::Instruction>(builder.CreateAdd(input, one))};
        builder.CreateBr(merge);
        builder.SetInsertPoint(merge);
        auto* const phi{builder.CreatePHI(vector, 2u)};
        phi->addIncoming(phi_only ? static_cast<::llvm::Value*>(input) : left_use, left);
        phi->addIncoming(phi_only ? static_cast<::llvm::Value*>(input) : right_use, right);
        auto* const merged_use{::llvm::cast<::llvm::Instruction>(builder.CreateXor(input, phi))};
        builder.CreateRet(merged_use);
        CHECK(!::llvm::verifyModule(module)); // Matches the runtime's pre-finalization verifier.
        metadata::restrict_public_code(*function, identities);
        CHECK(!::llvm::verifyModule(module));
        CHECK(merged_use->getOperand(0u) == input);
        if(phi_only)
        { CHECK(phi->getIncomingValue(0u) == rebound && phi->getIncomingValue(1u) == input); }
        else
        { CHECK(left_use->getOperand(0u) == rebound && right_use->getOperand(0u) == input); }
    }
    for(bool debug: {false, true})
    {
        auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage,
            debug ? "chained_identities" : "ordinary", module)};
        auto* const block{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{block};
        ::llvm::SmallVector<metadata::numeric_identity, 0u> identities{};
        auto* const scope{debug ? metadata::attach(*function, "uwvm-m2-f3-g4.wasm-native-v1") : nullptr};
        if(debug) { CHECK(scope != nullptr && metadata::location(builder, scope, 2u, 3u)); }
        auto* const input{function->getArg(0u)};
        auto* const one{::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u), builder.getInt8(1u))};
        auto* const first{debug ? identity(builder, input, identities) : ::llvm::cast<::llvm::Instruction>(builder.CreateXor(input, one))};
        CHECK(first != nullptr);
        auto* const second{debug ? identity(builder, input, identities) : ::llvm::cast<::llvm::Instruction>(builder.CreateXor(first, one))};
        CHECK(second != nullptr);
        auto* const result{::llvm::cast<::llvm::Instruction>(builder.CreateXor(input,
            ::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u), builder.getInt8(7u))))};
        builder.CreateRet(result);
        CHECK(!::llvm::verifyModule(module)); // Matches the runtime's pre-finalization verifier.
        metadata::restrict_public_code(*function, identities);
        CHECK(!::llvm::verifyModule(module));
        CHECK(result->getOperand(0u) == (debug ? static_cast<::llvm::Value*>(second) : input));
    }
    {
        auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "constant_identity", module)};
        auto* const block{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{block};
        ::llvm::SmallVector<metadata::numeric_identity, 0u> identities{};
        auto* const scope{metadata::attach(*function, "uwvm-m2-f3-g4.wasm-native-v1")};
        CHECK(scope != nullptr && metadata::location(builder, scope, 2u, 3u));
        auto* const literal{::llvm::ConstantVector::getSplat(::llvm::ElementCount::getFixed(16u), builder.getInt8(0xa5u))};
        auto* const first{identity(builder, literal, identities)}; CHECK(first != nullptr);
        auto* const second{identity(builder, literal, identities)}; CHECK(second != nullptr);
        auto* const result{builder.CreateRet(literal)};
        CHECK(!::llvm::verifyModule(module)); // Matches the runtime's pre-finalization verifier.
        metadata::restrict_public_code(*function, identities);
        CHECK(!::llvm::verifyModule(module));
        CHECK(result->getReturnValue() == second);
    }
    {
        auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "loop_prefix", module)};
        auto* const scope{metadata::attach(*function, "uwvm-m2-f3-g4.wasm-native-v1")}; CHECK(scope != nullptr);
        auto* const entry{::llvm::BasicBlock::Create(context, "entry", function)};
        auto* const loop{::llvm::BasicBlock::Create(context, "loop", function)};
        auto* const exit{::llvm::BasicBlock::Create(context, "exit", function)};
        ::llvm::IRBuilder<> builder{entry};
        ::llvm::SmallVector<metadata::numeric_identity, 0u> identities{};
        auto* const input{function->getArg(0u)};
        builder.CreateCondBr(function->getArg(1u), loop, exit);
        builder.SetInsertPoint(loop);
        auto* const phi{builder.CreatePHI(vector, 2u)};
        phi->addIncoming(input, entry); phi->addIncoming(input, loop);
        CHECK(metadata::location(builder, scope, 2u, 3u));
        auto* const rebound{identity(builder, input, identities)}; CHECK(rebound != nullptr);
        builder.CreateCondBr(function->getArg(1u), loop, exit);
        builder.SetInsertPoint(exit);
        auto* const result{builder.CreateRet(input)};
        CHECK(!::llvm::verifyModule(module)); // Matches the runtime's pre-finalization verifier.
        metadata::restrict_public_code(*function, identities);
        CHECK(!::llvm::verifyModule(module));
        CHECK(phi->getIncomingValue(0u) == input && phi->getIncomingValue(1u) == rebound);
        CHECK(result->getReturnValue() == input);
    }
    {
        auto* const function{::llvm::Function::Create(signature, ::llvm::Function::ExternalLinkage, "erased_identity", module)};
        auto* const block{::llvm::BasicBlock::Create(context, "entry", function)};
        ::llvm::IRBuilder<> builder{block};
        ::llvm::SmallVector<metadata::numeric_identity, 0u> identities{};
        auto* const scope{metadata::attach(*function, "uwvm-m2-f3-g4.wasm-native-v1")};
        CHECK(scope != nullptr && metadata::location(builder, scope, 2u, 3u));
        auto* const input{function->getArg(0u)};
        auto* const rebound{identity(builder, input, identities)}; CHECK(rebound != nullptr);
        rebound->replaceAllUsesWith(input);
        rebound->eraseFromParent();
        auto* const result{builder.CreateRet(input)};
        CHECK(!::llvm::verifyModule(module));
        metadata::restrict_public_code(*function, identities);
        CHECK(!::llvm::verifyModule(module));
        CHECK(result->getReturnValue() == input);
    }
    ::fast_io::io::println("PASS actual LLVM IR verification: sibling uses, merge prefix, PHI edges, repeated identities, constants, loop backedges, RAUW/deletion, ordinary opt-out; no generated function executed");
#endif
}
