#pragma once

// An actual same-module function record is an identity, never a code pointer.
// Authenticate its whole native range and exact record stride before indexing
// the fixed publication slots. The immutable admission bitmap is compiled from
// the same canonical type interval used by the old resolver. This moves type
// projection to compilation and avoids a C++ resolver on this typed path.
[[nodiscard]] inline bool runtime_llvm_jit_local_reference_dispatch_selected(
    runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
#if defined(__linux__) && defined(__x86_64__)
    namespace cp = ::uwvm2::runtime::checkpoint;
    return state.debug_activation_enabled && state.checkpoint_plan != nullptr && state.checkpoint_plan->profile &&
        state.checkpoint_plan->profile->purpose() == cp::compilation_purpose::observe_values &&
        state.debug_full_patchable_typed_target_base_address != 0u && state.llvm_module != nullptr &&
        private_host_object_emission_scope::select(state.llvm_module) == private_host_object_emission_scope::selection::ordinary;
#else
    (void)state; return false;
#endif
}

// Compiler-only reuse of an actual immutable identity/type gate, not a
// native pointer, code lease or runtime/capture authority. Only this emitter
// can fill it. The selected entry is never retained across observation.
class llvm_jit_local_reference_dispatch_witness
{
    ::llvm::Module* module_{};
    ::llvm::Function* function_{};
    void const* owner_{};
    void const* plan_{};
    ::llvm::Value* reference_{};
    ::llvm::PHINode* index_{};
    ::llvm::PHINode* admitted_{};
    ::llvm::BasicBlock* ready_{};
    ::llvm::Value* targets_{};
    ::llvm::ArrayType* targets_type_{};
    ::std::uintptr_t records_address_{}, targets_address_{};
    ::std::size_t count_{};
    ::std::uint_least32_t expected_id_{}, expected_width_{};
    ::llvm::FunctionType* abi_{};
    friend ::llvm::Value* emit_runtime_llvm_jit_local_reference_dispatch(
        runtime_local_func_llvm_jit_emit_state_t&,::llvm::Value*,::std::uint_least32_t,
        ::std::uint_least32_t,::llvm::FunctionType*,llvm_jit_local_reference_dispatch_witness*);
    friend ::llvm::Value* emit_runtime_llvm_jit_local_reference_dispatch_after_observation(
        runtime_local_func_llvm_jit_emit_state_t&,::llvm::Value*,::std::uint_least32_t,
        ::std::uint_least32_t,::llvm::FunctionType*,llvm_jit_local_reference_dispatch_witness const&);
public:
    llvm_jit_local_reference_dispatch_witness() = default;
};

// The IR is still being built, so do not build a DominatorTree over incomplete
// terminators. Walk genuine incoming terminator edges only. Reject cycles,
// entry bypasses and oversized walks; rejection retains full original dispatch.
[[nodiscard]] inline bool llvm_jit_reference_witness_dominates(
    ::llvm::BasicBlock* proof,::llvm::BasicBlock* current,::llvm::Function* function)
{
    if(proof == nullptr || current == nullptr || function == nullptr ||
       proof->getParent() != function || current->getParent() != function) { return false; }
    struct visit { ::llvm::BasicBlock* block; bool exit; };
    ::std::vector<visit> pending{{current,false}};
    ::std::vector<::llvm::BasicBlock*> active{},complete{};
    auto const contains{[](auto const& values,auto value)
    { for(auto actual:values) { if(actual == value) { return true; } } return false; }};
    ::std::size_t visited{};
    while(!pending.empty())
    {
        auto const next{pending.back()};pending.pop_back();
        if(next.block == proof) { continue; }
        if(next.exit)
        {
            if(active.empty() || active.back() != next.block) { return false; }
            active.pop_back();complete.push_back(next.block);continue;
        }
        if(contains(complete,next.block)) { continue; }
        if(contains(active,next.block) || next.block == ::std::addressof(function->getEntryBlock()) ||
           ++visited > 128u) { return false; }
        active.push_back(next.block);pending.push_back({next.block,true});
        bool incoming{};
        for(auto const user:next.block->users())
        {
            auto const instruction{::llvm::dyn_cast<::llvm::Instruction>(user)};
            if(instruction == nullptr || !instruction->isTerminator()) { continue; }
            auto const parent{instruction->getParent()};
            if(parent == nullptr || parent->getParent() != function) { return false; }
            bool edge{};
            for(unsigned i{};i != instruction->getNumSuccessors();++i)
            { if(instruction->getSuccessor(i) == next.block) { edge=true;break; } }
            if(edge) { incoming=true;pending.push_back({parent,false}); }
        }
        if(!incoming) { return false; }
    }
    return true;
}

