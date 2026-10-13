// Source candidate: real LLVM GC emitter + native TargetMachine output.
// Not a VM benchmark. Native compilation/execution is only for the Linux keeper.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <memory>
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace d = compiler::details;
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace c = ::uwvm2::utils::container;
using wasm_type = d::runtime_operand_stack_value_type;
static void require(bool value, unsigned line)
{
    if(!value) { ::fast_io::io::perrln("FAIL GC status dispatch line=", line); ::fast_io::fast_terminate(); }
}
#define GC_STATUS_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
static void write_bytes(char const* directory, char const* name, ::std::byte const* begin, ::std::size_t size)
{
    auto const path{c::concat_uwvm(::fast_io::mnp::os_c_str(directory), "/", ::fast_io::mnp::os_c_str(name))};
    ::fast_io::native_file file{::fast_io::mnp::os_c_str(path.c_str()), ::fast_io::open_mode::out};
    // [complete live buffer][size bytes] end
    // [safe ] begin+size is the end of the caller-owned IR/object allocation.
    ::fast_io::operations::write_all_bytes(file, begin, begin + size);
}
static void write_ir(::llvm::Module const& ir, char const* directory, char const* name)
{
    c::u8string text{}; d::raw_uwvm_string_ostream stream{text};
    ir.print(stream, nullptr); stream.flush();
    write_bytes(directory, name, reinterpret_cast<::std::byte const*>(text.data()), text.size());
}
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
static bool matches_actual_riscv64_bridge_address(::llvm::Value* called, ::std::uintptr_t expected)
{
    if(called == nullptr || expected == 0u) { return false; }
    auto const pointer{called->stripPointerCasts()};
    ::llvm::Value* address{};
    if(auto cast{::llvm::dyn_cast<::llvm::IntToPtrInst>(pointer)}) { address = cast->getOperand(0u); }
    else if(auto expression{::llvm::dyn_cast<::llvm::ConstantExpr>(pointer)};
        expression && expression->getOpcode() == ::llvm::Instruction::IntToPtr)
    { address = expression->getOperand(0u); }
    else { return false; }
    // The production RV64 emitter forces a full-width assembler `li` before
    // inttoptr, preventing O3 from introducing a far data relocation. Check
    // that exact integer source; neither a symbol name nor a pointer type
    // identifies the native bridge. The constant form is checked separately.
    if(auto materializer{::llvm::dyn_cast<::llvm::CallInst>(address)})
    {
        auto const assembly{::llvm::dyn_cast<::llvm::InlineAsm>(materializer->getCalledOperand()->stripPointerCasts())};
        if(assembly == nullptr || assembly->getAsmString() != "li $0, $1" ||
           assembly->getConstraintString() != "=r,i" || assembly->hasSideEffects() ||
           materializer->arg_size() != 1u || !materializer->doesNotAccessMemory() || !materializer->doesNotThrow())
        { return false; }
        auto const i64{::llvm::Type::getInt64Ty(materializer->getContext())};
        if(materializer->getFunctionType() != ::llvm::FunctionType::get(i64, {i64}, false)) { return false; }
        address = materializer->getArgOperand(0u);
    }
    auto const integer{::llvm::dyn_cast<::llvm::ConstantInt>(address)};
    return integer != nullptr && integer->getType()->isIntegerTy(64u) && integer->getZExtValue() == expected;
}
#endif
// Interpret only the actual emitted cold classifier's integer SSA. This is
// IR semantics evidence; it never invokes a synthetic replacement native setter.
static ::std::uint64_t classifier_value(::llvm::Value const* value,
    ::llvm::Value const* actual_status, ::std::uintptr_t status)
{
    GC_STATUS_CHECK(value != nullptr);
    if(value == actual_status) { return status; }
    if(auto constant{::llvm::dyn_cast<::llvm::ConstantInt>(value)}) { return constant->getZExtValue(); }
    if(auto comparison{::llvm::dyn_cast<::llvm::ICmpInst>(value)})
    {
        GC_STATUS_CHECK(comparison->getPredicate() == ::llvm::ICmpInst::ICMP_EQ);
        return classifier_value(comparison->getOperand(0u), actual_status, status) ==
            classifier_value(comparison->getOperand(1u), actual_status, status);
    }
    if(auto operation{::llvm::dyn_cast<::llvm::BinaryOperator>(value)})
    {
        GC_STATUS_CHECK(operation->getOpcode() == ::llvm::Instruction::Or);
        return classifier_value(operation->getOperand(0u), actual_status, status) |
            classifier_value(operation->getOperand(1u), actual_status, status);
    }
    if(auto selection{::llvm::dyn_cast<::llvm::SelectInst>(value)})
    {
        return classifier_value(classifier_value(selection->getCondition(), actual_status, status) ?
            selection->getTrueValue() : selection->getFalseValue(), actual_status, status);
    }
    GC_STATUS_CHECK(false); return 0u;
}
static bool is_actual_trap(::llvm::CallInst const& call)
{
    auto const type{d::get_llvm_runtime_trap_bridge_function_type(call.getContext())};
    if(call.getFunctionType() != type) { return false; }
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    return matches_actual_riscv64_bridge_address(call.getCalledOperand(),
        d::get_llvm_runtime_bridge_function_address(::uwvm2::runtime::lib::llvm_jit_runtime_trap));
#else
    auto const name{d::get_llvm_runtime_bridge_function_symbol_name<::uwvm2::runtime::lib::llvm_jit_runtime_trap>(type)};
    auto const function{call.getCalledFunction()};
    return function != nullptr && function->getName() == d::get_llvm_string_ref(name) &&
        reinterpret_cast<::std::uintptr_t>(::llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(
            d::get_llvm_string_ref(name).str())) ==
        d::get_llvm_runtime_bridge_function_address(::uwvm2::runtime::lib::llvm_jit_runtime_trap);
#endif
}

