// Real LLVM IR/native execution of the independent checkpoint-only flag helper.
// Does not qualify whole-Wasm activation provenance or executable restoration.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <fast_io.h>
#include <bit>
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_initialization_emit.h>
}
namespace cp = ::uwvm2::runtime::checkpoint;
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace emit = compiler::details;
static void require(bool ok, char const* message)
{ if(!ok) { ::fast_io::io::perrln("checkpoint flag IR: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
static cp::function_plan plan()
{
    cp::function_plan p{}; p.profile = cp::compilation_profile::create_for_trusted_manager();
    p.expression_bytes = 16u; p.function_generation = 1u;
    cp::safepoint_layout entry{}; entry.identifier = 1u; entry.local_count = 4u;
    auto const i31_heap{static_cast<::std::int_least64_t>(cp::types::abstract_heap_type::i31)};
    entry.slots = {{{cp::types::value_kind::reference, {i31_heap}, false}, true},
                   {{cp::types::value_kind::reference, {i31_heap}, false}, false},
                   {{cp::types::value_kind::i32}, true},
                   {{cp::types::value_kind::reference, {i31_heap}, true}, true}};
    cp::control_layout function{}; function.end_offset = 15u; entry.controls.push_back(function);
    p.sites.push_back(::std::move(entry)); return p;
}
int main()
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual native LLVM target initialized");
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("checkpoint-executed-flags", context)};
    auto const signature{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context), {::llvm::Type::getInt1Ty(context)}, false)};
    auto const function{::llvm::Function::Create(signature, ::llvm::GlobalValue::ExternalLinkage, "checkpoint_flags", *module)};
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder = &context; state.llvm_module = module.get(); state.llvm_function = function;
    state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry);
    state.local_pointers.resize(4u);
    auto& builder{*state.ir_builder};
    ::llvm::AllocaInst* flags{};
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state, 1u, flags) && flags == nullptr && entry->empty(),
        "ordinary engine creates no alloca/store/checkpoint IR");
    auto metadata{plan()}; state.checkpoint_plan = &metadata;
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state, 0u, flags) && flags == nullptr && entry->empty(),
        "wrong actual parameter classification rejected before IR mutation");
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state, 1u, flags) && flags != nullptr,
        "opt-in true declaration initialization emitted");
    auto const before_invalid{entry->size()};
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(state, flags, 4u) && entry->size() == before_invalid,
        "out-of-range original local rejects before GEP or store");
    auto const other{::llvm::Function::Create(signature, ::llvm::GlobalValue::InternalLinkage, "foreign_flags", *module)};
    auto const other_entry{::llvm::BasicBlock::Create(context, "entry", other)};
    ::llvm::IRBuilder<> foreign_builder{other_entry};
    auto const foreign{foreign_builder.CreateAlloca(::llvm::ArrayType::get(builder.getInt8Ty(), 4u))};
    foreign_builder.CreateRet(foreign_builder.getInt32(0u));
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(state, foreign, 1u) && entry->size() == before_invalid,
        "native flag alloca from another function cannot be substituted");
    auto const carrier_type{emit::get_llvm_type_from_wasm_value_type(context, emit::runtime_operand_stack_value_type::funcref)};
    require(carrier_type != nullptr, "actual LLVM tagged-reference carrier type");
    auto const local_value{emit::create_llvm_jit_entry_block_alloca(builder, carrier_type, nullptr, "actual.local.value")};
    require(local_value != nullptr, "actual nondefaultable i31ref local alloca");
    auto const assigned{::llvm::BasicBlock::Create(context, "actual_assignment", function)};
    auto const merged{::llvm::BasicBlock::Create(context, "merge", function)};
    builder.CreateCondBr(function->getArg(0u), assigned, merged);
    builder.SetInsertPoint(assigned);
    auto const payload_type{builder.getIntNTy(static_cast<unsigned>(sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) * CHAR_BIT))};
    ::llvm::Value* i31_payload{::llvm::ConstantInt::get(payload_type, 17u)};
    constexpr bool little{::std::endian::native == ::std::endian::little};
    if constexpr(!little && sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) > sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31))
    { i31_payload = builder.CreateShl(i31_payload, static_cast<unsigned>((sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) -
        sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31)) * CHAR_BIT)); }
    auto const assigned_ref{emit::emit_llvm_jit_ref_from_payload(builder, i31_payload, ::uwvm2::object::global::wasm_ref_kind::wasm_i31, little)};
    require(assigned_ref != nullptr && assigned_ref->getType() == carrier_type, "real nonnull i31 tagged SSA value");
    builder.CreateStore(assigned_ref, local_value); // genuine typed selected value-store edge precedes flag
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(state, flags, 1u), "executed value-store flag emitted");
    builder.CreateBr(merged); builder.SetInsertPoint(merged);
    auto const array{::llvm::cast<::llvm::ArrayType>(flags->getAllocatedType())};
    auto read_flag{[&](::std::size_t index)
    {
        require(index < 4u, "actual bounded flag index before byte GEP");
        // [actual N=4-byte flags ... index ...] end
        // [safe                              ] index<4 before native GEP.
        auto const slot{builder.CreateInBoundsGEP(array, flags, {builder.getInt32(0u), builder.getInt64(index)})};
        return builder.CreateZExt(builder.CreateLoad(builder.getInt8Ty(), slot), builder.getInt32Ty());
    }};
    auto const assigned_before_reset{read_flag(1u)};
    auto const parameter_before_reset{read_flag(0u)};
    // Merge itself leaves the actual executed flag untouched. Only a genuine
    // next activation/self-tail reset emits the declaration reset here.
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_reset_initialization_flags(state, flags), "actual new-frame reset emitted");
    auto const assigned_after_reset{read_flag(1u)};
    auto const numeric_after_reset{read_flag(2u)};
    builder.CreateRet(builder.CreateOr(builder.CreateOr(assigned_before_reset, builder.CreateShl(parameter_before_reset, 1u)),
        builder.CreateOr(builder.CreateShl(numeric_after_reset, 2u), builder.CreateShl(assigned_after_reset, 3u))));
    require(!::llvm::verifyModule(*module), "actual helper-generated LLVM IR verified");
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::move(module)}.setEngineKind(::llvm::EngineKind::JIT).create()};
    require(bool(engine), "actual native JIT engine created"); engine->finalizeObject();
    auto const address{engine->getFunctionAddress("checkpoint_flags")}; require(address != 0u, "actual emitted native function resolved");
    using entry_function = ::std::uint32_t(*)(bool);
    auto const native{reinterpret_cast<entry_function>(static_cast<::std::uintptr_t>(address))};
    require(native(false) == 6u && native(true) == 7u, "actual selected branch/merge/reset preserves precise executed initialization");
    ::fast_io::io::println("checkpoint executed-local-init LLVM IR/native helper PASS; whole-Wasm continuation/restore acceptance=false");
}
