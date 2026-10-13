// Include AFTER original aggregate/immutable-cast functions have been renamed
// *_unsealed (default arguments retained). No hand-authored positive IR: these
// routines emit from the ACTUAL validated GC opcode path and actual state.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
namespace sealed_compact_codegen
{
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST == 1
    [[nodiscard]] inline ::std::uintptr_t local_compact_cast_leaf(
        ::std::uintptr_t context, ::std::uintptr_t module,
        ::std::uint32_t reference_kind, ::std::uintptr_t token,
        ::std::uint32_t expected_index) noexcept
    {
        // The runtime resolves its own real current entry before comparing the
        // supplied integers. It never dereferences context/module from a guest.
        auto* entry{::uwvm2::runtime::gc::borrow_actual_sealed_entry(module)};
        if(!entry || !entry->exact_view_identity(context)) { return 0u; }
        ::uwvm2::uwvm::runtime::storage::gc_reference reference{};
        reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(reference_kind);
        // Opaque integer key only. No object pointer is derived or dereferenced;
        // actual local descriptor membership supplies the eventual cell borrow.
        reference.storage.ptr = reinterpret_cast<void*>(token);
        return entry->local_compact_immutable_cast_values(reference, expected_index);
    }
#endif
    using view = ::uwvm2::runtime::gc::sealed_compact_cursor_view;
    inline bool native_layout(runtime_local_func_llvm_jit_emit_state_t& s) noexcept
    {
        // Pending numeric cores admit only numeric operands and numeric tag
        // payloads. They have no GC operation that can use a compact cursor.
        // Decline before declaring a runtime-module address or capture helper;
        // their closed native binding set must not acquire unused host objects.
        if(s.pending_numeric_plan != nullptr || !s.ir_builder || !s.llvm_module || !s.local_func_storage_ptr ||
           !s.emit_precise_gc_root_frames || s.emit_debug_safe_points || sizeof(::std::uintptr_t) != 8uz) { return false; }
        auto const& d{s.llvm_module->getDataLayout()};
        return !s.llvm_module->getDataLayoutStr().empty() && d.getPointerSize() == sizeof(::std::uintptr_t) &&
            d.isLittleEndian() == (::std::endian::native == ::std::endian::little) &&
            sizeof(::std::atomic_size_t) == sizeof(::std::size_t) && sizeof(::std::atomic<::std::uint64_t>) == 8uz;
    }
    inline ::llvm::Type* word(::llvm::IRBuilder<>& b) noexcept { return b.getIntNTy(sizeof(::std::uintptr_t)*CHAR_BIT); }
    inline ::llvm::Value* pointer(::llvm::IRBuilder<>& b, ::llvm::Value* integer) noexcept
    { return b.CreateIntToPtr(integer, b.getPtrTy()); }
    inline ::llvm::Value* member(::llvm::IRBuilder<>& b, ::llvm::Value* ctx, ::std::size_t off) noexcept
    { return b.CreateInBoundsGEP(b.getInt8Ty(), pointer(b, ctx), ::llvm::ConstantInt::get(word(b), off)); }
    inline ::llvm::LoadInst* field(::llvm::IRBuilder<>& b, ::llvm::Value* ctx, ::std::size_t off, ::llvm::Type* type) noexcept
    { auto* v{b.CreateLoad(type, member(b,ctx,off))}; v->setAlignment(::llvm::Align{1}); return v; }
    inline ::llvm::LoadInst* atomic(::llvm::IRBuilder<>& b, ::llvm::Type* type, ::llvm::Value* ptr,
        unsigned alignment, ::llvm::AtomicOrdering order) noexcept
    { auto* v{b.CreateLoad(type,ptr)};v->setAlignment(::llvm::Align{alignment});v->setAtomic(order);return v; }
    inline ::llvm::Value* module_address(runtime_local_func_llvm_jit_emit_state_t& s) noexcept
    {
        auto const* module{s.local_func_storage_ptr->runtime_module_ptr};
        if(!module) { return nullptr; }
        auto const name{get_llvm_runtime_module_object_symbol_name(*module)};
        return get_llvm_external_host_object_address(*s.ir_builder, reinterpret_cast<::std::uintptr_t>(module),
            ::uwvm2::utils::container::u8string_view{name.data(),name.size()});
    }
    template<auto Function>
    inline ::llvm::CallInst* leaf(::llvm::IRBuilder<>& b, ::llvm::FunctionType* type,
        ::llvm::ArrayRef<::llvm::Value*> args, ::uwvm2::utils::container::u8string_view semantic) noexcept
    {
        auto* f{get_llvm_runtime_bridge_function_symbol_value<Function>(b,type,semantic)};
        if(!f) { return nullptr; }
        auto* call{apply_llvm_jit_host_calling_conv(b.CreateCall(type,f,args))};call->setDoesNotThrow();return call;
    }
    // Actual core-entry setup calls this once AFTER prepare_gc_root_frame and
    // before body/OSR entry. Function/raw/Wasm ABI stays unchanged. This private
    // SSA address is not pushed as a guest reference and never stored in tables.
    inline bool capture(runtime_local_func_llvm_jit_emit_state_t& s) noexcept
    {
        if(!native_layout(s)) { return true; }
        auto* module{module_address(s)};if(!module) { return false; }
        auto& b{*s.ir_builder};
        auto* type{::llvm::FunctionType::get(word(b),{word(b)},false)};
        s.sealed_compact_cursor = leaf<::uwvm2::runtime::gc::sealed_compact_capture_leaf>(b,type,{module},
            ::uwvm2::utils::container::u8string_view{u8"gc_sealed_compact_capture_v1"});
        return s.sealed_compact_cursor != nullptr;
    }
    inline bool retire(runtime_local_func_llvm_jit_emit_state_t& s) noexcept
    {
        if(!s.sealed_compact_cursor) { return true; }
        auto& b{*s.ir_builder};auto* type{::llvm::FunctionType::get(b.getVoidTy(),{word(b)},false)};
        return leaf<::uwvm2::runtime::gc::sealed_compact_boundary_leaf>(b,type,{s.sealed_compact_cursor},
            ::uwvm2::utils::container::u8string_view{u8"gc_sealed_compact_retire_v1"}) != nullptr;
    }
    // Each call creates dominating control, not an eager boolean AND containing
    // potentially-null loads. Unarmed/retired metadata is NEVER dereferenced.
    inline void guard(runtime_local_func_llvm_jit_emit_state_t& s, ::std::uint32_t index,
        ::llvm::BasicBlock* good, ::llvm::BasicBlock* bad) noexcept
    {
        auto& b{*s.ir_builder};auto* ctx{s.sealed_compact_cursor};auto* fn{b.GetInsertBlock()->getParent()};
        auto* nonnull{::llvm::BasicBlock::Create(b.getContext(),"compact.ctx",fn)};
        auto* armed{::llvm::BasicBlock::Create(b.getContext(),"compact.armed",fn)};
        b.CreateCondBr(b.CreateICmpNE(ctx,::llvm::ConstantInt::get(word(b),0)),nonnull,bad);
        b.SetInsertPoint(nonnull);
        auto* nonce{atomic(b,word(b),member(b,ctx,offsetof(view,armed_nonce)),alignof(::std::atomic<::std::uintptr_t>),::llvm::AtomicOrdering::Acquire)};
        b.CreateCondBr(b.CreateICmpNE(nonce,::llvm::ConstantInt::get(word(b),0)),armed,bad);
        b.SetInsertPoint(armed);
        auto* stopped{atomic(b,word(b),member(b,ctx,offsetof(view,interrupts)),alignof(::std::atomic<::std::uintptr_t>),::llvm::AtomicOrdering::Acquire)};
        auto* pause{field(b,ctx,offsetof(view,pause_requested),word(b))};
        auto* requested{atomic(b,b.getInt8Ty(),pointer(b,pause),alignof(::std::atomic_bool),::llvm::AtomicOrdering::Acquire)};
        auto* policyptr{field(b,ctx,offsetof(view,policy_disabled),word(b))};
        auto* disabled{atomic(b,b.getInt8Ty(),pointer(b,policyptr),alignof(::std::atomic_bool),::llvm::AtomicOrdering::Acquire)};
        auto* epochptr{field(b,ctx,offsetof(view,epoch_address),word(b))};
        auto* epoch{b.CreateLoad(b.getInt64Ty(),pointer(b,epochptr))};epoch->setAlignment(::llvm::Align{1});
        auto* expected{field(b,ctx,offsetof(view,epoch),b.getInt64Ty())};
        auto* actualtype{field(b,ctx,offsetof(view,type_index),b.getInt32Ty())};
        auto* okay{b.CreateAnd(b.CreateICmpEQ(stopped,::llvm::ConstantInt::get(word(b),0)),b.CreateICmpEQ(requested,b.getInt8(0)))};
        okay=b.CreateAnd(okay,b.CreateICmpEQ(epoch,expected));
        okay=b.CreateAnd(okay,b.CreateICmpEQ(disabled,b.getInt8(0)));
        okay=b.CreateAnd(okay,b.CreateICmpEQ(actualtype,b.getInt32(index)));
        b.CreateCondBr(okay,good,bad);
    }
    inline ::llvm::Value* pack_reference(::llvm::IRBuilder<>& b, ::llvm::Value* opaque) noexcept
    {
        using ref=::uwvm2::uwvm::runtime::storage::gc_reference;
        constexpr auto n{sizeof(ref)}, p{offsetof(ref,storage)}, k{offsetof(ref,kind)};
        static_assert(sizeof(ref::storage)==sizeof(::std::uintptr_t) && sizeof(ref::kind)==4uz);
        auto* carrier{b.getIntNTy(n*CHAR_BIT)};
        constexpr auto ps{(::std::endian::native==::std::endian::little?p:n-p-sizeof(ref::storage))*CHAR_BIT};
        constexpr auto ks{(::std::endian::native==::std::endian::little?k:n-k-sizeof(ref::kind))*CHAR_BIT};
        auto* bits{b.CreateShl(b.CreateZExt(opaque,carrier),ps)};
        auto* kind{::llvm::ConstantInt::get(carrier,static_cast<::std::uint32_t>(::uwvm2::object::global::wasm_ref_kind::wasm_struct))};
        return b.CreateOr(bits,b.CreateShl(kind,ks),"compact.new.carrier"); // ALL padding zero
    }
    // Only invoked from the ACTUAL validated StructNew opcode, after original
    // decoded/count/layout checks and before any original buffer initialization.
    inline bool eligible(runtime_local_func_llvm_jit_emit_state_t& s,
        ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& d) noexcept
    {
        if(!s.valid||!native_layout(s)||!s.sealed_compact_cursor||(d.opcode!=0u&&d.opcode!=1u)) { return false; }
        auto const* m{s.local_func_storage_ptr->runtime_module_ptr};::std::size_t count{};
        namespace t=::uwvm2::parser::wasm::standard::wasm3::type;
        if(!m||!m->gc_store||!m->gc_store->field_count(d.first,count)||count!=1uz) { return false; }
        auto const* f{m->gc_store->field_at(d.first,0)};
        if(!f||f->mutable_||f->storage.packed!=t::packed_kind::none||
           (f->storage.value.kind!=t::value_kind::i32&&f->storage.value.kind!=t::value_kind::f32)) { return false; }
        if(d.opcode==1u) { return true; }
        if(s.operand_stack.empty()||!s.operand_stack.back().value) { return false; }
        auto const& operand{s.operand_stack.back()};
        return f->storage.value.kind==t::value_kind::i32 ?
            operand.type==runtime_operand_stack_value_type::i32&&operand.value->getType()->isIntegerTy(32) :
            operand.type==runtime_operand_stack_value_type::f32&&operand.value->getType()->isFloatTy();
    }
    inline bool new32(runtime_local_func_llvm_jit_emit_state_t& s,
        ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& d) noexcept
    {
        auto& b{*s.ir_builder};auto& stack{s.operand_stack};auto* ctx{s.sealed_compact_cursor};
        auto* fn{b.GetInsertBlock()->getParent()};auto* module{module_address(s)};if(!module){return false;}
        // Owned SSA handles: do not retain vector element addresses/iterators.
        auto input=d.opcode==0u?stack.back():llvm_jit_stack_value_t{};
        auto* bits=d.opcode==0u?(input.value->getType()->isFloatTy()?b.CreateBitCast(input.value,b.getInt32Ty()):input.value):b.getInt32(0);
        // Infallible bounded hot stores cannot collect. The actual cold refill
        // below publishes the ORIGINAL full prefix+allocation inputs BEFORE
        // calling the original allocation poll; no input is popped first.
        auto* try_block{::llvm::BasicBlock::Create(b.getContext(),"compact.try",fn)};
        auto* check_room{::llvm::BasicBlock::Create(b.getContext(),"compact.room",fn)};
        auto* fast{::llvm::BasicBlock::Create(b.getContext(),"compact.new.fast",fn)};
        auto* refill{::llvm::BasicBlock::Create(b.getContext(),"compact.refill",fn)};
        auto* uncharged{::llvm::BasicBlock::Create(b.getContext(),"compact.original.uncharged",fn)};
        auto* charged{::llvm::BasicBlock::Create(b.getContext(),"compact.original.charged",fn)};
        auto* join{::llvm::BasicBlock::Create(b.getContext(),"compact.new.join",fn)};
        b.CreateBr(try_block);b.SetInsertPoint(try_block);guard(s,d.first,check_room,refill);
        b.SetInsertPoint(check_room);
        auto* frontier{field(b,ctx,offsetof(view,frontier),word(b))};
        auto* slot{atomic(b,word(b),pointer(b,frontier),alignof(::std::atomic_size_t),::llvm::AtomicOrdering::Monotonic)};
        auto* capacity{field(b,ctx,offsetof(view,capacity),word(b))};
        auto* budget{field(b,ctx,offsetof(view,budget),word(b))};
        auto* credit{field(b,ctx,offsetof(view,first_credit),word(b))};
        auto* can{b.CreateAnd(b.CreateICmpULT(slot,capacity),b.CreateOr(b.CreateICmpNE(budget,::llvm::ConstantInt::get(word(b),0)),
            b.CreateICmpNE(credit,::llvm::ConstantInt::get(word(b),0))))};b.CreateCondBr(can,fast,refill);
        b.SetInsertPoint(refill);
        // ctx null cannot be passed as authority; runtime callback resolves it
        // without dereferencing and returns ORIGINAL_ROUTE/uncredited.
        if(!snapshot_runtime_local_func_llvm_jit_gc_roots(s)){return false;}
        auto* ft{::llvm::FunctionType::get(word(b),{word(b),word(b),b.getInt32Ty()},false)};
        auto* status{leaf<::uwvm2::runtime::gc::sealed_compact_refill_leaf>(b,ft,{ctx,module,b.getInt32(d.first)},
            ::uwvm2::utils::container::u8string_view{u8"gc_sealed_compact_refill_v1"})};if(!status){return false;}
        auto* classify{::llvm::BasicBlock::Create(b.getContext(),"compact.refill.classify",fn)};
        b.CreateCondBr(b.CreateICmpEQ(status,::llvm::ConstantInt::get(word(b),0)),try_block,classify);
        b.SetInsertPoint(classify);
        emit_llvm_conditional_trap(*s.llvm_module,b,b.CreateICmpEQ(status,::llvm::ConstantInt::get(word(b),5)),
            ::uwvm2::runtime::lib::llvm_jit_trap_kind::runtime_invariant_failure);
        b.CreateCondBr(b.CreateICmpEQ(status,::llvm::ConstantInt::get(word(b),1)),uncharged,charged);
        auto emit_original=[&](::llvm::BasicBlock* block,bool already_polled)->::llvm::Value*
        {
            b.SetInsertPoint(block);if(!retire(s)){return nullptr;}
            if(!try_emit_runtime_local_func_llvm_jit_gc_aggregate_unsealed(s,d,already_polled)){return nullptr;}
            auto* result{stack.back().value};stack.pop_back();
            if(d.opcode==0u){stack.push_back(input);} // restore ONLY consumed operand
            return result;
        };
        auto* plain_result{emit_original(uncharged,false)};if(!plain_result){return false;}
        auto* plain_end{b.GetInsertBlock()};b.CreateBr(join);
        auto* charged_result{emit_original(charged,true)};if(!charged_result){return false;}
        auto* charged_end{b.GetInsertBlock()};b.CreateBr(join);
        b.SetInsertPoint(fast);
        auto* cells{field(b,ctx,offsetof(view,cells),word(b))};
        // GEP from actual cell[] allocation base, within slot<capacity. It never
        // casts opaque token to a native object pointer or fabricates C++ cells.
        auto* cell{b.CreateInBoundsGEP(b.getInt8Ty(),pointer(b,cells),b.CreateMul(slot,::llvm::ConstantInt::get(word(b),4)))};
        b.CreateStore(bits,cell)->setAlignment(::llvm::Align{1});
        auto* live{field(b,ctx,offsetof(view,live),word(b))};
        auto* bitmap{b.CreateInBoundsGEP(b.getInt64Ty(),pointer(b,live),b.CreateUDiv(slot,::llvm::ConstantInt::get(word(b),64)))};
        auto* old{atomic(b,b.getInt64Ty(),bitmap,alignof(::std::atomic<::std::uint64_t>),::llvm::AtomicOrdering::Monotonic)};
        auto* bit{b.CreateShl(b.getInt64(1),b.CreateURem(slot,::llvm::ConstantInt::get(word(b),64)))};
        auto* set{b.CreateStore(b.CreateOr(old,bit),bitmap)};set->setAlignment(::llvm::Align{alignof(::std::atomic<::std::uint64_t>)});set->setAtomic(::llvm::AtomicOrdering::Monotonic);
        auto* used{b.CreateSelect(b.CreateICmpEQ(credit,::llvm::ConstantInt::get(word(b),0)),::llvm::ConstantInt::get(word(b),1),::llvm::ConstantInt::get(word(b),0))};
        auto* pending{field(b,ctx,offsetof(view,unaccounted),word(b))};
        b.CreateStore(b.CreateAdd(pending,used),member(b,ctx,offsetof(view,unaccounted)))->setAlignment(::llvm::Align{1});
        b.CreateStore(b.CreateSub(budget,used),member(b,ctx,offsetof(view,budget)))->setAlignment(::llvm::Align{1});
        b.CreateStore(::llvm::ConstantInt::get(word(b),0),member(b,ctx,offsetof(view,first_credit)))->setAlignment(::llvm::Align{1});
        auto* publish{b.CreateStore(b.CreateAdd(slot,::llvm::ConstantInt::get(word(b),1)),pointer(b,frontier))};
        publish->setAlignment(::llvm::Align{alignof(::std::atomic_size_t)});publish->setAtomic(::llvm::AtomicOrdering::Release);
        auto* base{field(b,ctx,offsetof(view,token_begin),word(b))};auto* result{pack_reference(b,b.CreateAdd(base,slot))};
        auto* fast_end{b.GetInsertBlock()};b.CreateBr(join);b.SetInsertPoint(join);
        auto* phi{b.CreatePHI(result->getType(),3,"compact.new.result")};phi->addIncoming(result,fast_end);
        phi->addIncoming(plain_result,plain_end);phi->addIncoming(charged_result,charged_end);
        if(d.opcode==0u){stack.pop_back();}stack.push_back({.type=runtime_operand_stack_value_type::funcref,.value=phi});return true;
    }
    // Exact-index/current-range cast succeeds without a native call. Other
    // subtype/null/foreign/stale cases retire first, then ORIGINAL typed cast.
    inline ::llvm::Value* cast(runtime_local_func_llvm_jit_emit_state_t& s,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target) noexcept
    {
        auto& b{*s.ir_builder};auto* ctx{s.sealed_compact_cursor};auto* fn{b.GetInsertBlock()->getParent()};
        using ref=::uwvm2::uwvm::runtime::storage::gc_reference;
        auto* value{s.operand_stack.back().value};
        constexpr auto bytes{sizeof(ref)},po{offsetof(ref,storage)},ko{offsetof(ref,kind)};
        constexpr auto ps{(::std::endian::native==::std::endian::little?po:bytes-po-sizeof(ref::storage))*CHAR_BIT};
        constexpr auto ks{(::std::endian::native==::std::endian::little?ko:bytes-ko-sizeof(ref::kind))*CHAR_BIT};
        auto* token{b.CreateTrunc(b.CreateLShr(value,ps),word(b))};auto* tag{b.CreateTrunc(b.CreateLShr(value,ks),b.getInt32Ty())};
        auto* geometry{::llvm::BasicBlock::Create(b.getContext(),"compact.cast.geometry",fn)};
        auto* live_block{::llvm::BasicBlock::Create(b.getContext(),"compact.cast.live",fn)};
        auto* fast{::llvm::BasicBlock::Create(b.getContext(),"compact.cast.fast",fn)};
        auto* slow{::llvm::BasicBlock::Create(b.getContext(),"compact.cast.original",fn)};
        auto* join{::llvm::BasicBlock::Create(b.getContext(),"compact.cast.join",fn)};
        guard(s,static_cast<::std::uint32_t>(target.heap.code),geometry,slow);b.SetInsertPoint(geometry);
        auto* base{field(b,ctx,offsetof(view,token_begin),word(b))};auto* slot{b.CreateSub(token,base)};
        auto* frontptr{field(b,ctx,offsetof(view,frontier),word(b))};
        auto* frontier{atomic(b,word(b),pointer(b,frontptr),alignof(::std::atomic_size_t),::llvm::AtomicOrdering::Acquire)};
        auto* valid{b.CreateAnd(b.CreateICmpEQ(tag,b.getInt32(static_cast<::std::uint32_t>(::uwvm2::object::global::wasm_ref_kind::wasm_struct))),
            b.CreateAnd(b.CreateICmpUGE(token,base),b.CreateICmpULT(slot,frontier)))};
        valid=b.CreateAnd(valid,b.CreateICmpULT(slot,field(b,ctx,offsetof(view,capacity),word(b))));b.CreateCondBr(valid,live_block,slow);
        b.SetInsertPoint(live_block);auto* bitmapbase{field(b,ctx,offsetof(view,live),word(b))};
        auto* ptr{b.CreateInBoundsGEP(b.getInt64Ty(),pointer(b,bitmapbase),b.CreateUDiv(slot,::llvm::ConstantInt::get(word(b),64)))};
        auto* bitmap{atomic(b,b.getInt64Ty(),ptr,alignof(::std::atomic<::std::uint64_t>),::llvm::AtomicOrdering::Acquire)};
        auto* bit{b.CreateShl(b.getInt64(1),b.CreateURem(slot,::llvm::ConstantInt::get(word(b),64)))};
        b.CreateCondBr(b.CreateICmpNE(b.CreateAnd(bitmap,bit),b.getInt64(0)),fast,slow);
        b.SetInsertPoint(fast);auto* cells{field(b,ctx,offsetof(view,cells),word(b))};
        auto* raw{b.CreateInBoundsGEP(b.getInt8Ty(),pointer(b,cells),b.CreateMul(slot,::llvm::ConstantInt::get(word(b),4)))};
        auto* address{b.CreatePtrToInt(raw,word(b))};auto* fast_end{b.GetInsertBlock()};b.CreateBr(join);
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST) && UWVM_EXPERIMENTAL_SEALED_LOCAL_COMPACT_CAST == 1
        b.SetInsertPoint(slow);
        auto* module{module_address(s)}; if(!module) { return nullptr; }
        auto* signature{::llvm::FunctionType::get(word(b),
            {word(b),word(b),b.getInt32Ty(),word(b),b.getInt32Ty()},false)};
        auto* local_values{leaf<local_compact_cast_leaf>(b,signature,
            {ctx,module,tag,token,b.getInt32(static_cast<::std::uint32_t>(target.heap.code))},
            ::uwvm2::utils::container::u8string_view{u8"gc_sealed_local_compact_cast_v1"})};
        if(!local_values) { return nullptr; }
        // The nonzero native borrow is consumed by the unchanged adjacent
        // witness getter. Decline first retires, then calls the original cast;
        // stale/foreign/null/type-mismatch values keep the original semantics.
        auto* local_end{b.GetInsertBlock()};
        auto* fallback{::llvm::BasicBlock::Create(b.getContext(),"compact.cast.retire",fn)};
        b.CreateCondBr(b.CreateICmpUGT(local_values,::llvm::ConstantInt::get(word(b),1)),join,fallback);
        b.SetInsertPoint(fallback); if(!retire(s)) { return nullptr; }
        auto* old{emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness_unsealed(s,target)}; if(!old) { return nullptr; }
        auto* slow_end{b.GetInsertBlock()};b.CreateBr(join);b.SetInsertPoint(join);
        auto* result{b.CreatePHI(word(b),3,"gc.cast.native.values")};
        result->addIncoming(address,fast_end);result->addIncoming(local_values,local_end);result->addIncoming(old,slow_end);
#else
        b.SetInsertPoint(slow);if(!retire(s)){return nullptr;}
        auto* old{emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness_unsealed(s,target)};if(!old){return nullptr;}
        auto* slow_end{b.GetInsertBlock()};b.CreateBr(join);b.SetInsertPoint(join);
        auto* result{b.CreatePHI(word(b),2,"gc.cast.native.values")};result->addIncoming(address,fast_end);result->addIncoming(old,slow_end);
#endif
        return result; // original strict byte-offset/empty-BB getter consumes it
    }
}
#endif

