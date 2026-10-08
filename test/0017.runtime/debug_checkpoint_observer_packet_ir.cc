// Actual private observer packet -> LLVM verifier -> native MCJIT / target ELF.
// Metadata and reference carriers here are component DATA, not VM/source/ASM
// leases, executable continuation admission or complete target C++ ABI proof.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/llvm_jit/mcjit_target_support.h>
#include <uwvm2/uwvm/runtime/macro/push_macros.h>
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/section_memory_manager.h>
#include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <fast_io.h>
#include <array>
#include <cstdint>
#include <memory>
namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace support = ::uwvm2::runtime::compiler::llvm_jit::details;
namespace cp = ::uwvm2::runtime::checkpoint;
static void require(bool ok, char const* message)
{
    if(!ok) { ::fast_io::io::perrln("observer packet IR: ",::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
class fast_io_ir_stream final : public ::llvm::raw_ostream
{
    ::fast_io::native_io_observer out_;
    ::std::uint64_t position_{};
    void write_impl(char const* bytes, ::std::size_t size) override
    {
        if(size==0u) { return; }
        ::fast_io::io::print(out_,::fast_io::mnp::strvw(bytes,bytes+size)); position_ += size;
    }
    ::std::uint64_t current_pos() const override { return position_; }
public:
    explicit fast_io_ir_stream(::fast_io::native_io_observer out) : ::llvm::raw_ostream{true},out_{out} {}
};

// A real native DATA stress witness for unchanged numeric prefixes, popped
// cells, exact float/vector bits, reference refresh and a distinct CFG block.
// The same optimizer is enabled only by a matching lexical observer control map.
static ::llvm::Function* add_incremental_packet_probe(::llvm::LLVMContext& context,::llvm::Module& module)
{
    ::llvm::IRBuilder<> types{context};
    auto const function{::llvm::Function::Create(::llvm::FunctionType::get(types.getInt32Ty(),false),
        ::llvm::GlobalValue::InternalLinkage,"component.incremental.packet",module)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder=&context;state.llvm_module=&module;state.llvm_function=function;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(context,"entry",function));
    auto& ir{*state.ir_builder};
    constexpr ::std::size_t maximum{300u}, guard{16u}, bytes{maximum*cp::native_slot_bytes};
    ::std::array<::std::uint8_t,bytes+guard*2u> poison{};poison.fill(0xa5u);
    auto const storage{new ::llvm::GlobalVariable(module,::llvm::ArrayType::get(ir.getInt8Ty(),poison.size()),false,
        ::llvm::GlobalValue::PrivateLinkage,::llvm::ConstantDataArray::get(context,::llvm::ArrayRef<::std::uint8_t>{poison}),"component.incremental.storage")};
    auto const integer{ir.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*8u))};
    auto const allocator{::llvm::Function::Create(::llvm::FunctionType::get(integer,{integer},false),
        ::llvm::GlobalValue::InternalLinkage,"component.incremental.allocate",module)};
    ::llvm::IRBuilder<> allocate{::llvm::BasicBlock::Create(context,"entry",allocator)};
    allocate.CreateRet(allocate.CreatePtrToInt(allocate.CreateInBoundsGEP(allocate.getInt8Ty(),storage,allocate.getInt64(guard)),integer));
    state.checkpoint_observer_workspace_allocation=ir.CreateCall(allocator,{::llvm::ConstantInt::get(integer,bytes)});
    state.checkpoint_observer_workspace=ir.CreateIntToPtr(state.checkpoint_observer_workspace_allocation,ir.getPtrTy());
    cp::function_plan plan{};plan.profile=cp::compilation_profile::create_for_trusted_observer();
    plan.expression_bytes=2048u;plan.function_generation=1u;
    emit::llvm_jit_checkpoint_observer_control_map controls{.plan=&plan};
    state.checkpoint_plan=&plan;state.checkpoint_observer_controls=&controls;
    ::llvm::Value* ok{ir.getTrue()};
    auto check{[&](::llvm::Value* condition) { ok=ir.CreateAnd(ok,condition); }};
    auto cell{[&](::std::size_t offset) { return ir.CreateInBoundsGEP(ir.getInt8Ty(),state.checkpoint_observer_workspace,ir.getInt64(offset)); }};
    auto bits{[&](::std::size_t slot,unsigned width,::llvm::APInt expected)
    {
        auto const value{ir.CreateLoad(ir.getIntNTy(width),cell(slot*cp::native_slot_bytes))};value->setAlignment(::llvm::Align{1u});
        check(ir.CreateICmpEQ(value,::llvm::ConstantInt::get(context,expected)));
        for(auto i{width/8u};i!=cp::native_slot_bytes;++i)
        {
            auto const padding{ir.CreateLoad(ir.getInt8Ty(),cell(slot*cp::native_slot_bytes+i))};padding->setAlignment(::llvm::Align{1u});
            check(ir.CreateICmpEQ(padding,ir.getInt8(0u)));
        }
    }};
    auto capture{[&](::llvm::ArrayRef<::llvm::Value*> live,::llvm::ArrayRef<cp::types::core_value_type> semantic)
    {
        require(live.size()==semantic.size(),"incremental DATA exact semantic tuple");
        cp::safepoint_layout site{};site.identifier=plan.sites.size()+1u;site.opcode_offset=plan.sites.size();site.operand_count=live.size();
        cp::control_layout root{};root.end_offset=plan.expression_bytes-1u;site.controls.push_back(root);
        for(auto type:semantic) { site.slots.push_back({type,true}); }
        plan.sites.push_back(::std::move(site));
        require(emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state,plan.sites.back(),nullptr,live).valid,"incremental actual packet emission");
    }};
    ::std::vector<::llvm::Value*> live{};::std::vector<cp::types::core_value_type> semantic{};
    capture(live,semantic);
    for(::std::size_t i{};i!=maximum;++i)
    {
        live.push_back(ir.getInt32(static_cast<unsigned>(i+1u)));semantic.push_back({cp::types::value_kind::i32});
        capture(live,semantic);
        // Check every new top while retaining the genuine unchanged prefix.
        auto const value{ir.CreateLoad(ir.getInt32Ty(),cell(i*16u))};value->setAlignment(::llvm::Align{1u});
        check(ir.CreateICmpEQ(value,ir.getInt32(static_cast<unsigned>(i+1u))));
    }
    bits(0u,32u,::llvm::APInt{32u,1u});bits(299u,32u,::llvm::APInt{32u,300u});
    for(::std::size_t i{maximum};i!=0u;--i)
    {
        live.pop_back();semantic.pop_back();capture(live,semantic);
        if(!live.empty())
        {
            auto const value{ir.CreateLoad(ir.getInt32Ty(),cell((i-2u)*16u))};value->setAlignment(::llvm::Align{1u});
            check(ir.CreateICmpEQ(value,ir.getInt32(static_cast<unsigned>(i-1u))));
        }
    }
    // Poison a popped cell, then restore the same uniqued numeric constant:
    // the old SSA identity must not keep stale data/padding after shrink.
    ir.CreateStore(::llvm::Constant::getAllOnesValue(ir.getInt128Ty()),cell(0u))->setAlignment(::llvm::Align{1u});
    live={ir.getInt32(1u)};semantic={{cp::types::value_kind::i32}};capture(live,semantic);bits(0u,32u,::llvm::APInt{32u,1u});
    live={ir.CreateBitCast(ir.getInt32(0x7f812345u),ir.getFloatTy())};semantic={{cp::types::value_kind::f32}};
    capture(live,semantic);bits(0u,32u,::llvm::APInt{32u,0x7f812345u});
    live={ir.CreateBitCast(ir.getInt64(0x7ff0123456789abcull),ir.getDoubleTy())};semantic={{cp::types::value_kind::f64}};
    capture(live,semantic);bits(0u,64u,::llvm::APInt{64u,0x7ff0123456789abcull});
    auto const vector_type{emit::get_llvm_type_from_wasm_value_type(context,emit::runtime_operand_stack_value_type::v128)};
    ::std::uint64_t const vector_words[]{0x8877665544332211ull,0xffeeddccbbaa0099ull};::llvm::APInt vector_bits{128u,2u,vector_words};
    live={::llvm::ConstantExpr::getBitCast(::llvm::ConstantInt::get(context,vector_bits),vector_type)};semantic={{cp::types::value_kind::v128}};
    capture(live,semantic);bits(0u,128u,vector_bits);
    live={ir.getInt32(1u)};semantic={{cp::types::value_kind::i32}};capture(live,semantic);bits(0u,32u,::llvm::APInt{32u,1u});
    // A distinct Wasm/CFG block cannot inherit packet dominance from a sibling.
    auto const next{::llvm::BasicBlock::Create(context,"different.cfg.block",function)};
    ir.CreateStore(::llvm::Constant::getAllOnesValue(ir.getInt128Ty()),cell(0u))->setAlignment(::llvm::Align{1u});
    ir.CreateBr(next);ir.SetInsertPoint(next);capture(live,semantic);bits(0u,32u,::llvm::APInt{32u,1u});
    auto const reference_type{emit::get_llvm_type_from_wasm_value_type(context,emit::runtime_operand_stack_value_type::funcref)};
    live={::llvm::ConstantInt::get(reference_type,17u)};
    semantic={{cp::types::value_kind::reference,{static_cast<::std::int_least64_t>(cp::types::abstract_heap_type::i31)},false}};
    capture(live,semantic);ir.CreateStore(::llvm::Constant::getAllOnesValue(ir.getInt128Ty()),cell(0u))->setAlignment(::llvm::Align{1u});
    capture(live,semantic);bits(0u,static_cast<unsigned>(sizeof(::std::uintptr_t)*8u),::llvm::APInt{static_cast<unsigned>(sizeof(::std::uintptr_t)*8u),17u});
    for(::std::size_t i{};i!=guard;++i)
    {
        for(auto offset:{i,guard+bytes+i})
        {
            auto const address{ir.CreateInBoundsGEP(ir.getInt8Ty(),storage,ir.getInt64(offset))};
            auto const value{ir.CreateLoad(ir.getInt8Ty(),address)};value->setAlignment(::llvm::Align{1u});check(ir.CreateICmpEQ(value,ir.getInt8(0xa5u)));
        }
    }
    require(cp::validate_plan(plan)==cp::status::ok,"complete exact incremental plan");
    ::std::size_t count{};for(auto const& block:*function) { count+=block.size(); }
    // Disabling observation must still return BEFORE creating packet IR.
    emit::runtime_local_func_llvm_jit_emit_state_t ordinary{};ordinary.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(next);
    require(emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(ordinary,plan.sites.back(),nullptr,live).valid &&
        ordinary.checkpoint_observer_nonlocals.empty(),"ordinary path has no observer cache");
    require(emit::emit_runtime_local_func_llvm_jit_native_numeric_values(ordinary) &&
        ordinary.debug_native_numeric_definitions.empty(),"ordinary path has no numeric IR/cache");
    ::std::size_t unchanged{};for(auto const& block:*function) { unchanged+=block.size(); }
    require(count==unchanged && count<12000u,"no ordinary IR and bounded linear 300-push/drop packet IR");
    ir.CreateRet(ir.CreateSelect(ok,ir.getInt32(0u),ir.getInt32(1u)));
    ::fast_io::io::println("incremental packet native DATA instructions=",count," snapshots=",plan.sites.size());
    return function;
}

