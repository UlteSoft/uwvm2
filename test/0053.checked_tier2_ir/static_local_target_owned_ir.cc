// Real LLVM IR/bitcode reader/verifier/optimizer/native MCJIT component.
// Synthetic compiler DATA covers shapes only; it grants no runtime capability.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <fast_io.h>
#include <uwvm2/utils/container/impl.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/checked_whole_local_target_specialization.h>
namespace targets = ::uwvm2::runtime::compiler::llvm_jit::checked_whole_local_targets;
namespace producer
{
    ::llvm::StringRef get_llvm_string_ref(::uwvm2::utils::container::u8string const& value)
    { return {reinterpret_cast<char const*>(value.data()), value.size()}; }
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/relocatable_host_symbol_emit.h>
}
namespace
{
    void require(bool valid, char const* message)
    {
        if(valid) { return; }
        ::fast_io::print(::fast_io::err(), "checked whole local targets: ", ::fast_io::mnp::os_c_str(message), "\n");
        ::fast_io::fast_terminate();
    }
    ::std::string dump(::llvm::Module const& module)
    { ::std::string result{}; ::llvm::raw_string_ostream stream{result}; module.print(stream, nullptr); return result; }
    enum class base_kind { symbol, constant, carrier, riscv, relocatable, relocatable_riscv };
    enum class fault { none, wrong_slot, wrong_metadata, bad_caller, declaration, wrong_cc, escape,
        nonzero_guard, wrong_symbol, carrier_write, carrier_escape, wrong_riscv, release_order, missing_layout,
        late_malformed, numeric_binding, nonvolatile_binding, underaligned_binding,
        underaligned_cell, thread_local_cell, location_escape };
    struct fixture
    {
        ::std::unique_ptr<::llvm::LLVMContext> context{new ::llvm::LLVMContext{}};
        ::std::unique_ptr<::llvm::Module> module{new ::llvm::Module{"actual-checked-local-shape", *context}};
        ::std::vector<targets::function_binding> bindings{};
    };
    fixture create(unsigned bits, bool little, base_kind kind, fault problem = fault::none, bool zero_index = false)
    {
        fixture owned{}; auto& module{*owned.module}; auto& context{*owned.context};
        if(problem != fault::missing_layout)
        { module.setDataLayout(bits == 32u ? (little ? "e-p:32:32" : "E-p:32:32") : (little ? "e-p:64:64" : "E-p:64:64")); }
        if(kind == base_kind::riscv || kind == base_kind::relocatable_riscv || kind == base_kind::relocatable)
        {
            auto const triple{kind == base_kind::relocatable ?
                (bits == 32u ? "i686-unknown-linux-gnu" : "aarch64-unknown-linux-gnu") :
                (bits == 32u ? "riscv32-unknown-linux-gnu" : "riscv64-unknown-linux-gnu")};
#if LLVM_VERSION_MAJOR >= 21
            module.setTargetTriple(::llvm::Triple{triple});
#else
            module.setTargetTriple(triple);
#endif
        }
        auto const i32{::llvm::Type::getInt32Ty(context)};
        auto const intptr{::llvm::Type::getIntNTy(context, bits)};
        auto const signature{::llvm::FunctionType::get(i32, {}, false)};
        auto const caller{::llvm::Function::Create(signature, ::llvm::GlobalValue::ExternalLinkage, "caller", module)};
        auto const target{::llvm::Function::Create(signature, ::llvm::GlobalValue::ExternalLinkage, "target", module)};
        if(problem != fault::declaration)
        { ::llvm::IRBuilder<> body{::llvm::BasicBlock::Create(context, "entry", target)}; body.CreateRet(::llvm::ConstantInt::get(i32, 42u)); }
        auto const entry{::llvm::BasicBlock::Create(context, "entry", caller)};
        auto const fast{::llvm::BasicBlock::Create(context, "fast", caller)};
        auto const slow{::llvm::BasicBlock::Create(context, "slow", caller)};
        ::llvm::IRBuilder<> builder{entry};
        ::llvm::Value* base{};
        ::llvm::GlobalVariable* carrier{};
        constexpr ::std::uint64_t address{0x12345000u};
        if(kind == base_kind::symbol)
        {
            base = new ::llvm::GlobalVariable{module, intptr, false, ::llvm::GlobalValue::ExternalLinkage, nullptr,
                problem == fault::wrong_symbol ? "different_slots" : "actual_slots"};
        }
        else if(kind == base_kind::constant)
        { base = ::llvm::ConstantExpr::getIntToPtr(::llvm::ConstantInt::get(intptr, address), ::llvm::PointerType::getUnqual(context)); }
        else if(kind == base_kind::carrier)
        {
            carrier = new ::llvm::GlobalVariable{module, intptr, false, ::llvm::GlobalValue::PrivateLinkage,
                ::llvm::ConstantInt::get(intptr, address), "actual_carrier"};
            carrier->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global);
            carrier->setAlignment(::llvm::Align{bits / 8u});
            auto const loaded{builder.CreateLoad(intptr, carrier)};
            loaded->setVolatile(true); loaded->setAlignment(::llvm::Align{bits / 8u});
            base = builder.CreateIntToPtr(loaded, ::llvm::PointerType::getUnqual(context));
            if(problem == fault::carrier_write) { builder.CreateStore(::llvm::ConstantInt::get(intptr, address + 8u), carrier); }
            if(problem == fault::carrier_escape)
            {
                auto const capture{::llvm::Function::Create(::llvm::FunctionType::get(builder.getVoidTy(),
                    {carrier->getType()}, false), ::llvm::GlobalValue::ExternalLinkage, "capture_carrier", module)};
                builder.CreateCall(capture, {carrier});
            }
        }
        else if(kind == base_kind::relocatable || kind == base_kind::relocatable_riscv)
        {
            auto const symbol{new ::llvm::GlobalVariable{module, intptr, false, ::llvm::GlobalValue::ExternalLinkage, nullptr,
                problem == fault::wrong_symbol ? "different_slots" : "actual_slots"}};
            base = producer::get_llvm_relocatable_host_symbol_pointer(builder, symbol);
            auto const reader{::llvm::dyn_cast_or_null<::llvm::LoadInst>(base)};
            require(reader != nullptr, "actual production relocatable pointer load");
            auto const location{reader->getPointerOperand()};
            auto const address_producer{::llvm::dyn_cast<::llvm::CallInst>(location)};
            carrier = ::llvm::dyn_cast<::llvm::GlobalVariable>(address_producer == nullptr ? location : address_producer->getArgOperand(0u));
            require(carrier != nullptr, "actual production private pointer cell");
            if(problem == fault::numeric_binding)
            { carrier->setInitializer(::llvm::ConstantExpr::getIntToPtr(::llvm::ConstantInt::get(intptr, address), symbol->getType())); }
            if(problem == fault::nonvolatile_binding) { reader->setVolatile(false); }
            if(problem == fault::underaligned_binding) { reader->setAlignment(::llvm::Align{1u}); }
            if(problem == fault::underaligned_cell) { carrier->setAlignment(::llvm::Align{1u}); }
            if(problem == fault::thread_local_cell) { carrier->setThreadLocal(true); }
            if(problem == fault::carrier_write)
            { builder.CreateStore(::llvm::ConstantPointerNull::get(::llvm::cast<::llvm::PointerType>(carrier->getValueType())), location); }
            if(problem == fault::carrier_escape || problem == fault::location_escape)
            {
                auto const capture{::llvm::Function::Create(::llvm::FunctionType::get(builder.getVoidTy(),
                    {carrier->getType()}, false), ::llvm::GlobalValue::ExternalLinkage, "capture_carrier", module)};
                builder.CreateCall(capture, {problem == fault::location_escape ? location : carrier});
            }
            if(problem == fault::wrong_riscv)
            {
                require(address_producer != nullptr, "actual RV PC-relative producer before mutation");
                address_producer->setCalledFunction(address_producer->getFunctionType(),
                    ::llvm::InlineAsm::get(address_producer->getFunctionType(), "la $0, $1", "=r,i", false));
            }
        }
        else
        {
            require(bits == 64u, "real RV address producer requires i64");
            auto const assembly_type{::llvm::FunctionType::get(intptr, {intptr}, false)};
            auto const assembly{::llvm::InlineAsm::get(assembly_type,
                problem == fault::wrong_riscv ? "addi $0, $1, 1" : "li $0, $1", "=r,i", false)};
            auto const loaded{builder.CreateCall(assembly, {::llvm::ConstantInt::get(intptr, address)})};
            loaded->setDoesNotAccessMemory(); loaded->setDoesNotThrow();
            base = builder.CreateIntToPtr(loaded, ::llvm::PointerType::getUnqual(context));
        }
        auto const slot{builder.CreateInBoundsGEP(intptr, base,
            ::llvm::ConstantInt::get(intptr, problem == fault::wrong_slot ? 0u : (zero_index ? 0u : 1u)))};
        auto const loaded{builder.CreateLoad(intptr, slot, "checked.local.target")};
        loaded->setVolatile(true); loaded->setAlignment(::llvm::Align{bits / 8u});
        loaded->setAtomic(problem == fault::release_order ? ::llvm::AtomicOrdering::Monotonic : ::llvm::AtomicOrdering::Acquire);
        auto const integer{::llvm::Type::getInt64Ty(context)};
        ::llvm::Metadata* identity[]{
            ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, 7u)),
            ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, zero_index ? 101u : 100u)),
            ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, zero_index ? 100u : 101u)),
            ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, problem == fault::wrong_metadata ? 2u : (zero_index ? 0u : 1u))),
            ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, address)),
            ::llvm::ConstantAsMetadata::get(target),
            ::llvm::ConstantAsMetadata::get(problem == fault::bad_caller ? target : caller)};
        loaded->setMetadata(targets::identity_name, ::llvm::MDNode::get(context, identity));
        if(problem == fault::escape)
        { auto const escaped{new ::llvm::GlobalVariable{module, intptr, false, ::llvm::GlobalValue::ExternalLinkage, nullptr, "escape"}}; builder.CreateStore(loaded, escaped); }
        builder.CreateCondBr(builder.CreateICmpNE(loaded, ::llvm::ConstantInt::get(intptr,
            problem == fault::nonzero_guard ? 1u : 0u)), fast, slow);
        builder.SetInsertPoint(slow); builder.CreateRet(::llvm::ConstantInt::get(i32, 99u));
        builder.SetInsertPoint(fast);
        auto const pointer{builder.CreateIntToPtr(loaded, target->getType())};
        auto const call{builder.CreateCall(signature, pointer)};
        if(problem == fault::wrong_cc) { call->setCallingConv(::llvm::CallingConv::Fast); }
        auto const returned{builder.CreateRet(call)};
        if(problem == fault::late_malformed)
        { returned->setMetadata(targets::identity_name, ::llvm::MDNode::get(context, {})); }
        owned.bindings = zero_index ? ::std::vector<targets::function_binding>{{target, nullptr, 7u, 100u}, {caller, nullptr, 7u, 101u}} :
            ::std::vector<targets::function_binding>{{caller, nullptr, 7u, 100u}, {target, nullptr, 7u, 101u}};
        return owned;
    }
    void round_trip(fixture& owned)
    {
        ::std::string bytes{}; ::llvm::raw_string_ostream stream{bytes}; ::llvm::WriteBitcodeToFile(*owned.module, stream); stream.flush();
        auto parsed{::llvm::parseBitcodeFile(::llvm::MemoryBufferRef{bytes, "actual-owned-bitcode"}, *owned.context)};
        require(bool(parsed), "actual LLVM bitcode reader accepts only global/integer attachments");
        auto const first_is_target{owned.bindings[0uz].typed->getName() == "target"};
        owned.module = ::std::move(*parsed);
        owned.bindings = first_is_target ? ::std::vector<targets::function_binding>{
            {owned.module->getFunction("target"), nullptr, 7u, 100u}, {owned.module->getFunction("caller"), nullptr, 7u, 101u}} :
            ::std::vector<targets::function_binding>{
            {owned.module->getFunction("caller"), nullptr, 7u, 100u}, {owned.module->getFunction("target"), nullptr, 7u, 101u}};
    }
    void corpus()
    {
        unsigned positives{}, negatives{};
        for(auto const bits: {32u, 64u}) for(auto const little: {false, true})
            for(auto const kind: {base_kind::symbol, base_kind::constant, base_kind::carrier,
                                 base_kind::relocatable, base_kind::relocatable_riscv}) for(auto const zero: {false, true})
        {
            auto owned{create(bits, little, kind, fault::none, zero)}; round_trip(owned);
            auto const result{targets::specialize(*owned.module, owned.bindings, {0x12345000u, "actual_slots", true})};
            require(result.state == targets::status::transformed && result.replaced == 1uz && !::llvm::verifyModule(*owned.module), "complete exact compiler shape transforms and verifies");
            ++positives;
        }
        for(auto const little: {false, true}) for(auto const zero: {false, true})
        {
            auto owned{create(64u, little, base_kind::riscv, fault::none, zero)}; round_trip(owned);
            auto const result{targets::specialize(*owned.module, owned.bindings, {0x12345000u, "actual_slots", true})};
            require(result.state == targets::status::transformed && result.replaced == 1uz, "actual RV producer SSA shape, not RV native qualification"); ++positives;
        }
        for(auto const problem: {fault::wrong_slot, fault::wrong_metadata, fault::bad_caller, fault::declaration,
            fault::wrong_cc, fault::escape, fault::nonzero_guard, fault::wrong_symbol, fault::carrier_write,
            fault::carrier_escape, fault::wrong_riscv, fault::release_order, fault::missing_layout, fault::late_malformed})
        {
            auto const kind{problem == fault::carrier_write || problem == fault::carrier_escape ? base_kind::carrier :
                problem == fault::wrong_riscv ? base_kind::riscv : base_kind::symbol};
            auto owned{create(64u, true, kind, problem)}; auto const before{dump(*owned.module)};
            auto const result{targets::specialize(*owned.module, owned.bindings, {0x12345000u, "actual_slots", true})};
            require(result.state == targets::status::rejected_before_mutation && dump(*owned.module) == before,
                "all-owner/ABI/source/slot/SSA rejection leaves actual LLVM module unchanged"); ++negatives;
        }
        for(auto const kind: {base_kind::relocatable, base_kind::relocatable_riscv})
            for(auto const problem: {fault::wrong_symbol, fault::numeric_binding, fault::nonvolatile_binding,
                fault::underaligned_binding, fault::underaligned_cell, fault::thread_local_cell,
                fault::carrier_write, fault::carrier_escape, fault::location_escape, fault::wrong_riscv})
        {
            if(kind != base_kind::relocatable_riscv && problem == fault::wrong_riscv) { continue; }
            auto owned{create(64u, true, kind, problem)}; round_trip(owned);
            auto const before{dump(*owned.module)};
            auto const result{targets::specialize(*owned.module, owned.bindings, {0x12345000u, "actual_slots", true})};
            require(result.state == targets::status::rejected_before_mutation && dump(*owned.module) == before,
                "relocation identity and complete cell/address use proof reject unchanged"); ++negatives;
        }
        ::fast_io::print(::fast_io::out(), "CHECKED_LOCAL_IR shape_positive=", positives, " rejected_unchanged=", negatives, " native_target_qualified=0\n");
    }
    void native_execution()
    {
        require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual native target initialization");
        for(auto const kind: {base_kind::symbol, base_kind::relocatable})
        {
        ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()}; require(bool(target), "actual target machine");
        auto owned{create(sizeof(::std::uintptr_t) * 8u, true, kind)};
#if LLVM_VERSION_MAJOR >= 21
        owned.module->setTargetTriple(target->getTargetTriple());
#else
        owned.module->setTargetTriple(target->getTargetTriple().str());
#endif
        owned.module->setDataLayout(target->createDataLayout());
        auto const result{targets::specialize(*owned.module, owned.bindings, {0x12345000u, "actual_slots", true})};
        require(result.state == targets::status::transformed && result.replaced == 1uz, "actual target DL compiler transform");
        ::llvm::LoopAnalysisManager loops{}; ::llvm::FunctionAnalysisManager functions{};
        ::llvm::CGSCCAnalysisManager calls{}; ::llvm::ModuleAnalysisManager modules{};
        ::llvm::PassBuilder passes{target.get()};
        passes.registerModuleAnalyses(modules); passes.registerCGSCCAnalyses(calls);
        passes.registerFunctionAnalyses(functions); passes.registerLoopAnalyses(loops);
        passes.crossRegisterProxies(loops, functions, calls, modules);
        auto optimizer{passes.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O1)};
        optimizer.run(*owned.module, modules);
        require(!::llvm::verifyModule(*owned.module), "actual optimizer derivative verifies");
        for(auto const& function: *owned.module) for(auto const& block: function) for(auto const& instruction: block)
        { require(!::llvm::isa<::llvm::LoadInst>(instruction), "actual transformed+optimized module contains no target/carrier load"); }
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::move(owned.module)}.setEngineKind(::llvm::EngineKind::JIT).create(target.release())};
        require(bool(engine), "actual MCJIT owns the derivative"); engine->finalizeObject();
        auto const address{engine->getFunctionAddress("caller")}; require(address != 0u, "actual caller symbol resolves");
        require(reinterpret_cast<::std::int32_t(*)()>(static_cast<::std::uintptr_t>(address))() == 42, "actual native result remains 42");
        ::fast_io::print(::fast_io::out(), "CHECKED_LOCAL_IR actual_native_result=42 static_target_loads=0\n");
        }
    }
}
int main() { corpus(); native_execution(); }
