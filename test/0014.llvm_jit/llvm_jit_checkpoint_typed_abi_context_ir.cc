// Real LLVM typed-ABI context/continuation component. No actual VM management,
// pause, canonical resume mailbox, host/root census or whole-instance restore.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <fast_io.h>
#include <array>
#include <cstring>
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_initialization_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_packet_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_resume_landing_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_context_emit.h>
}
namespace cp = ::uwvm2::runtime::checkpoint;
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("checkpoint typed ABI context IR: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
static cp::function_plan make_plan()
{
    cp::function_plan plan{}; plan.profile = cp::compilation_profile::create_for_trusted_manager();
    plan.module = 3u; plan.function = 7u; plan.expression_bytes = 8u; plan.function_generation = 1u;
    cp::safepoint_layout entry{}; entry.identifier = 1u; entry.local_count = 2u;
    entry.slots = {{{cp::types::value_kind::f64}, true}, {{cp::types::value_kind::i64}, true}};
    cp::control_layout function{}; function.end_offset = 7u; entry.controls.push_back(function); plan.sites.push_back(entry);
    auto after{entry}; after.identifier = 2u; after.opcode_offset = 2u; after.operand_count = 1u;
    after.slots.push_back({{cp::types::value_kind::i64}, true}); plan.sites.push_back(::std::move(after)); return plan;
}
template<typename T> static void set_slot(::std::array<::std::byte, 48u>& packet, ::std::size_t index, T value)
{
    static_assert(sizeof(T) <= cp::native_slot_bytes);
    require(index < 3u, "test owner slot bound");
    // [actual driver-owned packet slots0 ... index*16 ... 48] packet_end
    // [safe                                               ] index<3 and
    // complete sizeof(T)<=16 BEFORE this byte-pointer advance and copy.
    ::std::memcpy(packet.data() + index * cp::native_slot_bytes, ::std::addressof(value), sizeof(T));
}
int main()
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual native target");
    ::llvm::LLVMContext context{}; auto module{::std::make_unique<::llvm::Module>("checkpoint-typed-ABI-context", context)};
    ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};
    require(bool(target), "actual native target DataLayout before IR"); module->setDataLayout(target->createDataLayout());
    ::llvm::IRBuilder<> types{context};
    auto const integer{types.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const typed_signature{::llvm::FunctionType::get(types.getInt64Ty(), {types.getDoubleTy(), types.getInt64Ty()}, false)};
    auto const bridge_signature{::llvm::FunctionType::get(types.getVoidTy(), {types.getInt64Ty(), types.getInt64Ty(), types.getPtrTy()}, false)};
    auto const native_context{::llvm::StructType::get(context,
        {types.getInt64Ty(), types.getPtrTy(), integer, types.getPtrTy(), integer}, false)};
    ::std::array<::llvm::GlobalVariable*, 5u> inputs{};
    ::std::array<char const*, 5u> names{"cp_selector", "cp_payload", "cp_payload_bytes", "cp_flags", "cp_flag_count"};
    for(unsigned i{}; i != inputs.size(); ++i)
    {
        // [actual fixed five LLVM context fields] end
        // [safe                                ] i<5 before type/name lookup.
        inputs[i] = new ::llvm::GlobalVariable{*module, native_context->getElementType(i), false,
            ::llvm::GlobalValue::ExternalLinkage, ::llvm::Constant::getNullValue(native_context->getElementType(i)), names[i]};
    }
    auto const count{new ::llvm::GlobalVariable{*module, types.getInt64Ty(), false, ::llvm::GlobalValue::ExternalLinkage,
        types.getInt64(0u), "cp_bridge_calls"}};
    auto const bridge{::llvm::Function::Create(bridge_signature, ::llvm::GlobalValue::InternalLinkage, "driver_data_context", *module)};
    bridge->setDoesNotThrow(); bridge->addFnAttr(::llvm::Attribute::NoInline);
    ::llvm::IRBuilder<> bridge_builder{::llvm::BasicBlock::Create(context, "entry", bridge)};
    auto const identity_valid{bridge_builder.CreateAnd(bridge_builder.CreateICmpEQ(bridge->getArg(0u), types.getInt64(3u)),
        bridge_builder.CreateICmpEQ(bridge->getArg(1u), types.getInt64(7u)))};
    auto const fill{::llvm::BasicBlock::Create(context, "fill", bridge)};
    auto const done{::llvm::BasicBlock::Create(context, "done", bridge)};
    bridge_builder.CreateCondBr(identity_valid, fill, done); bridge_builder.SetInsertPoint(fill);
    auto const previous{bridge_builder.CreateLoad(types.getInt64Ty(), count)};
    bridge_builder.CreateStore(bridge_builder.CreateAdd(previous, types.getInt64(1u)), count);
    for(unsigned i{}; i != inputs.size(); ++i)
    {
        // [actual compiler-owned context fields0 ... i ... 5] context_end
        // [safe                                             ] complete same
        // fixed context type and i<5 BEFORE field pointer selection/store.
        auto const cell{bridge_builder.CreateStructGEP(native_context, bridge->getArg(2u), i)};
        bridge_builder.CreateStore(bridge_builder.CreateLoad(native_context->getElementType(i), inputs[i]), cell);
    }
    bridge_builder.CreateBr(done); bridge_builder.SetInsertPoint(done); bridge_builder.CreateRetVoid();
    auto const function{::llvm::Function::Create(typed_signature, ::llvm::GlobalValue::ExternalLinkage, "cp_typed_entry", *module)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder = &context; state.llvm_module = module.get(); state.llvm_function = function;
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry);
    auto& builder{*state.ir_builder};
    auto const absent{emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, nullptr)};
    require(absent.valid && !absent.selected && absent.context_owner == nullptr && entry->empty() && function->getFunctionType() == typed_signature,
        "ordinary null profile emits no context/allocation/call and preserves typed signature");
    auto plan{make_plan()}; require(cp::validate_plan(plan) == cp::status::ok, "actual compiler metadata DATA"); state.checkpoint_plan = &plan;
    auto const actual_layout{module->getDataLayout()}; module->setDataLayout("");
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, bridge).valid && entry->empty(),
        "unspecified target DataLayout fails before context IR");
    module->setDataLayout(sizeof(::std::uintptr_t) == 8u ? "e-p:32:32" : "e-p:64:64");
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, bridge).valid && entry->empty(),
        "foreign target pointer width fails before context IR"); module->setDataLayout(actual_layout);
    auto const wrong_bridge{::llvm::Function::Create(::llvm::FunctionType::get(types.getVoidTy(), {types.getPtrTy()}, false),
        ::llvm::GlobalValue::ExternalLinkage, "invalid_bridge", *module)};
    wrong_bridge->setDoesNotThrow();
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, wrong_bridge).valid && entry->empty(),
        "wrong bridge signature fails before context IR");
    ::llvm::Module foreign{"foreign-context-owner", context};
    auto const foreign_bridge{::llvm::Function::Create(bridge_signature, ::llvm::GlobalValue::ExternalLinkage, "foreign_bridge", foreign)};
    foreign_bridge->setDoesNotThrow();
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, foreign_bridge).valid && entry->empty(),
        "foreign module bridge fails before context IR");
    auto const throwing_bridge{::llvm::Function::Create(bridge_signature, ::llvm::GlobalValue::ExternalLinkage, "throwing_bridge", *module)};
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, throwing_bridge).valid && entry->empty(),
        "unproven exception behavior fails before context IR");
    auto const attributed_bridge{::llvm::Function::Create(bridge_signature, ::llvm::GlobalValue::ExternalLinkage, "attributed_bridge", *module)};
    attributed_bridge->setDoesNotThrow(); attributed_bridge->addParamAttr(0u, ::llvm::Attribute::SExt);
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, attributed_bridge).valid && entry->empty(),
        "hidden parameter ABI modifier fails before context IR");
    state.local_types.resize(2u); state.local_pointers.resize(2u);
    for(::std::size_t i{}; i != 2u; ++i)
    {
        state.local_types[i] = emit::checkpoint_packet_physical_carrier(plan.sites[0u].slots[i].type);
        state.local_pointers[i] = emit::create_llvm_jit_entry_block_alloca(builder,
            emit::get_llvm_type_from_wasm_value_type(context, state.local_types[i]), nullptr, "typed.parameter.local");
        builder.CreateStore(function->getArg(static_cast<unsigned>(i)), state.local_pointers[i]);
    }
    ::llvm::AllocaInst* flags{};
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state, 2u, flags), "actual parameters initialized");
    auto const supplied{emit::emit_runtime_local_func_llvm_jit_checkpoint_context(state, bridge)};
    require(supplied.valid && supplied.selected && supplied.bridge_call != nullptr && supplied.context_type == native_context &&
        function->getFunctionType() == typed_signature && function->arg_size() == 2u && function->getCallingConv() == ::llvm::CallingConv::C,
        "actual selected context leaves floating parameter ABI untouched");
    auto const rejected{::llvm::BasicBlock::Create(context, "rejected", function)};
    ::llvm::IRBuilder<> reject_builder{rejected}; reject_builder.CreateRet(types.getInt64(UINT64_MAX));
    emit::llvm_jit_checkpoint_resume_dispatch_emit_state resume{};
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state, flags, supplied.logical_site,
        supplied.payload, supplied.payload_bytes, supplied.original_flags, supplied.flag_count, rejected, resume), "context to logical dispatch");
    auto const first{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, plan.sites[0u], {}, {})};
    require(first.valid && first.selected, "entry logical landing");
    ::std::array<::llvm::Value*, 1u> operand{types.getInt64(9u)};
    ::std::array<cp::types::core_value_type, 1u> exact{plan.sites[1u].slots[2u].type};
    auto const second{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, plan.sites[1u], operand, exact)};
    require(second.valid && second.selected && second.actual_nonlocals.size() == 1u, "typed operand landing");
    auto const floating{builder.CreateLoad(types.getDoubleTy(), state.local_pointers[0u])};
    auto const integral{builder.CreateLoad(types.getInt64Ty(), state.local_pointers[1u])};
    builder.CreateRet(builder.CreateAdd(builder.CreateAdd(builder.CreateFPToUI(floating, types.getInt64Ty()), integral), second.actual_nonlocals[0u]));
    auto const tail{::llvm::Function::Create(typed_signature, ::llvm::GlobalValue::ExternalLinkage, "cp_typed_tail", *module)};
    ::llvm::IRBuilder<> tail_builder{::llvm::BasicBlock::Create(context, "entry", tail)};
    auto const call{tail_builder.CreateCall(function, {tail->getArg(0u), tail->getArg(1u)})};
    call->setTailCallKind(::llvm::CallInst::TCK_MustTail); tail_builder.CreateRet(call);
    require(call->isMustTailCall() && call->getNextNode() != nullptr && ::llvm::isa<::llvm::ReturnInst>(call->getNextNode()) &&
        tail->getFunctionType() == typed_signature, "same typed ABI musttail followed immediately by return");
    auto const ordinary{::llvm::Function::Create(typed_signature, ::llvm::GlobalValue::ExternalLinkage, "cp_ordinary_typed_entry", *module)};
    emit::runtime_local_func_llvm_jit_emit_state_t ordinary_state{};
    ordinary_state.llvm_context_holder = &context; ordinary_state.llvm_module = module.get(); ordinary_state.llvm_function = ordinary;
    auto const ordinary_entry{::llvm::BasicBlock::Create(context, "entry", ordinary)};
    ordinary_state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(ordinary_entry);
    auto const ordinary_context{emit::emit_runtime_local_func_llvm_jit_checkpoint_context(ordinary_state, bridge)};
    require(ordinary_context.valid && !ordinary_context.selected && ordinary_entry->empty(), "actual ordinary function adds no supplier call");
    auto& ordinary_builder{*ordinary_state.ir_builder};
    ordinary_builder.CreateRet(ordinary_builder.CreateAdd(ordinary_builder.CreateAdd(
        ordinary_builder.CreateFPToUI(ordinary->getArg(0u), types.getInt64Ty()), ordinary->getArg(1u)), types.getInt64(9u)));
    require(!::llvm::verifyModule(*module), "whole component actual typed ABI/SSA/musttail verifier");
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::move(module)}.setEngineKind(::llvm::EngineKind::JIT).create()};
    require(bool(engine), "real MCJIT"); engine->finalizeObject();
    require(engine->getDataLayout().getPointerSize() == sizeof(void*) && sizeof(::std::uintptr_t) == sizeof(void*),
        "actual engine pointer ABI matches the native component target");
    auto const address{engine->getFunctionAddress("cp_typed_entry")}; auto const tail_address{engine->getFunctionAddress("cp_typed_tail")};
    auto const ordinary_address{engine->getFunctionAddress("cp_ordinary_typed_entry")};
    require(address != 0u && tail_address != 0u && ordinary_address != 0u, "real typed native entries");
    using typed_entry = ::std::uint64_t(*)(double, ::std::uint64_t);
    // [actual live MCJIT-owned entry] engine retained through every real call
    // [safe                         ] exact independently emitted native typed
    // ABI; NOT database/native-PC authority or VM logical-dispatch permission.
    auto const invoke{reinterpret_cast<typed_entry>(static_cast<::std::uintptr_t>(address))};
    auto const invoke_tail{reinterpret_cast<typed_entry>(static_cast<::std::uintptr_t>(tail_address))};
    auto const invoke_ordinary{reinterpret_cast<typed_entry>(static_cast<::std::uintptr_t>(ordinary_address))};
    auto const set_global{[&]<typename T>(char const* name, T const& value)
    {
        auto const target{engine->getGlobalValueAddress(name)}; require(target != 0u, "actual owned LLVM global DATA");
        // [actual live LLVM global symbol] native extent == corresponding T
        // [safe                         ] symbols/types fixed by this driver,
        // engine owns lifetime; complete native bits, no aggregate offset guess.
        ::std::memcpy(reinterpret_cast<void*>(static_cast<::std::uintptr_t>(target)), ::std::addressof(value), sizeof(T));
    }};
    auto const calls{[&]
    {
        auto const target{engine->getGlobalValueAddress("cp_bridge_calls")}; require(target != 0u, "actual bridge count owner");
        ::std::uint64_t result{};
        // [actual LLVM i64 global] end
        // [safe fixed eight bytes] owning engine lifetime BEFORE complete copy.
        ::std::memcpy(::std::addressof(result), reinterpret_cast<void const*>(static_cast<::std::uintptr_t>(target)), sizeof(result)); return result;
    }};
    require(invoke(2.5, 5u) == 16u && invoke_tail(3.5, 7u) == 19u && calls() == 2u,
        "normal floating/integer ABI and real musttail with no restore input");
    ::std::array<::std::byte, 48u> input{}; ::std::array<::std::uint8_t, 2u> markers{1u, 1u};
    set_slot(input, 0u, 4.5); set_slot(input, 1u, ::std::uint64_t{70u}); set_slot(input, 2u, ::std::uint64_t{80u});
    auto const payload{static_cast<void*>(input.data())}; auto const original_flags{static_cast<void*>(markers.data())};
    set_global("cp_payload", payload); set_global("cp_flags", original_flags);
    set_global("cp_flag_count", ::std::uintptr_t{markers.size()});
    set_global("cp_selector", ::std::uint64_t{1u}); set_global("cp_payload_bytes", ::std::uintptr_t{32u});
    require(invoke(999.5, 999u) == 83u, "logical entry restores locals while retaining actual typed ABI");
    set_global("cp_selector", ::std::uint64_t{2u}); set_global("cp_payload_bytes", ::std::uintptr_t{input.size()});
    require(invoke_tail(999.5, 999u) == 154u, "logical operand restore reached through same-ABI musttail");
    auto const sentinel{input};
    set_global("cp_selector", ::std::uint64_t{99u}); require(invoke(0.5, 0u) == UINT64_MAX && input == sentinel, "unknown logical selector rejects without input mutation");
    set_global("cp_selector", ::std::uint64_t{2u}); set_global("cp_payload_bytes", ::std::uintptr_t{47u});
    require(invoke(0.5, 0u) == UINT64_MAX && input == sentinel, "bad packet extent rejects before values");
    set_global("cp_payload_bytes", ::std::uintptr_t{48u}); markers[1u] = 2u;
    require(invoke(0.5, 0u) == UINT64_MAX && input == sentinel, "all flags prevalidated before native local stores");
    markers[1u] = 0u; require(invoke(0.5, 0u) == UINT64_MAX && input == sentinel, "unavailable proven parameter rejects");
    markers[1u] = 1u; set_global("cp_payload", static_cast<void*>(nullptr));
    require(invoke(0.5, 0u) == UINT64_MAX && input == sentinel, "null payload rejects before payload read");
    require(invoke_ordinary(6.5, 9u) == 24u && calls() == 9u,
        "actual ordinary typed code neither reads invalid context nor calls its supplier");
    require(calls() == 9u, "one private context DATA call per selected typed entry");
    ::fast_io::io::println("checkpoint typed ABI context IR: passed unchanged f64/i64 ABI, musttail, logical landing and negative bounds");
}
