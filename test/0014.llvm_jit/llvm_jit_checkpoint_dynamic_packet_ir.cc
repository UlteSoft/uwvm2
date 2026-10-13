// Real LLVM native helper component. Metadata here is component DATA, never
// actual Wasm source/activation/root/host admission or restore authority.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/checkpoint/dynamic_native_packet.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <fast_io.h>
#include <array>
#include <bit>
#include <cstring>
#include <memory>
namespace uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details
{
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_initialization_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_packet_emit.h>
}
namespace cp = ::uwvm2::runtime::checkpoint;
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static void require(bool ok, char const* message)
{ if(!ok) { ::fast_io::io::perrln("checkpoint dynamic packet IR: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
inline constexpr ::std::size_t local_count{7u}, slot_count{9u}, payload_bytes{slot_count * cp::native_slot_bytes};
struct output_packet
{
    ::std::array<::std::byte, payload_bytes> slots{};
    ::std::array<::std::uint8_t, local_count> flags{};
};
static_assert(offsetof(output_packet, flags) == payload_bytes);
static_assert(sizeof(output_packet) == payload_bytes + local_count);

static cp::function_plan make_plan()
{
    cp::function_plan p{}; p.profile = cp::compilation_profile::create_for_trusted_manager();
    p.expression_bytes = 16u; p.function_generation = 1u;
    auto const i31_heap{static_cast<::std::int_least64_t>(cp::types::abstract_heap_type::i31)};
    cp::safepoint_layout entry{}; entry.identifier = 1u; entry.local_count = local_count;
    entry.slots = {{{cp::types::value_kind::reference, {i31_heap}, false}, true},
                   {{cp::types::value_kind::reference, {i31_heap}, false}, false},
                   {{cp::types::value_kind::i32}, true},
                   {{cp::types::value_kind::reference, {i31_heap}, true}, true},
                   {{cp::types::value_kind::f32}, true},
                   {{cp::types::value_kind::f64}, true},
                   {{cp::types::value_kind::v128}, true}};
    cp::control_layout function{}; function.end_offset = 15u; entry.controls.push_back(function);
    p.sites.push_back(entry);
    auto at_merge{entry}; at_merge.identifier = 2u; at_merge.opcode_offset = 2u;
    at_merge.operand_count = 1u; at_merge.saved_parameter_count = 1u;
    at_merge.slots.push_back({{cp::types::value_kind::reference, {i31_heap}, false}, true});
    at_merge.slots.push_back({{cp::types::value_kind::i64}, true});
    cp::control_layout saved_if{}; saved_if.kind = cp::control_kind::if_then;
    saved_if.entry_offset = 1u; saved_if.end_offset = 14u; saved_if.outer_operand_height = 1u;
    saved_if.saved_parameter_count = 1u; saved_if.declared_parameters = {{cp::types::value_kind::i64}};
    at_merge.controls.push_back(::std::move(saved_if)); p.sites.push_back(::std::move(at_merge));
    return p;
}

int main()
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual LLVM native target initialized");
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("checkpoint-dynamic-packet", context)};
    // Actual reusable native owners require an explicit target ABI before any
    // packet IR. A default empty DataLayout cannot attest pointer-width bounds.
    ::llvm::EngineBuilder select{};select.setEngineKind(::llvm::EngineKind::JIT);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{select.selectTarget()}};
    require(bool(target), "actual native packet target");
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto const integer{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const signature{::llvm::FunctionType::get(integer, {integer, integer}, false)};
    auto const function{::llvm::Function::Create(signature, ::llvm::GlobalValue::ExternalLinkage, "checkpoint_packet", *module)};
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder = &context; state.llvm_module = module.get(); state.llvm_function = function;
    state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry);
    auto& builder{*state.ir_builder}; auto metadata{make_plan()};
    require(cp::validate_plan(metadata) == cp::status::ok, "bounded exact Core3 site data");
    auto const default_packet{emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state, metadata.sites[1u], nullptr, {})};
    require(default_packet.valid && default_packet.slots_address == nullptr && entry->empty(), "ordinary engine emits no packet IR");
    state.checkpoint_plan = &metadata; state.local_types.resize(local_count); state.local_pointers.resize(local_count);
    for(::std::size_t index{}; index != local_count; ++index)
    {
        state.local_types[index] = emit::checkpoint_packet_physical_carrier(metadata.sites[0u].slots[index].type);
        auto const type{emit::get_llvm_type_from_wasm_value_type(context, state.local_types[index])};
        state.local_pointers[index] = emit::create_llvm_jit_entry_block_alloca(builder, type, nullptr, "actual.typed.local");
        require(state.local_pointers[index] != nullptr, "actual original-index native typed alloca");
    }
    ::llvm::AllocaInst* flags{};
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state, 1u, flags) && flags != nullptr,
        "actual executed initialization flags");
    auto const payload_type{builder.getIntNTy(static_cast<unsigned>(sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) * CHAR_BIT))};
    constexpr bool little{::std::endian::native == ::std::endian::little};
    auto make_i31{[&](::std::uint32_t value)
    {
        ::llvm::Value* payload{::llvm::ConstantInt::get(payload_type, value)};
        if constexpr(!little && sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) > sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31))
        { payload = builder.CreateShl(payload, static_cast<unsigned>((sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) -
            sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31)) * CHAR_BIT)); }
        return emit::emit_llvm_jit_ref_from_payload(builder, payload, ::uwvm2::object::global::wasm_ref_kind::wasm_i31, little);
    }};
    auto const i31{make_i31(17u)}, operand_i31{make_i31(0x7fffffffu)};
    require(i31 != nullptr && operand_i31 != nullptr, "real tagged i31 SSA carriers");
    builder.CreateStore(i31, state.local_pointers[0u]);
    // Original index 1 deliberately has NO initial value store or load.
    builder.CreateStore(builder.getInt32(0x01234567u), state.local_pointers[2u]);
    auto const null_ref{emit::emit_llvm_jit_ref_from_payload(builder, ::llvm::ConstantInt::get(payload_type, 0u),
        ::uwvm2::object::global::wasm_ref_kind::wasm_null, little)};
    require(null_ref != nullptr, "actual nullable local null carrier"); builder.CreateStore(null_ref, state.local_pointers[3u]);
    builder.CreateStore(builder.CreateBitCast(builder.getInt32(0x7fc01234u), builder.getFloatTy()), state.local_pointers[4u]);
    builder.CreateStore(builder.CreateBitCast(builder.getInt64(0x7ff8000000000123u), builder.getDoubleTy()), state.local_pointers[5u]);
    ::std::array<::std::uint8_t, 16u> vector_bytes{};
    for(::std::size_t i{}; i != vector_bytes.size(); ++i) { vector_bytes[i] = static_cast<::std::uint8_t>(i * 13u + 1u); }
    auto const vector_value{::llvm::ConstantDataVector::get(context, ::llvm::ArrayRef<::std::uint8_t>{vector_bytes.data(), vector_bytes.size()})};
    builder.CreateStore(vector_value, state.local_pointers[6u]);
    ::std::array<::llvm::Value*, 2u> live_values{operand_i31, builder.getInt64(0x8899aabbccddeeffu)};
    auto copied_site{metadata.sites[1u]}; auto const before_bad_site{entry->size()};
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state, copied_site, flags, live_values).valid &&
        entry->size() == before_bad_site, "copied site labels cannot substitute actual builder site identity");
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state, metadata.sites[1u], flags,
        {live_values.data(), 1u}).valid && entry->size() == before_bad_site, "truncated nonlocal SSA count rejects before packet IR");
    auto const assigned{::llvm::BasicBlock::Create(context, "actual_assignment", function)};
    auto const merged{::llvm::BasicBlock::Create(context, "actual_merge", function)};
    builder.CreateCondBr(builder.CreateICmpEQ(function->getArg(0u), ::llvm::ConstantInt::get(integer, 1u)), assigned, merged);
    builder.SetInsertPoint(assigned); builder.CreateStore(i31, state.local_pointers[1u]);
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(state, flags, 1u), "actual value-store edge marks original slot");
    builder.CreateBr(merged); builder.SetInsertPoint(merged);
    auto const corrupt_flag{::llvm::BasicBlock::Create(context, "component_fault_flag", function)};
    auto const capture{::llvm::BasicBlock::Create(context, "capture", function)};
    builder.CreateCondBr(builder.CreateICmpEQ(function->getArg(0u), ::llvm::ConstantInt::get(integer, 2u)), corrupt_flag, capture);
    builder.SetInsertPoint(corrupt_flag);
    auto const actual_flags_type{::llvm::cast<::llvm::ArrayType>(flags->getAllocatedType())};
    // Component-only native fault injection, not a guest writable flag path.
    // [actual flags[7] ... 1 ...] end
    // [safe                    ] 1<7 BEFORE byte GEP; no local value store.
    auto const faulty_cell{builder.CreateInBoundsGEP(actual_flags_type, flags, {builder.getInt32(0u), builder.getInt64(1u)})};
    builder.CreateStore(builder.getInt8(2u), faulty_cell); builder.CreateBr(capture); builder.SetInsertPoint(capture);
    auto const packet{emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state, metadata.sites[1u], flags, live_values)};
    require(packet.valid && packet.slots_bytes == payload_bytes && packet.local_flags == local_count, "complete actual conditional native packet IR");
    auto const output{builder.CreateIntToPtr(function->getArg(1u), builder.getPtrTy())};
    auto const slots{builder.CreateIntToPtr(packet.slots_address, builder.getPtrTy())};
    auto const saved_flags{builder.CreateIntToPtr(packet.flags_address, builder.getPtrTy())};
    // [actual caller-owned output_packet[151] bytes] end
    // [safe sizeof/offsetof assertions above      ] the test passes only that
    // real complete object; bounded packet144 and flags7 precede copy/GEP.
    builder.CreateMemCpy(output, ::llvm::Align{1u}, slots, ::llvm::Align{1u}, payload_bytes);
    auto const output_flags{builder.CreateInBoundsGEP(builder.getInt8Ty(), output, builder.getInt64(payload_bytes))};
    builder.CreateMemCpy(output_flags, ::llvm::Align{1u}, saved_flags, ::llvm::Align{1u}, local_count);
    // [actual bounded captured flags[7] ... 1 ...] flags_end
    // [safe                                     ] 1<7 before byte GEP.
    auto const original_one{builder.CreateInBoundsGEP(builder.getInt8Ty(), saved_flags, builder.getInt64(1u))};
    builder.CreateRet(builder.CreateZExt(builder.CreateLoad(builder.getInt8Ty(), original_one), integer));
    require(!::llvm::verifyModule(*module), "real generated conditional packet LLVM IR verified");
    auto const sealed{cp::sealed_function_plan::seal_compiler_metadata(metadata)}; require(bool(sealed), "owned exact metadata data component");
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::move(module)}.setEngineKind(::llvm::EngineKind::JIT).create(target.release())};
    require(bool(engine), "real native JIT engine"); engine->finalizeObject();
    auto const address{engine->getFunctionAddress("checkpoint_packet")}; require(address != 0u, "actual emitted native entry");
    using entry_type = ::std::uintptr_t(*)(::std::uintptr_t, ::std::uintptr_t);
    // [actual MCJIT-owned emitted function] engine remains live through calls.
    // [safe                              ] exact integer-only prototype and
    // real named symbol resolution, never a serialized/native guest address.
    auto const native{reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))};
    cp::dynamic_native_packet::owner last_captured{};
    for(::std::uintptr_t executed{}; executed != 2u; ++executed)
    {
        output_packet output_value{}; output_value.slots.fill(::std::byte{0x5au}); output_value.flags.fill(0x5au);
        require(native(executed, reinterpret_cast<::std::uintptr_t>(::std::addressof(output_value))) == executed,
            "actual selected native assignment survives merge");
        cp::dynamic_native_packet::owner captured{};
        require(cp::dynamic_native_packet::copy_compiler_packet(sealed, 2u, output_value.slots, output_value.flags, captured) == cp::status::ok &&
            captured && captured->values().size() == slot_count, "actual native typed packet accepted as bounded component data");
        auto const values{captured->values()};
        require(values[1u].declaration.initialized == (executed != 0u), "unset versus assigned preserved after proof merge");
        if(executed == 0u)
        { for(auto byte : values[1u].bits) { require(byte == ::std::byte{}, "unset nondefaultable slot zero without loading its uninitialized alloca"); } }
        else
        {
            cp::native_reference reference{}; ::std::memcpy(::std::addressof(reference), values[1u].bits.data(), sizeof(reference));
            require(reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31 && reference.storage.wasm_i31.get_s() == 17,
                "actual initialized nonnull i31 carrier");
        }
        ::std::uint32_t f32_bits{}; ::std::uint64_t f64_bits{}, saved_parameter{};
        ::std::memcpy(::std::addressof(f32_bits), values[4u].bits.data(), sizeof(f32_bits));
        ::std::memcpy(::std::addressof(f64_bits), values[5u].bits.data(), sizeof(f64_bits));
        ::std::memcpy(::std::addressof(saved_parameter), values[8u].bits.data(), sizeof(saved_parameter));
        require(f32_bits == 0x7fc01234u && f64_bits == 0x7ff8000000000123u && saved_parameter == 0x8899aabbccddeeffu,
            "actual IEEE NaN payloads and saved control parameter bits preserved");
        for(::std::size_t i{}; i != vector_bytes.size(); ++i)
        { require(values[6u].bits[i] == static_cast<::std::byte>(vector_bytes[i]), "actual complete v128 native bits preserved"); }
        cp::native_reference operand{}; ::std::memcpy(::std::addressof(operand), values[7u].bits.data(), sizeof(operand));
        require(operand.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31 && operand.storage.wasm_i31.get_s() == -1,
            "actual complete typed operand carrier");
        require(captured->executable_restore_capability() == cp::status::unavailable_resume,
            "native packet is not a complete continuation or actual restore capability");
        last_captured = ::std::move(captured);
    }
    output_packet invalid_flags{};
    require(native(2u, reinterpret_cast<::std::uintptr_t>(::std::addressof(invalid_flags))) == 2u,
        "component noncanonical marker copied as marker, never interpreted as truthy initialized");
    // [actual native output payload ... slot1 at offset16 ... slot_end32]
    // [safe] complete144-byte owned array and 16+16<=144 BEFORE byte indexing.
    for(::std::size_t i{}; i != cp::native_slot_bytes; ++i)
    { require(invalid_flags.slots[cp::native_slot_bytes + i] == ::std::byte{}, "invalid marker branch never loads unset nondefaultable local"); }
    auto const earlier{last_captured};
    require(cp::dynamic_native_packet::copy_compiler_packet(sealed, 2u, invalid_flags.slots, invalid_flags.flags, last_captured) ==
        cp::status::invalid_layout && earlier == last_captured, "all flags validated before payload and prior owned packet retained on rejection");
    ::fast_io::io::println("checkpoint dynamic typed packet LLVM IR/native helper PASS; whole-state continuation/restore acceptance=false");
}
