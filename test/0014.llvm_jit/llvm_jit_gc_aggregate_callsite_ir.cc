// Exercise the actual product LLVM emitter with the actual runtime module and
// GC type layouts. The MCJIT executes helper-generated functions using native
// integer-address test entries. This does not parse, validate or execute a full
// Wasm module, and it performs no GC.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Operator.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <array>
#include <initializer_list>
#include <memory>
#include <vector>

namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace details = compiler::details;
namespace storage = ::uwvm2::uwvm::runtime::storage;
namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace container = ::uwvm2::utils::container;
using wasm_type = details::runtime_operand_stack_value_type;
using status = storage::gc_object_status;

static void require(bool condition, char const* label)
{
    if(!condition)
    {
        ::fast_io::io::perrln("GC aggregate callsite: ", ::fast_io::mnp::os_c_str(label));
        ::fast_io::fast_terminate();
    }
}

static gc_type::field_type field(gc_type::value_kind kind, bool mutable_ = true,
    gc_type::packed_kind packed = gc_type::packed_kind::none)
{
    gc_type::field_type result{};
    result.storage.value.kind = kind;
    result.storage.packed = packed;
    result.mutable_ = mutable_;
    if(kind == gc_type::value_kind::reference)
    {
        result.storage.value.heap.code = static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::func);
        result.storage.value.nullable = true;
    }
    return result;
}

static gc_type::recursive_type_section declarations()
{
    gc_type::recursive_type_section section{};
    gc_type::recursive_group group{};
    auto const structure{[&](::std::initializer_list<gc_type::field_type> fields)
    {
        gc_type::sub_type entry{};
        entry.kind = gc_type::composite_kind::struct_;
        for(auto const item : fields) { entry.fields.push_back(item); }
        group.types.push_back(::std::move(entry));
    }};
    auto const array{[&](gc_type::field_type item)
    {
        gc_type::sub_type entry{};
        entry.kind = gc_type::composite_kind::array;
        entry.fields.push_back(item);
        group.types.push_back(::std::move(entry));
    }};
    structure({field(gc_type::value_kind::i32)}); // 0
    structure({}); // 1
    structure({field(gc_type::value_kind::i32, true, gc_type::packed_kind::i8),
        field(gc_type::value_kind::i32, true, gc_type::packed_kind::i16),
        field(gc_type::value_kind::i64, false)}); // 2
    array(field(gc_type::value_kind::i32, true, gc_type::packed_kind::i16)); // 3
    array(field(gc_type::value_kind::reference)); // 4
    structure({field(gc_type::value_kind::v128)}); // 5
    array(field(gc_type::value_kind::i32)); // 6
    for(::std::size_t count{}; count != 10uz; ++count)
    {
        gc_type::sub_type entry{};
        entry.kind = gc_type::composite_kind::struct_;
        for(::std::size_t index{}; index != count; ++index) { entry.fields.push_back(field(gc_type::value_kind::i32)); }
        group.types.push_back(::std::move(entry)); // 7 + count
    }
    for(auto const count : {128uz, 129uz})
    {
        gc_type::sub_type entry{};
        entry.kind = gc_type::composite_kind::struct_;
        for(::std::size_t index{}; index != count; ++index) { entry.fields.push_back(field(gc_type::value_kind::i32)); }
        group.types.push_back(::std::move(entry)); // 17, 18: exact stack/heap scratch boundary
    }
    structure({field(gc_type::value_kind::i32), field(gc_type::value_kind::i64),
        field(gc_type::value_kind::f32), field(gc_type::value_kind::f64),
        field(gc_type::value_kind::v128), field(gc_type::value_kind::reference)}); // 19
    array(field(gc_type::value_kind::i32)); // 20: a distinct but compatible array.copy source type
    section.type_count = static_cast<::std::uint_least32_t>(group.types.size());
    section.groups.push_back(::std::move(group));
    return section;
}

static wasm_type scalar(gc_type::field_type const& item)
{
    if(item.storage.packed != gc_type::packed_kind::none) { return wasm_type::i32; }
    switch(item.storage.value.kind)
    {
        case gc_type::value_kind::i32: return wasm_type::i32;
        case gc_type::value_kind::i64: return wasm_type::i64;
        case gc_type::value_kind::f32: return wasm_type::f32;
        case gc_type::value_kind::f64: return wasm_type::f64;
        case gc_type::value_kind::v128: return wasm_type::v128;
        case gc_type::value_kind::reference: return wasm_type::funcref;
    }
    ::fast_io::fast_terminate();
}

struct operation
{
    ::std::uint_least32_t opcode{}, first{}, second{};
    ::std::vector<wasm_type> inputs{};
    bool has_result{};
    wasm_type result{};
};