static ::llvm::BranchInst const& check_diamond(::llvm::Function const& function, ::llvm::Value const& status)
{
    auto const source{::llvm::dyn_cast<::llvm::Instruction>(::std::addressof(status))};
    auto const branch{::llvm::dyn_cast<::llvm::BranchInst>((source ? source->getParent() :
        ::std::addressof(function.getEntryBlock()))->getTerminator())};
    GC_STATUS_CHECK(branch && branch->isConditional());
    auto const guard{::llvm::dyn_cast<::llvm::ICmpInst>(branch->getCondition())};
    GC_STATUS_CHECK(guard && guard->getPredicate() == ::llvm::ICmpInst::ICMP_EQ &&
        guard->getOperand(0u) == ::std::addressof(status));
    auto const zero{::llvm::dyn_cast<::llvm::ConstantInt>(guard->getOperand(1u))};
    GC_STATUS_CHECK(zero && zero->isZero());
    auto const ok{branch->getSuccessor(0u)}; auto const error{branch->getSuccessor(1u)};
    GC_STATUS_CHECK(ok != error && ::llvm::isa<::llvm::UnreachableInst>(error->getTerminator()));
    ::llvm::CallInst const* trap{}; bool clobber{};
    for(auto const& instruction : *error)
    {
        auto const call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))};
        if(!call) { continue; }
        if(is_actual_trap(*call)) { GC_STATUS_CHECK(trap == nullptr); trap = call; }
        if(auto assembly{::llvm::dyn_cast<::llvm::InlineAsm>(call->getCalledOperand()->stripPointerCasts())};
           assembly && assembly->getConstraintString() == "~{memory}")
        { GC_STATUS_CHECK(assembly->hasSideEffects() && call->getTailCallKind() == ::llvm::CallInst::TCK_NoTail); clobber = true; }
    }
    GC_STATUS_CHECK(trap && clobber && trap->getCallingConv() == d::get_llvm_jit_host_calling_conv() &&
        trap->getTailCallKind() == ::llvm::CallInst::TCK_NoTail && trap->arg_size() == 3u);
    for(auto const* user : status.users())
    {
        auto const instruction{::llvm::dyn_cast<::llvm::Instruction>(user)};
        GC_STATUS_CHECK(instruction && (instruction == guard || instruction->getParent() == error));
    }
    using trap_kind = ::uwvm2::runtime::lib::llvm_jit_trap_kind;
    for(::std::uint_least64_t value : {1ull,2ull,3ull,4ull,5ull,6ull,7ull,8ull,9ull,10ull,0x80000000ull,0xffffffffull})
    {
        auto const expected{value == 3u ? trap_kind::null_reference : value == 6u ? trap_kind::array_out_of_bounds :
            value == 8u || value == 9u ? trap_kind::gc_allocation_failure : trap_kind::runtime_invariant_failure};
        GC_STATUS_CHECK(classifier_value(guard, ::std::addressof(status), value) == 0u &&
            classifier_value(trap->getArgOperand(0u), ::std::addressof(status), value) == static_cast<::std::uint_least64_t>(expected));
    }
    GC_STATUS_CHECK(classifier_value(guard, ::std::addressof(status), 0u) == 1u);
    if(status.getType()->getIntegerBitWidth() > 32u)
    {
        GC_STATUS_CHECK(classifier_value(trap->getArgOperand(0u), ::std::addressof(status), UINT64_MAX) ==
            static_cast<::std::uint_least64_t>(trap_kind::runtime_invariant_failure));
    }
    auto const intptr{::llvm::Type::getIntNTy(function.getContext(), sizeof(::std::uintptr_t) * CHAR_BIT)};
    for(unsigned index{1u}; index != 3u; ++index)
    {
        auto const argument{trap->getArgOperand(index)}; GC_STATUS_CHECK(argument->getType() == intptr);
        if constexpr(d::llvm_jit_win64_seh_explicit_trap_context_enabled())
        {
            auto const read{::llvm::dyn_cast<::llvm::IntrinsicInst>(argument)};
            GC_STATUS_CHECK(read && read->getIntrinsicID() == ::llvm::Intrinsic::read_register && read->getParent() == error);
        }
        else { auto const ignored{::llvm::dyn_cast<::llvm::ConstantInt>(argument)}; GC_STATUS_CHECK(ignored && ignored->isZero()); }
    }
    return *branch;
}
static t::recursive_type_section declarations()
{
    t::recursive_type_section out{}; out.type_count = 2u; t::recursive_group group{};
    t::sub_type structure{}; structure.kind = t::composite_kind::struct_;
    for(auto kind : {t::value_kind::i32,t::value_kind::f32,t::value_kind::i32,t::value_kind::i32,t::value_kind::i64})
    { t::field_type field{}; field.mutable_ = true; field.storage.value.kind = kind; structure.fields.push_back(field); }
    structure.fields[2uz].storage.packed = t::packed_kind::i8;
    structure.fields[3uz].storage.packed = t::packed_kind::i16;
    group.types.push_back(::std::move(structure));
    t::sub_type array{}; array.kind = t::composite_kind::array;
    t::field_type element{}; element.mutable_ = true; element.storage.value.kind = t::value_kind::i64;
    array.fields.push_back(element); group.types.push_back(::std::move(array)); out.groups.push_back(::std::move(group)); return out;
}
static void state_at(d::runtime_local_func_llvm_jit_emit_state_t& state, compiler::local_func_storage_t& local,
    ::llvm::Function& function, ::llvm::BasicBlock& entry)
{
    state.valid = true; state.local_func_storage_ptr = ::std::addressof(local);
    state.llvm_context_holder = ::std::addressof(function.getContext()); state.llvm_module = function.getParent();
    state.llvm_function = state.llvm_public_entry_function = ::std::addressof(function);
    state.emit_call_stack_frames = false; state.emit_precise_gc_root_frames = false;
    state.ir_builder = c::make_delete_owned<::llvm::IRBuilder<>>(::std::addressof(entry));
}
static void protocol_and_rollback(::llvm::Module& ir, gc::wasm_module_storage_t& module)
{
    auto& context{ir.getContext()};
    for(unsigned bits : {32u, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT)})
    {
        auto const name{c::concat_uwvm("gc_status_protocol_",bits,"_",ir.size())};
        auto const type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context), {::llvm::Type::getIntNTy(context,bits)}, false)};
        auto const function{::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage, ::llvm::StringRef{name.data(),name.size()},ir)};
        auto const entry{::llvm::BasicBlock::Create(context,"entry",function)}; ::llvm::IRBuilder<> builder{entry};
        d::llvm_jit_gc_status_dispatch_plan plan{}; GC_STATUS_CHECK(plan.prepare(ir,builder));
        plan.commit(*function->getArg(0u)); builder.CreateRet(builder.getInt32(17u));
        check_diamond(*function,*function->getArg(0u)); GC_STATUS_CHECK(!::llvm::verifyFunction(*function, &::llvm::errs()));
    }
    auto const type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), false)};
    auto const function{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,"gc_status_abandon",ir)};
    auto const entry{::llvm::BasicBlock::Create(context,"entry",function)}; ::llvm::IRBuilder<> builder{entry};
    {
        d::llvm_jit_gc_status_dispatch_plan plan{}; GC_STATUS_CHECK(plan.prepare(ir,builder));
        GC_STATUS_CHECK(function->size() == 3uz && entry->empty());
        // [three live owned blocks][cold block cursor]
        // [safe ] the plan must restore this cursor before erasing its blocks.
        builder.SetInsertPoint(::std::addressof(function->back()));
    }
    GC_STATUS_CHECK(function->size() == 1uz && builder.GetInsertBlock() == entry && entry->empty());
    ::llvm::Module foreign{"foreign",context};
    { d::llvm_jit_gc_status_dispatch_plan plan{}; GC_STATUS_CHECK(!plan.prepare(foreign,builder)); }
    builder.CreateRetVoid();
    { d::llvm_jit_gc_status_dispatch_plan plan{}; GC_STATUS_CHECK(!plan.prepare(ir,builder)); }
    GC_STATUS_CHECK(function->size() == 1uz && !::llvm::verifyFunction(*function, &::llvm::errs()));
