// Real LLVM logical-site dispatch/typed SSA landing component, not actual VM
// pause/GC/host/continuation authority or whole-instance restore acceptance.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
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
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_control_storage_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_initialization_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_packet_emit.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_checkpoint_resume_landing_emit.h>
}
namespace cp = ::uwvm2::runtime::checkpoint;
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
static void require(bool valid, char const* message)
{
    if(!valid) { ::fast_io::io::perrln("checkpoint resume landing IR: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
inline constexpr ::std::size_t locals{3u}, slots{6u}, bytes{slots * cp::native_slot_bytes};
struct packet { ::std::array<::std::byte, bytes> values{}; ::std::array<::std::uint8_t, locals> flags{}; };
static_assert(offsetof(packet, flags) == bytes && sizeof(packet) == bytes + locals);
static cp::function_plan make_plan()
{
    cp::function_plan plan{}; plan.profile = cp::compilation_profile::create_for_trusted_manager();
    plan.expression_bytes = 16u; plan.function_generation = 1u;
    auto const i31{static_cast<::std::int_least64_t>(cp::types::abstract_heap_type::i31)};
    cp::safepoint_layout entry{}; entry.identifier = 1u; entry.local_count = locals;
    entry.slots = {{{cp::types::value_kind::i64}, true},
        {{cp::types::value_kind::reference, {i31}, false}, false}, {{cp::types::value_kind::v128}, true}};
    cp::control_layout function{}; function.end_offset = 15u; entry.controls.push_back(function); plan.sites.push_back(entry);
    auto point{entry}; point.identifier = 2u; point.opcode_offset = 2u; point.operand_count = 1u; point.saved_parameter_count = 2u;
    point.slots.push_back({{cp::types::value_kind::i64}, true}); point.slots.push_back({{cp::types::value_kind::i64}, true});
    point.slots.push_back({{cp::types::value_kind::reference, {i31}, false}, true});
    cp::control_layout saved{}; saved.kind = cp::control_kind::if_then; saved.entry_offset = 1u; saved.end_offset = 14u;
    saved.outer_operand_height = 1u; saved.saved_parameter_count = 2u;
    saved.declared_parameters = {{cp::types::value_kind::i64}, {cp::types::value_kind::reference, {i31}, false}};
    point.controls.push_back(::std::move(saved)); plan.sites.push_back(::std::move(point)); return plan;
}
template<typename T> static void set_slot(packet& target, ::std::size_t index, T const& value)
{
    static_assert(sizeof(T) <= cp::native_slot_bytes);
    require(index < slots, "native test slot bound");
    // [main-owned packet slots0 ... index*16 ... slots*16] end
    // [safe                                             ] index<slots and
    // sizeof(T)<=16 BEFORE advancing this owning native byte array.
    ::fast_io::freestanding::my_memcpy(target.values.data() + index * cp::native_slot_bytes, ::std::addressof(value), sizeof(T));
}
int main()
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "real LLVM native target");
    ::llvm::LLVMContext context{}; auto module{::std::make_unique<::llvm::Module>("checkpoint-resume-landing", context)};
    ::std::unique_ptr<::llvm::TargetMachine> actual_target{::llvm::EngineBuilder{}.selectTarget()};
    require(bool(actual_target), "actual native target layout before selected continuation IR");
    module->setDataLayout(actual_target->createDataLayout());
    auto const integer{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const signature{::llvm::FunctionType::get(integer, {::llvm::Type::getInt64Ty(context), integer, integer, integer, integer, integer}, false)};
    auto const function{::llvm::Function::Create(signature, ::llvm::GlobalValue::ExternalLinkage, "checkpoint_resume", *module)};
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder = &context; state.llvm_module = module.get(); state.llvm_function = function;
    state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry);
    auto& builder{*state.ir_builder}; auto plan{make_plan()};
    require(cp::validate_plan(plan) == cp::status::ok, "independently bounded Core3 logical sites");
    emit::llvm_jit_checkpoint_resume_dispatch_emit_state resume{};
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, resume) &&
        entry->empty() && resume.dispatch == nullptr, "ordinary null profile adds no dispatch/probe/flag IR");
    auto const disabled_landing{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, plan.sites[1u], {}, {})};
    require(disabled_landing.valid && !disabled_landing.selected && disabled_landing.actual_nonlocals.empty() && entry->empty(),
        "ordinary landing remains explicitly unselected; caller retains original SSA handles without copy");
    auto observational{plan}; observational.profile = cp::compilation_profile::create_for_trusted_observer();
    require(bool(observational.profile) && cp::validate_plan(observational) == cp::status::ok, "actual observation-only metadata");
    state.checkpoint_plan = &observational;
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, resume) &&
        entry->empty() && resume.dispatch == nullptr, "observation emits no selector, payload read or resume context IR");
    auto const observed_landing{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, observational.sites[1u], {}, {})};
    require(observed_landing.valid && !observed_landing.selected && entry->empty(), "observation does not mutate actual SSA with restore PHIs");
    observational.resume_sites = {1u}; observational.resume_abi_revision = 1u;
    require(cp::validate_plan(observational) == cp::status::invalid_plan &&
        !cp::sealed_function_plan::seal_compiler_metadata(observational), "observation cannot seal fabricated executable continuation DATA");
    observational.sites.clear(); observational.producer_availability = cp::status::quota_exceeded;
    require(cp::validate_plan(observational) == cp::status::invalid_plan &&
        !cp::sealed_function_plan::seal_compiler_metadata(observational), "empty quota decline cannot bypass observation resume metadata rejection");
    auto declined{plan}; declined.sites.clear(); declined.producer_availability = cp::status::quota_exceeded;
    require(cp::validate_plan(declined) == cp::status::ok, "empty real quota decline stays valid ordinary lowering");
    declined.resume_sites = {1u}; declined.resume_abi_revision = 1u;
    require(cp::validate_plan(declined) == cp::status::invalid_plan, "empty quota decline cannot seal an absent resume landing");
    state.checkpoint_plan = &plan; state.local_types.resize(locals); state.local_pointers.resize(locals);
    for(::std::size_t i{}; i != locals; ++i)
    {
        state.local_types[i] = emit::checkpoint_packet_physical_carrier(plan.sites[0u].slots[i].type);
        state.local_pointers[i] = emit::create_llvm_jit_entry_block_alloca(builder,
            emit::get_llvm_type_from_wasm_value_type(context, state.local_types[i]), nullptr, "actual.local");
    }
    builder.CreateStore(builder.getInt64(10u), state.local_pointers[0u]);
    // The nondefaultable i31 local 1 has no value initialization or load.
    auto const vector_type{emit::get_llvm_type_from_wasm_value_type(context, state.local_types[2u])};
    builder.CreateStore(::llvm::Constant::getNullValue(vector_type), state.local_pointers[2u]);
    ::llvm::AllocaInst* flags{};
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(state, 0u, flags), "actual new-frame default flags");
    auto const rejected{::llvm::BasicBlock::Create(context, "rejected", function)};
    ::llvm::IRBuilder<> reject_builder{rejected}; reject_builder.CreateRet(::llvm::ConstantInt::get(integer, UINTPTR_MAX));
    auto const payload{builder.CreateIntToPtr(function->getArg(1u), builder.getPtrTy())};
    auto const markers{builder.CreateIntToPtr(function->getArg(3u), builder.getPtrTy())};
    auto const actual_layout{module->getDataLayout()};
    auto const before_layout_dispatch{builder.GetInsertBlock()->size()}; auto const before_layout_blocks{function->size()};
    auto const rejects_layout{[&]()
    {
        return !emit::prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state, flags,
            function->getArg(0u), payload, function->getArg(2u), markers, function->getArg(4u), rejected, resume) &&
            builder.GetInsertBlock()->size() == before_layout_dispatch && function->size() == before_layout_blocks;
    }};
    module->setDataLayout(""); require(rejects_layout(), "empty target layout rejects before any dispatch IR");
    constexpr bool target_little{::std::endian::native == ::std::endian::little};
    module->setDataLayout(sizeof(::std::uintptr_t) == 8u ? (target_little ? "e-p:32:32" : "E-p:32:32") :
        (target_little ? "e-p:64:64" : "E-p:64:64"));
    require(rejects_layout(), "foreign pointer width rejects before any dispatch IR");
    module->setDataLayout(sizeof(::std::uintptr_t) == 8u ? (target_little ? "E-p:64:64" : "e-p:64:64") :
        (target_little ? "E-p:32:32" : "e-p:32:32"));
    require(rejects_layout(), "foreign byte order rejects before any dispatch IR");
    module->setDataLayout(actual_layout);
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state, flags,
        function->getArg(0u), payload, function->getArg(2u), markers, function->getArg(4u), rejected, resume), "real logical selector switch");
    // Genuine compiler-owned function context; component metadata alone does
    // not stand in for persistent if native storage or VM resume authority.
    state.control_stack.push_back({.type=emit::llvm_jit_control_context_type::function});
    auto const entry_site{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, plan.sites[0u], {}, {})};
    require(entry_site.valid && entry_site.selected && entry_site.actual_nonlocals.empty(), "actual logical entry landing");
    auto const ref_payload_type{builder.getIntNTy(static_cast<unsigned>(sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) * CHAR_BIT))};
    ::llvm::Value* ref_payload{::llvm::ConstantInt::get(ref_payload_type, 17u)};
    constexpr bool little{::std::endian::native == ::std::endian::little};
    if constexpr(!little && sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) > sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31))
    { ref_payload = builder.CreateShl(ref_payload, static_cast<unsigned>((sizeof(::uwvm2::object::global::wasm_global_ref_storage_u) -
        sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31)) * CHAR_BIT)); }
    auto const normal_i31{emit::emit_llvm_jit_ref_from_payload(builder, ref_payload, ::uwvm2::object::global::wasm_ref_kind::wasm_i31, little)};
    ::std::array<::llvm::Value*, 3u> normal_values{builder.getInt64(30u), builder.getInt64(40u), normal_i31};
    ::std::array<cp::types::core_value_type, 3u> exact{plan.sites[1u].slots[3u].type, plan.sites[1u].slots[4u].type, plan.sites[1u].slots[5u].type};
    ::uwvm2::utils::container::vector<emit::llvm_jit_stack_value_t> saved_parameters{};
    saved_parameters.push_back({emit::runtime_operand_stack_value_type::i64,normal_values[1u]});
    saved_parameters.push_back({emit::runtime_operand_stack_value_type::funcref,normal_values[2u]});
    ::std::size_t saved_first{};
    require(emit::prepare_runtime_local_func_llvm_jit_checkpoint_saved_if(state,saved_parameters,saved_first) &&
        saved_first==0u && state.checkpoint_saved_control_storage!=nullptr && state.checkpoint_saved_control_max_slots==2u,
        "genuine bounded entry-owned saved tuple before logical landing");
    state.control_stack.push_back({.type=emit::llvm_jit_control_context_type::if_then,
        .entry_params=saved_parameters,.checkpoint_saved_parameter_first=saved_first});
    auto restored_witness{saved_parameters[1u]};restored_witness.known_ref_func_index=5u;
    restored_witness.immutable_gc_values_witness=normal_values[1u];restored_witness.immutable_gc_values_witness_block=builder.GetInsertBlock();
    restored_witness.immutable_gc_values_witness_type=7u;restored_witness.immutable_gc_values_witness_next_offset=3u;
    emit::invalidate_runtime_local_func_llvm_jit_checkpoint_value_witness(state,restored_witness);
    require(restored_witness.known_ref_func_index==SIZE_MAX && restored_witness.immutable_gc_values_witness==nullptr &&
        restored_witness.immutable_gc_values_witness_block==nullptr && restored_witness.immutable_gc_values_witness_next_offset==SIZE_MAX,
        "restored mixed SSA cannot retain the original ref.func or one-block GC optimization witness");
    auto wrong_types{exact}; wrong_types[2u].nullable = true;
    auto const before_wrong{builder.GetInsertBlock()->size()}; auto const blocks_before{function->size()};
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, plan.sites[1u], normal_values, wrong_types).valid &&
        builder.GetInsertBlock()->size() == before_wrong && function->size() == blocks_before,
        "same physical ref carrier with wrong semantic nullability rejects before any IR");
    auto copied{plan.sites[1u]};
    require(!emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, copied, normal_values, exact).valid &&
        function->size() == blocks_before, "copied metadata site cannot substitute actual fused builder identity");
    auto const merged{emit::emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state, resume, plan.sites[1u], normal_values, exact)};
    require(merged.valid && merged.selected && merged.actual_nonlocals.size() == 3u, "real operand and saved-control SSA PHI landing");
    auto const captured{emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state, plan.sites[1u], flags, merged.actual_nonlocals)};
    require(captured.valid && captured.slots_bytes == bytes && captured.local_flags == locals, "actual restored typed state captured for component evidence");
    auto const output{builder.CreateIntToPtr(function->getArg(5u), builder.getPtrTy())};
    auto const capture_values{builder.CreateIntToPtr(captured.slots_address, builder.getPtrTy())};
    auto const capture_flags{builder.CreateIntToPtr(captured.flags_address, builder.getPtrTy())};
    // [actual caller-owned output packet[99]] end
    // [safe sizeof/offsetof proof above     ] bounded96 values and3 flags
    // BEFORE copy/byte GEP; only this real main-owned object is passed below.
    builder.CreateMemCpy(output, ::llvm::Align{1u}, capture_values, ::llvm::Align{1u}, bytes);
    auto const output_flags{builder.CreateInBoundsGEP(builder.getInt8Ty(), output, builder.getInt64(bytes))};
    builder.CreateMemCpy(output_flags, ::llvm::Align{1u}, capture_flags, ::llvm::Align{1u}, locals);
    auto const local{builder.CreateLoad(builder.getInt64Ty(), state.local_pointers[0u])};
    auto const saved_read{emit::read_runtime_local_func_llvm_jit_checkpoint_saved_if(state,builder,state.control_stack.back(),0u)};
    require(saved_read!=nullptr,"actual shared native control tuple readable after real restore merge");
    auto const sum{builder.CreateAdd(builder.CreateAdd(local, merged.actual_nonlocals[0u]), saved_read)};
    builder.CreateRet(builder.CreateZExtOrTrunc(sum, integer));
    require(!::llvm::verifyModule(*module), "real selector/conditional loads/local stores/SSA PHIs verify");
    auto const sealed{cp::sealed_function_plan::seal_compiler_metadata(plan)}; require(bool(sealed), "component typed packet declaration");
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::move(module)}.setEngineKind(::llvm::EngineKind::JIT).create()};
    require(bool(engine), "real native MCJIT"); engine->finalizeObject();
    auto const address{engine->getFunctionAddress("checkpoint_resume")}; require(address != 0u, "actual named native component entry");
    using entry_type = ::std::uintptr_t(*)(::std::uint64_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t);
    // [actual MCJIT-owned generated entry] engine remains live through calls
    // [safe                             ] exact component integer-only ABI,
    // not a restored native PC, guest code pointer or serialized host address.
    auto const native{reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))};
    packet input{}; input.flags = {1u, 1u, 1u};
    set_slot(input, 0u, ::std::uint64_t{77u});
    auto const i31{::uwvm2::object::global::make_wasm_i31_reference(-1)}; set_slot(input, 1u, i31);
    ::std::array<::std::byte, 16u> vector{};
    for(::std::size_t i{}; i != vector.size(); ++i) { vector[i] = static_cast<::std::byte>(i * 13u + 1u); }
    set_slot(input, 2u, vector); set_slot(input, 3u, ::std::uint64_t{88u}); set_slot(input, 4u, ::std::uint64_t{99u}); set_slot(input, 5u, i31);
    auto const invoke{[&](::std::uint64_t site, ::std::size_t payload_size, packet& output)
    {
        // [real input/output native packets] owners live through synchronous
        // [safe                            ] component ABI call; the byte
        // counts select bounded logical declarations, never pointer authority.
        return native(site, reinterpret_cast<::std::uintptr_t>(input.values.data()), payload_size,
            reinterpret_cast<::std::uintptr_t>(input.flags.data()), locals, reinterpret_cast<::std::uintptr_t>(::std::addressof(output)));
    }};
    packet normal{};
    require(native(0u, 0u, 0u, 0u, 0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(normal))) == 80u,
        "normal selector0 has no restore input read and preserves original execution");
    for(::std::uint64_t site{1u}; site != 3u; ++site)
    {
        packet output{};
        require(invoke(site, site == 1u ? locals * cp::native_slot_bytes : bytes, output) == (site == 1u ? 147u : 264u),
            "actual logical entry/operand-continuation landing result");
        cp::dynamic_native_packet::owner captured{};
        require(cp::dynamic_native_packet::copy_compiler_packet(sealed, 2u, output.values, output.flags, captured) == cp::status::ok && captured,
            "restored native typed packet validates independently as DATA");
        auto const values{captured->values()}; cp::native_reference reference{};
        ::fast_io::freestanding::my_memcpy(::std::addressof(reference), values[1u].bits.data(), sizeof(reference));
        require(reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31 && reference.storage.wasm_i31.get_s() == -1 &&
            values[2u].bits == vector, "complete restored nonnull i31 and v128 bits");
        ::fast_io::freestanding::my_memcpy(::std::addressof(reference), values[5u].bits.data(), sizeof(reference));
        require(reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31 && reference.storage.wasm_i31.get_s() == (site == 1u ? 17 : -1),
            "saved control reference takes correct normal/restored SSA PHI edge");
    }
    input.flags[1u] = 0u; input.values.fill(::std::byte{});
    set_slot(input, 0u, ::std::uint64_t{77u}); set_slot(input, 3u, ::std::uint64_t{88u}); set_slot(input, 4u, ::std::uint64_t{99u}); set_slot(input, 5u, i31);
    packet unset{}; require(invoke(2u, bytes, unset) == 264u && unset.flags[1u] == 0u,
        "valid unset nondefaultable restore does not load its packet/local value");
    for(::std::size_t i{}; i != cp::native_slot_bytes; ++i)
    { require(unset.values[cp::native_slot_bytes + i] == ::std::byte{}, "unset original slot preserved as unavailable zero DATA"); }
    packet rejected_output{}; rejected_output.values.fill(::std::byte{0x5au}); rejected_output.flags.fill(0x5au);
    auto const unchanged{[&] { for(auto byte : rejected_output.values) { require(byte == ::std::byte{0x5au}, "reject cannot write output payload"); }
        for(auto flag : rejected_output.flags) { require(flag == 0x5au, "reject cannot write output flags"); } }};
    input.flags[1u] = 2u; require(invoke(2u, bytes, rejected_output) == UINTPTR_MAX, "noncanonical flag rejects before any restore payload/local mutation"); unchanged();
    input.flags[1u] = 0u; input.flags[0u] = 0u;
    require(invoke(2u, bytes, rejected_output) == UINTPTR_MAX, "proven-initialized numeric local cannot be unset"); unchanged();
    require(native(2u, 0u, 0u, 0u, 0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(rejected_output))) == UINTPTR_MAX,
        "wrong extent/null input rejects before byte access"); unchanged();
    require(native(99u, 0u, 0u, 0u, 0u, reinterpret_cast<::std::uintptr_t>(::std::addressof(rejected_output))) == UINTPTR_MAX,
        "unknown logical selector cannot become native branch target"); unchanged();
    ::fast_io::io::println("checkpoint logical LLVM dispatch/typed SSA landing component PASS; whole-VM restore/reverse/replay acceptance=false");
}