// The typed operands below come from the Core 3 instruction signatures. They
// are built independently of the emitter's native ABI selection and compared
// with the resulting IR. An unused i64 prefix also tests stack preservation.
static operation signature(storage::gc_object_store const& store,
    ::std::uint_least32_t opcode, ::std::uint_least32_t first, ::std::uint_least32_t second)
{
    operation result{opcode, first, second};
    auto const ref{wasm_type::funcref};
    auto const i32{wasm_type::i32};
    auto const element{[&]()
    {
        auto const item{store.field_at(first, 0uz)};
        require(item != nullptr, "fixture element exists");
        return scalar(*item);
    }};
    auto const member{[&]()
    {
        auto const item{store.field_at(first, second)};
        require(item != nullptr, "fixture struct member exists");
        return scalar(*item);
    }};
    switch(opcode)
    {
        case 0u:
        {
            ::std::size_t count{};
            require(store.field_count(first, count), "fixture struct layout exists");
            for(::std::size_t index{}; index != count; ++index)
            {
                auto const item{store.field_at(first, index)};
                require(item != nullptr, "complete struct field layout");
                result.inputs.push_back(scalar(*item));
            }
            result.has_result = true; result.result = ref; break;
        }
        case 1u: result.has_result = true; result.result = ref; break;
        case 2u: case 3u: case 4u:
            result.inputs = {ref}; result.has_result = true; result.result = member(); break;
        case 5u: result.inputs = {ref, member()}; break;
        case 6u: result.inputs = {element(), i32}; result.has_result = true; result.result = ref; break;
        case 7u: result.inputs = {i32}; result.has_result = true; result.result = ref; break;
        case 8u: result.inputs.assign(second, element()); result.has_result = true; result.result = ref; break;
        case 9u: case 10u: result.inputs = {i32, i32}; result.has_result = true; result.result = ref; break;
        case 11u: case 12u: case 13u:
            result.inputs = {ref, i32}; result.has_result = true; result.result = element(); break;
        case 14u: result.inputs = {ref, i32, element()}; break;
        case 15u: result.inputs = {ref}; result.has_result = true; result.result = i32; break;
        case 16u: result.inputs = {ref, i32, element(), i32}; break;
        case 17u: result.inputs = {ref, i32, ref, i32, i32}; break;
        case 18u: case 19u: result.inputs = {ref, i32, i32, i32}; break;
        default: ::fast_io::fast_terminate();
    }
    return result;
}

struct bridge_identity
{
    container::u8string name{};
    ::std::uintptr_t address{};
    ::llvm::FunctionType* type{};
    bool fixed{};
};

template<auto Function>
static bridge_identity identity(::llvm::FunctionType* type, bool fixed)
{
    return {details::get_llvm_runtime_bridge_function_symbol_name<Function>(type),
        details::get_llvm_runtime_bridge_function_address(Function), type, fixed};
}

static bridge_identity expected_bridge(::llvm::LLVMContext& context, operation const& item)
{
    auto const intptr{::llvm::Type::getIntNTy(context, sizeof(::std::uintptr_t) * CHAR_BIT)};
    auto const i32{::llvm::Type::getInt32Ty(context)};
    auto const generic{::llvm::FunctionType::get(i32, {intptr, i32, i32, i32, intptr, intptr, intptr}, false)};
#if defined(UWVM2TEST_GC_LEGACY_AGGREGATE_ABI)
    static_cast<void>(item);
    return identity<details::llvm_jit_gc_aggregate_bridge>(generic, false);
#else
    auto const fixed{::llvm::FunctionType::get(i32, {intptr, i32, i32, intptr, intptr}, false)};
    auto const count{item.inputs.size()};
    auto const with_count{[&]<::std::uint_least32_t Opcode>() -> bridge_identity
    {
        switch(count)
        {
            case 0uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 0uz>>(fixed, true);
            case 1uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 1uz>>(fixed, true);
            case 2uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 2uz>>(fixed, true);
            case 3uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 3uz>>(fixed, true);
            case 4uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 4uz>>(fixed, true);
            case 5uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 5uz>>(fixed, true);
            case 6uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 6uz>>(fixed, true);
            case 7uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 7uz>>(fixed, true);
            case 8uz: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<Opcode, 8uz>>(fixed, true);
            default: return identity<details::llvm_jit_gc_aggregate_bridge<>>(generic, false);
        }
    }};
    switch(item.opcode)
    {
        case 0u: return with_count.template operator()<0u>();
        case 1u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<1u, 0uz>>(fixed, true);
        case 2u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<2u, 1uz>>(fixed, true);
        case 3u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<3u, 1uz>>(fixed, true);
        case 4u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<4u, 1uz>>(fixed, true);
        case 5u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<5u, 2uz>>(fixed, true);
        case 6u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<6u, 2uz>>(fixed, true);
        case 7u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<7u, 1uz>>(fixed, true);
        case 8u: return with_count.template operator()<8u>();
        case 9u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<9u, 2uz>>(fixed, true);
        case 10u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<10u, 2uz>>(fixed, true);
        case 11u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<11u, 2uz>>(fixed, true);
        case 12u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<12u, 2uz>>(fixed, true);
        case 13u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<13u, 2uz>>(fixed, true);
        case 14u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<14u, 3uz>>(fixed, true);
        case 15u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<15u, 1uz>>(fixed, true);
        case 16u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<16u, 4uz>>(fixed, true);
        case 17u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<17u, 5uz>>(fixed, true);
        case 18u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<18u, 4uz>>(fixed, true);
        case 19u: return identity<details::llvm_jit_gc_aggregate_fixed_bridge<19u, 4uz>>(fixed, true);
        default: ::fast_io::fast_terminate();
    }
#endif
}

static bool integer_equals(::llvm::Value const* value, ::std::uint64_t expected)
{
    auto const integer{::llvm::dyn_cast<::llvm::ConstantInt>(value)};
    return integer != nullptr && integer->getZExtValue() == expected;
}

static bool comparison(::llvm::Value const* condition, ::llvm::Value const* value,
    ::llvm::CmpInst::Predicate predicate, ::std::uint64_t expected)
{
    auto const test{::llvm::dyn_cast<::llvm::ICmpInst>(condition)};
    return test != nullptr && test->getPredicate() == predicate &&
        test->getOperand(0) == value && integer_equals(test->getOperand(1), expected);
}