#if !(defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64))
    ::llvm::Module collision{"trap-type-collision",context}; collision.setDataLayout(ir.getDataLayout());
    auto const trap_type{d::get_llvm_runtime_trap_bridge_function_type(context)};
    auto const symbol{d::get_llvm_runtime_bridge_function_symbol_name<::uwvm2::runtime::lib::llvm_jit_runtime_trap>(trap_type)};
    auto const wrong{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), {::llvm::Type::getInt8Ty(context)}, false)};
    ::llvm::Function::Create(wrong,::llvm::GlobalValue::ExternalLinkage,d::get_llvm_string_ref(symbol),collision);
    auto const ref{d::get_llvm_type_from_wasm_value_type(context,wasm_type::funcref)};
    auto const actual_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context),{ref},false)};
    auto const actual{::llvm::Function::Create(actual_type,::llvm::GlobalValue::ExternalLinkage,"declined_actual_getter",collision)};
    auto const initial{::llvm::BasicBlock::Create(context,"entry",actual)};
    compiler::local_func_storage_t local{}; local.runtime_module_ptr = ::std::addressof(module);
    d::runtime_local_func_llvm_jit_emit_state_t state{}; state_at(state,local,*actual,*initial);
    state.operand_stack.push_back({.type=wasm_type::funcref,.value=actual->getArg(0u)});
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate decoded{}; decoded.opcode = 2u; decoded.first = 0u; decoded.second = 0u;
    GC_STATUS_CHECK(!d::try_emit_runtime_local_func_llvm_jit_gc_aggregate(state,decoded));
    GC_STATUS_CHECK(initial->empty() && actual->size() == 1uz && state.operand_stack.size() == 1uz);
    // Generic heap-input dispatch must decline before any input allocation,
    // root snapshot, stores or native aggregate call in the same collision.
    state.operand_stack.clear(); decoded.opcode = 8u; decoded.first = 1u; decoded.second = 130u;
    for(unsigned index{}; index != 130u; ++index)
    { state.operand_stack.push_back({.type=wasm_type::i64,.value=state.ir_builder->getInt64(index)}); }
    GC_STATUS_CHECK(!d::try_emit_runtime_local_func_llvm_jit_gc_aggregate(state,decoded));
    GC_STATUS_CHECK(initial->empty() && actual->size() == 1uz && state.operand_stack.size() == 130uz);
    state.ir_builder->CreateRetVoid(); GC_STATUS_CHECK(!::llvm::verifyModule(collision,&::llvm::errs()));