[[nodiscard]] inline ::llvm::Value* emit_runtime_llvm_jit_local_reference_dispatch(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::Value* reference,
    ::std::uint_least32_t expected_id, ::std::uint_least32_t expected_width, ::llvm::FunctionType* expected_abi,
    llvm_jit_local_reference_dispatch_witness* capture = nullptr)
{
    if(capture != nullptr) { *capture = {}; }
    if(!runtime_llvm_jit_local_reference_dispatch_selected(state) || state.ir_builder == nullptr ||
       state.local_func_storage_ptr == nullptr || state.local_func_storage_ptr->runtime_module_ptr == nullptr ||
       reference == nullptr || !reference->getType()->isIntegerTy(128u) || expected_abi == nullptr ||
       expected_id == invalid_runtime_canonical_type_id() || expected_width == 0u || expected_width > UINT32_MAX-expected_id)
    { return nullptr; }
    auto const& module{*state.local_func_storage_ptr->runtime_module_ptr};
    auto const& functions{module.local_defined_function_vec_storage};
    auto const count{functions.size()};
    using record_type = ::std::remove_cvref_t<decltype(functions.index_unchecked(0u))>;
    constexpr auto stride{sizeof(record_type)};
    auto const records_address{reinterpret_cast<::std::uintptr_t>(functions.data())};
    if(count == 0u || count != state.debug_full_patchable_typed_target_count ||
       count > static_cast<::std::size_t>(PTRDIFF_MAX)/stride || records_address == 0u ||
       records_address > UINTPTR_MAX-count*stride || sizeof(::std::uintptr_t) != 8u ||
       sizeof(::uwvm2::object::global::wasm_funcref_t) != 16u ||
       offsetof(::uwvm2::object::global::wasm_global_ref_t,kind) != 8u ||
       state.llvm_module->getDataLayout().isDefault() || !state.llvm_module->getDataLayout().isLittleEndian()) { return nullptr; }
    auto& builder{*state.ir_builder};auto& context{builder.getContext()};
    auto const integer{builder.getInt64Ty()};
    auto const types_begin{module.type_section_storage.type_section_begin};
    auto const types_end{module.type_section_storage.type_section_end};
    if(types_begin == nullptr || types_end == nullptr || types_end < types_begin) { return nullptr; }
    auto const type_count{static_cast<::std::size_t>(types_end-types_begin)};
    auto const type_base{reinterpret_cast<::std::uintptr_t>(types_begin)};
    constexpr auto type_stride{sizeof(*types_begin)};
    if(type_count > static_cast<::std::size_t>(PTRDIFF_MAX)/type_stride || type_base > UINTPTR_MAX-type_count*type_stride) { return nullptr; }
    ::uwvm2::utils::container::vector<::std::uint8_t> admitted{};admitted.reserve(count);
    for(auto const& function : functions)
    {
        auto const type_address{reinterpret_cast<::std::uintptr_t>(function.function_type_ptr)};
        auto const delta{type_address-type_base};
        bool valid{type_address >= type_base && delta < type_count*type_stride && delta%type_stride == 0u};
        if(valid)
        {
            auto const index{delta/type_stride};
            if(index > UINT32_MAX) { valid=false; }
            else
            {
                auto const actual{resolve_runtime_canonical_type_id(module,static_cast<validation_module_traits_t::wasm_u32>(index))};
                valid = actual != invalid_runtime_canonical_type_id() && actual >= expected_id &&
                    actual-expected_id < expected_width &&
                    get_llvm_function_type_from_wasm_function_type(context,*function.function_type_ptr) == expected_abi;
            }
        }
        admitted.push_back(valid ? 1u:0u);
    }
    auto const bitmap_type{::llvm::ArrayType::get(builder.getInt8Ty(),count)};
    auto const bitmap{new ::llvm::GlobalVariable(*state.llvm_module,bitmap_type,true,::llvm::GlobalValue::PrivateLinkage,
        ::llvm::ConstantDataArray::get(context,::llvm::ArrayRef<::std::uint8_t>{admitted.data(),admitted.size()}),
        ::llvm::Twine{state.llvm_function->getName()}+".call_ref.admitted")};
    bitmap->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global);bitmap->setAlignment(::llvm::Align{1u});
    auto const records_name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(module),u8"_debug_full_local_function_records")};
    auto const records{get_llvm_external_host_object_pointer(builder,records_address,
        ::llvm::ArrayType::get(builder.getInt8Ty(),count*stride),::uwvm2::utils::container::u8string_view{records_name.data(),records_name.size()})};
    auto const targets_name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(module),u8"_debug_full_typed_targets")};
    auto const targets_type{::llvm::ArrayType::get(integer,count)};
    auto const targets{get_llvm_external_host_object_pointer(builder,state.debug_full_patchable_typed_target_base_address,
        targets_type,::uwvm2::utils::container::u8string_view{targets_name.data(),targets_name.size()})};
    if(records == nullptr || targets == nullptr) { return nullptr; }
    auto const storage{builder.CreateTrunc(reference,integer)};
    auto const kind{builder.CreateTrunc(builder.CreateLShr(reference,64u),builder.getInt32Ty())};
    auto const offset{builder.CreateSub(storage,builder.CreatePtrToInt(records,integer))};
    auto const valid{builder.CreateAnd(builder.CreateICmpEQ(kind,builder.getInt32(
        static_cast<unsigned>(::uwvm2::object::global::wasm_ref_kind::wasm_func_defined))),
        builder.CreateAnd(builder.CreateICmpULT(offset,builder.getInt64(count*stride)),
            builder.CreateICmpEQ(builder.CreateURem(offset,builder.getInt64(stride)),builder.getInt64(0u))))};
    auto const origin{builder.GetInsertBlock()};
    auto const check{::llvm::BasicBlock::Create(context,"call_ref.local.type",state.llvm_function)};
    auto const load{::llvm::BasicBlock::Create(context,"call_ref.local.entry",state.llvm_function)};
    auto const ready{::llvm::BasicBlock::Create(context,"call_ref.local.ready",state.llvm_function)};
    builder.CreateCondBr(valid,check,ready);
    builder.SetInsertPoint(check);
    // [actual records 0..count) ... index < count] proven before any GEP.
    // Numeric identity alone never selects memory outside these owned arrays.
    auto const index{builder.CreateUDiv(offset,builder.getInt64(stride))};
    auto const bit{builder.CreateLoad(builder.getInt8Ty(),builder.CreateInBoundsGEP(bitmap_type,bitmap,
        {builder.getInt64(0u),index}))};bit->setAlignment(::llvm::Align{1u});
    builder.CreateCondBr(builder.CreateICmpEQ(bit,builder.getInt8(1u)),load,ready);
    builder.SetInsertPoint(load);
    auto const slot{builder.CreateInBoundsGEP(targets_type,targets,{builder.getInt64(0u),index})};
    auto const entry{builder.CreateLoad(integer,slot,"call_ref.local.published")};
    entry->setAlignment(::llvm::Align{::std::atomic_ref<::std::uintptr_t>::required_alignment});
    entry->setAtomic(::llvm::AtomicOrdering::Acquire);entry->setVolatile(true);
    builder.CreateBr(ready);builder.SetInsertPoint(ready);
    auto const result{builder.CreatePHI(integer,3u,"call_ref.local.target")};
    result->addIncoming(builder.getInt64(0u),origin);result->addIncoming(builder.getInt64(0u),check);result->addIncoming(entry,load);
    if(capture != nullptr)
    {
        auto const selected_index{builder.CreatePHI(integer,3u,"call_ref.local.verified.index")};
        selected_index->addIncoming(builder.getInt64(0u),origin);selected_index->addIncoming(index,check);
        selected_index->addIncoming(index,load);
        auto const admitted_value{builder.CreatePHI(builder.getInt1Ty(),3u,"call_ref.local.verified.admission")};
        admitted_value->addIncoming(builder.getFalse(),origin);admitted_value->addIncoming(builder.getFalse(),check);
        admitted_value->addIncoming(builder.getTrue(),load);
        capture->module_=state.llvm_module;capture->function_=state.llvm_function;
        capture->owner_=state.local_func_storage_ptr->runtime_module_ptr;capture->plan_=state.checkpoint_plan;
        capture->reference_=reference;capture->index_=selected_index;capture->admitted_=admitted_value;
        capture->ready_=ready;capture->targets_=targets;capture->targets_type_=targets_type;
        capture->records_address_=records_address;capture->targets_address_=state.debug_full_patchable_typed_target_base_address;
        capture->count_=count;capture->expected_id_=expected_id;capture->expected_width_=expected_width;capture->abi_=expected_abi;
    }
    return result; // Zero preserves the full original resolver/trap behavior.
}