static void emit_and_check(::llvm::Module& ir, storage::wasm_module_storage_t& module,
    operation const& item, ::std::size_t id)
{
    auto& context{ir.getContext()};
    ::std::vector<::llvm::Type*> parameters{::llvm::Type::getInt64Ty(context)};
    for(auto const input : item.inputs)
    {
        auto const type{details::get_llvm_type_from_wasm_value_type(context, input)};
        require(type != nullptr, "fixture parameter has a real LLVM type");
        parameters.push_back(type);
    }
    auto const result_type{item.has_result ? details::get_llvm_type_from_wasm_value_type(context, item.result) :
        ::llvm::Type::getVoidTy(context)};
    require(result_type != nullptr, "fixture result has a real LLVM type");
    auto const name{container::concat_uwvm("gc_case_", id)};
    auto const type{::llvm::FunctionType::get(result_type, parameters, false)};
    auto const function{::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage,
        ::llvm::StringRef{name.data(), name.size()}, ir)};
    // Keep the typed helper body distinct from the test-only raw entry below,
    // so its selected native call remains independently inspectable at O3.
    function->addFnAttr(::llvm::Attribute::NoInline);
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    compiler::local_func_storage_t local{};
    local.runtime_module_ptr = ::std::addressof(module);
    details::runtime_local_func_llvm_jit_emit_state_t state{};
    state.valid = true;
    state.local_func_storage_ptr = ::std::addressof(local);
    state.llvm_context_holder = ::std::addressof(context);
    state.llvm_module = ::std::addressof(ir);
    state.llvm_function = state.llvm_public_entry_function = function;
    state.emit_call_stack_frames = false;
    state.ir_builder = container::make_delete_owned<::llvm::IRBuilder<>>(entry);
    state.operand_stack.push_back({.type = wasm_type::i64, .value = function->getArg(0u)});
    for(::std::size_t index{}; index != item.inputs.size(); ++index)
    {
        // [preserved prefix argument][one argument per typed operand] end
        // [safe                                                   ] index+1 < function->arg_size().
        state.operand_stack.push_back({.type = item.inputs[index],
            .value = function->getArg(static_cast<unsigned>(index + 1uz))});
    }
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate decoded{};
    decoded.opcode = item.opcode; decoded.first = item.first; decoded.second = item.second;
    require(details::try_emit_runtime_local_func_llvm_jit_gc_aggregate(state, decoded), "actual aggregate emitter accepts typed operation");
    require(state.operand_stack.size() == (item.has_result ? 2uz : 1uz), "all inputs consumed and only optional result pushed");
    require(state.operand_stack[0uz].type == wasm_type::i64 && state.operand_stack[0uz].value == function->getArg(0u), "prefix descriptor preserved");
    ::llvm::LoadInst* result_load{};
    if(item.has_result)
    {
        auto const& result{state.operand_stack[1uz]};
        require(result.type == item.result && result.value->getType() == result_type, "result type and SSA carrier preserved");
        result_load = ::llvm::dyn_cast<::llvm::LoadInst>(result.value);
        require(result_load != nullptr && result_load->getAlign() == ::llvm::Align{1u}, "result load uses safe byte-buffer alignment");
        state.ir_builder->CreateRet(result.value);
    }
    else { state.ir_builder->CreateRetVoid(); }
    require(!::llvm::verifyFunction(*function, &::llvm::errs()), "LLVM verifies complete helper-generated function");
    auto const expected{expected_bridge(context, item)};
    auto const expected_name{details::get_llvm_string_ref(expected.name)};
    auto const declaration{ir.getFunction(expected_name)};
    require(declaration != nullptr && declaration->getFunctionType() == expected.type, "exact native bridge declaration selected");
    require(reinterpret_cast<::std::uintptr_t>(::llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(expected_name.str())) == expected.address,
        "registered external symbol names the actual selected native function address");
    ::llvm::CallInst* aggregate{};
    ::llvm::CallInst* allocate{};
    ::llvm::CallInst* release{};
    ::llvm::AllocaInst* input_slot{};
    ::llvm::AllocaInst* output_slot{};
    ::llvm::MemSetInst* input_zero{};
    ::std::vector<::llvm::StoreInst*> stores{};
    ::std::vector<::llvm::BranchInst*> conditional{};
    auto const intptr{::llvm::Type::getIntNTy(context, sizeof(::std::uintptr_t) * CHAR_BIT)};
    auto const allocator_type{::llvm::FunctionType::get(intptr, {intptr}, false)};
    auto const free_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), {intptr}, false)};
    auto const allocate_name{details::get_llvm_runtime_bridge_function_symbol_name<details::llvm_jit_gc_input_allocate_bridge>(allocator_type)};
    auto const free_name{details::get_llvm_runtime_bridge_function_symbol_name<details::llvm_jit_gc_input_free_bridge>(free_type)};
    for(auto& block : *function) for(auto& instruction : block)
    {
        if(auto const call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))})
        {
            auto const target{call->getCalledFunction()};
            if(target == declaration) { require(aggregate == nullptr, "one aggregate call"); aggregate = call; }
            else if(target != nullptr && target->getName() == details::get_llvm_string_ref(allocate_name)) { require(allocate == nullptr, "one input allocation"); allocate = call; }
            else if(target != nullptr && target->getName() == details::get_llvm_string_ref(free_name)) { require(release == nullptr, "one input release"); release = call; }
        }
        if(auto const allocation{::llvm::dyn_cast<::llvm::AllocaInst>(::std::addressof(instruction))})
        {
            if(allocation->getName() == "gc.input") { input_slot = allocation; }
            else if(allocation->getName() == "gc.output") { output_slot = allocation; }
        }
        if(auto const zero{::llvm::dyn_cast<::llvm::MemSetInst>(::std::addressof(instruction))})
        { require(input_zero == nullptr, "one complete input initialization"); input_zero = zero; }
        if(auto const store{::llvm::dyn_cast<::llvm::StoreInst>(::std::addressof(instruction))}) { stores.push_back(store); }
        if(auto const branch{::llvm::dyn_cast<::llvm::BranchInst>(::std::addressof(instruction))}; branch != nullptr && branch->isConditional())
        { conditional.push_back(branch); }
    }
    require(aggregate != nullptr && aggregate->getFunctionType() == expected.type && !aggregate->isMustTailCall(), "host call uses exact ABI type and returns status");
    require(aggregate->getCallingConv() == details::get_llvm_jit_host_calling_conv(), "host calling convention applied at actual callsite");
    auto const argc{expected.fixed ? 5uz : 7uz};
    require(aggregate->arg_size() == argc, "five fixed arguments or seven generic arguments");
    auto const first_index{expected.fixed ? 1u : 2u};
    auto const second_index{expected.fixed ? 2u : 3u};
    auto const input_index{expected.fixed ? 3u : 4u};
    auto const output_index{expected.fixed ? 4u : 6u};
    require(integer_equals(aggregate->getArgOperand(first_index), item.first) &&
        integer_equals(aggregate->getArgOperand(second_index), item.second), "first/second constants retain exact independent metadata");
    if(!expected.fixed)
    {
        require(integer_equals(aggregate->getArgOperand(1u), item.opcode) &&
            integer_equals(aggregate->getArgOperand(5u), item.inputs.size()), "generic fallback retains opcode and full dynamic count");
    }
    auto const module_address{::llvm::dyn_cast<::llvm::PtrToIntOperator>(aggregate->getArgOperand(0u))};
    auto const module_name{details::get_llvm_runtime_module_object_symbol_name(module)};
    require(module_address != nullptr && ::llvm::isa<::llvm::GlobalVariable>(module_address->getPointerOperand()) &&
        module_address->getPointerOperand()->getName() == details::get_llvm_string_ref(module_name), "runtime module uses its real registered external object symbol");
    require(output_slot != nullptr && integer_equals(output_slot->getArraySize(), sizeof(storage::gc_object_value)) &&
        output_slot->getAlign().value() >= alignof(storage::gc_object_value), "one aligned complete output value");
    auto const output_address{::llvm::dyn_cast<::llvm::PtrToIntOperator>(aggregate->getArgOperand(output_index))};
    require(output_address != nullptr && output_address->getPointerOperand() == output_slot, "last native argument points at the actual output buffer");
    auto const bytes{item.inputs.size() * sizeof(storage::gc_object_value)};
    bool const heap{bytes > 2048uz};
    require(stores.size() == item.inputs.size(), "one typed store per live input SSA value");
    if(bytes == 0uz)
    { require(input_zero == nullptr && input_slot == nullptr && integer_equals(aggregate->getArgOperand(input_index), 0u), "zero-input fixed operation does not form an input range"); }
    else
    {
        require(input_zero != nullptr && integer_equals(input_zero->getLength(), bytes) && integer_equals(input_zero->getValue(), 0u), "complete 16-byte slots initialized before typed stores");
        for(::std::size_t index{}; index != stores.size(); ++index)
        {
            auto const store{stores[index]};
            require(store->getValueOperand() == function->getArg(static_cast<unsigned>(index + 1uz)) && store->getAlign() == ::llvm::Align{1u}, "typed SSA input stored with byte-safe alignment");
            auto const pointer{::llvm::dyn_cast<::llvm::GEPOperator>(store->getPointerOperand())};
            // LLVM may fold the index-zero GEP to its base pointer.
            if(pointer != nullptr)
            { require(pointer->isInBounds() && pointer->getNumIndices() == 1u && integer_equals(pointer->getOperand(1u), index * sizeof(storage::gc_object_value)), "typed store has the proved 16-byte slot offset"); }
            else { require(index == 0uz, "only zero-offset GEP may fold to base"); }
        }
        if(!heap)
        {
            require(input_slot != nullptr && integer_equals(input_slot->getArraySize(), bytes) &&
                input_slot->getAlign().value() >= alignof(storage::gc_object_value), "small operation has the exact stack buffer extent");
            auto const address{::llvm::dyn_cast<::llvm::PtrToIntOperator>(aggregate->getArgOperand(input_index))};
            require(address != nullptr && address->getPointerOperand() == input_slot, "input integer address is the actual stack buffer");
        }
    }
    ::llvm::DominatorTree dominance{*function};
    require(heap == (allocate != nullptr) && heap == (release != nullptr) && heap == (input_slot == nullptr && bytes != 0uz), "2048-byte threshold selects matched stack or allocation/release path");
    if(heap)
    {
        require(integer_equals(allocate->getArgOperand(0u), bytes) &&
            aggregate->getArgOperand(input_index) == allocate && release->getArgOperand(0u) == allocate,
            "heap allocation, native input and release share exact byte extent and address");
        require(dominance.dominates(aggregate, release), "input release follows the synchronous native bridge call");
    }
    ::std::array<bool, 4uz> guarded{};
    ::std::size_t allocation_guards{};
    for(auto const branch : conditional)
    {
        auto const condition{branch->getCondition()};
        ::std::size_t guard_index{4uz};
        if(comparison(condition, aggregate, ::llvm::CmpInst::ICMP_EQ, static_cast<unsigned>(status::null_reference))) { guard_index = 0uz; }
        else if(comparison(condition, aggregate, ::llvm::CmpInst::ICMP_EQ, static_cast<unsigned>(status::out_of_bounds))) { guard_index = 1uz; }
        else if(comparison(condition, aggregate, ::llvm::CmpInst::ICMP_NE, 0u)) { guard_index = 3uz; }
        else if(auto const exhausted{::llvm::dyn_cast<::llvm::BinaryOperator>(condition)};
            exhausted != nullptr && exhausted->getOpcode() == ::llvm::Instruction::Or &&
            comparison(exhausted->getOperand(0u), aggregate, ::llvm::CmpInst::ICMP_EQ, static_cast<unsigned>(status::out_of_memory)) &&
            comparison(exhausted->getOperand(1u), aggregate, ::llvm::CmpInst::ICMP_EQ, static_cast<unsigned>(status::size_overflow))) { guard_index = 2uz; }
        if(guard_index != 4uz)
        {
            require(!guarded[guard_index], "one branch for each distinct status guard"); guarded[guard_index] = true;
            require(::llvm::isa<::llvm::UnreachableInst>(branch->getSuccessor(0u)->getTerminator()), "error edge terminates before any result read");
            if(result_load != nullptr) { require(dominance.dominates(branch->getSuccessor(1u), result_load->getParent()), "every successful status edge dominates output load"); }
            if(heap) { require(dominance.dominates(release, branch), "heap input released before every status trap edge"); }
        }
        else if(heap && comparison(condition, allocate, ::llvm::CmpInst::ICMP_EQ, 0u))
        {
            ++allocation_guards;
            require(::llvm::isa<::llvm::UnreachableInst>(branch->getSuccessor(0u)->getTerminator()) &&
                dominance.dominates(branch->getSuccessor(1u), aggregate->getParent()), "failed input allocation traps before host input use");
        }
        else { require(false, "unexpected conditional branch in minimal aggregate emitter"); }
    }
    for(auto const complete : guarded) { require(complete, "all status checks precede optional output load"); }
    require(allocation_guards == (heap ? 1uz : 0uz), "one heap allocation failure guard when required");
    require(!item.has_result || result_load->getPointerOperand() == output_slot, "successful result reads actual complete output slot");
    ::fast_io::io::println("GC_CALLSITE_JSON {\"function\":\"", name, "\",\"opcode\":", item.opcode,
        ",\"first\":", item.first, ",\"second\":", item.second, ",\"input_count\":", item.inputs.size(),
        ",\"input_bytes\":", bytes, ",\"heap_input\":", heap ? "true" : "false", ",\"bridge\":\"",
        ::fast_io::mnp::os_c_str(expected_name.str().c_str()), "\",\"argument_count\":", argc,
        ",\"result_bytes\":", item.has_result ? details::get_runtime_wasm_value_type_abi_size(item.result) : 0uz,
        ",\"status_guards\":4,\"llvm_function_verified\":true}");
}