#endif
}
static void emit_actual(::llvm::Module& ir, gc::wasm_module_storage_t& module, unsigned opcode, unsigned field,
    bool array = false)
{
    auto& context{ir.getContext()};
    auto const ref{d::get_llvm_type_from_wasm_value_type(context,wasm_type::funcref)};
    auto const i64{::llvm::Type::getInt64Ty(context)};
    wasm_type result_type{array ? wasm_type::funcref : field == 1u ? wasm_type::f32 : field < 4u ? wasm_type::i32 : wasm_type::i64};
    auto const result{opcode == 5u ? i64 : d::get_llvm_type_from_wasm_value_type(context,result_type)};
    auto const type{::llvm::FunctionType::get(result,{ref,i64},false)};
    auto const name{c::concat_uwvm(array ? "gc_heap_array_" : "gc_actual_",opcode,"_",field)};
    auto const function{::llvm::Function::Create(type,::llvm::GlobalValue::ExternalLinkage,::llvm::StringRef{name.data(),name.size()},ir)};
    function->addFnAttr(::llvm::Attribute::NoInline);
    auto const entry{::llvm::BasicBlock::Create(context,"entry",function)};
    compiler::local_func_storage_t local{}; local.runtime_module_ptr = ::std::addressof(module);
    d::runtime_local_func_llvm_jit_emit_state_t state{}; state_at(state,local,*function,*entry);
    if(array)
    { for(unsigned index{}; index != 130u; ++index) { state.operand_stack.push_back({.type=wasm_type::i64,.value=function->getArg(1u)}); } }
    else
    {
        state.operand_stack.push_back({.type=wasm_type::funcref,.value=function->getArg(0u)});
        if(opcode == 5u) { state.operand_stack.push_back({.type=wasm_type::i64,.value=function->getArg(1u)}); }
    }
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate decoded{};
    decoded.opcode = opcode; decoded.first = array ? 1u : 0u; decoded.second = array ? 130u : field;
    GC_STATUS_CHECK(d::try_emit_runtime_local_func_llvm_jit_gc_aggregate(state,decoded));
    GC_STATUS_CHECK(state.operand_stack.size() == (opcode == 5u ? 0uz : 1uz));
    auto const ok{state.ir_builder->GetInsertBlock()};
    state.ir_builder->CreateRet(opcode == 5u ? function->getArg(1u) : state.operand_stack.back().value);
    GC_STATUS_CHECK(!::llvm::verifyFunction(*function,&::llvm::errs()));
    ::llvm::Value const* status{}; ::llvm::CallInst const* free{}; ::llvm::CallInst const* native{};
    auto const intptr{::llvm::Type::getIntNTy(context,sizeof(::std::uintptr_t)*CHAR_BIT)};
    auto const free_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context),{intptr},false)};
    auto const free_name{d::get_llvm_runtime_bridge_function_symbol_name<d::llvm_jit_gc_input_free_bridge>(free_type)};
    for(auto const& block : *function) for(auto const& instruction : block)
    {
        if(auto comparison{::llvm::dyn_cast<::llvm::ICmpInst>(::std::addressof(instruction))};
           comparison && comparison->getName() == "gc.status.succeeded")
        { GC_STATUS_CHECK(status == nullptr); status = comparison->getOperand(0u); }
        if(auto call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))};call && !is_actual_trap(*call))
        {
            if(auto declaration{call->getCalledFunction()};declaration && declaration->getName() == d::get_llvm_string_ref(free_name)) { free = call; }
            // Native status bridge: scalar getter returns packed i64; generic
            // aggregate returns i32. Intrinsic/memset/materializers are excluded.
            if(!call->getCalledOperand()->stripPointerCasts()->getType()->isPointerTy()) { continue; }
            if(call->getFunctionType()->getNumParams() == (field < 4u && !array ? 4u : array ? 7u : 5u) &&
               call->getType()->isIntegerTy(field < 4u && !array ? 64u : 32u)) { GC_STATUS_CHECK(native == nullptr); native = call; }
        }
        if(auto load{::llvm::dyn_cast<::llvm::LoadInst>(::std::addressof(instruction))};load && load->getName() == "gc.result")
        { GC_STATUS_CHECK(load->getParent() == ok); }
    }
    GC_STATUS_CHECK(status && native); auto const& branch{check_diamond(*function,*status)};
    GC_STATUS_CHECK(branch.getSuccessor(0u) == ok);
    if(array)
    {
#if !(defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64))
        GC_STATUS_CHECK(free && free->getParent() == native->getParent() && native->comesBefore(free) && free->comesBefore(&branch));
#else
        bool actual_free{};
        for(auto const& instruction : *native->getParent()) if(auto call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))};call &&
            matches_actual_riscv64_bridge_address(call->getCalledOperand(),d::get_llvm_runtime_bridge_function_address(d::llvm_jit_gc_input_free_bridge)))
        { GC_STATUS_CHECK(native->comesBefore(call) && call->comesBefore(&branch)); actual_free = true; }
        GC_STATUS_CHECK(actual_free);
