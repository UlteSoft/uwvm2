// PRIVATE actual validated-stream lowering experiment; no production import.
// Included after runtime_local_func_llvm_jit_emit_state_t inside the emitter namespace.
[[nodiscard]] inline ::llvm::FunctionType* pending_numeric_core_type(
    ::llvm::LLVMContext& context,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_type) noexcept
{
    auto const public_type{get_llvm_function_type_from_wasm_function_type(context, wasm_type)};
    if(public_type == nullptr) { return nullptr; }
    auto const parameter_count{static_cast<::std::size_t>(public_type->getNumParams())};
    constexpr auto maximum_pointer_count{static_cast<::std::size_t>(
        (::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(::llvm::Type*)};
    static_assert(maximum_pointer_count >= 2uz);
    // LLVM's contained type array counts BOTH the result type and every
    // argument. The core adds one context argument to the public parameters;
    // leave room for these TWO complete Type* elements and both unsigned
    // additions before constructing or handing its span to FunctionType::get.
    if(parameter_count >= static_cast<::std::size_t>((::std::numeric_limits<unsigned>::max)()) - 1uz ||
       parameter_count >= maximum_pointer_count - 1uz) { return nullptr; }
    ::uwvm2::utils::container::vector<::llvm::Type*> parameters{};
    parameters.reserve(parameter_count + 1uz);
    parameters.push_back(::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT)));
    for(auto const parameter: public_type->params()) { parameters.push_back(parameter); }
    // Hidden native context FIRST. The original multi-result caller buffer
    // remains LAST; no existing raw/public Wasm ABI or tuple layout changes.
    return ::llvm::FunctionType::get(public_type->getReturnType(), {parameters.data(), parameters.size()}, false);
}

[[nodiscard]] inline ::llvm::Function* pending_numeric_core_declaration(
    ::llvm::Module& llvm_module, ::llvm::LLVMContext& context,
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
    validation_module_traits_t::wasm_u32 index,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_type) noexcept
{
    auto const type{pending_numeric_core_type(context, wasm_type)};
    if(type == nullptr) { return nullptr; }
    auto const name{::uwvm2::utils::container::u8concat_uwvm(
        get_llvm_wasm_function_name(module, index), u8"_pending_numeric_core_r2")};
    auto* function{llvm_module.getFunction(get_llvm_string_ref(name))};
    if(function == nullptr)
    {
        // [owning LLVM module][new native-context typed declaration]
        // [safe                                                  ] declaration
        // is never the unchanged public/raw entry and has an explicit ABI name.
        function = ::llvm::Function::Create(type, ::llvm::Function::InternalLinkage,
            get_llvm_string_ref(name), llvm_module);
        auto const identity{::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(
            ::llvm::Type::getInt64Ty(context), static_cast<::std::uint_least64_t>(index)))};
        function->setMetadata("uwvm2.pending.core.index", ::llvm::MDNode::get(context, {identity}));
    }
    else if(function->getFunctionType() != type) { return nullptr; }
    apply_llvm_jit_wasm_calling_conv(*function);
    return function;
}

// The pending numeric island accepts numeric/control/EH bytecode, owned numeric globals and scalar/SIMD memory. It
// refuses indirect/ref/host/GC and bulk/atomic memory operations instead of letting a call to an
// unqualified ABI consume pending state or publish an incomplete native module.
// The authoritative validator still processes all instructions and reports the
// original feature/type/bytecode errors. Selection still requires the admitted full-source plan.
[[nodiscard]] inline constexpr bool pending_numeric_primary_opcode(unsigned opcode) noexcept
{
    // Scalar owned-memory accesses retain the ordinary validated lowering,
    // bounds/trap checks and native unwind edges. No reference is introduced
    // into the private numeric context. The complete source plan rejects all
    // imported memories; grow, bulk and atomic remain outside this island.
    // SIMD carries only numeric bits. Prefix admission never replaces the
    // ordinary complete SIMD subopcode/immediate/type/feature validation.
    if(opcode >= 0x28u && opcode <= 0x3fu) { return true; }
    if(opcode >= 0x41u && opcode <= 0xc4u) { return true; }
    switch(opcode)
    {
        case 0x00u: case 0x01u: case 0x02u: case 0x03u: case 0x04u: case 0x05u:
        case 0x08u: case 0x0bu: case 0x0cu: case 0x0du: case 0x0eu: case 0x0fu:
        case 0x10u: case 0x12u: case 0x1au: case 0x1bu: case 0x1fu:
        case 0x20u: case 0x21u: case 0x22u: case 0x23u: case 0x24u: case 0xfdu:
            return true;
        default: return false;
    }
}

[[nodiscard]] inline bool emit_pending_numeric_public_wrapper(runtime_local_func_llvm_jit_emit_state_t&) noexcept;
[[nodiscard]] inline bool emit_pending_numeric_route(runtime_local_func_llvm_jit_emit_state_t&,
    validation_module_traits_t::wasm_u32) noexcept;
[[nodiscard]] inline bool try_emit_pending_numeric_throw_tuple(runtime_local_func_llvm_jit_emit_state_t&,
    ::std::size_t, runtime_block_result_type) noexcept;
[[nodiscard]] inline bool try_emit_pending_numeric_return_call(runtime_local_func_llvm_jit_emit_state_t&,
    validation_module_traits_t::wasm_u32) noexcept;

// A real catch_all in any active lexical try_table consumes every guest tag
// escaping an ordinary call. It never consumes a foreign C++ unwind. Typed
// catches do not clear an unknown callee tag set in this first graph analysis.
[[nodiscard]] inline bool pending_numeric_call_can_escape_guest(
    runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    for(auto const& control: state.control_stack)
    {
        for(auto const& handler: control.exception_handlers)
        { if(handler.catch_all) { return false; } }
    }
    return true;
}

inline void record_pending_numeric_call(::llvm::CallBase& call,
    validation_module_traits_t::wasm_u32 target, bool guest_can_escape) noexcept
{
    auto& context{call.getContext()};
    auto const identity{::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(
        ::llvm::Type::getInt64Ty(context), static_cast<::std::uint_least64_t>(target)))};
    auto const escaping{::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(
        ::llvm::Type::getInt1Ty(context), guest_can_escape))};
    // Actual validator-created direct call, after complete ABI operand checks.
    // Immutable indices survive graph construction; no guest address is used.
    call.setMetadata("uwvm2.pending.core.call", ::llvm::MDNode::get(context, {identity, escaping}));
}