static container::string raw_entry_name(::std::size_t id)
{ return container::concat_uwvm("gc_raw_case_", id); }

static void emit_raw_entry(::llvm::Module& ir, operation const& item, ::std::size_t id)
{
    auto& context{ir.getContext()};
    auto const intptr{::llvm::Type::getIntNTy(context, sizeof(::std::uintptr_t) * CHAR_BIT)};
    auto const pointer{::llvm::PointerType::get(context, 0u)};
    auto const i8{::llvm::Type::getInt8Ty(context)};
    auto const typed_name{container::concat_uwvm("gc_case_", id)};
    auto const typed{ir.getFunction(::llvm::StringRef{typed_name.data(), typed_name.size()})};
    require(typed != nullptr, "typed helper exists before raw test entry");
    auto const name{raw_entry_name(id)};
    auto const type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context), {intptr, intptr}, false)};
    auto const function{::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage,
        ::llvm::StringRef{name.data(), name.size()}, ir)};
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    ::llvm::IRBuilder<> builder{entry};
    auto const input{builder.CreateIntToPtr(function->getArg(0u), pointer)};
    auto const output{builder.CreateIntToPtr(function->getArg(1u), pointer)};
    ::std::vector<::llvm::Value*> arguments{::llvm::ConstantInt::get(builder.getInt64Ty(), 0xfedc'ba98'7654'3210ull)};
    for(::std::size_t index{}; index != item.inputs.size(); ++index)
    {
        // [one complete 16-byte slot per input] end
        // [safe                              ] the native fixture supplies exactly item.inputs.size() live values.
        auto const slot{builder.CreateInBoundsGEP(i8, input,
            ::llvm::ConstantInt::get(intptr, index * sizeof(storage::gc_object_value)))};
        auto const loaded{builder.CreateLoad(details::get_llvm_type_from_wasm_value_type(context, item.inputs[index]), slot)};
        loaded->setAlignment(::llvm::Align{1u});
        arguments.push_back(loaded);
    }
    auto const call{builder.CreateCall(typed->getFunctionType(), typed, arguments)};
    call->setCallingConv(typed->getCallingConv());
    // [one native fixture output value, 16 bytes] end
    // [safe                                    ] this is fixture-owned storage, never a guest pointer.
    builder.CreateMemSet(output, builder.getInt8(0u), sizeof(storage::gc_object_value), ::llvm::Align{1u});
    if(item.has_result)
    {
        auto const stored{builder.CreateStore(call, output)};
        stored->setAlignment(::llvm::Align{1u});
    }
    builder.CreateRetVoid();
    require(!::llvm::verifyFunction(*function, &::llvm::errs()), "LLVM verifies raw fixture entry");
}

using value = storage::gc_object_value;
using reference = storage::gc_reference;
namespace global = ::uwvm2::object::global;

static reference canonical_function(storage::wasm_module_storage_t& module)
{
    reference result{};
    result.kind = global::wasm_ref_kind::wasm_func_defined;
    result.storage.ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(0uz));
    return result;
}

