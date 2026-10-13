// Actual LLVM normal-child-return / post-call SSA landing component. It does
// not mint a VM returned-child, pause, GC/resource or whole-restore capability.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/checkpoint/caller_return_projection.h>
#include <uwvm2/runtime/checkpoint/dynamic_native_packet.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <fast_io.h>
#include <array>
#include <bit>
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_initialization_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_packet_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_resume_landing_emit.h>
}
namespace cp = ::uwvm2::runtime::checkpoint;
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static void require(bool condition, char const* text)
{
    if(!condition) { ::fast_io::io::perrln("checkpoint LLVM caller return: ", ::fast_io::mnp::os_c_str(text)); ::fast_io::fast_terminate(); }
}
inline constexpr ::std::size_t local_count{1u}, slot_count{6u}, packet_bytes{slot_count * cp::native_slot_bytes}, result_count{3u};
struct packet { ::std::array<::std::byte, packet_bytes> values{}; ::std::array<::std::uint8_t, local_count> flags{}; };
static_assert(offsetof(packet, flags) == packet_bytes && sizeof(packet) == packet_bytes + local_count);
static cp::types::core_value_type i31_type()
{ return {cp::types::value_kind::reference, {static_cast<::std::int_least64_t>(cp::types::abstract_heap_type::i31)}, false}; }
static cp::function_plan make_plan()
{
    cp::function_plan plan{}; plan.profile = cp::compilation_profile::create_for_trusted_manager();
    plan.function_generation = 1u; plan.expression_bytes = 16u;
    cp::safepoint_layout waiting{}; waiting.identifier = 1u; waiting.opcode_offset = 2u; waiting.caller_return_offset = 3u;
    waiting.phase = cp::frame_phase::awaiting_call_return; waiting.local_count = local_count; waiting.operand_count = 1u; waiting.saved_parameter_count = 1u;
    waiting.slots = {{{cp::types::value_kind::i64}, true}, {{cp::types::value_kind::i64}, true}, {{cp::types::value_kind::i64}, true}};
    cp::control_layout function{}; function.end_offset = 15u; waiting.controls.push_back(function);
    cp::control_layout saved{}; saved.kind = cp::control_kind::if_then; saved.entry_offset = 1u; saved.end_offset = 14u;
    saved.outer_operand_height = 1u; saved.saved_parameter_count = 1u; saved.declared_parameters = {{cp::types::value_kind::i64}};
    waiting.controls.push_back(saved); auto after{waiting}; after.identifier = 2u; after.opcode_offset = 3u;
    after.phase = cp::frame_phase::before_opcode; after.caller_return_offset = 0u; after.operand_count = 4u;
    after.slots = {waiting.slots[0u], waiting.slots[1u], {{cp::types::value_kind::i64}, true},
        {{cp::types::value_kind::v128}, true}, {i31_type(), true}, waiting.slots[2u]};
    plan.sites.push_back(::std::move(waiting)); plan.sites.push_back(::std::move(after)); return plan;
}
template<typename T> static cp::native_value data(cp::types::core_value_type type, T const& bits)
{
    static_assert(sizeof(T) <= cp::native_slot_bytes); cp::native_value value{}; value.declaration = {type, true};
    // [constructed owning 16-byte native_value bits] end
    // [safe] complete T width checked before fixed-object copy; no token read.
    ::fast_io::freestanding::my_memcpy(value.bits.data(), ::std::addressof(bits), sizeof(T)); return value;
}
int main()
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual LLVM native target");
    ::llvm::LLVMContext context{}; auto module{::std::make_unique<::llvm::Module>("checkpoint-caller-return", context)};
    ::std::unique_ptr<::llvm::TargetMachine> actual_target{::llvm::EngineBuilder{}.selectTarget()};
    require(bool(actual_target), "actual native target layout before selected continuation IR");
    module->setDataLayout(actual_target->createDataLayout());
    auto const integer{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const counter{new ::llvm::GlobalVariable{*module, ::llvm::Type::getInt64Ty(context), false,
        ::llvm::GlobalValue::ExternalLinkage, ::llvm::ConstantInt::get(::llvm::Type::getInt64Ty(context), 0u), "checkpoint_calls"}};
    auto const child_signature{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), {::llvm::Type::getInt64Ty(context), integer}, false)};
    auto const child{::llvm::Function::Create(child_signature, ::llvm::GlobalValue::ExternalLinkage, "checkpoint_child", *module)};
    child->addFnAttr(::llvm::Attribute::NoInline);
    auto const child_entry{::llvm::BasicBlock::Create(context, "entry", child)}; ::llvm::IRBuilder<> leaf{child_entry};
    auto const calls{leaf.CreateLoad(leaf.getInt64Ty(), counter)}; leaf.CreateStore(leaf.CreateAdd(calls, leaf.getInt64(1u)), counter);
    auto const result_pointer{leaf.CreateIntToPtr(child->getArg(1u), leaf.getPtrTy())};
    // [actual caller-owned result array[3*16]] end
    // [safe] the component driver/caller passes only its complete live owner;
    // exact 48-byte extent before clear, stores or input pointer advances.
    leaf.CreateMemSet(result_pointer, leaf.getInt8(0u), result_count * cp::native_slot_bytes, ::llvm::Align{1u});
    auto const number_store{leaf.CreateStore(child->getArg(0u), result_pointer)}; number_store->setAlignment(::llvm::Align{1u});
    ::std::array<::std::uint8_t, 16u> expected_vector{};
    for(::std::size_t i{}; i != expected_vector.size(); ++i) { expected_vector[i] = static_cast<::std::uint8_t>(i * 9u + 3u); }
    auto const vector{::llvm::ConstantDataVector::get(context, ::llvm::ArrayRef<::std::uint8_t>{expected_vector})};
    // [actual result packet slot0 | slot1 | slot2] end
    // [safe] fixed complete 3*16 owner and 16-byte vector width before +16 GEP.
    auto const vector_slot{leaf.CreateInBoundsGEP(leaf.getInt8Ty(), result_pointer, leaf.getInt64(cp::native_slot_bytes))};
    auto const vector_store{leaf.CreateStore(vector, vector_slot)}; vector_store->setAlignment(::llvm::Align{1u});
    auto const payload_type{leaf.getIntNTy(static_cast<unsigned>(sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) * CHAR_BIT))};
    ::llvm::Value* payload{::llvm::ConstantInt::get(payload_type, 0x7fff'ffffu)};
    constexpr bool little{::std::endian::native == ::std::endian::little};
    if constexpr(!little && sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) > sizeof(cp::types::wasm_i31))
    { payload = leaf.CreateShl(payload, static_cast<unsigned>((sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) - sizeof(cp::types::wasm_i31)) * CHAR_BIT)); }
    auto const reference{emit::emit_llvm_jit_ref_from_payload(leaf, payload, ::uwvm2::object::global::wasm_ref_kind::wasm_i31, little)};
    // [actual result packet slot0 | slot1 | slot2] end
    // [safe] complete 48 bytes and reference width<=16 before +32 GEP/store.
    auto const reference_slot{leaf.CreateInBoundsGEP(leaf.getInt8Ty(), result_pointer, leaf.getInt64(2u * cp::native_slot_bytes))};
    auto const reference_store{leaf.CreateStore(reference, reference_slot)}; reference_store->setAlignment(::llvm::Align{1u}); leaf.CreateRetVoid();
    auto const signature{::llvm::FunctionType::get(integer, {::llvm::Type::getInt64Ty(context), integer, integer, integer, integer, integer}, false)};
    auto const caller{::llvm::Function::Create(signature, ::llvm::GlobalValue::ExternalLinkage, "checkpoint_caller", *module)};
    auto const entry{::llvm::BasicBlock::Create(context, "entry", caller)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{}; state.llvm_context_holder = &context; state.llvm_module = module.get(); state.llvm_function = caller;
    state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry); auto& builder{*state.ir_builder};
    auto plan{make_plan()}; require(cp::validate_plan(plan) == cp::status::ok, "exact waiting and after-call Core3 metadata");
    state.checkpoint_plan = &plan; state.local_types.resize(local_count); state.local_pointers.resize(local_count);
    state.local_types[0u] = emit::checkpoint_packet_physical_carrier(plan.sites[0u].slots[0u].type);
    state.local_pointers[0u] = emit::create_llvm_jit_entry_block_alloca(builder, builder.getInt64Ty(), nullptr, "actual.local");
    builder.CreateStore(builder.getInt64(3u), state.local_pointers[0u]); ::llvm::AllocaInst* flags{};
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state, 0u, flags), "default local executed initialization");
    auto const result_array{::llvm::ArrayType::get(builder.getInt8Ty(), result_count * cp::native_slot_bytes)};
    auto const normal_results{emit::create_llvm_jit_entry_block_alloca(builder, result_array, nullptr, "actual.child.results")};
    auto const rejected{::llvm::BasicBlock::Create(context, "reject", caller)}; ::llvm::IRBuilder<> reject{rejected};
    reject.CreateRet(::llvm::ConstantInt::get(integer, UINTPTR_MAX));
    emit::llvm_jit_checkpoint_resume_dispatch_emit_state dispatch{};
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state, flags, caller->getArg(0u),
        builder.CreateIntToPtr(caller->getArg(1u), builder.getPtrTy()), caller->getArg(2u),
        builder.CreateIntToPtr(caller->getArg(3u), builder.getPtrTy()), caller->getArg(4u), rejected, dispatch), "actual logical native dispatch");
    // This actual normal-edge call has a visible generated side effect. A
    // selected after-call restore case MUST bypass it and consume the separately
    // executed child's complete actual returned tuple from the private driver.
    builder.CreateCall(child, {builder.getInt64(7u), builder.CreatePtrToInt(normal_results, integer)});
    ::std::array<cp::types::core_value_type, result_count> result_types{{{cp::types::value_kind::i64}, {cp::types::value_kind::v128}, i31_type()}};
    ::std::array<::llvm::Value*, 5u> normal{builder.getInt64(5u), nullptr, nullptr, nullptr, builder.getInt64(11u)};
    for(::std::size_t i{}; i != result_count; ++i)
    {
        auto const native_type{emit::get_llvm_type_from_wasm_value_type(context, emit::checkpoint_packet_physical_carrier(result_types[i]))};
        // [actual same-function child results[48]] end
        // [safe] i<3 and exact alloca extent before offset i*16/typed load.
        auto const slot{builder.CreateInBoundsGEP(builder.getInt8Ty(), normal_results, builder.getInt64(i * cp::native_slot_bytes))};
        auto const returned{builder.CreateLoad(native_type, slot)}; returned->setAlignment(::llvm::Align{1u}); normal[i + 1u] = returned;
    }
    ::std::array<cp::types::core_value_type, 5u> exact{{{cp::types::value_kind::i64}, result_types[0u], result_types[1u], result_types[2u], {cp::types::value_kind::i64}}};
    auto const merged{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, dispatch, plan.sites[1u], normal, exact)};
    require(merged.valid && merged.selected && merged.actual_nonlocals.size() == normal.size(), "actual after-call operand/results/saved SSA PHIs");
    auto const captured{emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state, plan.sites[1u], flags, merged.actual_nonlocals)};
    require(captured.valid && captured.slots_bytes == packet_bytes && captured.local_flags == local_count, "complete actual post-call capture");
    auto const output{builder.CreateIntToPtr(caller->getArg(5u), builder.getPtrTy())};
    // [actual synchronous native driver's owning output packet[97]] end
    // [safe] sizeof/offsetof proof above before 96-byte copy and +96 flag GEP.
    builder.CreateMemCpy(output, ::llvm::Align{1u}, builder.CreateIntToPtr(captured.slots_address, builder.getPtrTy()), ::llvm::Align{1u}, packet_bytes);
    auto const output_flags{builder.CreateInBoundsGEP(builder.getInt8Ty(), output, builder.getInt64(packet_bytes))};
    builder.CreateMemCpy(output_flags, ::llvm::Align{1u}, builder.CreateIntToPtr(captured.flags_address, builder.getPtrTy()), ::llvm::Align{1u}, local_count);
    auto const local{builder.CreateLoad(builder.getInt64Ty(), state.local_pointers[0u])};
    builder.CreateRet(builder.CreateZExtOrTrunc(builder.CreateAdd(builder.CreateAdd(local, merged.actual_nonlocals[0u]),
        builder.CreateAdd(merged.actual_nonlocals[1u], merged.actual_nonlocals[4u])), integer));
    require(!::llvm::verifyModule(*module), "complete real child-call/dispatch/PHI native module verifies");
    auto const sealed{cp::sealed_function_plan::seal_compiler_metadata(plan)};
    auto const projection{cp::caller_return_projection::seal_compiler_data(sealed, 1u, 2u, result_types)};
    require(bool(projection) && !projection->returned_child_execution_authority(), "sealed projection remains DATA without VM return authority");
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::move(module)}.setEngineKind(::llvm::EngineKind::JIT).create()};
    require(bool(engine), "actual native MCJIT engine"); engine->finalizeObject();
    auto const caller_address{engine->getFunctionAddress("checkpoint_caller")}, child_address{engine->getFunctionAddress("checkpoint_child")};
    auto const counter_address{engine->getGlobalValueAddress("checkpoint_calls")}; require(caller_address != 0u && child_address != 0u && counter_address != 0u, "real owned native symbols");
    using caller_entry = ::std::uintptr_t(*)(::std::uint64_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t);
    using child_entry_type = void(*)(::std::uint64_t, ::std::uintptr_t);
    // [actual named MCJIT symbols retained by this live engine] end
    // [safe] exact integer-only component ABIs / constructed global object;
    // neither an untrusted saved PC nor a serialized host pointer is invoked.
    auto const run{reinterpret_cast<caller_entry>(static_cast<::std::uintptr_t>(caller_address))};
    auto const run_child{reinterpret_cast<child_entry_type>(static_cast<::std::uintptr_t>(child_address))};
    auto const call_count{reinterpret_cast<::std::uint64_t const*>(static_cast<::std::uintptr_t>(counter_address))};
    packet normal_output{};
    require(run(0u, 0u, 0u, 0u, 0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(normal_output))) == 26u && *call_count == 1u,
        "normal selected entry performs original child call exactly once");
    packet rejected_output{}; rejected_output.values.fill(::std::byte{0x5au}); rejected_output.flags.fill(0x5au);
    auto const unchanged{[&]
    {
        for(auto const byte : rejected_output.values) { require(byte == ::std::byte{0x5au}, "rejected selector/packet cannot mutate output values"); }
        for(auto const flag : rejected_output.flags) { require(flag == 0x5au, "rejected selector/packet cannot mutate original-index output flags"); }
    }};
    require(run(99u, 0u, 0u, 0u, 0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(rejected_output))) == UINTPTR_MAX && *call_count == 1u,
        "unknown logical site rejects without child execution or output mutation"); unchanged();
    ::std::array<::std::byte, result_count * cp::native_slot_bytes> actual_leaf_results{};
    run_child(70u, reinterpret_cast<::std::uintptr_t>(actual_leaf_results.data()));
    require(*call_count == 2u, "real separate generated child returns before typed tuple transfer");
    ::std::array<cp::native_value, result_count> returned{};
    for(::std::size_t i{}; i != result_count; ++i)
    {
        returned[i].declaration = {result_types[i], true};
        // [owned real returned child native results[3*16]] end
        // [safe] child returned, i<3 BEFORE offset i*16 and complete slot copy.
        ::fast_io::freestanding::my_memcpy(returned[i].bits.data(), actual_leaf_results.data() + i * cp::native_slot_bytes, cp::native_slot_bytes);
    }
    cp::logical_frame waiting{}; waiting.identity = {3u, 0u, 4u, 9u}; waiting.plan = sealed; waiting.site = 1u; waiting.materialized = true;
    waiting.values = {data({cp::types::value_kind::i64}, ::std::uint64_t{30u}), data({cp::types::value_kind::i64}, ::std::uint64_t{50u}),
        data({cp::types::value_kind::i64}, ::std::uint64_t{110u})};
    cp::logical_frame after{}; require(projection->project_typed_return_data(waiting, returned, after) == cp::status::ok && after.values.size() == slot_count,
        "actual returned multi-result tuple projected into exact post-call layout as DATA");
    packet input{}; input.flags = {1u};
    for(::std::size_t i{}; i != after.values.size(); ++i)
    {
        // [driver-owned input packet slots0..6] end
        // [safe] exact validated six values before offset i*16 and slot copy.
        ::fast_io::freestanding::my_memcpy(input.values.data() + i * cp::native_slot_bytes, after.values[i].bits.data(), cp::native_slot_bytes);
    }
    input.flags[0u] = 2u;
    require(run(after.site, reinterpret_cast<::std::uintptr_t>(input.values.data()), packet_bytes,
        reinterpret_cast<::std::uintptr_t>(input.flags.data()), local_count, reinterpret_cast<::std::uintptr_t>(::std::addressof(rejected_output))) == UINTPTR_MAX &&
        *call_count == 2u, "invalid returned-parent flag rejects before payload/output mutation without repeating child"); unchanged();
    input.flags[0u] = 1u;
    require(run(after.site, 0u, packet_bytes, reinterpret_cast<::std::uintptr_t>(input.flags.data()), local_count,
        reinterpret_cast<::std::uintptr_t>(::std::addressof(rejected_output))) == UINTPTR_MAX && *call_count == 2u,
        "null returned-parent input rejects before native byte access and child execution"); unchanged();
    packet restored{};
    require(run(after.site, reinterpret_cast<::std::uintptr_t>(input.values.data()), packet_bytes,
        reinterpret_cast<::std::uintptr_t>(input.flags.data()), local_count, reinterpret_cast<::std::uintptr_t>(::std::addressof(restored))) == 260u && *call_count == 2u,
        "actual restored parent executes after call without repeating already returned child");
    cp::dynamic_native_packet::owner captured_packet{};
    require(cp::dynamic_native_packet::copy_compiler_packet(sealed, 2u, restored.values, restored.flags, captured_packet) == cp::status::ok && captured_packet,
        "actual resulting native packet validates complete exact typed DATA");
    auto const values{captured_packet->values()};
    require(values[3u].bits == returned[1u].bits && values[4u].bits == returned[2u].bits && values[5u].bits == waiting.values[2u].bits,
        "returned v128/nonnullable i31 and saved caller parameter survive real SSA landing");
    packet final_normal{};
    require(run(0u, 0u, 0u, 0u, 0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(final_normal))) == 26u && *call_count == 3u,
        "subsequent normal entry still performs its own child call");
    ::fast_io::io::println("checkpoint real LLVM caller-return/typed-result/SSA bypass component PASS; full-VM restore authority=false");
}
