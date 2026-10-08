// Real LLVM module, real helper lowering and real MCJIT execution.
// This tests owned compiler DATA; it does not mint guest/native-debug authority.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <utility>
#include <fast_io.h>
namespace e = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
namespace v = ::uwvm2::validation::standard::wasm3;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
static void require(bool good, char const* message)
{
    if(!good) { ::fast_io::io::perrln("i32 numeric owned-event IR: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
template<unsigned Opcode>
static void emit_one(::llvm::LLVMContext& context, ::llvm::Module& module)
{
    auto const name{::fast_io::concat_std("i32_event_", ::fast_io::mnp::hex0x(Opcode))};
    auto const signature{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(context),
        {::llvm::Type::getInt32Ty(context), ::llvm::Type::getInt32Ty(context)}, false)};
    auto const function{::llvm::Function::Create(signature, ::llvm::GlobalValue::ExternalLinkage, name, module)};
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    e::runtime_local_func_llvm_jit_emit_state_t state{};
    state.valid = true; state.current_wasm_op_offset = 7uz;
    // Real typed compiler fixture owner, not a fake Wasm/source/stop proof.
    // The normalized wrapper accepts no raw bytes. Ordinary metadata/observations
    // are disabled; this owner remains live throughout synchronous IR lowering.
    ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t local{};
    state.local_func_storage_ptr = ::std::addressof(local);
    state.llvm_context_holder = &context; state.llvm_module = &module; state.llvm_function = function;
    state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(entry);
    state.control_stack.push_back({.is_reachable = true});
    state.operand_stack.push_back({.type = e::runtime_operand_stack_value_type::i32, .value = function->getArg(0u)});
    if constexpr(Opcode >= 0x6au)
    { state.operand_stack.push_back({.type = e::runtime_operand_stack_value_type::i32, .value = function->getArg(1u)}); }
    ::std::size_t abstract_count{Opcode <= 0x69u ? 1uz : 2uz};
    v::validated_i32_numeric_event event{};
    auto const transitioned{v::transition_i32_numeric_event<Opcode>(event, false,
        [&]() noexcept { return abstract_count; },
        [&]() noexcept -> v::core3_operand { --abstract_count; return {{t::value_kind::i32}, false}; },
        [&](t::core_value_type value) { require(value.kind == t::value_kind::i32, "exact output abstract kind"); ++abstract_count; },
        7uz, 1uz)};
    require(transitioned.error == v::typed_stack_error::ok && abstract_count == 1uz, "actual common first-decode transition");
    // A logical child frame inside a dead parent is NOT stack-polymorphic
    // after its block entry, but LLVM still skips physical child frames/IR.
    // Exercise that real helper contract for every opcode before live lowering.
    state.control_stack.back().is_reachable = false; state.unreachable_control_depth = 2uz;
    auto const preserved_stack{state.operand_stack.size()}; auto const preserved_ir{entry->size()};
    ::std::size_t skipped_abstract_count{Opcode <= 0x69u ? 1uz : 2uz};
    v::validated_i32_numeric_event skipped{};
    auto const skipped_transition{v::transition_i32_numeric_event<Opcode>(skipped, false,
        [&]() noexcept { return skipped_abstract_count; },
        [&]() noexcept -> v::core3_operand { --skipped_abstract_count; return {{t::value_kind::i32}, false}; },
        [&](t::core_value_type value) { require(value.kind == t::value_kind::i32, "child logical numeric result"); ++skipped_abstract_count; },
        7uz, 3uz)};
    require(skipped_transition.error == v::typed_stack_error::ok && !skipped.stack_polymorphic &&
        e::try_emit_runtime_local_func_llvm_jit_i32_numeric(state, skipped) &&
        state.operand_stack.size() == preserved_stack && entry->size() == preserved_ir,
        "actual dead-parent skipped depth keeps legal child numeric IR absent");
    auto invalid_depth{skipped}; invalid_depth.control_depth = 1uz;
    require(!e::try_emit_runtime_local_func_llvm_jit_i32_numeric(state, invalid_depth) &&
        state.operand_stack.size() == preserved_stack && entry->size() == preserved_ir,
        "logical/physical depth mismatch rejects before mutation");
    state.control_stack.back().is_reachable = true; state.unreachable_control_depth = 0uz;
    auto malformed{event}; malformed.source_bytes = 2uz;
    auto const before{entry->size()};
    require(!e::try_emit_runtime_local_func_llvm_jit_i32_numeric(state, malformed) && entry->size() == before,
        "inconsistent owned event rejected before IR mutation");
    require(e::try_emit_runtime_local_func_llvm_jit_i32_numeric(state, event) && state.operand_stack.size() == 1uz,
        "actual owned-event helper emitted one result");
    auto const result{state.operand_stack.back().value};
    require(result != nullptr && result->getType()->isIntegerTy(32u), "real scalar SSA result");
    state.ir_builder->CreateRet(result);
    require(!::llvm::verifyFunction(*function), "actual function IR verifies, including trap/diamond PHIs");
}
template<unsigned... Offsets>
static void emit_all(::llvm::LLVMContext& context, ::llvm::Module& module, ::std::integer_sequence<unsigned, Offsets...>)
{ (emit_one<0x67u + Offsets>(context, module), ...); }
struct row { unsigned opcode; ::std::uint32_t left, right, result; };
static constexpr row cases[]{
    {0x67u,0u,0u,32u},{0x67u,1u,0u,31u},{0x67u,0x80000000u,0u,0u},
    {0x68u,0u,0u,32u},{0x68u,1u,0u,0u},{0x68u,0x80000000u,0u,31u},
    {0x69u,0u,0u,0u},{0x69u,0xffffffffu,0u,32u},{0x69u,0x55555555u,0u,16u},
    {0x6au,0xffffffffu,1u,0u},{0x6au,0x80000000u,0x80000000u,0u},{0x6au,25u,17u,42u},
    {0x6bu,0u,1u,0xffffffffu},{0x6bu,0x80000000u,1u,0x7fffffffu},{0x6bu,59u,17u,42u},
    {0x6cu,0x80000000u,2u,0u},{0x6cu,0xffffffffu,0xffffffffu,1u},{0x6cu,6u,7u,42u},
    {0x6du,0xffffffacu,0xfffffffeu,42u},{0x6du,0xffffffacu,2u,0xffffffd6u},{0x6du,0x80000000u,1u,0x80000000u},
    {0x6eu,0xffffffffu,3u,0x55555555u},{0x6eu,84u,2u,42u},{0x6eu,0x80000000u,2u,0x40000000u},
    {0x6fu,0x80000000u,0xffffffffu,0u},{0x6fu,0xffffffabu,2u,0xffffffffu},{0x6fu,85u,0xfffffffeu,1u},
    {0x70u,0xffffffffu,2u,1u},{0x70u,85u,2u,1u},{0x70u,0x80000000u,3u,2u},
    {0x71u,0x55u,0x33u,0x11u},{0x71u,0xffffffffu,42u,42u},
    {0x72u,0x55u,0x33u,0x77u},{0x72u,0u,42u,42u},
    {0x73u,0x55u,0x33u,0x66u},{0x73u,0xffffffffu,0xffffffffu,0u},
    {0x74u,1u,32u,1u},{0x74u,1u,33u,2u},{0x74u,0x80000000u,1u,0u},
    {0x75u,0x80000000u,32u,0x80000000u},{0x75u,0x80000000u,33u,0xc0000000u},
    {0x76u,0x80000000u,32u,0x80000000u},{0x76u,0x80000000u,33u,0x40000000u},
    {0x77u,0x80000001u,0u,0x80000001u},{0x77u,0x80000001u,1u,3u},{0x77u,1u,33u,2u},
    {0x78u,0x80000001u,0u,0x80000001u},{0x78u,0x80000001u,1u,0xc0000000u},{0x78u,1u,33u,0x80000000u}
};
int main()
{
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "real native LLVM target initialization");
    ::llvm::LLVMContext context{};
    auto module{::std::make_unique<::llvm::Module>("all18-first-decode-i32", context)};
    emit_all(context, *module, ::std::make_integer_sequence<unsigned,18>{});
    require(!::llvm::verifyModule(*module), "mandatory complete actual module verifier");
    ::std::unique_ptr<::llvm::ExecutionEngine> engine{
        ::llvm::EngineBuilder{::std::move(module)}.setEngineKind(::llvm::EngineKind::JIT).create()};
    require(bool(engine), "real native MCJIT engine"); engine->finalizeObject();
    for(auto const& test : cases)
    {
        auto const name{::fast_io::concat_std("i32_event_", ::fast_io::mnp::hex0x(test.opcode))};
        auto const address{engine->getFunctionAddress(name)};
        require(address != 0u, "actual owned emitted function address");
        using function_type = ::std::uint32_t(*)(::std::uint32_t, ::std::uint32_t);
        auto const function{reinterpret_cast<function_type>(static_cast<::std::uintptr_t>(address))};
        require(function(test.left, test.right) == test.result, "real generated operation exact edge bits");
    }
    ::fast_io::io::println("all18 i32 typed-event LLVM verifier/native edge-bit component PASS; wholeVM/ASM/performance qualification=false");
}