static value example_value(storage::wasm_module_storage_t& module, gc_type::field_type const& item, ::std::size_t index)
{
    switch(scalar(item))
    {
        case wasm_type::i32: return value::i32(static_cast<::std::uint32_t>(0x80uz + index));
        case wasm_type::i64: return value::i64(0xfedc'ba98'7654'3210ull + index);
        // These are raw IEEE bits, including signaling NaNs. No host FP
        // arithmetic or FP-valued C++ callback is used by the test entry.
        case wasm_type::f32: return value::i32(0x7f80'0123u);
        case wasm_type::f64: return value::i64(0x7ff0'0000'0000'0123ull);
        case wasm_type::v128:
        {
            ::std::array<::std::byte, 16uz> bytes{};
            for(::std::size_t offset{}; offset != bytes.size(); ++offset) { bytes[offset] = static_cast<::std::byte>(0x80uz + offset); }
            return value::from(bytes);
        }
        case wasm_type::funcref: return value::reference(canonical_function(module));
        default: ::fast_io::fast_terminate();
    }
}

static ::std::vector<value> struct_values(storage::wasm_module_storage_t& module, ::std::uint_least32_t type)
{
    ::std::size_t count{};
    require(module.gc_store->field_count(type, count), "native setup struct layout");
    ::std::vector<value> result{};
    for(::std::size_t index{}; index != count; ++index)
    {
        auto const item{module.gc_store->field_at(type, index)};
        require(item != nullptr, "native setup complete member layout");
        result.push_back(example_value(module, *item, index));
    }
    return result;
}

static reference new_struct(storage::wasm_module_storage_t& module, ::std::uint_least32_t type)
{
    auto const fields{struct_values(module, type)};
    reference result{};
    require(module.gc_store->struct_new(type, fields.data(), fields.size(), result) == status::ok, "native setup real struct");
    return result;
}

static reference new_array(storage::wasm_module_storage_t& module, ::std::uint_least32_t type, ::std::size_t length,
    bool default_ = false)
{
    reference result{};
    if(default_) { require(module.gc_store->array_new_default(type, length, result) == status::ok, "native setup default array"); }
    else
    {
        auto const item{module.gc_store->field_at(type, 0uz)};
        require(item != nullptr, "native setup array element layout");
        require(module.gc_store->array_new(type, example_value(module, *item, 0uz), length, result) == status::ok, "native setup real array");
    }
    return result;
}

static ::std::vector<value> execution_inputs(storage::wasm_module_storage_t& module, operation const& item)
{
    ::std::vector<value> inputs(item.inputs.size());
    auto const array{[&](::std::uint_least32_t type, bool default_ = false)
    { return value::reference(new_array(module, type, 4uz, default_)); }};
    switch(item.opcode)
    {
        case 0u: return struct_values(module, item.first);
        case 1u: break;
        case 2u: case 3u: case 4u: inputs[0uz] = value::reference(new_struct(module, item.first)); break;
        case 5u: inputs = {value::reference(new_struct(module, item.first)), value::i32(0x42u)}; break;
        case 6u: inputs = {value::i32(0x1234'5678u), value::i32(4u)}; break;
        case 7u: inputs = {value::i32(4u)}; break;
        case 8u:
            for(::std::size_t index{}; index != inputs.size(); ++index) { inputs[index] = value::i32(static_cast<::std::uint32_t>(100uz + index)); }
            break;
        case 9u: inputs = {value::i32(0u), value::i32(2u)}; break;
        case 10u: inputs = {value::i32(0u), value::i32(1u)}; break;
        case 11u: case 12u: case 13u: inputs = {array(item.first), value::i32(1u)}; break;
        case 14u: inputs = {array(item.first), value::i32(1u), value::i32(0xdead'beefu)}; break;
        case 15u: inputs = {array(3u)}; break;
        case 16u: inputs = {array(item.first), value::i32(1u), value::i32(0x1234u), value::i32(2u)}; break;
        case 17u:
        {
            auto const destination{array(item.first)};
            auto const source{new_array(module, item.second, 4uz)};
            for(::std::size_t index{}; index != 4uz; ++index)
            { require(module.gc_store->array_set(source, index, value::i32(static_cast<::std::uint32_t>(200uz + index))) == status::ok, "native setup distinct copy source"); }
            inputs = {destination, value::i32(1u), value::reference(source), value::i32(0u), value::i32(2u)};
            break;
        }
        case 18u: inputs = {array(item.first), value::i32(1u), value::i32(2u), value::i32(1u)}; break;
        case 19u: inputs = {array(item.first, true), value::i32(1u), value::i32(0u), value::i32(1u)}; break;
        default: ::fast_io::fast_terminate();
    }
    require(inputs.size() == item.inputs.size(), "native execution supplies every complete typed slot");
    return inputs;
}

static bool same_value(value const& left, value const& right, gc_type::field_type const& field)
{
    if(field.storage.value.kind == gc_type::value_kind::reference && field.storage.packed == gc_type::packed_kind::none)
    {
        auto const lhs{left.as<reference>()}; auto const rhs{right.as<reference>()};
        // Reference equality uses the kind and opaque identity, not unspecified
        // C++ padding bytes in the tagged host reference representation.
        return lhs.kind == rhs.kind && lhs.storage.ptr == rhs.storage.ptr;
    }
    return left.bits == right.bits;
}

static void compare_struct(storage::gc_object_store& store, ::std::uint_least32_t type,
    reference left, reference right)
{
    ::std::size_t count{};
    require(store.field_count(type, count), "comparison struct layout");
    for(::std::size_t index{}; index != count; ++index)
    {
        value lhs{}, rhs{};
        require(store.struct_get(left, index, false, lhs) == status::ok && store.struct_get(right, index, false, rhs) == status::ok,
            "MCJIT and direct generic result resolve real struct objects");
        require(same_value(lhs, rhs, *store.field_at(type, index)), "MCJIT preserves every struct field");
    }
}

static void compare_array(storage::gc_object_store& store, ::std::uint_least32_t type,
    reference left, reference right)
{
    ::std::size_t left_length{}, right_length{};
    require(store.array_length(left, left_length) == status::ok && store.array_length(right, right_length) == status::ok && left_length == right_length,
        "MCJIT and direct generic array lengths agree");
    auto const element{store.field_at(type, 0uz)};
    require(element != nullptr, "comparison element layout");
    for(::std::size_t index{}; index != left_length; ++index)
    {
        value lhs{}, rhs{};
        require(store.array_get(left, index, false, lhs) == status::ok && store.array_get(right, index, false, rhs) == status::ok,
            "MCJIT and direct generic result resolve real array objects");
        require(same_value(lhs, rhs, *element), "MCJIT preserves every array element");
    }
}

static void execute_and_compare(::llvm::ExecutionEngine& engine, storage::wasm_module_storage_t& module,
    operation const& item, ::std::size_t id)
{
    // Independent object sets prevent a preceding oracle mutation from making
    // a missing JIT write look correct. Both sets use the actual same store.
    auto const native_inputs{execution_inputs(module, item)};
    auto const jit_inputs{execution_inputs(module, item)};
    value expected{}, observed{};
    expected.bits.fill(::std::byte{0xa5u}); observed.bits.fill(::std::byte{0xa5u});
#if defined(UWVM2TEST_GC_LEGACY_AGGREGATE_ABI)
    auto const generic{details::llvm_jit_gc_aggregate_bridge};
#else
    auto const generic{details::llvm_jit_gc_aggregate_bridge<>};
#endif
    require(generic(reinterpret_cast<::std::uintptr_t>(::std::addressof(module)), item.opcode, item.first, item.second,
        reinterpret_cast<::std::uintptr_t>(native_inputs.data()), native_inputs.size(), reinterpret_cast<::std::uintptr_t>(::std::addressof(expected))) ==
        static_cast<::std::uint_least32_t>(status::ok), "direct actual generic native bridge accepts execution case");
    auto const name{raw_entry_name(id)};
    auto const address{engine.getFunctionAddress(::std::string{name.data(), name.size()})};
    require(address != 0u, "actual MCJIT resolves raw generated entry");
    using raw_entry = void(*)(::std::uintptr_t, ::std::uintptr_t) noexcept;
    static_assert(sizeof(raw_entry) == sizeof(address));
    raw_entry entry{};
    // [one native function-address representation] -> [same-size C ABI pointer]
    // [safe                                     ] copy bits without an object-pointer dereference or fake callback.
    ::std::memcpy(::std::addressof(entry), ::std::addressof(address), sizeof(entry));
    entry(reinterpret_cast<::std::uintptr_t>(jit_inputs.data()), reinterpret_cast<::std::uintptr_t>(::std::addressof(observed)));
    if(item.opcode <= 1u) { compare_struct(*module.gc_store, item.first, expected.as<reference>(), observed.as<reference>()); }
    else if(item.opcode >= 6u && item.opcode <= 10u) { compare_array(*module.gc_store, item.first, expected.as<reference>(), observed.as<reference>()); }
    else if(item.opcode == 5u)
    { compare_struct(*module.gc_store, item.first, native_inputs[0uz].as<reference>(), jit_inputs[0uz].as<reference>()); }
    else if(item.opcode == 14u || (item.opcode >= 16u && item.opcode <= 19u))
    { compare_array(*module.gc_store, item.first, native_inputs[0uz].as<reference>(), jit_inputs[0uz].as<reference>()); }
    else if(item.has_result && item.result == wasm_type::funcref)
    { require(same_value(expected, observed, *module.gc_store->field_at(item.first, item.second)), "MCJIT reference result identity matches generic bridge"); }
    else { require(expected.bits == observed.bits, "MCJIT scalar/vector result bits match generic bridge"); }
    ::fast_io::io::println("GC_CALLSITE_EXEC {\"case\":", id, ",\"opcode\":", item.opcode,
        ",\"actual_mcjit_called\":true,\"matches_direct_generic_bridge\":true,\"separate_mutation_targets\":true}");
}

static void write_bytes(char const* directory, char const* name,
    ::std::byte const* begin, ::std::size_t size)
{
    auto const path{container::concat_uwvm(::fast_io::mnp::os_c_str(directory), "/", ::fast_io::mnp::os_c_str(name))};
    ::fast_io::native_file file{::fast_io::mnp::os_c_str(path.c_str()), ::fast_io::open_mode::out};
    // [size bytes in the caller-owned IR/object buffer] end
    // [safe                                         ] end is formed within that complete live allocation.
    ::fast_io::operations::write_all_bytes(file, begin, begin + size);
}

static void write_ir(::llvm::Module const& module, char const* directory, char const* name)
{
    container::u8string text{};
    details::raw_uwvm_string_ostream stream{text};
    module.print(stream, nullptr); stream.flush();
    write_bytes(directory, name, reinterpret_cast<::std::byte const*>(text.data()), text.size());
}

int main(int argc, char** argv)
{
    require(argc == 2, "one output directory required");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter() &&
        !::llvm::InitializeNativeTargetAsmParser(), "actual LLVM native target initializes");
    ::llvm::EngineBuilder machine_builder{};
    machine_builder.setOptLevel(::llvm::CodeGenOptLevel::Aggressive);
    machine_builder.setCodeModel(::llvm::CodeModel::Large);
    ::std::unique_ptr<::llvm::TargetMachine> machine{machine_builder.selectTarget()};
    require(machine != nullptr, "actual LLVM native target machine exists");
    ::llvm::LLVMContext context{};
    ::llvm::Module ir{"actual-gc-aggregate-callsite", context};
#if LLVM_VERSION_MAJOR >= 21
    ir.setTargetTriple(machine->getTargetTriple());
#else
    ir.setTargetTriple(machine->getTargetTriple().str());
#endif
    ir.setDataLayout(machine->createDataLayout());
    storage::wasm_module_storage_t module{};
    module.module_name = u8"gc-aggregate-callsite";
    auto types{declarations()};
    module.gc_lease_roots = ::std::make_shared<storage::gc_lease_owner>();
    module.gc_store = ::std::make_shared<storage::gc_object_store>(types, module.gc_lease_roots);
    require(module.gc_store->valid(), "actual product GC type layouts initialized");
    ::std::array<::std::byte, 4uz> data_bytes{::std::byte{1u}, ::std::byte{2u}, ::std::byte{3u}, ::std::byte{4u}};
    ::std::array<storage::wasm_element_storage_t::func_idx_t, 1uz> functions{0u};
    module.local_defined_function_vec_storage.push_back({});
    for(unsigned segment{}; segment != 2u; ++segment)
    {
        storage::local_defined_data_storage_t data{};
        // [four borrowed immutable data bytes] end
        // [safe                            ] both endpoint assignments borrow this main-scope allocation.
        data.data.byte_begin = data_bytes.data();
        data.data.byte_end = data_bytes.data() + data_bytes.size();
        data.data.kind = storage::wasm_data_segment_kind::passive;
        module.local_defined_data_vec_storage.push_back(data);
        storage::local_defined_element_storage_t element{};
        // [one borrowed in-range function index] end
        // [safe                               ] endpoints remain live through all helper emission.
        element.element.funcidx_begin = functions.data();
        element.element.funcidx_end = functions.data() + functions.size();
        element.element.kind = storage::wasm_element_segment_kind::passive;
        module.local_defined_element_vec_storage.push_back(element);
    }
    ::std::vector<operation> operations{};
    auto const add{[&](::std::uint_least32_t opcode, ::std::uint_least32_t first, ::std::uint_least32_t second = 0u)
    { operations.push_back(signature(*module.gc_store, opcode, first, second)); }};
    for(::std::uint_least32_t count{}; count != 10u; ++count) { add(0u, 7u + count); add(8u, 6u, count); }
    add(0u, 17u); add(0u, 18u); add(8u, 6u, 128u); add(8u, 6u, 129u);
    add(1u, 2u); add(2u, 2u, 2u); add(3u, 2u, 0u); add(4u, 2u, 1u); add(5u, 2u, 0u);
    add(6u, 6u); add(7u, 3u); add(9u, 3u, 1u); add(10u, 4u, 1u);
    add(11u, 6u); add(12u, 3u); add(13u, 3u); add(14u, 6u); add(15u, 0u);
    add(16u, 6u); add(17u, 6u, 20u); add(18u, 3u, 1u); add(19u, 4u, 1u);
    for(::std::uint_least32_t member{}; member != 6u; ++member) { add(2u, 19u, member); }
    add(0u, 19u); // mixed i32/i64/f32/f64/v128/ref stores in one real aggregate
    for(::std::size_t index{}; index != operations.size(); ++index)
    { emit_and_check(ir, module, operations[index], index); emit_raw_entry(ir, operations[index], index); }
    require(!::llvm::verifyModule(ir, &::llvm::errs()), "LLVM verifies all declarations and helper bodies together");
    write_ir(ir, argv[1], "aggregate.ll");
    // All optimization and object emission use this fixture's actual linked
    // LLVM SDK, not opt/llc from a possibly different installed major version.
    ::llvm::LoopAnalysisManager loop{};
    ::llvm::FunctionAnalysisManager function{};
    ::llvm::CGSCCAnalysisManager cgscc{};
    ::llvm::ModuleAnalysisManager module_analysis{};
    ::llvm::PassBuilder passes{machine.get()};
    passes.registerLoopAnalyses(loop); passes.registerFunctionAnalyses(function);
    passes.registerCGSCCAnalyses(cgscc); passes.registerModuleAnalyses(module_analysis);
    passes.crossRegisterProxies(loop, function, cgscc, module_analysis);
    auto pipeline{passes.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O3)};
    pipeline.run(ir, module_analysis);
    require(!::llvm::verifyModule(ir, &::llvm::errs()), "actual SDK verifies O3 optimized IR");
    write_ir(ir, argv[1], "aggregate.optimized.ll");
    // Preserve a clone before object emission passes mutate the compilation
    // module. Engine/context/store/segment lifetimes cover every synchronous
    // generated call and its registered actual host module address.
    auto execution_ir{::llvm::CloneModule(ir)};
    ::llvm::SmallVector<char, 0u> object{};
    ::llvm::raw_svector_ostream object_stream{object};
    ::llvm::legacy::PassManager codegen{};
    require(!machine->addPassesToEmitFile(codegen, object_stream, nullptr, ::llvm::CodeGenFileType::ObjectFile, false), "actual SDK supports verified native object emission");
    codegen.run(ir);
    require(!object.empty(), "actual SDK emitted a nonempty object");
    write_bytes(argv[1], "aggregate.o", reinterpret_cast<::std::byte const*>(object.data()), object.size());
    ::std::string engine_error{};
    ::llvm::EngineBuilder execution_builder{::std::move(execution_ir)};
    execution_builder.setEngineKind(::llvm::EngineKind::JIT);
    execution_builder.setOptLevel(::llvm::CodeGenOptLevel::Aggressive);
    execution_builder.setCodeModel(::llvm::CodeModel::Large);
    execution_builder.setErrorStr(::std::addressof(engine_error));
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{execution_builder.create()};
    if(engine == nullptr) { ::fast_io::io::perrln("actual MCJIT: ", ::fast_io::mnp::os_c_str(engine_error.c_str())); }
    require(engine != nullptr, "actual MCJIT engine created from verified optimized helper IR");
    engine->finalizeObject();
    for(::std::size_t index{}; index != operations.size(); ++index) { execute_and_compare(*engine, module, operations[index], index); }
    ::fast_io::io::println("GC_CALLSITE_ENV {\"llvm_major\":", LLVM_VERSION_MAJOR,
        ",\"pointer_bits\":", sizeof(::std::uintptr_t) * CHAR_BIT,
        ",\"cases\":", operations.size(), ",\"object_bytes\":", object.size(),
        ",\"optimized_ir_verified\":true,\"codegen\":\"aggressive\",\"code_model\":\"large\",\"actual_mcjit_cases\":",
        operations.size(), ",\"executed_wasm\":false,\"collections\":0}");
    ::fast_io::io::println("PASS actual GC aggregate LLVM helper IR: ", operations.size(),
        " typed cases; no full Wasm execution or collector qualification");
}
