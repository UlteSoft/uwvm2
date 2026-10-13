// Real emitter/type layout and native TargetMachine object output. This is
// compiler/helper evidence, not a full Wasm validator or a timing benchmark.
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <memory>
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace d = compiler::details;
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace c = ::uwvm2::utils::container;
using wasm_type = d::runtime_operand_stack_value_type;
static void require(bool condition, unsigned line)
{
    if(!condition)
    {
        ::fast_io::io::perrln("FAIL struct.set32 IR line=", line);
        ::fast_io::fast_terminate();
    }
}
#define SET32_IR_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
static t::recursive_type_section declarations()
{
    t::recursive_type_section out{}; out.type_count = 1u;
    t::recursive_group group{}; t::sub_type entry{}; entry.kind = t::composite_kind::struct_;
    for(auto kind : {t::value_kind::i32, t::value_kind::f32, t::value_kind::i32,
        t::value_kind::i32, t::value_kind::i64, t::value_kind::f64,
        t::value_kind::v128, t::value_kind::reference, t::value_kind::i32})
    {
        t::field_type field{}; field.mutable_ = true; field.storage.value.kind = kind;
        if(kind == t::value_kind::reference)
        {
            field.storage.value.heap.code = static_cast<::std::int_least64_t>(t::abstract_heap_type::eq);
            field.storage.value.nullable = true;
        }
        entry.fields.push_back(field);
    }
    entry.fields[2uz].storage.packed = t::packed_kind::i8;
    entry.fields[3uz].storage.packed = t::packed_kind::i16;
    entry.fields[8uz].mutable_ = false;
    group.types.push_back(::std::move(entry)); out.groups.push_back(::std::move(group)); return out;
}
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
static void emit_case(::llvm::Module& ir, gc::wasm_module_storage_t& module, ::std::uint32_t field, wasm_type value_type)
{
    ::fast_io::io::println("CASE struct.set32 field=", field, " value_type=", static_cast<unsigned>(value_type));
    auto& context{ir.getContext()};
    auto const intptr{::llvm::Type::getIntNTy(context, sizeof(::std::uintptr_t) * CHAR_BIT)};
    auto const i32{::llvm::Type::getInt32Ty(context)};
    auto const ref_type{d::get_llvm_type_from_wasm_value_type(context, wasm_type::funcref)};
    auto const input_type{d::get_llvm_type_from_wasm_value_type(context, value_type)};
    auto const type{::llvm::FunctionType::get(::llvm::Type::getInt64Ty(context),
        {::llvm::Type::getInt64Ty(context), ref_type, input_type}, false)};
    auto const name{c::concat_uwvm("struct_set32_field_", field)};
    auto const function{::llvm::Function::Create(type, ::llvm::GlobalValue::ExternalLinkage,
        ::llvm::StringRef{name.data(), name.size()}, ir)};
    function->addFnAttr(::llvm::Attribute::NoInline);
    auto const entry{::llvm::BasicBlock::Create(context, "entry", function)};
    compiler::local_func_storage_t local{}; local.runtime_module_ptr = ::std::addressof(module);
    d::runtime_local_func_llvm_jit_emit_state_t state{};
    state.valid = true; state.local_func_storage_ptr = ::std::addressof(local);
    state.llvm_context_holder = ::std::addressof(context); state.llvm_module = ::std::addressof(ir);
    state.llvm_function = state.llvm_public_entry_function = function;
    state.emit_call_stack_frames = false; state.ir_builder = c::make_delete_owned<::llvm::IRBuilder<>>(entry);
    state.operand_stack.push_back({.type = wasm_type::i64, .value = function->getArg(0u)});
    state.operand_stack.push_back({.type = wasm_type::funcref, .value = function->getArg(1u)});
    state.operand_stack.push_back({.type = value_type, .value = function->getArg(2u)});
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate decoded{};
    decoded.opcode = 5u; decoded.first = 0u; decoded.second = field;
#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
    auto invalid{decoded}; invalid.second = UINT32_MAX;
    SET32_IR_CHECK(!d::try_emit_runtime_local_func_llvm_jit_gc_struct_set32(state, invalid, wasm_type::i32));
    SET32_IR_CHECK(entry->empty() && state.operand_stack.size() == 3uz);
    // Actual unsupported types/immutable/out-of-bounds decline before emitting
    // any IR, so a caller can still use the complete original path.
    if(field >= 4u)
    {
        SET32_IR_CHECK(!d::try_emit_runtime_local_func_llvm_jit_gc_struct_set32(state, decoded,
            value_type == wasm_type::f32 ? wasm_type::f32 : wasm_type::i32));
        SET32_IR_CHECK(entry->empty() && state.operand_stack.size() == 3uz);
    }
#endif
    SET32_IR_CHECK(d::try_emit_runtime_local_func_llvm_jit_gc_aggregate(state, decoded));
    SET32_IR_CHECK(state.operand_stack.size() == 1uz && state.operand_stack.front().value == function->getArg(0u));
    state.ir_builder->CreateRet(function->getArg(0u));
    SET32_IR_CHECK(!::llvm::verifyFunction(*function, &::llvm::errs()));
    bool scalar{};
#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
    scalar = field < 4u;
#endif
    auto const bridge_type{scalar ? ::llvm::FunctionType::get(intptr, {intptr, intptr, intptr, intptr, intptr}, false) :
        ::llvm::FunctionType::get(i32, {intptr, i32, i32, intptr, intptr}, false)};
    c::u8string bridge_name{};
#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
    if(scalar) { bridge_name = d::get_llvm_runtime_bridge_function_symbol_name<d::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1>(
        bridge_type, c::u8string_view{u8"gc_struct_set32_registerwide_v1"}); }
    else
#endif
    {
        // The real fixed aggregate emitter names its operation and input count
        // explicitly. Verify that exact declaration; a template-value spelling
        // alone can alias a different equal-signature aggregate operation.
        constexpr c::u8string_view discriminator{u8"gc_fixed_5_2"};
        bridge_name = d::get_llvm_runtime_bridge_function_symbol_name<d::llvm_jit_gc_aggregate_fixed_bridge<5u, 2uz>>(
            bridge_type, discriminator);
        SET32_IR_CHECK(bridge_name != d::get_llvm_runtime_bridge_function_symbol_name<d::llvm_jit_gc_aggregate_fixed_bridge<5u, 2uz>>(
            bridge_type, c::u8string_view{u8"gc_fixed_0_2"}));
        SET32_IR_CHECK(bridge_name != d::get_llvm_runtime_bridge_function_symbol_name<d::llvm_jit_gc_aggregate_fixed_bridge<5u, 2uz>>(
            bridge_type, c::u8string_view{u8"gc_fixed_5_1"}));
    }
    auto const declaration{ir.getFunction(d::get_llvm_string_ref(bridge_name))};
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    SET32_IR_CHECK(declaration == nullptr);
    ::std::uintptr_t expected_address{};
# if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
    if(scalar) { expected_address = d::get_llvm_runtime_bridge_function_address(d::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1); }
    else
# endif
    { expected_address = d::get_llvm_runtime_bridge_function_address(d::llvm_jit_gc_aggregate_fixed_bridge<5u, 2uz>); }
#else
    SET32_IR_CHECK(declaration != nullptr && declaration->getFunctionType() == bridge_type);
#endif
    ::llvm::CallInst* actual{}; bool input_slot{}, output_slot{};
    for(auto& block : *function) for(auto& instruction : block)
    {
        if(auto call{::llvm::dyn_cast<::llvm::CallInst>(::std::addressof(instruction))})
        {
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
            bool const is_bridge{matches_actual_riscv64_bridge_address(call->getCalledOperand(), expected_address)};
#else
            bool const is_bridge{call->getCalledFunction() == declaration};
#endif
            if(is_bridge) { SET32_IR_CHECK(actual == nullptr); actual = call; }
        }
        if(auto slot{::llvm::dyn_cast<::llvm::AllocaInst>(::std::addressof(instruction))})
        { input_slot |= slot->getName() == "gc.input"; output_slot |= slot->getName() == "gc.output"; }
    }
    SET32_IR_CHECK(actual != nullptr && actual->getFunctionType() == bridge_type && actual->arg_size() == 5u && actual->doesNotThrow() &&
        actual->getCallingConv() == d::get_llvm_jit_host_calling_conv());
    SET32_IR_CHECK(input_slot == !scalar && output_slot == !scalar);
    auto const field_arg{::llvm::dyn_cast<::llvm::ConstantInt>(actual->getArgOperand(scalar ? 3u : 2u))};
    SET32_IR_CHECK(field_arg != nullptr && field_arg->getZExtValue() == field);
    if(scalar)
    {
        SET32_IR_CHECK(actual->getType() == intptr);
        for(unsigned index{}; index != 5u; ++index) { SET32_IR_CHECK(actual->getArgOperand(index)->getType() == intptr); }
        auto const raw{actual->getArgOperand(4u)};
        ::llvm::Value* narrow_raw{raw};
        if(sizeof(::std::uintptr_t) > sizeof(::std::uint32_t))
        {
            auto const extension{::llvm::dyn_cast<::llvm::ZExtInst>(raw)};
            SET32_IR_CHECK(extension != nullptr && extension->getSrcTy()->isIntegerTy(32u));
            narrow_raw = extension->getOperand(0u);
            SET32_IR_CHECK(::llvm::isa<::llvm::ZExtInst>(actual->getArgOperand(1u)));
        }
        if(value_type == wasm_type::f32)
        { SET32_IR_CHECK(::llvm::isa<::llvm::BitCastInst>(narrow_raw)); }
#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
# if !(defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64))
        SET32_IR_CHECK(reinterpret_cast<::std::uintptr_t>(::llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(
            d::get_llvm_string_ref(bridge_name).str())) ==
            d::get_llvm_runtime_bridge_function_address(d::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1));
# endif
        // On i386 both five-integer layouts collapse to i32. Use wrong arity
        // to test the type discriminator without assuming a 64-bit host.
        auto const wrong_type{::llvm::FunctionType::get(intptr, {intptr, intptr, intptr, intptr}, false)};
        SET32_IR_CHECK(bridge_name != d::get_llvm_runtime_bridge_function_symbol_name<d::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1>(wrong_type,
            c::u8string_view{u8"gc_struct_set32_registerwide_v1"}));
        SET32_IR_CHECK(bridge_name != d::get_llvm_runtime_bridge_function_symbol_name<d::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1>(bridge_type,
            c::u8string_view{u8"gc_struct_set32_registerwide_v2"}));
#endif
    }
}
int main(int argc, char** argv)
{
    SET32_IR_CHECK(argc == 2);
    SET32_IR_CHECK(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter());
    ::llvm::EngineBuilder builder{}; builder.setOptLevel(::llvm::CodeGenOptLevel::Aggressive);
    builder.setCodeModel(::llvm::CodeModel::Large);
    ::std::unique_ptr<::llvm::TargetMachine> machine{builder.selectTarget()}; SET32_IR_CHECK(machine != nullptr);
    ::llvm::LLVMContext context{}; ::llvm::Module ir{"struct-set32-candidate", context};
#if LLVM_VERSION_MAJOR >= 21
    ir.setTargetTriple(machine->getTargetTriple());
#else
    ir.setTargetTriple(machine->getTargetTriple().str());
#endif
    ir.setDataLayout(machine->createDataLayout());
    gc::wasm_module_storage_t module{}; module.module_name = u8"struct-set32-candidate";
    module.gc_lease_roots = ::std::make_shared<gc::gc_lease_owner>(); auto section{declarations()};
    module.gc_store = ::std::make_shared<gc::gc_object_store>(section, module.gc_lease_roots);
    SET32_IR_CHECK(module.gc_store->valid());
    ::std::array<wasm_type, 9uz> inputs{wasm_type::i32, wasm_type::f32, wasm_type::i32, wasm_type::i32,
        wasm_type::i64, wasm_type::f64, wasm_type::v128, wasm_type::funcref, wasm_type::i32};
    for(::std::uint32_t field{}; field != inputs.size(); ++field) { emit_case(ir, module, field, inputs[field]); }
    write_ir(ir, argv[1], "struct-set32.input.ll");
    ::llvm::LoopAnalysisManager loop; ::llvm::FunctionAnalysisManager functions;
    ::llvm::CGSCCAnalysisManager graph; ::llvm::ModuleAnalysisManager modules;
    ::llvm::PassBuilder passes{machine.get()}; passes.registerLoopAnalyses(loop); passes.registerFunctionAnalyses(functions);
    passes.registerCGSCCAnalyses(graph); passes.registerModuleAnalyses(modules);
    passes.crossRegisterProxies(loop, functions, graph, modules);
    auto pipeline{passes.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O3)}; pipeline.run(ir, modules);
    SET32_IR_CHECK(!::llvm::verifyModule(ir, &::llvm::errs())); write_ir(ir, argv[1], "struct-set32.optimized.ll");
    ::llvm::SmallVector<char, 0u> bytes{}; ::llvm::raw_svector_ostream stream{bytes}; ::llvm::legacy::PassManager codegen{};
    SET32_IR_CHECK(!machine->addPassesToEmitFile(codegen, stream, nullptr, ::llvm::CodeGenFileType::ObjectFile, false));
    codegen.run(ir); SET32_IR_CHECK(!bytes.empty());
    write_bytes(argv[1], "struct-set32.o", reinterpret_cast<::std::byte const*>(bytes.data()), bytes.size());
    ::fast_io::io::println("PASS struct.set32 compiler cases=9 whole_vm=false timing=false object_bytes=", bytes.size());
}