[[nodiscard]] inline ::llvm::Value* emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target) noexcept
{
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    if(state.valid && state.sealed_compact_cursor && sealed_compact_codegen::native_layout(state) &&
       target.heap.code >= 0 && static_cast<::std::uint_least64_t>(target.heap.code) <= UINT32_MAX &&
       !state.operand_stack.empty() && state.operand_stack.back().value &&
       state.operand_stack.back().value->getType()->isIntegerTy(sizeof(::uwvm2::uwvm::runtime::storage::gc_reference)*CHAR_BIT))
    {
        auto const* module{state.local_func_storage_ptr->runtime_module_ptr}; ::std::size_t count{};
        if(module && module->gc_store && module->gc_store->field_count(target.heap.code,count) && count==1uz &&
           llvm_jit_gc_immutable_cast_witness_type(*module->gc_store,target.heap.code))
        { return sealed_compact_codegen::cast(state,target); }
    }
    if(!sealed_compact_codegen::retire(state)) { return nullptr; }
#endif
    return emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness_unsealed(state,target);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_aggregate(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded) noexcept
{
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    if(sealed_compact_codegen::eligible(state,decoded)) { return sealed_compact_codegen::new32(state,decoded); }
    // Adjacent getter MUST consume its existing lifetime-bounded witness before
    // any retire helper. Its fallback bridge performs the actual cold boundary.
    if(decoded.opcode < 2u || decoded.opcode > 4u)
    {
        if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state)) { return false; }
    }
#endif
    return try_emit_runtime_local_func_llvm_jit_gc_aggregate_unsealed(state,decoded);
}