#endif
    }
}
static void check_optimized_hot_edges(::llvm::Module const& ir)
{
    unsigned actuals{};
    for(auto const& function : ir)
    {
        if(!function.getName().starts_with("gc_actual_") && !function.getName().starts_with("gc_heap_array_")) { continue; }
        ++actuals; bool found{};
        for(auto const& block : function)
        {
            auto const branch{::llvm::dyn_cast<::llvm::BranchInst>(block.getTerminator())};
            if(!branch || !branch->isConditional()) { continue; }
            auto const condition{::llvm::dyn_cast<::llvm::ICmpInst>(branch->getCondition())};
            if(!condition) { continue; }
            bool gc_call{};
            for(auto const& instruction : block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))};
                if(call && !call->getType()->isVoidTy() && !::llvm::isa<::llvm::IntrinsicInst>(call) &&
                   call->arg_size() >= 4u && !::llvm::isa<::llvm::InlineAsm>(call->getCalledOperand()->stripPointerCasts())) { gc_call = true; }
            }
            if(!gc_call) { continue; }
            GC_STATUS_CHECK(!found); found = true; unsigned comparisons{};
            for(auto const& instruction : block)
            {
                comparisons += ::llvm::isa<::llvm::ICmpInst>(instruction);
                GC_STATUS_CHECK(!::llvm::isa<::llvm::SelectInst>(instruction));
            }
            GC_STATUS_CHECK(comparisons == 1u);
            // Packed getter statuses may become `icmp ult packed, 2^32`.
            // No null/bounds/OOM classifier may be hoisted beside that guard.
        }
        GC_STATUS_CHECK(found);
    }
    GC_STATUS_CHECK(actuals == 7u);
}
int main(int argc,char** argv)
{
    GC_STATUS_CHECK(argc == 2); GC_STATUS_CHECK(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter());
    ::llvm::EngineBuilder engine{}; engine.setOptLevel(::llvm::CodeGenOptLevel::Aggressive); engine.setCodeModel(::llvm::CodeModel::Large);
    ::std::unique_ptr<::llvm::TargetMachine> machine{engine.selectTarget()}; GC_STATUS_CHECK(machine);
    ::llvm::LLVMContext context{}; ::llvm::Module ir{"gc-status-dispatch",context};
#if LLVM_VERSION_MAJOR >= 21
    ir.setTargetTriple(machine->getTargetTriple());
#else
    ir.setTargetTriple(machine->getTargetTriple().str());
#endif
    ir.setDataLayout(machine->createDataLayout());
    gc::wasm_module_storage_t module{}; module.module_name = u8"gc-status-dispatch";
    module.gc_lease_roots = ::std::make_shared<gc::gc_lease_owner>(); auto section{declarations()};
    module.gc_store = ::std::make_shared<gc::gc_object_store>(section,module.gc_lease_roots); GC_STATUS_CHECK(module.gc_store->valid());
    protocol_and_rollback(ir,module);
    emit_actual(ir,module,2u,0u); emit_actual(ir,module,2u,1u); emit_actual(ir,module,3u,2u); emit_actual(ir,module,4u,3u);
    emit_actual(ir,module,2u,4u); emit_actual(ir,module,5u,4u); emit_actual(ir,module,8u,0u,true);
    write_ir(ir,argv[1],"gc-status.input.ll"); GC_STATUS_CHECK(!::llvm::verifyModule(ir,&::llvm::errs()));
    ::llvm::LoopAnalysisManager loops; ::llvm::FunctionAnalysisManager functions; ::llvm::CGSCCAnalysisManager graph; ::llvm::ModuleAnalysisManager modules;
    ::llvm::PassBuilder passes{machine.get()}; passes.registerLoopAnalyses(loops); passes.registerFunctionAnalyses(functions);
    passes.registerCGSCCAnalyses(graph); passes.registerModuleAnalyses(modules); passes.crossRegisterProxies(loops,functions,graph,modules);
    auto pipeline{passes.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O3)}; pipeline.run(ir,modules);
    GC_STATUS_CHECK(!::llvm::verifyModule(ir,&::llvm::errs())); write_ir(ir,argv[1],"gc-status.optimized.ll"); check_optimized_hot_edges(ir);
    ::llvm::SmallVector<char,0u> bytes{}; ::llvm::raw_svector_ostream stream{bytes}; ::llvm::legacy::PassManager codegen{};
    GC_STATUS_CHECK(!machine->addPassesToEmitFile(codegen,stream,nullptr,::llvm::CodeGenFileType::ObjectFile,false)); codegen.run(ir);
    GC_STATUS_CHECK(!bytes.empty()); write_bytes(argv[1],"gc-status.o",reinterpret_cast<::std::byte const*>(bytes.data()),bytes.size());
    ::fast_io::io::println("PASS GC status dispatch real_emitters=7 protocol_widths=2 rollback_controls=4 timing=false object_bytes=",bytes.size());
}