// Actual native numeric identities grow with changed definitions/opcode tops,
// rather than repeatedly forcing every retained prefix back into registers.
// This is compiler DATA; register authority still comes only from native DWARF.
static ::llvm::Function* add_incremental_numeric_probe(::llvm::LLVMContext& context,::llvm::Module& module)
{
    namespace origin = ::uwvm2::runtime::compiler::llvm_jit::native_provenance;
    ::llvm::IRBuilder<> types{context};
    auto const function{::llvm::Function::Create(::llvm::FunctionType::get(types.getInt32Ty(),false),
        ::llvm::GlobalValue::InternalLinkage,"component.incremental.numeric",module)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder=&context;state.llvm_module=&module;state.llvm_function=function;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(context,"entry",function));
    auto& ir{*state.ir_builder};
    state.emit_debug_safe_points=true;state.debug_native_provenance=origin::attach(*function,"uwvm-m0-f0-g1.wasm-native-v1");
    require(state.debug_native_provenance!=nullptr,"actual numeric compiler-owned scope");
    ::std::array<::std::uint32_t,300u> globals{};for(::std::size_t i{};i!=globals.size();++i) { globals[i]=static_cast<::std::uint32_t>(37u+i); }
    auto const storage{new ::llvm::GlobalVariable(module,::llvm::ArrayType::get(ir.getInt32Ty(),globals.size()),false,
        ::llvm::GlobalValue::PrivateLinkage,::llvm::ConstantDataArray::get(context,::llvm::ArrayRef<::std::uint32_t>{globals}),"component.numeric.globals")};
    ::llvm::Value* sum{ir.getInt32(0u)};
    auto count{[&]() { ::std::size_t n{};for(auto const& b:*function) { n+=b.size(); }return n; }};
    auto observe{[&](::std::size_t offset)
    {
        require(origin::location(ir,state.debug_native_provenance,offset,2048u),"exact numeric opcode metadata");
        require(emit::emit_runtime_local_func_llvm_jit_native_numeric_values(state),"actual incremental numeric emission");
        auto const before{count()};
        require(emit::emit_runtime_local_func_llvm_jit_native_numeric_values(state) && before==count(),
            "same opcode/SSA identity emits no duplicate register witness");
    }};
    for(::std::size_t i{};i!=globals.size();++i)
    {
        observe(i);
        auto const pointer{ir.CreateInBoundsGEP(storage->getValueType(),storage,{ir.getInt32(0u),ir.getInt32(static_cast<unsigned>(i))})};
        auto const value{ir.CreateLoad(ir.getInt32Ty(),pointer)};value->setAlignment(::llvm::Align{4u});
        state.operand_stack.push_back({.type=emit::runtime_operand_stack_value_type::i32,.value=value});sum=ir.CreateAdd(sum,value);
        observe(i);
        require(value->getMetadata("uwvm.native.numeric.code")==nullptr,"native memory load remains private");
    }
    for(::std::size_t i{};i!=globals.size();++i) { state.operand_stack.pop_back();observe(300u+i); }
    // Forget a popped uniqued constant; a fresh push must produce its witness.
    state.operand_stack.push_back({.type=emit::runtime_operand_stack_value_type::i32,.value=ir.getInt32(17u)});
    observe(601u);state.operand_stack.pop_back();observe(602u);
    auto const before_push{count()};state.operand_stack.push_back({.type=emit::runtime_operand_stack_value_type::i32,.value=ir.getInt32(17u)});
    observe(603u);require(count()>before_push,"popped constant has a fresh numeric witness");
    // A real new CFG block cannot inherit the old block's register witness.
    auto const next{::llvm::BasicBlock::Create(context,"separate.cfg",function)};
    origin::unknown(ir,state.debug_native_provenance);ir.CreateBr(next);ir.SetInsertPoint(next);
    auto const before_cfg{count()};observe(604u);require(count()>before_cfg,"distinct CFG numeric refresh");
    // Type changes cannot turn opaque reference DATA into numeric authority.
    state.operand_stack.back().type=emit::runtime_operand_stack_value_type::funcref;
    state.operand_stack.back().value=ir.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*8u))==ir.getInt32Ty() ?
        static_cast<::llvm::Value*>(ir.getInt32(17u)) : static_cast<::llvm::Value*>(ir.getInt64(17u));
    auto const before_ref{count()};observe(605u);require(count()==before_ref,"reference carrier has no synthetic numeric register");
    auto const before_off{count()};state.emit_debug_safe_points=false;
    require(emit::emit_runtime_local_func_llvm_jit_native_numeric_values(state) && count()==before_off,"DBG disabled emits zero numeric IR");
    origin::unknown(ir,state.debug_native_provenance);
    ir.CreateRet(ir.CreateSelect(ir.CreateICmpEQ(sum,ir.getInt32(55950u)),ir.getInt32(0u),ir.getInt32(1u)));
    origin::restrict_public_code(*function,state.debug_native_numeric_identities);
    require(count()<6000u,"bounded linear native register witness IR for 300 pushes/drops");
    ::fast_io::io::println("incremental numeric native DATA instructions=",count()," pushes=",globals.size());
    return function;
}

