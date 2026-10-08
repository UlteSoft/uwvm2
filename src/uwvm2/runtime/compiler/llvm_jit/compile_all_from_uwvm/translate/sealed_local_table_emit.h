// Emitted ONLY by the actual validated table.get/table.set dispatcher. Never
// hand-positive IR. This slice intentionally narrows r1: current compact ONE
// numeric range and one local-defined table0; all other cases roots+retire+old.
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
# if !defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) || UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR != 1
#  error "Sealed local table requires the actual sealed entry cursor"
# endif
namespace sealed_local_table_codegen
{
    using view = ::uwvm2::runtime::gc::sealed_compact_cursor_view;
    using slot = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t;
    using reference = ::uwvm2::uwvm::runtime::storage::gc_reference;
    using sc = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    namespace native = sealed_compact_codegen;
    static_assert(sizeof(slot) == 16uz && sizeof(reference) == 16uz);
    static_assert(sizeof(slot::storage) == sizeof(::std::uintptr_t) && sizeof(slot::type) == 4uz);
    static_assert(::std::is_standard_layout_v<slot> && ::std::is_trivially_copyable_v<slot>);
    static_assert(::std::is_standard_layout_v<reference> && ::std::is_trivially_copyable_v<reference>);

    inline bool eligible(runtime_local_func_llvm_jit_emit_state_t& s, ::std::uint_least32_t index) noexcept
    {
        if(index != 0u || !native::native_layout(s) || !s.sealed_compact_cursor) { return false; }
        auto const* m{s.local_func_storage_ptr->runtime_module_ptr};
        if(!m || !m->gc_store || !m->imported_table_vec_storage.empty() ||
           m->local_defined_table_vec_storage.size() != 1uz) { return false; }
        auto const& t{m->local_defined_table_vec_storage.index_unchecked(0uz)};
        return t.owner_module_rt_ptr == m && t.table_type_ptr && t.table_type_ptr->has_core_type &&
            t.table_type_ptr->core_type.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::reference &&
            ::uwvm2::uwvm::runtime::storage::runtime_table_family(t) ==
                ::uwvm2::uwvm::runtime::storage::runtime_table_reference_family::gc;
    }
    // No eagerly evaluated load of a null/unarmed/retired context or table.
    // Native capture supplies actual same-entry storage; zero budget is NOT
    // authority and plays no role in these non-allocating table instructions.
    inline void guard(runtime_local_func_llvm_jit_emit_state_t& s, bool wide,
        ::llvm::BasicBlock* good, ::llvm::BasicBlock* fallback) noexcept
    {
        auto& b{*s.ir_builder};auto* ctx{s.sealed_compact_cursor};auto* fn{b.GetInsertBlock()->getParent()};
        auto* present{::llvm::BasicBlock::Create(b.getContext(),"compact.table.ctx",fn)};
        auto* armed{::llvm::BasicBlock::Create(b.getContext(),"compact.table.armed",fn)};
        b.CreateCondBr(b.CreateICmpNE(ctx,::llvm::ConstantInt::get(native::word(b),0)),present,fallback);
        b.SetInsertPoint(present);
        auto* nonce{native::atomic(b,native::word(b),native::member(b,ctx,offsetof(view,armed_nonce)),
            alignof(::std::atomic<::std::uintptr_t>),::llvm::AtomicOrdering::Acquire)};
        b.CreateCondBr(b.CreateICmpNE(nonce,::llvm::ConstantInt::get(native::word(b),0)),armed,fallback);
        b.SetInsertPoint(armed);
        auto* stopped{native::atomic(b,native::word(b),native::member(b,ctx,offsetof(view,interrupts)),
            alignof(::std::atomic<::std::uintptr_t>),::llvm::AtomicOrdering::Acquire)};
        auto* pause{native::field(b,ctx,offsetof(view,pause_requested),native::word(b))};
        auto* requested{native::atomic(b,b.getInt8Ty(),native::pointer(b,pause),
            alignof(::std::atomic_bool),::llvm::AtomicOrdering::Acquire)};
        auto* policy{native::field(b,ctx,offsetof(view,policy_disabled),native::word(b))};
        auto* disabled{native::atomic(b,b.getInt8Ty(),native::pointer(b,policy),
            alignof(::std::atomic_bool),::llvm::AtomicOrdering::Acquire)};
        auto* epoch_address{native::field(b,ctx,offsetof(view,epoch_address),native::word(b))};
        auto* epoch{b.CreateLoad(b.getInt64Ty(),native::pointer(b,epoch_address))};epoch->setAlignment(::llvm::Align{1});
        auto* expected{native::field(b,ctx,offsetof(view,epoch),b.getInt64Ty())};
        auto* ready{native::field(b,ctx,offsetof(view,table_ready),b.getInt32Ty())};
        auto* address64{native::field(b,ctx,offsetof(view,table_address64),b.getInt32Ty())};
        auto* okay{b.CreateAnd(b.CreateICmpEQ(stopped,::llvm::ConstantInt::get(native::word(b),0)),
            b.CreateICmpEQ(requested,b.getInt8(0)))};
        okay=b.CreateAnd(okay,b.CreateICmpEQ(disabled,b.getInt8(0)));
        okay=b.CreateAnd(okay,b.CreateICmpEQ(epoch,expected));
        okay=b.CreateAnd(okay,b.CreateICmpEQ(ready,b.getInt32(1)));
        okay=b.CreateAnd(okay,b.CreateICmpEQ(address64,b.getInt32(wide?1:0)));
        b.CreateCondBr(okay,good,fallback);
    }
    // Values are extracted by native byte offsets, not shape/enum coincidences.
    // Full carrier reconstruction sets ALL unused padding to zero, matching the
    // original runtime_table_slot_{from,to}_gc_reference value initialization.
    inline ::llvm::Value* extract(::llvm::IRBuilder<>& b, ::llvm::Value* bits,
        ::std::size_t offset, ::std::size_t count) noexcept
    {
        constexpr auto size{sizeof(slot)};
        auto const shift{(::std::endian::native==::std::endian::little?offset:size-offset-count)*CHAR_BIT};
        return b.CreateTrunc(b.CreateLShr(bits,shift),b.getIntNTy(count*CHAR_BIT));
    }
    inline ::llvm::Value* pack_slot(::llvm::IRBuilder<>& b, ::llvm::Value* token) noexcept
    {
        constexpr auto size{sizeof(slot)}, po{offsetof(slot,storage)}, ko{offsetof(slot,type)};
        constexpr auto ps{(::std::endian::native==::std::endian::little?po:size-po-sizeof(slot::storage))*CHAR_BIT};
        constexpr auto ks{(::std::endian::native==::std::endian::little?ko:size-ko-sizeof(slot::type))*CHAR_BIT};
        auto* type{b.getIntNTy(size*CHAR_BIT)};
        auto* payload{b.CreateShl(b.CreateZExt(token,type),ps)};
        auto* tag{::llvm::ConstantInt::get(type,static_cast<::std::uint32_t>(sc::gc_struct_ref))};
        return b.CreateOr(payload,b.CreateShl(tag,ks),"compact.table.slot.carrier");
    }
    inline void current_range(runtime_local_func_llvm_jit_emit_state_t& s,
        ::llvm::Value* token, ::llvm::Value* kind, ::std::uint32_t exact_kind,
        ::llvm::BasicBlock* good, ::llvm::BasicBlock* fallback) noexcept
    {
        auto& b{*s.ir_builder};auto* ctx{s.sealed_compact_cursor};auto* fn{b.GetInsertBlock()->getParent()};
        auto* issued{::llvm::BasicBlock::Create(b.getContext(),"compact.table.issued",fn)};
        auto* live_block{::llvm::BasicBlock::Create(b.getContext(),"compact.table.live",fn)};
        auto* first{native::field(b,ctx,offsetof(view,token_begin),native::word(b))};
        auto* capacity{native::field(b,ctx,offsetof(view,capacity),native::word(b))};
        auto* position{b.CreateSub(token,first)}; // unsigned integer only, not a native pointer
        auto* range{b.CreateAnd(b.CreateICmpEQ(kind,b.getInt32(exact_kind)),b.CreateICmpUGE(token,first))};
        range=b.CreateAnd(range,b.CreateICmpULT(position,capacity));
        b.CreateCondBr(range,issued,fallback);
        b.SetInsertPoint(issued);
        auto* frontier{native::field(b,ctx,offsetof(view,frontier),native::word(b))};
        auto* count{native::atomic(b,native::word(b),native::pointer(b,frontier),
            alignof(::std::atomic_size_t),::llvm::AtomicOrdering::Acquire)};
        b.CreateCondBr(b.CreateICmpULT(position,count),live_block,fallback);
        b.SetInsertPoint(live_block);
        auto* live{native::field(b,ctx,offsetof(view,live),native::word(b))};
        // [native live[0..16), position<capacity<=1024] comes from the genuine
        // entry-pinned descriptor. No guest token supplies a bitmap address.
        auto* address{b.CreateInBoundsGEP(b.getInt64Ty(),native::pointer(b,live),
            b.CreateUDiv(position,::llvm::ConstantInt::get(native::word(b),64)))};
        auto* word{native::atomic(b,b.getInt64Ty(),address,alignof(::std::atomic<::std::uint64_t>),
            ::llvm::AtomicOrdering::Acquire)};
        auto* mask{b.CreateShl(b.getInt64(1),b.CreateURem(position,::llvm::ConstantInt::get(native::word(b),64)))};
        b.CreateCondBr(b.CreateICmpNE(b.CreateAnd(word,mask),b.getInt64(0)),good,fallback);
    }
    inline ::llvm::Value* checked_element(runtime_local_func_llvm_jit_emit_state_t& s,
        ::llvm::Value* index, ::llvm::BasicBlock* bounded, ::llvm::BasicBlock* fallback) noexcept
    {
        auto& b{*s.ir_builder};auto* ctx{s.sealed_compact_cursor};
        auto* widened{b.CreateZExtOrTrunc(index,native::word(b),"compact.table.unsigned.index")};
        auto* extent{native::field(b,ctx,offsetof(view,table_extent),native::word(b))};
        b.CreateCondBr(b.CreateICmpULT(widened,extent),bounded,fallback);
        b.SetInsertPoint(bounded);
        auto* base{native::field(b,ctx,offsetof(view,table_elements),native::word(b))};
        // [actual initialized vector base ... index<extent<=PTRDIFF_MAX/16]
        // Native cold capture checked extent*16; the unsigned index is never
        // narrowed from table64 on this native64-only slice. GEP is formed only
        // on the bounded edge. No cached base survives a revoke/grow/poll.
        return b.CreateInBoundsGEP(b.getInt8Ty(),native::pointer(b,base),
            b.CreateMul(widened,::llvm::ConstantInt::get(native::word(b),sizeof(slot))),"compact.table.slot");
    }
    template<class Fallback>
    inline bool table_get(runtime_local_func_llvm_jit_emit_state_t& s, ::std::uint_least32_t table,
        runtime_operand_stack_value_type type, bool wide, Fallback&& original) noexcept
    {
        auto& stack{s.operand_stack};
        if(!eligible(s,table))
        { return snapshot_runtime_local_func_llvm_jit_gc_roots(s) && native::retire(s) && original(table,type); }
        if(stack.empty()) { return false; }
        auto input{stack.back()};
        if(!input.value || input.type!=(wide?runtime_operand_stack_value_type::i64:runtime_operand_stack_value_type::i32) ||
           !input.value->getType()->isIntegerTy(wide?64:32)) { return false; }
        auto& b{*s.ir_builder};auto* fn{b.GetInsertBlock()->getParent()};
        auto* check{::llvm::BasicBlock::Create(b.getContext(),"compact.table.get.bounds",fn)};
        auto* bounded{::llvm::BasicBlock::Create(b.getContext(),"compact.table.get.bounded",fn)};
        auto* fast{::llvm::BasicBlock::Create(b.getContext(),"compact.table.get.fast",fn)};
        auto* fallback{::llvm::BasicBlock::Create(b.getContext(),"compact.table.get.original",fn)};
        auto* join{::llvm::BasicBlock::Create(b.getContext(),"compact.table.get.join",fn)};
        guard(s,wide,check,fallback);b.SetInsertPoint(check);
        auto* address{checked_element(s,input.value,bounded,fallback)};
        auto* value{b.CreateLoad(b.getIntNTy(sizeof(slot)*CHAR_BIT),address,"compact.table.get.slot")};
        value->setAlignment(::llvm::Align{1});
        auto* token{extract(b,value,offsetof(slot,storage),sizeof(slot::storage))};
        auto* tag{extract(b,value,offsetof(slot,type),sizeof(slot::type))};
        current_range(s,token,tag,static_cast<::std::uint32_t>(sc::gc_struct_ref),fast,fallback);
        b.SetInsertPoint(fallback);
        // The original index remains on the real operand stack until its root
        // snapshot. Retire BEFORE native helper/GC reentry, even for old compact
        // ranges. This deliberately narrows r1's no-drain native older fastpath.
        if(!snapshot_runtime_local_func_llvm_jit_gc_roots(s) || !native::retire(s) || !original(table,type)) { return false; }
        auto result{stack.back()};stack.pop_back();stack.push_back(input);
        auto* slow_end{b.GetInsertBlock()};b.CreateBr(join);
        b.SetInsertPoint(fast);
        auto* packed{native::pack_reference(b,token)};
        auto* fast_end{b.GetInsertBlock()};b.CreateBr(join);b.SetInsertPoint(join);
        if(!result.value || result.value->getType()!=packed->getType()) { return false; }
        auto* phi{b.CreatePHI(packed->getType(),2,"compact.table.get.carrier")};
        phi->addIncoming(packed,fast_end);phi->addIncoming(result.value,slow_end);
        stack.pop_back();stack.push_back({.type=type,.value=phi});return true;
    }
    template<class Fallback>
    inline bool table_set(runtime_local_func_llvm_jit_emit_state_t& s, ::std::uint_least32_t table,
        runtime_operand_stack_value_type type, bool wide, Fallback&& original) noexcept
    {
        auto& stack{s.operand_stack};
        if(!eligible(s,table))
        { return snapshot_runtime_local_func_llvm_jit_gc_roots(s) && native::retire(s) && original(table,type); }
        if(stack.size()<2uz) { return false; }
        auto value{stack.back()}, input{stack[stack.size()-2uz]};
        if(!value.value || value.type!=type || !value.value->getType()->isIntegerTy(sizeof(reference)*CHAR_BIT) ||
           !input.value || input.type!=(wide?runtime_operand_stack_value_type::i64:runtime_operand_stack_value_type::i32) ||
           !input.value->getType()->isIntegerTy(wide?64:32)) { return false; }
        auto& b{*s.ir_builder};auto* fn{b.GetInsertBlock()->getParent()};
        auto* check{::llvm::BasicBlock::Create(b.getContext(),"compact.table.set.bounds",fn)};
        auto* bounded{::llvm::BasicBlock::Create(b.getContext(),"compact.table.set.bounded",fn)};
        auto* fast{::llvm::BasicBlock::Create(b.getContext(),"compact.table.set.fast",fn)};
        auto* fallback{::llvm::BasicBlock::Create(b.getContext(),"compact.table.set.original",fn)};
        auto* join{::llvm::BasicBlock::Create(b.getContext(),"compact.table.set.join",fn)};
        guard(s,wide,check,fallback);b.SetInsertPoint(check);
        auto* address{checked_element(s,input.value,bounded,fallback)};
        auto* token{extract(b,value.value,offsetof(reference,storage),sizeof(reference::storage))};
        auto* tag{extract(b,value.value,offsetof(reference,kind),sizeof(reference::kind))};
        current_range(s,token,tag,static_cast<::std::uint32_t>(::uwvm2::object::global::wasm_ref_kind::wasm_struct),fast,fallback);
        b.SetInsertPoint(fallback);
        // Both index AND full reference remain on stack through the snapshot.
        // Checked old bridge retains foreign ownership and preserves bounds/
        // table-family/nullability/trap order. No slot changes before success.
        if(!snapshot_runtime_local_func_llvm_jit_gc_roots(s) || !native::retire(s) || !original(table,type)) { return false; }
        stack.push_back(input);stack.push_back(value);
        auto* slow_end{b.GetInsertBlock()};b.CreateBr(join);
        b.SetInsertPoint(fast);
        auto* packed{pack_slot(b,token)};
        b.CreateStore(packed,address)->setAlignment(::llvm::Align{1});
        auto* fast_end{b.GetInsertBlock()};b.CreateBr(join);b.SetInsertPoint(join);
        stack.pop_back();stack.pop_back();return true;
    }
}
#endif