// Same SSA reference, same module/immutable type admission and a real CFG
// dominance proof. Runtime publication remains mutable: reload the actual
// aligned target AFTER observation, acquire+volatile, including zero entries.
[[nodiscard]] inline ::llvm::Value* emit_runtime_llvm_jit_local_reference_dispatch_after_observation(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::Value* reference,
    ::std::uint_least32_t expected_id, ::std::uint_least32_t expected_width, ::llvm::FunctionType* expected_abi,
    llvm_jit_local_reference_dispatch_witness const& witness)
{
    if(!runtime_llvm_jit_local_reference_dispatch_selected(state) || state.ir_builder == nullptr ||
       state.local_func_storage_ptr == nullptr || state.local_func_storage_ptr->runtime_module_ptr == nullptr ||
       witness.module_ != state.llvm_module || witness.function_ != state.llvm_function ||
       witness.owner_ != state.local_func_storage_ptr->runtime_module_ptr || witness.plan_ != state.checkpoint_plan ||
       reference == nullptr || witness.reference_ != reference || witness.abi_ != expected_abi ||
       witness.expected_id_ != expected_id || witness.expected_width_ != expected_width ||
       witness.targets_address_ != state.debug_full_patchable_typed_target_base_address ||
       witness.count_ != state.debug_full_patchable_typed_target_count ||
       witness.index_ == nullptr || witness.admitted_ == nullptr || witness.ready_ == nullptr ||
       witness.targets_ == nullptr || witness.targets_type_ == nullptr ||
       witness.index_->getParent() != witness.ready_ || witness.admitted_->getParent() != witness.ready_ ||
       !witness.index_->getType()->isIntegerTy(64u) || !witness.admitted_->getType()->isIntegerTy(1u)) { return nullptr; }
    auto const& functions{state.local_func_storage_ptr->runtime_module_ptr->local_defined_function_vec_storage};
    if(functions.size() != witness.count_ ||
       reinterpret_cast<::std::uintptr_t>(functions.data()) != witness.records_address_ ||
       !llvm_jit_reference_witness_dominates(witness.ready_,state.ir_builder->GetInsertBlock(),state.llvm_function))
    { return nullptr; }
    auto& builder{*state.ir_builder};auto& context{builder.getContext()};
    auto const origin{builder.GetInsertBlock()};
    auto const load{::llvm::BasicBlock::Create(context,"call_ref.local.reload",state.llvm_function)};
    auto const ready{::llvm::BasicBlock::Create(context,"call_ref.local.reloaded",state.llvm_function)};
    builder.CreateCondBr(witness.admitted_,load,ready);builder.SetInsertPoint(load);
    // An admitted edge carries the original complete range/stride/type proof.
    // The index is never used for an invalid carrier, ABI or foreign record.
    auto const slot{builder.CreateInBoundsGEP(witness.targets_type_,witness.targets_,
        {builder.getInt64(0u),witness.index_})};
    auto const entry{builder.CreateLoad(builder.getInt64Ty(),slot,"call_ref.local.published.after.observation")};
    entry->setAlignment(::llvm::Align{::std::atomic_ref<::std::uintptr_t>::required_alignment});
    entry->setAtomic(::llvm::AtomicOrdering::Acquire);entry->setVolatile(true);
    builder.CreateBr(ready);builder.SetInsertPoint(ready);
    auto const result{builder.CreatePHI(builder.getInt64Ty(),2u,"call_ref.local.current.target")};
    result->addIncoming(builder.getInt64(0u),origin);result->addIncoming(entry,load);
    return result;
}