int main(int argc, char** argv)
{
    require(argc==1 || argc==2 || argc==3 || argc==5,"optional IR/bitcode and target pointer layout");
    unsigned pointer_bits{static_cast<unsigned>(sizeof(::std::uintptr_t)*8u)};
    bool big_endian{};
    if(argc==5)
    {
        auto const text{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[3]))};
        auto const parsed{::fast_io::parse_by_scan(text.data(),text.data()+text.size(),pointer_bits)};
        require(parsed.code==::fast_io::parse_code::ok && parsed.iter==text.data()+text.size() &&
            (pointer_bits==32u || pointer_bits==64u),"exact target pointer layout");
        auto const order{::fast_io::concat_std(::fast_io::mnp::os_c_str(argv[4]))};
        require(order=="little" || order=="big","exact target byte order");big_endian=order=="big";
    }
    auto context{::std::make_unique<::llvm::LLVMContext>()};
    auto module{::std::make_unique<::llvm::Module>("observer-packet-padding",*context)};
    module->setDataLayout(pointer_bits==64u ? (big_endian ? "E-p:64:64" : "e-p:64:64") :
        (big_endian ? "E-p:32:32" : "e-p:32:32"));
    ::llvm::IRBuilder<> types{*context};
    constexpr ::std::size_t locals{7u}, maximum_slots{10u}, guard_bytes{16u};
    constexpr auto workspace_bytes{locals*cp::observer_local_metadata_bytes+maximum_slots*cp::native_slot_bytes};
    auto const workspace_type{::llvm::ArrayType::get(types.getInt8Ty(),workspace_bytes+2u*guard_bytes)};
    ::std::array<::std::uint8_t,workspace_bytes+2u*guard_bytes> poison{};poison.fill(0xa5u);
    auto const workspace{new ::llvm::GlobalVariable(*module,workspace_type,false,::llvm::GlobalValue::PrivateLinkage,
        ::llvm::ConstantDataArray::get(*context,::llvm::ArrayRef<::std::uint8_t>{poison}),"owned.component.workspace")};
    auto const integer{types.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*8u))};
    auto const allocate{::llvm::Function::Create(::llvm::FunctionType::get(integer,{integer},false),
        ::llvm::GlobalValue::InternalLinkage,"component.allocate",*module)};
    ::llvm::IRBuilder<> allocation{::llvm::BasicBlock::Create(*context,"entry",allocate)};
    auto const actual_workspace{allocation.CreateInBoundsGEP(allocation.getInt8Ty(),workspace,allocation.getInt64(guard_bytes))};
    allocation.CreateRet(allocation.CreatePtrToInt(actual_workspace,integer));
    auto const function{::llvm::Function::Create(::llvm::FunctionType::get(types.getInt32Ty(),false),
        ::llvm::GlobalValue::ExternalLinkage,"main",*module)};
    emit::runtime_local_func_llvm_jit_emit_state_t state{};
    state.llvm_context_holder=context.get();state.llvm_module=module.get();state.llvm_function=function;
    state.ir_builder=::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(::llvm::BasicBlock::Create(*context,"entry",function));
    auto& ir{*state.ir_builder};
    auto const owner{ir.CreateCall(allocate,{::llvm::ConstantInt::get(integer,workspace_bytes)})};
    state.checkpoint_observer_workspace_allocation=owner;
    state.checkpoint_observer_workspace=ir.CreateIntToPtr(owner,ir.getPtrTy());
    cp::function_plan plan{};plan.profile=cp::compilation_profile::create_for_trusted_observer();
    plan.expression_bytes=16u;plan.function_generation=1u;
    auto const i31{static_cast<::std::int_least64_t>(cp::types::abstract_heap_type::i31)};
    cp::safepoint_layout entry{};entry.identifier=1u;entry.local_count=locals;
    entry.slots={{{cp::types::value_kind::i32},true},{{cp::types::value_kind::i64},true},
        {{cp::types::value_kind::f32},true},{{cp::types::value_kind::f64},true},{{cp::types::value_kind::v128},true},
        {{cp::types::value_kind::reference,{i31},false},false},{{cp::types::value_kind::reference,{i31},true},true}};
    cp::control_layout root{};root.end_offset=15u;entry.controls.push_back(root);plan.sites.push_back(entry);
    auto wide{entry};wide.identifier=2u;wide.opcode_offset=1u;wide.operand_count=2u;wide.saved_parameter_count=1u;
    wide.slots.push_back({{cp::types::value_kind::i32},true});
    wide.slots.push_back({{cp::types::value_kind::f64},true});
    wide.slots.push_back({{cp::types::value_kind::i64},true});
    cp::control_layout saved_if{};saved_if.kind=cp::control_kind::if_then;saved_if.entry_offset=1u;saved_if.end_offset=14u;
    saved_if.outer_operand_height=1u;saved_if.saved_parameter_count=1u;saved_if.declared_parameters={{cp::types::value_kind::i64}};
    wide.controls.push_back(saved_if);plan.sites.push_back(wide);
    auto narrow{entry};narrow.identifier=3u;narrow.opcode_offset=2u;narrow.operand_count=1u;
    narrow.slots.push_back({{cp::types::value_kind::i32},true});plan.sites.push_back(narrow);
    auto updated{entry};updated.identifier=4u;updated.opcode_offset=3u;updated.slots[5u].initialized=true;plan.sites.push_back(updated);
    require(cp::validate_plan(plan)==cp::status::ok,"exact observer site metadata");state.checkpoint_plan=&plan;
    constexpr ::std::array<::std::uint64_t,locals> bits{0x12345678u,0x123456789abcdef0u,0x7f812345u,
        0x7ff0123456789abcu,0x8877665544332211ull,0u,0u};
    for(::std::size_t i{};i!=locals;++i)
    {
        auto const carrier{emit::checkpoint_packet_physical_carrier(entry.slots[i].type)};
        state.local_types.push_back(carrier);
        auto const type{emit::get_llvm_type_from_wasm_value_type(*context,carrier)};
        auto const local{emit::create_llvm_jit_entry_block_alloca(ir,type,nullptr,"actual.local")};state.local_pointers.push_back(local);
        if(i!=5u)
        {
            auto const physical{ir.getIntNTy(static_cast<unsigned>(emit::get_runtime_wasm_value_type_abi_size(carrier)*8u))};
            ir.CreateStore(::llvm::ConstantInt::get(physical,bits[i]),local)->setAlignment(::llvm::Align{1u});
        }
        auto const flag{ir.CreateInBoundsGEP(ir.getInt8Ty(),state.checkpoint_observer_workspace,ir.getInt64(i))};
        ir.CreateStore(ir.getInt8(i==5u ? 0u : 1u),flag)->setAlignment(::llvm::Align{1u});
    }
    ::llvm::Value* ok{ir.getTrue()};
    auto check{[&](::llvm::Value* condition) { ok=ir.CreateAnd(ok,condition); }};
    auto cell{[&](::llvm::Value* base,::std::size_t offset) { return ir.CreateInBoundsGEP(ir.getInt8Ty(),base,ir.getInt64(offset)); }};
    auto check_byte{[&](::llvm::Value* base,::std::size_t offset,unsigned expected)
    { auto const value{ir.CreateLoad(ir.getInt8Ty(),cell(base,offset))};value->setAlignment(::llvm::Align{1u});check(ir.CreateICmpEQ(value,ir.getInt8(expected))); }};
    auto check_slot{[&](::llvm::Value* base,::std::size_t index,::std::size_t width,::std::uint64_t expected)
    {
        auto const type{ir.getIntNTy(static_cast<unsigned>(width*8u))};
        auto const value{ir.CreateLoad(type,cell(base,index*cp::native_slot_bytes))};value->setAlignment(::llvm::Align{1u});
        check(ir.CreateICmpEQ(value,::llvm::ConstantInt::get(type,expected)));
        for(auto byte{width};byte!=cp::native_slot_bytes;++byte) { check_byte(base,index*cp::native_slot_bytes+byte,0u); }
    }};
    auto capture{[&](::std::size_t ordinal,::llvm::ArrayRef<::llvm::Value*> live,unsigned marker,::std::uint64_t first,::std::uint64_t reference)
    {
        auto const packet{emit::emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(state,plan.sites[ordinal],nullptr,live)};
        require(packet.valid && packet.slots_bytes==plan.sites[ordinal].slots.size()*16u,"actual bounded observer packet");
        auto const values{ir.CreateIntToPtr(packet.slots_address,ir.getPtrTy())};
        auto const flags{ir.CreateIntToPtr(packet.flags_address,ir.getPtrTy())};
        for(::std::size_t i{};i!=locals;++i)
        {
            auto const width{emit::get_runtime_wasm_value_type_abi_size(state.local_types[i])};
            check_slot(values,i,width,i==0u ? first : (i==5u ? reference : bits[i]));
            check_byte(flags,i,i==5u ? marker : 1u);
        }
        for(::std::size_t i{};i!=guard_bytes;++i)
        { check_byte(workspace,i,0xa5u);check_byte(workspace,guard_bytes+workspace_bytes+i,0xa5u); }
        return values;
    }};
    capture(0u,{},0u,bits[0],0u); // No nonlocals; helper alone must clear poisoned locals and flags.
    auto const table_type{::llvm::ArrayType::get(ir.getPtrTy(),locals)};
    auto const absent_address{ir.CreateInBoundsGEP(table_type,state.snapshot_local_pointers,{ir.getInt32(0u),ir.getInt32(5u)})};
    ir.CreateStore(::llvm::ConstantPointerNull::get(ir.getPtrTy()),absent_address)->setAlignment(::llvm::Align{1u});
    auto const invalid_marker{cell(state.checkpoint_observer_workspace,5u)};ir.CreateStore(ir.getInt8(2u),invalid_marker);
    ::std::array<::llvm::Value*,3u> live{ir.getInt32(0xdeadbeefu),ir.CreateBitCast(ir.getInt64(0x3ff4000000000000ull),ir.getDoubleTy()),ir.getInt64(0x8899aabbccddeeffull)};
    auto const wide_values{capture(1u,live,2u,bits[0],0u)};
    check_slot(wide_values,7u,4u,0xdeadbeefu);check_slot(wide_values,8u,8u,0x3ff4000000000000ull);check_slot(wide_values,9u,8u,0x8899aabbccddeeffull);
    // Deliberately dirty the narrow i32 slot's padding before reusing the same packet.
    ir.CreateStore(::llvm::Constant::getAllOnesValue(ir.getInt128Ty()),cell(wide_values,7u*16u))->setAlignment(::llvm::Align{1u});
    ir.CreateStore(ir.getInt8(0u),invalid_marker);
    auto const one{ir.getInt32(0x31415926u)};auto const narrow_values{capture(2u,{one},0u,bits[0],0u)};
    check_slot(narrow_values,7u,4u,0x31415926u);
    ir.CreateStore(ir.getInt32(42u),state.local_pointers[0u]);
    auto const reference_type{emit::get_llvm_type_from_wasm_value_type(*context,state.local_types[5u])};
    ir.CreateStore(::llvm::ConstantInt::get(reference_type,9u),state.local_pointers[5u]);
    ir.CreateStore(state.local_pointers[5u],absent_address)->setAlignment(::llvm::Align{1u});ir.CreateStore(ir.getInt8(1u),invalid_marker);
    capture(3u,{},1u,42u,9u);
    auto const incremental{add_incremental_packet_probe(*context,*module)};
    check(ir.CreateICmpEQ(ir.CreateCall(incremental),ir.getInt32(0u)));
    auto const numeric{add_incremental_numeric_probe(*context,*module)};
    check(ir.CreateICmpEQ(ir.CreateCall(numeric),ir.getInt32(0u)));
    ir.CreateRet(ir.CreateSelect(ok,ir.getInt32(0u),ir.getInt32(1u)));
    require(!::llvm::verifyModule(*module),"complete LLVM packet module verifies");
    if(argc>=2)
    { ::fast_io::native_file file{::fast_io::mnp::os_c_str(argv[1]),::fast_io::open_mode::out};fast_io_ir_stream stream{file};module->print(stream,nullptr);stream.flush(); }
    if(argc>=3)
    { ::fast_io::native_file file{::fast_io::mnp::os_c_str(argv[2]),::fast_io::open_mode::out};fast_io_ir_stream stream{file};::llvm::WriteBitcodeToFile(*module,stream);stream.flush(); }
    if(argc==5)
    { ::fast_io::io::println("PASS actual observer packet IR DATA; pointer-bits=",pointer_bits," big-endian=",big_endian);return 0; }
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() &&
        !::llvm::InitializeNativeTargetAsmParser(),"native target and real numeric witness assembler");
    ::llvm::Triple const triple{::llvm::sys::getProcessTriple()};
    ::llvm::EngineBuilder select{};select.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None);
    support::llvm_jit_mcjit_configure_code_model(select,triple);support::llvm_jit_mcjit_configure_host_unwind_abi(select);
    auto target{::std::unique_ptr<::llvm::TargetMachine>{select.selectTarget()}};require(bool(target),"actual native target");
    module->setTargetTriple(target->getTargetTriple());module->setDataLayout(target->createDataLayout());
    auto engine{::std::unique_ptr<::llvm::ExecutionEngine>{::llvm::EngineBuilder{::std::move(module)}
        .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(::llvm::CodeGenOptLevel::None)
        .setMCJITMemoryManager(::std::make_unique<support::runtime_llvm_jit_section_memory_manager>()).create(target.release())}};
    require(bool(engine),"actual native MCJIT");engine->finalizeObject();require(!engine->hasError(),"actual materialization");
    auto const address{engine->getFunctionAddress("main")};require(address!=0u,"native entry");
    require(reinterpret_cast<int(*)()>(static_cast<::std::uintptr_t>(address))()==0,"reused values/flags/padding/sentinels and actual local update");
    ::fast_io::io::println("PASS actual observer packet DATA: locals-only, live operands/control, reused padding, invalid flag/null candidate, fresh updates and sentinels; no Wasm/ASM authority");
}
