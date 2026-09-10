// This header owns the single-function LLVM JIT lowering path for validated Wasm MVP bytecode.
//
// The implementation is intentionally header-only because the opcode case files included near the end of this file
// depend on the local lambdas and type aliases declared by the instruction dispatcher.  Keep the following maintenance
// model in mind when changing this file:
//   * Runtime storage and validation have already established the Wasm-level shape of the function, but the JIT still
//     performs defensive null, bounds, and type checks before creating LLVM IR.
//   * Direct LLVM IR is preferred for same-module Wasm calls and for mmap-backed little-endian memory accesses.
//   * Host/imported functions, imported memories, lazy tier targets, and bridged memory operations cross the runtime
//     bridge ABI and therefore use raw address/byte-buffer conventions.
//   * Structured Wasm control flow is lowered with an explicit control stack, branch-target stack, and one PHI per tuple
//     field for full type-index block signatures and multi-value function results.
//   * A `false` return means native emission cannot be completed safely; callers discard the partial LLVM module and
//     fail materialization instead of relying on malformed IR.
// A value currently held on the JIT's transient operand stack.  The Wasm value type is stored beside the LLVM value so
// helper emitters can cheaply re-check stack discipline even though validation has already run.
#pragma push_macro("UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER")
#undef UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER
#if defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__)
# define UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER 1
#else
# define UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER 0
#endif

struct llvm_jit_stack_value_t
{
    // Wasm scalar type represented by `value`.
    runtime_operand_stack_value_type type{};

    // LLVM SSA value for the operand.  Null means the stack entry is unusable and should make the emitter fail.
    ::llvm::Value* value{};
};

// Packed byte-buffer layout used when a Wasm call must cross the raw runtime bridge ABI.
struct llvm_jit_runtime_wasm_call_abi_layout_t
{
    // True only when parameters/results are well-formed and supported by this JIT.
    bool valid{};

    // Number of scalar Wasm parameters in source order.
    ::std::size_t parameter_count{};

    // Number of scalar Wasm results in source order.
    ::std::size_t result_count{};

    // Total bytes required by the raw parameter buffer after scalar ABI packing.
    ::std::size_t parameter_bytes{};

    // Total bytes required by the raw result buffer, zero for void functions.
    ::std::size_t result_bytes{};
};

// Stack-allocated buffers passed to raw bridge calls.  The bridge sees integer addresses, while the JIT keeps the
// allocas so it can read the result back after the host/runtime call returns.
struct llvm_jit_runtime_raw_call_buffers_t
{
    // True when all required buffers and addresses were created successfully.
    bool valid{};

    // Integer address of the packed parameter buffer, or zero for functions without parameters.
    ::llvm::Value* param_buffer_address{};

    // Packed i8 result-buffer alloca used by the current LLVM function, or null for void callees.
    ::llvm::AllocaInst* result_buffer{};

    // Integer address of `result_buffer`, or zero for void callees.
    ::llvm::Value* result_buffer_address{};
};

// Result of emitting a raw bridge call that may also produce a typed Wasm result value.
struct llvm_jit_runtime_raw_bridge_emit_result_t
{
    // True when the bridge call and any result load were emitted successfully.
    bool valid{};

    // The emitted runtime/host call instruction.
    ::llvm::CallInst* bridge_call{};

    // Typed LLVM result loaded from the raw result buffer. Multi-value calls use the same literal LLVM struct type as
    // typed Wasm entries; null means the callee has no result.
    ::llvm::Value* result_value{};
};

// Arguments removed from the operand stack and normalized into source-order call operands.
struct llvm_jit_prepared_wasm_call_operands_t
{
    // True once the stack operands matched the callee type and `arguments` is complete.
    bool valid{};

    // Raw ABI layout derived from the callee Wasm function type.
    llvm_jit_runtime_wasm_call_abi_layout_t abi_layout{};

    // Complete callee result tuple in Wasm source order.
    runtime_block_result_type results{};

    // LLVM call operands in Wasm parameter order, not operand-stack pop order.
    ::uwvm2::utils::container::vector<::llvm::Value*> arguments{};
};

// Memory snapshot values used for imported memories that can provide an address/length view only through a bridge call.
struct llvm_jit_memory_snapshot_values_t
{
    // Integer address of the beginning of the current memory snapshot.
    ::llvm::Value* memory_begin_address{};

    // Byte length of the snapshot.
    ::llvm::Value* byte_length{};
};

// Convert uwvm string views into LLVM's non-owning StringRef without copying.
[[nodiscard]] inline constexpr ::llvm::StringRef get_llvm_string_ref(::uwvm2::utils::container::u8string_view str) noexcept
{
    using char_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char const*;
    return ::llvm::StringRef{reinterpret_cast<char_const_may_alias_ptr>(str.data()), str.size()};
}

[[nodiscard]] inline constexpr ::llvm::StringRef get_llvm_string_ref(::llvm::StringRef str) noexcept { return str; }

// Convert LLVM's byte view back to uwvm's UTF-8 byte view at API boundaries.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view get_uwvm_u8string_view(::llvm::StringRef str) noexcept
{
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    return ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t_const_may_alias_ptr>(str.data()), str.size()};
}

// Convenience overload for owning uwvm strings.
[[nodiscard]] inline constexpr ::llvm::StringRef get_llvm_string_ref(::uwvm2::utils::container::u8string const& str) noexcept
{ return get_llvm_string_ref(::uwvm2::utils::container::u8string_view{str.data(), str.size()}); }

// LLVM StringRef borrows bytes, so accepting an owning string temporary would immediately dangle.
[[nodiscard]] inline constexpr ::llvm::StringRef get_llvm_string_ref(::uwvm2::utils::container::u8string&&) noexcept = delete;
[[nodiscard]] inline constexpr ::llvm::StringRef get_llvm_string_ref(::uwvm2::utils::container::u8string const&&) noexcept = delete;

// Check whether a basic block is already closed.  Most helpers call this before adding branches to avoid invalid LLVM IR.
[[nodiscard]] inline constexpr bool llvm_jit_basic_block_has_terminator(::llvm::BasicBlock const* block) noexcept
{
    if(block == nullptr) [[unlikely]] { return false; }
    return !block->empty() && block->back().isTerminator();
}

// Optionally verify a completed LLVM function.  Verification failures are treated as internal storage bugs because the
// bytecode was already validated and the emitter should never create malformed IR.
[[nodiscard]] inline constexpr bool verify_llvm_jit_function(::llvm::Function& function, bool verify_llvm_jit_ir) noexcept
{
    if(!verify_llvm_jit_ir) { return true; }
    if(!::llvm::verifyFunction(function)) [[likely]] { return true; }
    runtime_storage_bug();
}

// Optionally verify a completed LLVM module under the same fail-fast policy as function verification.
[[nodiscard]] inline constexpr bool verify_llvm_jit_module(::llvm::Module& module, bool verify_llvm_jit_ir) noexcept
{
    if(!verify_llvm_jit_ir) { return true; }
    if(!::llvm::verifyModule(module)) [[likely]] { return true; }
    runtime_storage_bug();
}

// LLVM raw_ostream adapter for uwvm's string container.  This is used when LLVM diagnostics or IR dumps need to be
// accumulated without switching away from uwvm string ownership.
class raw_uwvm_string_ostream : public ::llvm::raw_ostream
{
    // Destination buffer owned by the caller.
    ::uwvm2::utils::container::u8string& output;

    // Append bytes exactly as LLVM provides them; no encoding transformation is performed.
    inline constexpr void write_impl(char const* ptr, ::std::size_t size) noexcept override
    {
        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
        output.append(reinterpret_cast<char8_t_const_may_alias_ptr>(ptr), size);
    }

    // LLVM uses current_pos() for stream bookkeeping and reserve hints.
    [[nodiscard]] inline constexpr ::std::uint_least64_t current_pos() const noexcept override { return output.size(); }

public:
    // `unbuffered = true` keeps LLVM writes immediately visible in `output`.
    inline constexpr explicit raw_uwvm_string_ostream(::uwvm2::utils::container::u8string& str) noexcept : ::llvm::raw_ostream{true}, output{str} {}

    // Preserve LLVM's reserveExtraSpace contract while mapping sizes back to uwvm's container type.
    inline constexpr void reserveExtraSpace(::std::uint_least64_t extra_size) noexcept override
    { output.reserve(static_cast<::std::size_t>(this->tell() + extra_size)); }
};

// Return the canonical Wasm byte encoding for a runtime scalar type.  Switches use the byte encoding so parser and JIT
// type handling stay tied to the same representation.
[[nodiscard]] inline constexpr ::std::uint_least8_t get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type value_type) noexcept
{ return static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type)); }

// Map a supported Wasm value to its LLVM register/storage type. References and v128 use opaque integers with the exact
// runtime storage width. Generated code crosses C++ bridges through pointer buffers, avoiding target-specific aggregate
// and native-vector ABIs while preserving all payload bits.
[[nodiscard]] inline constexpr ::llvm::Type* get_llvm_type_from_wasm_value_type(::llvm::LLVMContext& llvm_context,
                                                                                runtime_operand_stack_value_type value_type) noexcept
{
    switch(get_runtime_wasm_value_type_encoding(value_type))
    {
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i32)):
            return ::llvm::Type::getInt32Ty(llvm_context);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i64)):
            return ::llvm::Type::getInt64Ty(llvm_context);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f32)):
            return ::llvm::Type::getFloatTy(llvm_context);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f64)):
            return ::llvm::Type::getDoubleTy(llvm_context);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::v128)):
            return ::llvm::Type::getIntNTy(
                llvm_context,
                static_cast<unsigned>(sizeof(::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128) * CHAR_BIT));
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::funcref)):
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::externref)):
            return ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::uwvm2::object::global::wasm_global_ref_t) * CHAR_BIT));
        [[unlikely]] default:
            return nullptr;
    }
}

// Produce an LLVM pointer type in the requested address space.  The pointee is used only to recover the LLVM context
// because opaque pointers no longer encode pointee type information.
[[nodiscard]] inline constexpr ::llvm::PointerType* get_llvm_pointer_type(::llvm::Type* pointee_type, unsigned address_space = 0u) noexcept
{
    if(pointee_type == nullptr) [[unlikely]] { return nullptr; }

    // LLVM's address-space type is `unsigned`; keep this helper's public signature aligned with LLVM rather than
    // widening to size_t and then truncating at every call site.
    return ::llvm::PointerType::get(pointee_type->getContext(), address_space);
}

// Embed a host address as an LLVM constant pointer.  The JIT uses this for runtime bridge functions and storage objects
// that are known to outlive the generated module.
[[nodiscard]] inline constexpr ::llvm::Constant* get_llvm_host_pointer_constant(::std::uintptr_t host_address, ::llvm::Type* pointer_type) noexcept
{
    if(pointer_type == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{pointer_type->getContext()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto host_address_value{::llvm::ConstantInt::get(llvm_intptr_type, host_address)};
    return ::llvm::ConstantExpr::getIntToPtr(host_address_value, pointer_type);
}

// Extract the integer address of a C++ runtime bridge function pointer.
// The address is copied through object bytes instead of forming a ptrtoint-style LLVM constant from the function symbol:
// COFF/MCJIT on Windows may otherwise leave local C++ bridge symbols unresolved in lazily materialized tiered modules.
template <typename FunctionPtr>
[[nodiscard]] inline constexpr ::std::uintptr_t get_llvm_runtime_bridge_function_address(FunctionPtr function_pointer) noexcept
{
    static_assert(sizeof(FunctionPtr) <= sizeof(::std::uintptr_t));
    ::std::uintptr_t function_address{};
    ::std::memcpy(::std::addressof(function_address), ::std::addressof(function_pointer), sizeof(FunctionPtr));
    return function_address;
}

// Convert a runtime bridge function pointer into an LLVM constant pointer with the exact supplied function type.
template <typename FunctionPtr>
[[nodiscard]] inline constexpr ::llvm::Constant*
    get_llvm_runtime_bridge_function_pointer(::llvm::LLVMContext& llvm_context, ::llvm::FunctionType* function_type, FunctionPtr function_pointer) noexcept
{
    if(function_type == nullptr) [[unlikely]] { return nullptr; }

    auto const function_address{get_llvm_runtime_bridge_function_address(function_pointer)};
    if(function_address == 0u) [[unlikely]] { return nullptr; }
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_address{::llvm::ConstantInt::get(llvm_intptr_type, function_address)};
    return ::llvm::ConstantExpr::getIntToPtr(llvm_address, get_llvm_pointer_type(function_type));
}

#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
// Materialize a full process-local address without routing through a JIT data-section constant pool.  RuntimeDyld's
// RISC-V64 handling can truncate absolute references to the emitted object's own data section.  Build the value from
// byte-sized immediates seeded by a volatile stack zero so LLVM cannot fold it back into an inttoptr constant.
[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_riscv64_immediate_pointer_value(::llvm::IRBuilder<>& ir_builder,
                                                                                       ::std::uintptr_t host_address,
                                                                                       ::llvm::Type* pointer_type) noexcept
{
    if(host_address == 0u || pointer_type == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{ir_builder.getContext()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto current_block{ir_builder.GetInsertBlock()};
    auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
    if(current_function == nullptr || current_function->empty()) [[unlikely]] { return nullptr; }

    auto& entry_block{current_function->getEntryBlock()};
    ::llvm::IRBuilder<> entry_builder{&entry_block, entry_block.getFirstInsertionPt()};
    auto zero_slot{entry_builder.CreateAlloca(llvm_intptr_type, nullptr, get_llvm_string_ref(u8"uwvm.riscv64.host.addr.zero"))};
    zero_slot->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});

    auto zero_store{ir_builder.CreateStore(::llvm::ConstantInt::get(llvm_intptr_type, 0u), zero_slot)};
    zero_store->setVolatile(true);
    zero_store->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});

    auto address_load{ir_builder.CreateLoad(llvm_intptr_type, zero_slot, get_llvm_string_ref(u8"uwvm.riscv64.host.addr"))};
    address_load->setVolatile(true);
    address_load->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    ::llvm::Value* address_value{address_load};

    for(unsigned shift{static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u - 8u)};; shift -= 8u)
    {
        if(shift != static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u - 8u))
        {
            address_value = ir_builder.CreateShl(address_value, ::llvm::ConstantInt::get(llvm_intptr_type, 8u), get_llvm_string_ref(u8"uwvm.riscv64.host.addr.shl"));
        }

        auto const chunk{static_cast<::std::uint_least8_t>((host_address >> shift) & 0xffu)};
        if(chunk != 0u)
        {
            address_value = ir_builder.CreateOr(address_value,
                                                ::llvm::ConstantInt::get(llvm_intptr_type, chunk),
                                                get_llvm_string_ref(u8"uwvm.riscv64.host.addr.or"));
        }

        if(shift == 0u) { break; }
    }

    return ir_builder.CreateIntToPtr(address_value, pointer_type, get_llvm_string_ref(u8"uwvm.host.ptr"));
}
#endif

#if UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER
// Put host addresses in the JIT module's own data section on targets where MCJIT cannot reliably materialize arbitrary
// process addresses directly. COFF/Win64 may truncate far host-image addresses.
[[nodiscard]] inline constexpr ::llvm::Value*
    get_llvm_jit_host_address_value(::llvm::IRBuilder<>& ir_builder, ::std::uintptr_t host_address, ::llvm::StringRef name_prefix) noexcept
{
    if(host_address == 0u) [[unlikely]] { return nullptr; }

    // The helper must attach the address carrier to the same LLVM module that owns the insertion point; otherwise MCJIT
    // may materialize a relocation against a different object image than the one containing the generated code.
    auto current_block{ir_builder.GetInsertBlock()};
    auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
    auto llvm_module{current_function == nullptr ? nullptr : current_function->getParent()};
    if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{ir_builder.getContext()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

    // Store the absolute host address as data owned by the JIT object.  A non-constant private global prevents LLVM from
    // folding the load back into an immediate `inttoptr`, while PrivateLinkage keeps the carrier local to this module.
    ::uwvm2::utils::container::u8string address_symbol_name{};
    {
        ::uwvm2::utils::container::u8string_ref_uwvm ref{::std::addressof(address_symbol_name)};
        ::fast_io::io::print(ref,
                             ::uwvm2::utils::container::u8string_view{reinterpret_cast<char8_t const*>(name_prefix.data()), name_prefix.size()},
                             ::fast_io::mnp::hex<false, true>(host_address));
    }
    auto address_global_value{llvm_module->getOrInsertGlobal(get_llvm_string_ref(address_symbol_name), llvm_intptr_type)};
    auto address_global{::llvm::dyn_cast<::llvm::GlobalVariable>(address_global_value)};
    if(address_global == nullptr) [[unlikely]] { return nullptr; }
    address_global->setLinkage(::llvm::GlobalValue::PrivateLinkage);
    address_global->setInitializer(::llvm::ConstantInt::get(llvm_intptr_type, host_address));

    // The value itself has no externally observable identity, but its storage must be naturally aligned because the
    // generated code performs a real pointer-sized load from the JIT data section.
    address_global->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global);
    address_global->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});

    // Keep the load visible to codegen. This encourages a target-local load from nearby JIT data instead of a direct
    // relocation against the host image or runtime storage.
    auto loaded_address{ir_builder.CreateLoad(llvm_intptr_type, address_global, get_llvm_string_ref(u8"uwvm.host.addr"))};
    loaded_address->setVolatile(true);
    loaded_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    return loaded_address;
}

// Convert a host address into a typed pointer value by first loading the address from JIT-owned data.
[[nodiscard]] inline constexpr ::llvm::Value*
    get_llvm_jit_host_pointer_value(::llvm::IRBuilder<>& ir_builder, ::std::uintptr_t host_address, ::llvm::Type* pointer_type) noexcept
{
    if(pointer_type == nullptr) [[unlikely]] { return nullptr; }

    auto loaded_address{get_llvm_jit_host_address_value(ir_builder, host_address, get_llvm_string_ref(u8"uwvm.host.ptr."))};
    if(loaded_address == nullptr) [[unlikely]] { return nullptr; }
    return ir_builder.CreateIntToPtr(loaded_address, pointer_type, get_llvm_string_ref(u8"uwvm.host.ptr"));
}
#endif

// Return a typed LLVM pointer value for a stable host address, using the data-section workaround when required.
[[nodiscard]] inline constexpr ::llvm::Value*
    get_llvm_host_pointer_value(::llvm::IRBuilder<>& ir_builder, ::std::uintptr_t host_address, ::llvm::Type* pointer_type) noexcept
{
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    return get_llvm_riscv64_immediate_pointer_value(ir_builder, host_address, pointer_type);
#elif UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER
    return get_llvm_jit_host_pointer_value(ir_builder, host_address, pointer_type);
#else
    static_cast<void>(ir_builder);
    return get_llvm_host_pointer_constant(host_address, pointer_type);
#endif
}

// Materialize a runtime bridge function pointer at the current insertion point. Some MCJIT targets need a runtime load
// from JIT-owned data; on other targets an LLVM constant pointer is sufficient.
template <typename FunctionPtr>
[[nodiscard]] inline constexpr ::llvm::Value*
    get_llvm_runtime_bridge_function_pointer_value(::llvm::IRBuilder<>& ir_builder, ::llvm::FunctionType* function_type, FunctionPtr function_pointer) noexcept
{
    if(function_type == nullptr) [[unlikely]] { return nullptr; }

    [[maybe_unused]] auto& llvm_context{ir_builder.getContext()};
    auto const function_address{get_llvm_runtime_bridge_function_address(function_pointer)};
    if(function_address == 0u) [[unlikely]] { return nullptr; }

#if UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER
    // Keep this in sync with tiered/raw validation. Direct `inttoptr` host bridge constants are not reliable on every
    // MCJIT target covered by UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER.
    auto loaded_address{get_llvm_jit_host_address_value(ir_builder, function_address, get_llvm_string_ref(u8"uwvm.bridge."))};
    if(loaded_address == nullptr) [[unlikely]] { return nullptr; }
    return ir_builder.CreateIntToPtr(loaded_address, get_llvm_pointer_type(function_type), get_llvm_string_ref(u8"runtime.bridge.ptr"));
#else
    static_cast<void>(ir_builder);
    return get_llvm_runtime_bridge_function_pointer(llvm_context, function_type, function_pointer);
#endif
}

template <auto Function, typename FunctionSignature = decltype(Function)>
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_runtime_bridge_function_symbol_name(::uwvm2::utils::container::u8string_view discriminator = {}) noexcept
{
    char const* pretty{__PRETTY_FUNCTION__};
    ::std::size_t pretty_size{};
    while(pretty[pretty_size] != '\0') { ++pretty_size; }

    ::uwvm2::utils::container::u8string hash_input{};
    hash_input.append(reinterpret_cast<char8_t const*>(pretty), pretty_size);
    if(!discriminator.empty())
    {
        hash_input.push_back(u8'#');
        hash_input.append(discriminator.data(), discriminator.size());
    }

    auto const hash{::uwvm2::utils::hash::xxh3_64bits(reinterpret_cast<::std::byte const*>(hash_input.data()), hash_input.size())};
    return ::uwvm2::utils::container::u8concat_uwvm(u8"uwvm_bridge_", ::fast_io::mnp::hex<false, true>(hash));
}

[[nodiscard]] inline constexpr ::std::uint_least64_t get_llvm_runtime_bridge_function_type_hash(::llvm::FunctionType const* function_type) noexcept
{
    if(function_type == nullptr) [[unlikely]] { return 0u; }

    ::uwvm2::utils::container::u8string type_signature{};
    raw_uwvm_string_ostream type_stream{type_signature};
    function_type->print(type_stream);
    type_stream.flush();
    return ::uwvm2::utils::hash::xxh3_64bits(reinterpret_cast<::std::byte const*>(type_signature.data()), type_signature.size());
}

template <auto Function>
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_runtime_bridge_function_symbol_name(::llvm::FunctionType const* function_type,
                                                 ::uwvm2::utils::container::u8string_view discriminator = {}) noexcept
{
    auto const discriminator_hash{::uwvm2::utils::hash::xxh3_64bits(reinterpret_cast<::std::byte const*>(discriminator.data()), discriminator.size())};
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_bridge_function_symbol_name<Function>(),
                                                   u8"_",
                                                   ::fast_io::mnp::hex<false, true>(discriminator_hash),
                                                   u8"_",
                                                   ::fast_io::mnp::hex<false, true>(get_llvm_runtime_bridge_function_type_hash(function_type)));
}

template <auto Function>
[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_runtime_bridge_function_symbol_value(::llvm::IRBuilder<>& ir_builder,
                                                                                            ::llvm::FunctionType* function_type,
                                                                                            ::uwvm2::utils::container::u8string_view discriminator = {}) noexcept
{
    if(function_type == nullptr) [[unlikely]] { return nullptr; }

    auto current_block{ir_builder.GetInsertBlock()};
    auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
    auto llvm_module{current_function == nullptr ? nullptr : current_function->getParent()};
    if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

    auto const function_address{get_llvm_runtime_bridge_function_address(Function)};
    if(function_address == 0u) [[unlikely]] { return nullptr; }

#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    // RISC-V64 RuntimeDyld cannot reliably relocate far process-local bridge functions through an external symbol call.
    // Materialize the bridge address directly; callers that use object caching must disable it for this target.
    static_cast<void>(llvm_module);
    static_cast<void>(discriminator);
    return get_llvm_host_pointer_value(ir_builder, function_address, get_llvm_pointer_type(function_type));
#else
    auto symbol_name{get_llvm_runtime_bridge_function_symbol_name<Function>(function_type, discriminator)};
    auto symbol_name_ref{get_llvm_string_ref(symbol_name)};

    ::llvm::sys::DynamicLibrary::AddSymbol(symbol_name_ref, reinterpret_cast<void*>(function_address));
    if(auto existing{llvm_module->getFunction(symbol_name_ref)}; existing != nullptr)
    {
        if(existing->getFunctionType() != function_type) [[unlikely]] { return nullptr; }
        return existing;
    }
    return ::llvm::Function::Create(function_type, ::llvm::Function::ExternalLinkage, symbol_name_ref, llvm_module);
#endif
}

[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_external_host_object_pointer(::llvm::IRBuilder<>& ir_builder,
                                                                                    ::std::uintptr_t host_address,
                                                                                    ::llvm::Type* object_type,
                                                                                    ::uwvm2::utils::container::u8string_view symbol_name) noexcept
{
    if(host_address == 0u || object_type == nullptr || symbol_name.empty()) [[unlikely]] { return nullptr; }

    auto pointer_type{get_llvm_pointer_type(object_type)};
    if(pointer_type == nullptr) [[unlikely]] { return nullptr; }

#if defined(__i386__) || defined(_M_IX86) || defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    // ELF i386 MCJIT can materialize external data symbols as zero under some target-native/QEMU builds.  AArch64
    // RuntimeDyld can similarly mis-resolve small lazy-module external data references under qemu-user, producing
    // near-8GiB offsets for mmap memory base symbols.  These host objects are process-stable tables/storage, so materialize
    // their full pointer values directly.
    static_cast<void>(ir_builder);
    static_cast<void>(symbol_name);
    return get_llvm_host_pointer_value(ir_builder, host_address, pointer_type);
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    // ELF RISC-V64 RuntimeDyld handles some absolute HI20/LO12 references as 32-bit addresses and lacks LO12_S.
    // Avoid both external data symbols and JIT data-section constants by materializing the full pointer in code.
    static_cast<void>(symbol_name);
    return get_llvm_host_pointer_value(ir_builder, host_address, pointer_type);
#else
    auto current_block{ir_builder.GetInsertBlock()};
    auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
    auto llvm_module{current_function == nullptr ? nullptr : current_function->getParent()};
    if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

    auto symbol_name_ref{get_llvm_string_ref(symbol_name)};
    if(auto existing_value{llvm_module->getNamedValue(symbol_name_ref)}; existing_value != nullptr)
    {
        auto existing{::llvm::dyn_cast<::llvm::GlobalVariable>(existing_value)};
        // Opaque pointers do not encode the pointee type, but GlobalVariable still does. Reusing one symbol with a
        // different storage extent would silently invalidate the object/provenance contract of later GEPs. Validate
        // before touching LLVM's process-global symbol map so the fail-closed path has no externally visible side effect.
        if(existing == nullptr || existing->getValueType() != object_type || !existing->isDeclaration()) [[unlikely]] { return nullptr; }
        ::llvm::sys::DynamicLibrary::AddSymbol(symbol_name_ref, reinterpret_cast<void*>(host_address));
        return existing;
    }
    auto global_value{llvm_module->getOrInsertGlobal(symbol_name_ref, object_type)};
    auto global{::llvm::dyn_cast<::llvm::GlobalVariable>(global_value)};
    if(global == nullptr) [[unlikely]] { return nullptr; }
    global->setLinkage(::llvm::GlobalValue::ExternalLinkage);
    global->setInitializer(nullptr);
    ::llvm::sys::DynamicLibrary::AddSymbol(symbol_name_ref, reinterpret_cast<void*>(host_address));
    return global;
#endif
}

// Describe an externally allocated, process-stable byte reservation to LLVM without embedding its absolute address in
// cached objects. The external array extent is semantically important: a one-byte GlobalVariable would not describe
// the mmap object reached by later non-zero GEPs, even though RuntimeDyld resolves the symbol to the correct address.
[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_external_host_byte_span_pointer(
    ::llvm::IRBuilder<>& ir_builder,
    ::std::uintptr_t host_address,
    ::std::size_t byte_size,
    ::uwvm2::utils::container::u8string_view symbol_name) noexcept
{
    if(byte_size == 0uz) [[unlikely]] { return nullptr; }
    auto llvm_i8_type{::llvm::Type::getInt8Ty(ir_builder.getContext())};
    auto byte_span_type{::llvm::ArrayType::get(llvm_i8_type, static_cast<::std::uint_least64_t>(byte_size))};
    return get_llvm_external_host_object_pointer(ir_builder, host_address, byte_span_type, symbol_name);
}

// Every direct guest-memory store must stay observable until it either writes the mapped byte range or faults in a
// guard page. Centralizing both properties prevents integer and floating-point opcode families from drifting apart.
[[nodiscard]] inline constexpr ::llvm::StoreInst*
    finalize_llvm_jit_direct_memory_store(::llvm::StoreInst* store_inst, ::llvm::Align memory_alignment) noexcept
{
    if(store_inst == nullptr) [[unlikely]] { return nullptr; }
    store_inst->setAlignment(memory_alignment);
    store_inst->setVolatile(true);
    return store_inst;
}

[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_external_host_object_address(::llvm::IRBuilder<>& ir_builder,
                                                                                    ::std::uintptr_t host_address,
                                                                                    ::uwvm2::utils::container::u8string_view symbol_name) noexcept
{
    if(host_address == 0u || symbol_name.empty()) [[unlikely]] { return nullptr; }

    auto& llvm_context{ir_builder.getContext()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto object_pointer{get_llvm_external_host_object_pointer(ir_builder, host_address, ::llvm::Type::getInt8Ty(llvm_context), symbol_name)};
    if(object_pointer == nullptr) [[unlikely]] { return nullptr; }
    return ir_builder.CreatePtrToInt(object_pointer, llvm_intptr_type, get_llvm_string_ref(u8"uwvm.host.object.addr"));
}

// Allocate stack storage in the function entry block, regardless of the caller's current insertion point.  LLVM's mem2reg
// and lifetime reasoning work best when all allocas are anchored at the entry.
[[nodiscard]] inline constexpr ::llvm::AllocaInst* create_llvm_jit_entry_block_alloca(::llvm::IRBuilder<>& ir_builder,
                                                                                      ::llvm::Type* allocated_type,
                                                                                      ::llvm::Value* array_size,
                                                                                      ::llvm::StringRef name) noexcept
{
    if(allocated_type == nullptr) [[unlikely]] { return nullptr; }

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr) [[unlikely]] { return nullptr; }

    auto current_function{current_block->getParent()};
    if(current_function == nullptr || current_function->empty()) [[unlikely]] { return nullptr; }

    auto& entry_block{current_function->getEntryBlock()};
    // Use a short-lived builder so the caller's insertion point remains exactly where expression emission expects it.
    ::llvm::IRBuilder<> entry_builder{&entry_block, entry_block.getFirstInsertionPt()};
    return entry_builder.CreateAlloca(allocated_type, array_size, name);
}

// Host runtime bridge calls must use the platform C ABI.  Windows x64 requires the explicit Win64 convention, while most
// other supported targets can use LLVM's generic C calling convention.
[[nodiscard]] inline constexpr ::llvm::CallingConv::ID get_llvm_jit_host_calling_conv() noexcept
{
#if defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__)
    return ::llvm::CallingConv::Win64;
#else
    return ::llvm::CallingConv::C;
#endif
}

// Internal Wasm-to-Wasm JIT calls can use a private ABI when the platform offers a faster convention.  Keep this policy
// in lockstep with uwvm_int/macro/push_macros.h: Windows x86_64 uses SysV only when the C++ compiler can spell the same
// attributed function pointer, and every i686 target uses fastcall.  A generated LLVM calling convention without the
// matching C++ pointer attribute is a silent mixed-ABI bug at the tiered/raw-entry boundary.
[[nodiscard]] inline constexpr ::llvm::CallingConv::ID get_llvm_jit_wasm_calling_conv() noexcept
{
#if defined(_WIN32) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                    \
    (defined(__GNUC__) || defined(__clang__))
    return ::llvm::CallingConv::X86_64_SysV;
#elif defined(__i386__) || defined(_M_IX86)
    return ::llvm::CallingConv::X86_FastCall;
#else
    return ::llvm::CallingConv::C;
#endif
}

// Apply target-specific function attributes required by the chosen calling convention.
inline constexpr void apply_llvm_jit_platform_function_attrs([[maybe_unused]] ::llvm::Function& function) noexcept
{
#if defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__)
    // Disable red-zone stack slots for generated Win64 code.  The Windows x64 ABI does not reserve a SysV-style area
    // below RSP across asynchronous events, and JIT traps may enter SEH/runtime helpers while the native frame is being
    // unwound; keeping all spills above the adjusted stack pointer prevents hidden temporaries from being clobbered.
    function.addFnAttr(::llvm::Attribute::NoRedZone);
#endif
}

// Apply semantic attributes that are independent of the native platform.
inline constexpr void apply_llvm_jit_semantic_function_attrs(::llvm::Function& function) noexcept
{
    // WebAssembly MVP `call` always creates an ordinary call boundary.  LLVM must not infer a sibling/tail call from
    // native target profitability, because Wasm tail-call semantics are represented by separate tail-call proposal opcodes.
    // LLVM exposes this as a string-valued semantic attribute rather than a stable enum attribute on the Function API.
    function.addFnAttr(get_llvm_string_ref(u8"disable-tail-calls"), get_llvm_string_ref(u8"true"));

    // LLVM execution enters with FE_DFL_ENV, and both scalar formats require gradual underflow. State this explicitly
    // so target lowering cannot inherit a flush-to-zero denormal policy from ambient toolchain defaults.
    function.addFnAttr(get_llvm_string_ref(u8"denormal-fp-math"), get_llvm_string_ref(u8"ieee,ieee"));
    function.addFnAttr(get_llvm_string_ref(u8"denormal-fp-math-f32"), get_llvm_string_ref(u8"ieee,ieee"));
}

// Keep a physical frame pointer in functions that may need to report an exact trap call-site frame.
inline constexpr void apply_llvm_jit_frame_pointer_function_attrs(::llvm::Function& function) noexcept
{
    function.addFnAttr(get_llvm_string_ref(u8"frame-pointer"), get_llvm_string_ref(u8"all"));
    function.addFnAttr(get_llvm_string_ref(u8"no-frame-pointer-elim"), get_llvm_string_ref(u8"true"));
    function.addFnAttr(get_llvm_string_ref(u8"no-frame-pointer-elim-non-leaf"));
}

// Apply all common attributes shared by public, private, and raw-entry JIT functions.
inline constexpr void apply_llvm_jit_common_function_attrs(::llvm::Function& function) noexcept
{
    apply_llvm_jit_platform_function_attrs(function);
    apply_llvm_jit_semantic_function_attrs(function);

    // Keep every generated Wasm entry, tiered core, OSR entry, and raw wrapper as a distinct native function in every
    // optimization policy, including max/O3. Function-local optimization remains enabled, but LLVM may not merge one
    // Wasm call boundary into another or invalidate the address ranges published by full/lazy/tiered runtimes.
    function.addFnAttr(::llvm::Attribute::NoInline);
    function.addFnAttr(::llvm::Attribute::NoMerge);
    // LLVM 23 models nooutline as a string function attribute; both IROutliner and MachineOutliner query this spelling.
    function.addFnAttr(get_llvm_string_ref(u8"nooutline"));
#if defined(_WIN64) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__) &&                                                               \
    (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64))
    // Win64 trap bridges always pass explicit generated-frame context via read_register.  LLVM rejects reading the
    // architectural frame pointer from a function where that register remains allocatable, so every generated caller
    // needs a fixed frame pointer.
    apply_llvm_jit_frame_pointer_function_attrs(function);
#endif
}

// Preserve native unwind metadata for auxiliary POSIX diagnostics and explicit Win64 SEH reconstruction.
inline constexpr void apply_llvm_jit_unwind_call_stack_function_attrs(::llvm::Function& function) noexcept
{
    // Emit asynchronous unwind tables, not only call-site unwind info.  Wasm traps can be reported from arbitrary
    // instruction PCs after bounds checks, helper calls, or target signals/SEH faults, so the runtime unwinder needs CFI
    // that remains valid between calls when reconstructing optimized JIT frames.  On Win64 this causes LLVM to emit
    // .pdata/.xdata records; on DWARF targets it keeps CFI in .eh_frame only for an auxiliary ordinary walk. POSIX CFI
    // never replaces the instruction-emitted logical Wasm stack.
    function.setUWTableKind(::llvm::UWTableKind::Async);
}

#if defined(__i386__) || defined(_M_IX86)
[[nodiscard]] inline constexpr bool llvm_jit_i386_fastcall_inreg_eligible(::llvm::Type* type) noexcept
{
    if(type == nullptr) [[unlikely]] { return false; }
    if(type->isPointerTy()) { return true; }
    return type->isIntegerTy() && type->getIntegerBitWidth() <= 32u;
}

inline constexpr void apply_llvm_jit_i386_fastcall_param_attrs(::llvm::Function& function) noexcept
{
    auto function_type{function.getFunctionType()};
    if(function_type == nullptr) [[unlikely]] { return; }

    unsigned inreg_count{};
    auto const arg_count{function.arg_size()};
    for(unsigned arg_index{}; arg_index != arg_count && inreg_count != 2u; ++arg_index)
    {
        if(!llvm_jit_i386_fastcall_inreg_eligible(function_type->getParamType(arg_index))) { continue; }
        function.addParamAttr(arg_index, ::llvm::Attribute::InReg);
        ++inreg_count;
    }
}

inline constexpr void apply_llvm_jit_i386_fastcall_param_attrs(::llvm::CallInst& call_inst) noexcept
{
    unsigned inreg_count{};
    auto const arg_count{call_inst.arg_size()};
    for(unsigned arg_index{}; arg_index != arg_count && inreg_count != 2u; ++arg_index)
    {
        auto arg{call_inst.getArgOperand(arg_index)};
        if(arg == nullptr || !llvm_jit_i386_fastcall_inreg_eligible(arg->getType())) { continue; }
        call_inst.addParamAttr(arg_index, ::llvm::Attribute::InReg);
        ++inreg_count;
    }
}
#endif

// Set the calling convention on an LLVM function and attach the common JIT attributes expected for generated functions.
inline constexpr void apply_llvm_jit_calling_conv(::llvm::Function& function, ::llvm::CallingConv::ID calling_conv) noexcept
{
    function.setCallingConv(calling_conv);
#if defined(__i386__) || defined(_M_IX86)
    if(calling_conv == ::llvm::CallingConv::X86_FastCall)
    {
        // Clang lowers i386 __fastcall as x86_fastcallcc with the first two integer arguments marked inreg.  MCJIT's ELF
        // i386 lowering follows those parameter attributes; setting only the calling convention leaves the generated entry
        // callable as cdecl and corrupts C++ fastcall callers at the raw-entry boundary.
        apply_llvm_jit_i386_fastcall_param_attrs(function);
    }
#endif
    apply_llvm_jit_common_function_attrs(function);
}

// Set the calling convention on a call site.  Call sites must match the declaration or LLVM may emit an ABI-incompatible
// native call.
inline constexpr void apply_llvm_jit_calling_conv(::llvm::CallInst& call_inst, ::llvm::CallingConv::ID calling_conv) noexcept
{
    call_inst.setCallingConv(calling_conv);
#if defined(__i386__) || defined(_M_IX86)
    if(calling_conv == ::llvm::CallingConv::X86_FastCall) { apply_llvm_jit_i386_fastcall_param_attrs(call_inst); }
#endif
}

// Null-safe call-site convention helper used by expression-style call builders.
inline constexpr ::llvm::CallInst* apply_llvm_jit_calling_conv(::llvm::CallInst* call_inst, ::llvm::CallingConv::ID calling_conv) noexcept
{
    if(call_inst != nullptr) { apply_llvm_jit_calling_conv(*call_inst, calling_conv); }
    return call_inst;
}

// Mark a generated function as callable from the host/runtime bridge ABI.
inline constexpr void apply_llvm_jit_host_calling_conv(::llvm::Function& function) noexcept
{ apply_llvm_jit_calling_conv(function, get_llvm_jit_host_calling_conv()); }

// Mark a generated call site as crossing the host/runtime bridge ABI.
inline constexpr ::llvm::CallInst* apply_llvm_jit_host_calling_conv(::llvm::CallInst* call_inst) noexcept
{ return apply_llvm_jit_calling_conv(call_inst, get_llvm_jit_host_calling_conv()); }

// Raw Wasm entry targets are called both from generated code and from the C++ tiered runtime.  They deliberately share
// the private Wasm ABI above, so update the runtime-side entry pointer attributes whenever this convention changes.
[[nodiscard]] inline constexpr ::llvm::CallingConv::ID get_llvm_jit_raw_entry_calling_conv() noexcept { return get_llvm_jit_wasm_calling_conv(); }

// Mark a generated function as callable through the runtime raw-buffer ABI.
inline constexpr void apply_llvm_jit_raw_entry_calling_conv(::llvm::Function& function) noexcept
{ apply_llvm_jit_calling_conv(function, get_llvm_jit_raw_entry_calling_conv()); }

// Mark a generated raw-entry call site.
inline constexpr ::llvm::CallInst* apply_llvm_jit_raw_entry_calling_conv(::llvm::CallInst* call_inst) noexcept
{
    auto ret{apply_llvm_jit_calling_conv(call_inst, get_llvm_jit_raw_entry_calling_conv())};
    if(ret != nullptr) { ret->setTailCallKind(::llvm::CallInst::TCK_NoTail); }
    return ret;
}

// Mark a generated function as callable through the private Wasm-to-Wasm ABI.
inline constexpr void apply_llvm_jit_wasm_calling_conv(::llvm::Function& function) noexcept
{ apply_llvm_jit_calling_conv(function, get_llvm_jit_wasm_calling_conv()); }

// Mark a generated call site as using the private Wasm-to-Wasm ABI.
inline constexpr ::llvm::CallInst* apply_llvm_jit_wasm_calling_conv(::llvm::CallInst* call_inst) noexcept
{
    auto ret{apply_llvm_jit_calling_conv(call_inst, get_llvm_jit_wasm_calling_conv())};
    if(ret != nullptr) { ret->setTailCallKind(::llvm::CallInst::TCK_NoTail); }
    return ret;
}

// Return the byte size used by the raw bridge ABI for one Wasm scalar value.  These sizes intentionally match the
// parser's Wasm scalar storage types, not any native C++ promotion rules.
[[nodiscard]] inline constexpr ::std::size_t get_runtime_wasm_value_type_abi_size(runtime_operand_stack_value_type value_type) noexcept
{
    switch(get_runtime_wasm_value_type_encoding(value_type))
    {
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i32)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32);
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i64)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64);
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f32)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32);
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f64)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64);
        }
        [[unlikely]] default:
        {
            using wasm1p1_value_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type;
            switch(static_cast<wasm1p1_value_type>(get_runtime_wasm_value_type_encoding(value_type)))
            {
                case wasm1p1_value_type::v128:
                {
                    return sizeof(::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128);
                }
                case wasm1p1_value_type::funcref:
                {
                    return sizeof(::uwvm2::object::global::wasm_funcref_t);
                }
                case wasm1p1_value_type::externref:
                {
                    return sizeof(::uwvm2::object::global::wasm_externref_t);
                }
                default:
                {
                    return 0uz;
                }
            }
        }
    }
}

[[nodiscard]] inline constexpr bool is_runtime_wasm_value_type_llvm_scalar(runtime_operand_stack_value_type value_type) noexcept
{
    switch(get_runtime_wasm_value_type_encoding(value_type))
    {
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i32)):
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i64)):
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f32)):
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f64)):
        {
            return true;
        }
        default:
        {
            return false;
        }
    }
}

// Values accepted in internal LLVM locals/operand stacks.
[[nodiscard]] inline constexpr bool is_runtime_wasm_value_type_llvm_storage_supported(
    runtime_operand_stack_value_type value_type) noexcept
{
    if(is_runtime_wasm_value_type_llvm_scalar(value_type)) { return true; }

    switch(get_runtime_wasm_value_type_encoding(value_type))
    {
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::funcref)):
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::externref)):
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::v128)):
            return true;
        default:
            return false;
    }
}

// The private LLVM Wasm-to-Wasm ABI carries v128 and references as opaque integer storage. Host/import boundaries continue
// to use the tightly packed raw buffer ABI, so no C++ aggregate or ownership ABI is exposed.
[[nodiscard]] inline constexpr bool is_runtime_wasm_value_type_llvm_typed_entry_abi_supported(
    runtime_operand_stack_value_type value_type) noexcept
{ return is_runtime_wasm_value_type_llvm_storage_supported(value_type); }

// Create the zero/null constant for a Wasm scalar type, used for local initialization and default reentry arguments.
[[nodiscard]] inline constexpr ::llvm::Constant* get_llvm_zero_constant_from_wasm_value_type(::llvm::LLVMContext& llvm_context,
                                                                                             runtime_operand_stack_value_type value_type) noexcept
{
    auto llvm_type{get_llvm_type_from_wasm_value_type(llvm_context, value_type)};
    if(llvm_type == nullptr) [[unlikely]] { return nullptr; }
    return ::llvm::Constant::getNullValue(llvm_type);
}

// Convert an LLVM i1 predicate into Wasm's i32 boolean representation.
[[nodiscard]] inline constexpr ::llvm::Value* coerce_llvm_bool_to_i32(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* bool_value) noexcept
{
    if(bool_value == nullptr) [[unlikely]] { return nullptr; }
    return ir_builder.CreateZExt(bool_value, ::llvm::Type::getInt32Ty(ir_builder.getContext()));
}

// Lookup and call an LLVM intrinsic, including overloaded intrinsics whose concrete types are supplied by the caller.
[[nodiscard]] inline constexpr ::llvm::Value* call_llvm_intrinsic(::llvm::Module& llvm_module,
                                                                  ::llvm::IRBuilder<>& ir_builder,
                                                                  ::llvm::Intrinsic::ID intrinsic_id,
                                                                  ::llvm::ArrayRef<::llvm::Type*> overloaded_types,
                                                                  ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
{
    auto llvm_intrinsic{::llvm::Intrinsic::getOrInsertDeclaration(::std::addressof(llvm_module), intrinsic_id, overloaded_types)};
    return ir_builder.CreateCall(llvm_intrinsic, arguments);
}

// WebAssembly `nearest` is IEEE roundToIntegralTiesToEven and must not observe the host floating-point rounding mode.
// `llvm.rint` assumes the default FP environment; `llvm.roundeven` encodes the required mode-independent semantics.
[[nodiscard]] inline consteval ::llvm::Intrinsic::ID get_llvm_wasm_nearest_intrinsic_id() noexcept
{ return ::llvm::Intrinsic::roundeven; }

// Return the bit width of an integer LLVM value.  The emitter only calls this after type-specific opcode validation.
[[nodiscard]] inline constexpr unsigned get_llvm_integer_bit_width(::llvm::Value* value) noexcept
{
    if(value == nullptr) [[unlikely]] { return 0u; }
    return ::llvm::cast<::llvm::IntegerType>(value->getType())->getBitWidth();
}

// Mask Wasm shift/rotate counts to the lane width.  WebAssembly defines shifts modulo the integer bit width, while LLVM
// shifts are undefined when the count is greater than or equal to that width.
[[nodiscard]] inline constexpr ::llvm::Value*
    emit_llvm_shift_count_mask(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* shift_count, unsigned num_bits) noexcept
{
    if(shift_count == nullptr || num_bits == 0u) [[unlikely]] { return nullptr; }
    auto bits_minus_one{::llvm::ConstantInt::get(shift_count->getType(), num_bits - 1u)};
    return ir_builder.CreateAnd(shift_count, bits_minus_one);
}

// Emit integer rotate-left with explicit count masking so LLVM never observes an out-of-range shift count.
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_rotl(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right) noexcept
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto const bit_width{get_llvm_integer_bit_width(left)};
    if(bit_width == 0u) [[unlikely]] { return nullptr; }

    auto masked_right{emit_llvm_shift_count_mask(ir_builder, right, bit_width)};
    if(masked_right == nullptr) [[unlikely]] { return nullptr; }

    auto bit_width_value{::llvm::ConstantInt::get(right->getType(), bit_width)};
    // A zero rotate count must not become a shift by the full bit width.  Mask the inverse count as well so both LLVM
    // shifts stay in range for every Wasm-defined rotate count.
    auto inverse_shift{emit_llvm_shift_count_mask(ir_builder, ir_builder.CreateSub(bit_width_value, masked_right), bit_width)};
    return ir_builder.CreateOr(ir_builder.CreateShl(left, masked_right), ir_builder.CreateLShr(left, inverse_shift));
}

// Emit integer rotate-right with explicit count masking so LLVM never observes an out-of-range shift count.
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_rotr(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right) noexcept
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto const bit_width{get_llvm_integer_bit_width(left)};
    if(bit_width == 0u) [[unlikely]] { return nullptr; }

    auto masked_right{emit_llvm_shift_count_mask(ir_builder, right, bit_width)};
    if(masked_right == nullptr) [[unlikely]] { return nullptr; }

    auto bit_width_value{::llvm::ConstantInt::get(right->getType(), bit_width)};
    // The inverse shift has the same out-of-range hazard as the primary count, especially for rotate-by-zero.
    auto inverse_shift{emit_llvm_shift_count_mask(ir_builder, ir_builder.CreateSub(bit_width_value, masked_right), bit_width)};
    return ir_builder.CreateOr(ir_builder.CreateLShr(left, masked_right), ir_builder.CreateShl(left, inverse_shift));
}

// Build an f32 constant from its exact IEEE-754 bit pattern.  This preserves NaN payloads from Wasm immediates.
[[nodiscard]] inline constexpr ::llvm::Constant* get_llvm_f32_constant_from_bits(::llvm::LLVMContext& llvm_context, ::std::uint_least32_t bits) noexcept
{
    return ::llvm::ConstantFP::get(::llvm::Type::getFloatTy(llvm_context),
                                   ::llvm::APFloat(::llvm::APFloat::IEEEsingle(), ::llvm::APInt(32u, static_cast<::std::uint64_t>(bits))));
}

// Build an f64 constant from its exact IEEE-754 bit pattern.  This preserves NaN payloads from Wasm immediates.
[[nodiscard]] inline constexpr ::llvm::Constant* get_llvm_f64_constant_from_bits(::llvm::LLVMContext& llvm_context, ::std::uint_least64_t bits) noexcept
{ return ::llvm::ConstantFP::get(::llvm::Type::getDoubleTy(llvm_context), ::llvm::APFloat(::llvm::APFloat::IEEEdouble(), ::llvm::APInt(64u, bits))); }

// Report whether generated trap calls must pass explicit frame/stack context for Win64 SEH unwind reconstruction.
[[nodiscard]] inline consteval bool llvm_jit_win64_seh_explicit_trap_context_enabled() noexcept
{
    // Windows unwind state is reconstructed from a CONTEXT record, not from a DWARF cursor.  When a generated Wasm
    // frame calls into the C++ trap helper, the helper's own frame is already a different ABI boundary, so the generated
    // caller must pass its live frame/stack pointer values explicitly.
#if defined(_WIN64) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__) &&                                                               \
    (defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(_M_ARM64))
    return true;
#else
    return false;
#endif
}

// Emit the generated function's current frame address only for the Win64 SEH bridge.
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_jit_current_frame_address(::llvm::IRBuilder<>& ir_builder,
                                                                                  ::llvm::IntegerType* llvm_intptr_type) noexcept
{
#if defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__)
    // Use read_register instead of llvm.frameaddress on Win64.  The SEH path later initializes RtlVirtualUnwind with the
    // architectural register values captured at the trap call site, and LLVM's generic frameaddress intrinsic can be
    // lowered in terms of the current function's abstract frame rather than the exact machine register value we need.
    auto& llvm_context{ir_builder.getContext()};
    auto const register_name{::llvm::MDString::get(llvm_context, get_llvm_string_ref(u8"rbp"))};
    auto const register_metadata{::llvm::MDNode::get(llvm_context, {register_name})};
    return ir_builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {llvm_intptr_type}, {::llvm::MetadataAsValue::get(llvm_context, register_metadata)});
#elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__)
    auto& llvm_context{ir_builder.getContext()};
    auto const register_name{::llvm::MDString::get(llvm_context, get_llvm_string_ref(u8"x29"))};
    auto const register_metadata{::llvm::MDNode::get(llvm_context, {register_name})};
    return ir_builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {llvm_intptr_type}, {::llvm::MetadataAsValue::get(llvm_context, register_metadata)});
#else
    // POSIX uses an ordinary <unwind.h> walk from the runtime helper and keeps logical Wasm frames authoritative.
    static_cast<void>(ir_builder);
    return ::llvm::ConstantInt::get(llvm_intptr_type, 0u);
#endif
}

// Emit the generated function's current stack pointer when the platform trap bridge consumes it; otherwise return zero.
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_jit_current_stack_pointer(::llvm::IRBuilder<>& ir_builder,
                                                                                  ::llvm::IntegerType* llvm_intptr_type) noexcept
{
#if defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__)
    // RSP cannot be derived from RBP reliably on Win64 because UNWIND_INFO may describe dynamic stack allocation,
    // prologue state, and frame-register offsets.  Capture the live stack pointer before crossing into the helper.
    auto& llvm_context{ir_builder.getContext()};
    auto const register_name{::llvm::MDString::get(llvm_context, get_llvm_string_ref(u8"rsp"))};
    auto const register_metadata{::llvm::MDNode::get(llvm_context, {register_name})};
    return ir_builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {llvm_intptr_type}, {::llvm::MetadataAsValue::get(llvm_context, register_metadata)});
#elif defined(_WIN64) && (defined(__aarch64__) || defined(_M_ARM64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)) && !defined(__CYGWIN__)
    auto& llvm_context{ir_builder.getContext()};
    auto const register_name{::llvm::MDString::get(llvm_context, get_llvm_string_ref(u8"sp"))};
    auto const register_metadata{::llvm::MDNode::get(llvm_context, {register_name})};
    return ir_builder.CreateIntrinsic(::llvm::Intrinsic::read_register, {llvm_intptr_type}, {::llvm::MetadataAsValue::get(llvm_context, register_metadata)});
#else
    static_cast<void>(ir_builder);
    return ::llvm::ConstantInt::get(llvm_intptr_type, 0u);
#endif
}

// Runtime trap bridge signature.  The frame/stack context slots are always present so every generated caller uses one
// stable bridge ABI; Win64 SEH consumes them for unwind reconstruction, while other runtimes may ignore them.
[[nodiscard]] inline constexpr ::llvm::FunctionType* get_llvm_runtime_trap_bridge_function_type(::llvm::LLVMContext& llvm_context) noexcept
{
    // Keep this ABI synchronized with llvm_jit_runtime_trap.  Even when the non-Win64 runtime ignores the explicit
    // context values, passing them keeps trap emission uniform and avoids target-dependent call-site rewrites.
    auto trap_kind_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::uwvm2::runtime::lib::llvm_jit_trap_kind) * 8u))};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    return ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {trap_kind_type, llvm_intptr_type, llvm_intptr_type}, false);
}

inline constexpr void emit_llvm_jit_memory_clobber(::llvm::IRBuilder<>& ir_builder) noexcept
{
    auto& llvm_context{ir_builder.getContext()};
    auto anchor_function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), false)};
    auto anchor{::llvm::InlineAsm::get(anchor_function_type, get_llvm_string_ref(u8""), get_llvm_string_ref(u8"~{memory}"), true)};
    auto anchor_call{ir_builder.CreateCall(anchor_function_type, anchor)};
    anchor_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
}

// Emit a call to the runtime trap handler and a compiler barrier.  The caller is responsible for placing the final
// unreachable instruction, allowing conditional-trap helpers to create the surrounding blocks.
inline constexpr void emit_llvm_runtime_trap(::llvm::IRBuilder<>& ir_builder, ::uwvm2::runtime::lib::llvm_jit_trap_kind trap_kind) noexcept
{
    auto& llvm_context{ir_builder.getContext()};
    auto function_type{get_llvm_runtime_trap_bridge_function_type(llvm_context)};
    if(function_type == nullptr) [[unlikely]] { return; }

    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<::uwvm2::runtime::lib::llvm_jit_runtime_trap>(ir_builder, function_type)};
    if(bridge_pointer == nullptr) [[unlikely]] { return; }

    auto trap_kind_type{function_type->getParamType(0u)};
    ::uwvm2::utils::container::vector<::llvm::Value*> call_arguments{};
    call_arguments.emplace_back(::llvm::ConstantInt::get(trap_kind_type, static_cast<::std::uint_least64_t>(trap_kind)));
    auto const llvm_intptr_param_type{::llvm::cast<::llvm::IntegerType>(function_type->getParamType(1u))};
    // These operands are semantically meaningful for Win64 SEH and harmless placeholders elsewhere.  They let the
    // runtime reconstruct the generated caller rather than starting the unwind from the C++ trap helper frame.
    call_arguments.emplace_back(emit_llvm_jit_current_frame_address(ir_builder, llvm_intptr_param_type));
    call_arguments.emplace_back(emit_llvm_jit_current_stack_pointer(ir_builder, llvm_intptr_param_type));
    auto trap_call{ir_builder.CreateCall(function_type, bridge_pointer, {call_arguments.data(), call_arguments.size()})};
    trap_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
    apply_llvm_jit_host_calling_conv(trap_call);

    // Keep the trap call observable to LLVM optimizers even when surrounding code looks unreachable or side-effect-free.
    emit_llvm_jit_memory_clobber(ir_builder);
}

// Detailed memory trap bridge signature.  The runtime receives the static offset, dynamic offset, overflow flag, memory
// length, and access size so diagnostics can report the exact failed access.
[[nodiscard]] inline constexpr ::llvm::FunctionType* get_llvm_memory_out_of_bounds_trap_bridge_function_type(::llvm::LLVMContext& llvm_context) noexcept
{
    // Match the C++ bridge ABI exactly instead of reusing one generic integer width:
    // - size_t/uintptr_t fields use the host pointer-sized integer so 32-bit and 64-bit hosts keep their native ABI.
    // - Wasm address/length diagnostics are fixed 64-bit values and must not be truncated on 32-bit hosts.
    // - The overflow/carry diagnostic is a uint_least32_t-sized flag, so the IR signature keeps it as i32.
    // - Frame/stack context slots are always present; non-Win64 runtimes may ignore them, but the bridge ABI remains
    //   architecture-stable for every generated caller.
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    return ::llvm::FunctionType::get(
        ::llvm::Type::getVoidTy(llvm_context),
        {llvm_intptr_type, llvm_i64_type, llvm_i64_type, llvm_i32_type, llvm_i64_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type},
        false);
}

// Emit a detailed out-of-bounds memory trap.  Null dynamic values are replaced with zero to keep trap emission robust in
// late fallback paths while preserving all known details.
inline constexpr void emit_llvm_memory_out_of_bounds_trap(::llvm::IRBuilder<>& ir_builder,
                                                          ::std::size_t memory_idx,
                                                          ::std::uint_least64_t memory_static_offset,
                                                          ::llvm::Value* memory_offset,
                                                          ::llvm::Value* offset_65_bit,
                                                          ::llvm::Value* memory_length,
                                                          ::std::size_t memory_type_size) noexcept
{
    auto& llvm_context{ir_builder.getContext()};
    auto function_type{get_llvm_memory_out_of_bounds_trap_bridge_function_type(llvm_context)};
    if(function_type == nullptr) [[unlikely]] { return; }

    // Reuse the exact parameter types from the bridge signature for constants and casts below.  That keeps the call
    // operands ABI-identical to `get_llvm_memory_out_of_bounds_trap_bridge_function_type` even if the bridge signature is
    // adjusted later, and avoids duplicating integer-width assumptions in the emission path.
    auto llvm_intptr_type{function_type->getParamType(0u)};
    auto llvm_i64_type{function_type->getParamType(1u)};
    auto llvm_i32_type{function_type->getParamType(3u)};
    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap>(ir_builder, function_type)};
    if(bridge_pointer == nullptr) [[unlikely]] { return; }

    auto memory_offset_arg{memory_offset == nullptr ? ::llvm::ConstantInt::get(llvm_i64_type, 0u)
                                                    : ir_builder.CreateIntCast(memory_offset, llvm_i64_type, false)};
    auto offset_65_bit_arg{offset_65_bit == nullptr ? ::llvm::ConstantInt::get(llvm_i32_type, 0u) : ir_builder.CreateZExtOrTrunc(offset_65_bit, llvm_i32_type)};
    auto memory_length_arg{memory_length == nullptr ? ::llvm::ConstantInt::get(llvm_i64_type, 0u)
                                                    : ir_builder.CreateIntCast(memory_length, llvm_i64_type, false)};

    ::uwvm2::utils::container::vector<::llvm::Value*> call_arguments{};
    call_arguments.emplace_back(::llvm::ConstantInt::get(llvm_intptr_type, memory_idx));
    call_arguments.emplace_back(::llvm::ConstantInt::get(llvm_i64_type, memory_static_offset));
    call_arguments.emplace_back(memory_offset_arg);
    call_arguments.emplace_back(offset_65_bit_arg);
    call_arguments.emplace_back(memory_length_arg);
    call_arguments.emplace_back(::llvm::ConstantInt::get(llvm_intptr_type, memory_type_size));
    auto const llvm_intptr_param_type{::llvm::cast<::llvm::IntegerType>(llvm_intptr_type)};
    call_arguments.emplace_back(emit_llvm_jit_current_frame_address(ir_builder, llvm_intptr_param_type));
    call_arguments.emplace_back(emit_llvm_jit_current_stack_pointer(ir_builder, llvm_intptr_param_type));
    auto trap_call{ir_builder.CreateCall(function_type, bridge_pointer, {call_arguments.data(), call_arguments.size()})};
    trap_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
    apply_llvm_jit_host_calling_conv(trap_call);

    // Same optimizer anchor used by generic traps; the runtime call must remain visible as a side effect.
    emit_llvm_jit_memory_clobber(ir_builder);
}

// Split the current block on a boolean condition and emit a generic runtime trap on the true edge.
inline constexpr void emit_llvm_conditional_trap(::llvm::Module&,
                                                 ::llvm::IRBuilder<>& ir_builder,
                                                 ::llvm::Value* condition,
                                                 ::uwvm2::runtime::lib::llvm_jit_trap_kind trap_kind) noexcept
{
    if(condition == nullptr) [[unlikely]] { return; }

    auto current_block{ir_builder.GetInsertBlock()};
    auto function{current_block == nullptr ? nullptr : current_block->getParent()};
    if(function == nullptr) [[unlikely]] { return; }

    auto& llvm_context{ir_builder.getContext()};
    auto trap_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"wasmTrap"), function)};
    auto continue_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"wasmTrapCont"), function)};

    // Emit an explicit diamond instead of relying on a select-like helper: traps are control effects and must dominate the
    // dangerous LLVM instruction that follows on the continue edge.
    ir_builder.CreateCondBr(condition, trap_block, continue_block);

    ir_builder.SetInsertPoint(trap_block);
    emit_llvm_runtime_trap(ir_builder, trap_kind);
    ir_builder.CreateUnreachable();

    ir_builder.SetInsertPoint(continue_block);
}

// Convenience overload for internal invariant failures.
inline constexpr void emit_llvm_conditional_trap(::llvm::Module& llvm_module, ::llvm::IRBuilder<>& ir_builder, ::llvm::Value* condition) noexcept
{ emit_llvm_conditional_trap(llvm_module, ir_builder, condition, ::uwvm2::runtime::lib::llvm_jit_trap_kind::runtime_invariant_failure); }

// Split the current block on a boolean condition and emit the detailed memory out-of-bounds trap on the true edge.
inline constexpr void emit_llvm_conditional_memory_out_of_bounds_trap(::llvm::Module&,
                                                                      ::llvm::IRBuilder<>& ir_builder,
                                                                      ::llvm::Value* condition,
                                                                      ::std::size_t memory_idx,
                                                                      ::std::uint_least64_t memory_static_offset,
                                                                      ::llvm::Value* memory_offset,
                                                                      ::llvm::Value* offset_65_bit,
                                                                      ::llvm::Value* memory_length,
                                                                      ::std::size_t memory_type_size) noexcept
{
    if(condition == nullptr) [[unlikely]] { return; }

    auto current_block{ir_builder.GetInsertBlock()};
    auto function{current_block == nullptr ? nullptr : current_block->getParent()};
    if(function == nullptr) [[unlikely]] { return; }

    auto& llvm_context{ir_builder.getContext()};
    auto trap_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"memory.oob.trap"), function)};
    auto continue_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"memory.oob.cont"), function)};

    // Keep the detailed trap on its own block so all diagnostic operands are evaluated before the edge becomes
    // unreachable, while normal memory code continues in a clean successor block.
    ir_builder.CreateCondBr(condition, trap_block, continue_block);

    ir_builder.SetInsertPoint(trap_block);
    emit_llvm_memory_out_of_bounds_trap(ir_builder, memory_idx, memory_static_offset, memory_offset, offset_65_bit, memory_length, memory_type_size);
    ir_builder.CreateUnreachable();

    ir_builder.SetInsertPoint(continue_block);
}

// Emit Wasm's integer divide-by-zero trap before a division or remainder operation.
inline constexpr void emit_llvm_divide_by_zero_trap(::llvm::Module& llvm_module, ::llvm::IRBuilder<>& ir_builder, ::llvm::Value* divisor) noexcept
{
    if(divisor == nullptr) [[unlikely]] { return; }
    emit_llvm_conditional_trap(llvm_module,
                               ir_builder,
                               ir_builder.CreateICmpEQ(divisor, ::llvm::ConstantInt::get(divisor->getType(), 0u)),
                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::integer_divide_by_zero);
}

// Emit both signed division traps required by Wasm: divide-by-zero and signed-min divided by -1 overflow.
inline constexpr void
    emit_llvm_signed_div_overflow_trap(::llvm::Module& llvm_module, ::llvm::IRBuilder<>& ir_builder, ::llvm::Value* dividend, ::llvm::Value* divisor) noexcept
{
    if(dividend == nullptr || divisor == nullptr) [[unlikely]] { return; }

    emit_llvm_divide_by_zero_trap(llvm_module, ir_builder, divisor);

    auto const bit_width{get_llvm_integer_bit_width(dividend)};
    auto int_type{::llvm::cast<::llvm::IntegerType>(dividend->getType())};
    auto signed_min{::llvm::ConstantInt::get(int_type, ::llvm::APInt::getSignedMinValue(bit_width))};
    auto neg_one{::llvm::ConstantInt::getSigned(int_type, -1)};
    auto signed_overflow{ir_builder.CreateAnd(ir_builder.CreateICmpEQ(dividend, signed_min), ir_builder.CreateICmpEQ(divisor, neg_one))};
    emit_llvm_conditional_trap(llvm_module, ir_builder, signed_overflow, ::uwvm2::runtime::lib::llvm_jit_trap_kind::integer_overflow);
}

// Emit signed remainder with Wasm semantics.  LLVM `srem signed_min, -1` is poison/undefined, while Wasm defines the
// remainder result as zero after the divide-by-zero check, so this builds an explicit control-flow diamond.
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_signed_remainder_with_wasm_semantics(::llvm::Module& llvm_module,
                                                                                             ::llvm::IRBuilder<>& ir_builder,
                                                                                             ::llvm::Value* dividend,
                                                                                             ::llvm::Value* divisor) noexcept
{
    if(dividend == nullptr || divisor == nullptr) [[unlikely]] { return nullptr; }

    emit_llvm_divide_by_zero_trap(llvm_module, ir_builder, divisor);

    auto const bit_width{get_llvm_integer_bit_width(dividend)};
    auto int_type{::llvm::cast<::llvm::IntegerType>(dividend->getType())};
    auto signed_min{::llvm::ConstantInt::get(int_type, ::llvm::APInt::getSignedMinValue(bit_width))};
    auto neg_one{::llvm::ConstantInt::getSigned(int_type, -1)};

    auto pre_overflow_block{ir_builder.GetInsertBlock()};
    auto function{pre_overflow_block == nullptr ? nullptr : pre_overflow_block->getParent()};
    if(function == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{ir_builder.getContext()};
    auto no_overflow_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"wasmSRemNoOverflow"), function)};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"wasmSRemEnd"), function)};
    auto no_overflow{ir_builder.CreateOr(ir_builder.CreateICmpNE(dividend, signed_min), ir_builder.CreateICmpNE(divisor, neg_one))};
    ir_builder.CreateCondBr(no_overflow, no_overflow_block, end_block);

    ir_builder.SetInsertPoint(no_overflow_block);
    auto no_overflow_value{ir_builder.CreateSRem(dividend, divisor)};
    ir_builder.CreateBr(end_block);

    ir_builder.SetInsertPoint(end_block);
    auto phi{ir_builder.CreatePHI(int_type, 2u)};
    // The overflow edge contributes the Wasm-defined zero result without executing LLVM `srem` on the poison-producing
    // signed-min / -1 pair.
    phi->addIncoming(::llvm::ConstantInt::get(int_type, 0u), pre_overflow_block);
    phi->addIncoming(no_overflow_value, no_overflow_block);
    return phi;
}

// Convert a signaling NaN to a quiet NaN while preserving the payload bits used by Wasm min/max propagation rules.
[[nodiscard]] inline constexpr ::llvm::Value* quiet_llvm_nan(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* nan) noexcept
{
    if(nan == nullptr) [[unlikely]] { return nullptr; }

    if(nan->getType()->isFloatTy())
    {
        // IEEE-754 quiet bit for binary32 significands.
        auto int_value{ir_builder.CreateBitCast(nan, ::llvm::Type::getInt32Ty(ir_builder.getContext()))};
        auto quiet_mask{::llvm::ConstantInt::get(int_value->getType(), 0x00400000u)};
        return ir_builder.CreateBitCast(ir_builder.CreateOr(int_value, quiet_mask), nan->getType());
    }

    if(nan->getType()->isDoubleTy())
    {
        // IEEE-754 quiet bit for binary64 significands.
        auto int_value{ir_builder.CreateBitCast(nan, ::llvm::Type::getInt64Ty(ir_builder.getContext()))};
        auto quiet_mask{::llvm::ConstantInt::get(int_value->getType(), 0x0008000000000000ull)};
        return ir_builder.CreateBitCast(ir_builder.CreateOr(int_value, quiet_mask), nan->getType());
    }
    return nullptr;
}

// Emit Wasm floating-point min.  This differs from many native/library min operations because NaNs are quieted and the
// signed-zero tie is resolved by OR-ing the bit patterns, which preserves -0.0 for min(+0.0, -0.0).
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_float_min(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right) noexcept
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto int_type{left->getType()->isFloatTy() ? ::llvm::Type::getInt32Ty(ir_builder.getContext()) : ::llvm::Type::getInt64Ty(ir_builder.getContext())};
    auto is_left_nan{ir_builder.CreateFCmpUNO(left, left)};
    auto is_right_nan{ir_builder.CreateFCmpUNO(right, right)};
    auto is_left_less_than_right{ir_builder.CreateFCmpOLT(left, right)};
    auto is_left_greater_than_right{ir_builder.CreateFCmpOGT(left, right)};

    // Ordered comparisons handle normal unequal operands.  The final bitwise tie case is reached for equal operands,
    // including signed zeros, where Wasm requires a deterministic zero sign.
    return ir_builder.CreateSelect(
        is_left_nan,
        quiet_llvm_nan(ir_builder, left),
        ir_builder.CreateSelect(
            is_right_nan,
            quiet_llvm_nan(ir_builder, right),
            ir_builder.CreateSelect(is_left_less_than_right,
                                    left,
                                    ir_builder.CreateSelect(is_left_greater_than_right,
                                                            right,
                                                            ir_builder.CreateBitCast(ir_builder.CreateOr(ir_builder.CreateBitCast(left, int_type),
                                                                                                         ir_builder.CreateBitCast(right, int_type)),
                                                                                     left->getType())))));
}

// Emit Wasm floating-point max.  This mirrors `emit_llvm_float_min` but resolves signed-zero ties by AND-ing the bit
// patterns, which preserves +0.0 for max(+0.0, -0.0).
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_float_max(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right) noexcept
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto int_type{left->getType()->isFloatTy() ? ::llvm::Type::getInt32Ty(ir_builder.getContext()) : ::llvm::Type::getInt64Ty(ir_builder.getContext())};
    auto is_left_nan{ir_builder.CreateFCmpUNO(left, left)};
    auto is_right_nan{ir_builder.CreateFCmpUNO(right, right)};
    auto is_left_less_than_right{ir_builder.CreateFCmpOLT(left, right)};
    auto is_left_greater_than_right{ir_builder.CreateFCmpOGT(left, right)};

    // The NaN arms run before ordered comparisons because FCmpO* predicates are false for NaNs; the final bitwise tie
    // rule is what distinguishes Wasm max from many host `fmax` implementations.
    return ir_builder.CreateSelect(
        is_left_nan,
        quiet_llvm_nan(ir_builder, left),
        ir_builder.CreateSelect(
            is_right_nan,
            quiet_llvm_nan(ir_builder, right),
            ir_builder.CreateSelect(is_left_less_than_right,
                                    right,
                                    ir_builder.CreateSelect(is_left_greater_than_right,
                                                            left,
                                                            ir_builder.CreateBitCast(ir_builder.CreateAnd(ir_builder.CreateBitCast(left, int_type),
                                                                                                          ir_builder.CreateBitCast(right, int_type)),
                                                                                     left->getType())))));
}

// Emit a trapping float-to-int conversion.  LLVM's conversion instructions are undefined for NaN/out-of-range inputs, so
// the Wasm traps are emitted before the final FPToSI/FPToUI instruction.
template <typename Float>
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_trunc_float_to_int(::llvm::Module& llvm_module,
                                                                           ::llvm::IRBuilder<>& ir_builder,
                                                                           ::llvm::Type* dest_type,
                                                                           bool is_signed,
                                                                           Float min_bounds,
                                                                           Float max_bounds,
                                                                           ::llvm::Value* operand) noexcept
{
    if(dest_type == nullptr || operand == nullptr) [[unlikely]] { return nullptr; }

    auto is_nan{ir_builder.CreateFCmpUNO(operand, operand)};
    emit_llvm_conditional_trap(llvm_module, ir_builder, is_nan, ::uwvm2::runtime::lib::llvm_jit_trap_kind::invalid_conversion_to_integer);

    auto min_bound{::llvm::ConstantFP::get(operand->getType(), static_cast<double>(min_bounds))};
    auto max_bound{::llvm::ConstantFP::get(operand->getType(), static_cast<double>(max_bounds))};
    auto is_overflow{ir_builder.CreateOr(ir_builder.CreateFCmpOGE(operand, max_bound), ir_builder.CreateFCmpOLE(operand, min_bound))};
    emit_llvm_conditional_trap(llvm_module, ir_builder, is_overflow, ::uwvm2::runtime::lib::llvm_jit_trap_kind::integer_overflow);

    return is_signed ? ir_builder.CreateFPToSI(operand, dest_type) : ir_builder.CreateFPToUI(operand, dest_type);
}

// Emit a non-trapping saturating float-to-int conversion.  Clamp the operand before FPToSI/FPToUI so LLVM never observes
// a NaN or out-of-range value on the conversion instruction.
template <typename Float>
[[nodiscard]] inline constexpr ::llvm::Value* emit_llvm_trunc_sat_float_to_int(::llvm::IRBuilder<>& ir_builder,
                                                                               ::llvm::Type* dest_type,
                                                                               bool is_signed,
                                                                               Float min_bounds,
                                                                               Float max_bounds,
                                                                               ::std::uint_least64_t min_result,
                                                                               ::std::uint_least64_t max_result,
                                                                               ::llvm::Value* operand) noexcept
{
    if(dest_type == nullptr || operand == nullptr) [[unlikely]] { return nullptr; }

    auto fp_zero{::llvm::ConstantFP::get(operand->getType(), 0.0)};
    auto int_zero{::llvm::ConstantInt::get(dest_type, 0u)};
    auto int_min{::llvm::ConstantInt::get(dest_type, min_result)};
    auto int_max{::llvm::ConstantInt::get(dest_type, max_result)};
    auto min_bound{::llvm::ConstantFP::get(operand->getType(), static_cast<double>(min_bounds))};
    auto max_bound{::llvm::ConstantFP::get(operand->getType(), static_cast<double>(max_bounds))};

    auto is_nan{ir_builder.CreateFCmpUNO(operand, operand)};
    auto is_underflow{ir_builder.CreateFCmpOLE(operand, min_bound)};
    auto is_overflow{ir_builder.CreateFCmpOGE(operand, max_bound)};
    auto is_not_convertible{ir_builder.CreateOr(is_nan, ir_builder.CreateOr(is_underflow, is_overflow))};
    auto safe_operand{ir_builder.CreateSelect(is_not_convertible, fp_zero, operand)};
    auto converted{is_signed ? ir_builder.CreateFPToSI(safe_operand, dest_type) : ir_builder.CreateFPToUI(safe_operand, dest_type)};

    return ir_builder.CreateSelect(is_nan,
                                   int_zero,
                                   ir_builder.CreateSelect(is_underflow,
                                                           int_min,
                                                           ir_builder.CreateSelect(is_overflow, int_max, converted)));
}

// Resolve the declared Wasm function type for any module function index, including imports.  This is a type lookup only;
// it does not prove that an imported function can be called directly by generated LLVM code.
[[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const*
    resolve_runtime_callee_function_type(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                         validation_module_traits_t::wasm_u32 func_index) noexcept
{
    auto const import_func_count{runtime_module.imported_function_vec_storage.size()};
    auto const local_func_count{runtime_module.local_defined_function_vec_storage.size()};
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};

    if(func_index_uz < import_func_count)
    {
        // Imported function records carry their declared type even when their eventual target is a host bridge or another
        // module.  That declaration is the type the Wasm caller was validated against.
        auto const& imported_rec{runtime_module.imported_function_vec_storage.index_unchecked(func_index_uz)};
        auto import_type_ptr{imported_rec.import_type_ptr};
        if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::func) [[unlikely]] { return nullptr; }
        return import_type_ptr->imports.storage.function;
    }

    auto const local_func_index{func_index_uz - import_func_count};
    if(local_func_index >= local_func_count) [[unlikely]] { return nullptr; }

    return runtime_module.local_defined_function_vec_storage.index_unchecked(local_func_index).function_type_ptr;
}

// Result of following an import-link chain far enough to know whether the callee has a same-module typed entry.
struct runtime_direct_callee_resolution_t
{
    // False when runtime storage is internally inconsistent or the link chain looks cyclic/corrupt.
    bool state_valid{};

    // True when `func_index` identifies a local defined function that can be called through the private Wasm ABI.
    bool direct_callable{};

    // Resolved module function index for direct calls; meaningful only when `direct_callable` is true.
    validation_module_traits_t::wasm_u32 func_index{};

    // Best-known function type for the resolved target.  This may be present even when the target is not directly callable.
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr{};
};

// Runtime initialization rejects import-alias cycles and unresolved chains.  A post-initialization function alias chain
// can therefore visit at most one imported-function record per runtime import before reaching a concrete target.  Derive
// the defensive walk bound from runtime storage instead of using a fixed cap, so large but valid module graphs are not
// rejected by the JIT.
[[nodiscard]] inline constexpr ::std::size_t
    get_runtime_imported_function_link_walk_bound(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module) noexcept
{
    auto bound{runtime_module.imported_function_vec_storage.size()};
    for(auto const& module_entry: ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage)
    {
        auto const& other_module{module_entry.second};
        if(::std::addressof(other_module) == ::std::addressof(runtime_module)) { continue; }

        auto const imported_function_count{other_module.imported_function_vec_storage.size()};
        if(imported_function_count > ::std::numeric_limits<::std::size_t>::max() - bound) { return ::std::numeric_limits<::std::size_t>::max(); }
        bound += imported_function_count;
    }
    return bound;
}

// Follow imported-function forwarding records until the emitter can determine whether the target is a local defined
// function.  Non-local host/dynamic/weak targets are valid but not directly callable by typed LLVM IR.
[[nodiscard]] inline constexpr runtime_direct_callee_resolution_t
    resolve_runtime_direct_callee(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                  validation_module_traits_t::wasm_u32 func_index) noexcept
{
    using imported_function_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
    using function_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;

    runtime_direct_callee_resolution_t result{.state_valid = true};

    auto const import_func_count{runtime_module.imported_function_vec_storage.size()};
    auto const local_func_count{runtime_module.local_defined_function_vec_storage.size()};
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    auto local_func_begin{runtime_module.local_defined_function_vec_storage.data()};

    if(func_index_uz >= import_func_count)
    {
        // Local defined functions can be converted directly from module function index to local storage index; no import
        // forwarding chain is involved.
        auto const local_func_index{func_index_uz - import_func_count};
        if(local_func_index >= local_func_count) [[unlikely]] { return {}; }

        auto function_type_ptr{runtime_module.local_defined_function_vec_storage.index_unchecked(local_func_index).function_type_ptr};
        if(function_type_ptr == nullptr) [[unlikely]] { return {}; }

        result.direct_callable = true;
        result.func_index = func_index;
        result.function_type_ptr = function_type_ptr;
        return result;
    }

    imported_function_storage_t const* curr{::std::addressof(runtime_module.imported_function_vec_storage.index_unchecked(func_index_uz))};
    auto const max_link_walk_steps{get_runtime_imported_function_link_walk_bound(runtime_module)};
    for(::std::size_t steps{};; ++steps)
    {
        // Exceeding the storage-derived bound means the chain is no longer the acyclic structure the initializer proved.
        if(steps > max_link_walk_steps || curr == nullptr) [[unlikely]] { return {}; }

        switch(curr->link_kind)
        {
            case function_link_kind::imported:
            {
                curr = curr->target.imported_ptr;
                continue;
            }
            case function_link_kind::defined:
            {
                auto defined_func_ptr{curr->target.defined_ptr};
                if(defined_func_ptr == nullptr || defined_func_ptr->function_type_ptr == nullptr) [[unlikely]] { return {}; }

                result.function_type_ptr = defined_func_ptr->function_type_ptr;
                if(local_func_begin == nullptr || defined_func_ptr < local_func_begin || defined_func_ptr >= local_func_begin + local_func_count)
                {
                    // The target is a valid defined function, but not one stored in this module's local-function array.
                    // Keep the type so the caller can still use the raw ABI path safely.
                    return result;
                }

                // Pointer identity is the only reliable way to prove that an import-forwarded target is in this module;
                // convert the pointer back into the public module function index only after the range check above.
                auto const local_func_index{static_cast<::std::size_t>(defined_func_ptr - local_func_begin)};
                if(import_func_count > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) ||
                   local_func_index > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) - import_func_count)
                    [[unlikely]]
                {
                    return {};
                }

                result.direct_callable = true;
                result.func_index = static_cast<validation_module_traits_t::wasm_u32>(import_func_count + local_func_index);
                return result;
            }
            case function_link_kind::local_imported:
            case function_link_kind::unresolved:
            {
                return result;
            }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            case function_link_kind::dl:
            {
                return result;
            }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            case function_link_kind::weak_symbol:
            {
                return result;
            }
#endif
            [[unlikely]] default:
            {
                return {};
            }
        }
    }
}

// Unique symbol prefix for all LLVM IR objects derived from one runtime module.  Module names are globally unique after
// loading, so their stable hash keeps IR/object-cache identities independent from runtime storage addresses.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_runtime_module_symbol_prefix(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module) noexcept
{
    auto const module_name{runtime_module.module_name};
    if(!module_name.empty()) [[likely]]
    {
        auto const hash{::uwvm2::utils::hash::xxh3_64bits(reinterpret_cast<::std::byte const*>(module_name.cbegin()), module_name.size())};
        return ::uwvm2::utils::container::u8concat_uwvm(u8"uwvm_m_", ::fast_io::mnp::hex<false, true>(hash));
    }

    return ::uwvm2::utils::container::u8concat_uwvm(u8"uwvm_m_addr_", reinterpret_cast<::std::uintptr_t>(::std::addressof(runtime_module)));
}

// Public typed Wasm entry name.  This is the symbol used for direct JIT-to-JIT calls inside the same runtime module.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_wasm_function_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                validation_module_traits_t::wasm_u32 func_index) noexcept
{
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_func_", func_index_uz);
}

// Raw ABI wrapper name for a Wasm function.  Raw wrappers accept byte buffers and are used by host/import bridges.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_wasm_raw_function_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                    validation_module_traits_t::wasm_u32 func_index) noexcept
{
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_raw_func_", func_index_uz);
}

// Internal core function name used when tiered loop reentry support is enabled.  The public typed entry wraps this core
// function with normal-entry arguments, while OSR wrappers enter it at recorded loop IDs.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_wasm_tiered_core_function_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                            validation_module_traits_t::wasm_u32 func_index) noexcept
{
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_tiered_core_func_", func_index_uz);
}

// Raw OSR wrapper name for a tiered loop reentry point.  The Wasm byte offset is part of the symbol so a profiler/tiered
// compiler can target a specific hot loop.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_wasm_tiered_loop_reentry_raw_function_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                        validation_module_traits_t::wasm_u32 func_index,
                                                        ::std::size_t wasm_code_offset) noexcept
{
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module),
                                                    u8"_tiered_loop_raw_func_",
                                                    func_index_uz,
                                                    u8"_off_",
                                                    wasm_code_offset);
}

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_lazy_raw_target_table_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module) noexcept
{ return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_lazy_raw_targets"); }

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_lazy_typed_entry_target_table_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module) noexcept
{ return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_lazy_typed_entry_targets"); }

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_call_indirect_table_view_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module) noexcept
{ return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_call_indirect_table_views"); }

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_global_storage_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                        validation_module_traits_t::wasm_u32 global_index) noexcept
{
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module),
                                                    u8"_global_",
                                                    static_cast<::std::size_t>(global_index),
                                                    u8"_storage");
}

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_local_imported_global_module_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                      validation_module_traits_t::wasm_u32 global_index) noexcept
{
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module),
                                                    u8"_global_",
                                                    static_cast<::std::size_t>(global_index),
                                                    u8"_local_imported_module");
}

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_runtime_module_object_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module) noexcept
{ return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_runtime_module"); }

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_local_imported_memory_module_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                      validation_module_traits_t::wasm_u32 memory_index) noexcept
{
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module),
                                                    u8"_memory_",
                                                    static_cast<::std::size_t>(memory_index),
                                                    u8"_local_imported_module");
}

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_native_memory_object_symbol_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                              validation_module_traits_t::wasm_u32 memory_index) noexcept
{
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module),
                                                    u8"_memory_",
                                                    static_cast<::std::size_t>(memory_index),
                                                    u8"_native_memory");
}

// Per-function IR module name used by clients that compile one local function in isolation.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_wasm_function_ir_module_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                          validation_module_traits_t::wasm_u32 func_index) noexcept
{
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_ir_module_for_func_", func_index_uz);
}

// Whole-module IR module name used by full-module and tiered compilation paths.
[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string
    get_llvm_wasm_ir_module_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module) noexcept
{ return ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(runtime_module), u8"_ir_module"); }

// Forward declaration because function declarations need type conversion and type conversion is also used elsewhere.
[[nodiscard]] inline constexpr ::llvm::FunctionType*
    get_llvm_function_type_from_wasm_function_type(::llvm::LLVMContext& llvm_context,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept;

// Get or create the LLVM declaration for a typed Wasm function entry.  Existing declarations must be empty and have the
// exact same signature; otherwise the module would contain an ABI mismatch.
[[nodiscard]] inline constexpr ::llvm::Function*
    get_or_create_llvm_wasm_function_declaration(::llvm::Module& llvm_module,
                                                 ::llvm::LLVMContext& llvm_context,
                                                 ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                 validation_module_traits_t::wasm_u32 func_index,
                                                 ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    auto callee_function_type{get_llvm_function_type_from_wasm_function_type(llvm_context, wasm_function_type)};
    if(callee_function_type == nullptr) [[unlikely]] { return nullptr; }

    auto const callee_name{get_llvm_wasm_function_name(runtime_module, func_index)};
    auto callee_function{llvm_module.getFunction(get_llvm_string_ref(callee_name))};
    if(callee_function == nullptr)
    {
        callee_function =
            ::llvm::Function::Create(callee_function_type, ::llvm::Function::ExternalLinkage, get_llvm_string_ref(callee_name), ::std::addressof(llvm_module));
        apply_llvm_jit_wasm_calling_conv(*callee_function);
        return callee_function;
    }
    if(callee_function->getFunctionType() != callee_function_type) [[unlikely]] { return nullptr; }
    apply_llvm_jit_wasm_calling_conv(*callee_function);
    return callee_function;
}

// Convert a Wasm result range to the canonical typed LLVM result ABI: void for no results, the scalar itself for one
// result, and a Wasm-order literal struct for multiple results. Literal structs are uniqued by LLVMContext, so
// declarations, direct calls, lazy typed targets, and call_indirect all obtain the same structural ABI type.
[[nodiscard]] inline constexpr ::llvm::Type*
    get_llvm_result_type_from_wasm_result_range(::llvm::LLVMContext& llvm_context,
                                                runtime_operand_stack_value_type const* result_begin,
                                                runtime_operand_stack_value_type const* result_end) noexcept
{
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return nullptr; }

    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};
    if(result_count == 0uz) { return ::llvm::Type::getVoidTy(llvm_context); }

    if(result_count == 1uz) { return get_llvm_type_from_wasm_value_type(llvm_context, result_begin[0]); }
    if(result_count > static_cast<::std::size_t>((::std::numeric_limits<unsigned>::max)())) [[unlikely]] { return nullptr; }

    ::uwvm2::utils::container::vector<::llvm::Type*> llvm_result_types{};
    llvm_result_types.reserve(result_count);
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto llvm_result_type{get_llvm_type_from_wasm_value_type(llvm_context, result_begin[result_index])};
        if(llvm_result_type == nullptr) [[unlikely]] { return nullptr; }
        llvm_result_types.push_back(llvm_result_type);
    }

    return ::llvm::StructType::get(llvm_context, {llvm_result_types.data(), llvm_result_types.size()}, false);
}

// Convert a Wasm function type into a typed LLVM function signature. Multi-value results use the canonical struct result
// above, while parameters stay as independent LLVM operands in Wasm source order.
[[nodiscard]] inline constexpr ::llvm::FunctionType*
    get_llvm_function_type_from_wasm_function_type(::llvm::LLVMContext& llvm_context,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const parameter_end{wasm_function_type.parameter.end};
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};

    // Runtime type storage represents parameters/results as half-open pointer ranges.  A null begin pointer is valid only
    // for an empty range; any other null-backed range would make the type descriptor unusable for IR construction.
    if(parameter_begin == nullptr && parameter_begin != parameter_end) [[unlikely]] { return nullptr; }
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return nullptr; }

    // Pointer subtraction is performed only after the null/empty invariant above has been checked.
    auto const parameter_count{parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(parameter_end - parameter_begin)};

    ::uwvm2::utils::container::vector<::llvm::Type*> llvm_parameter_types{};
    llvm_parameter_types.reserve(parameter_count);

    // Preserve Wasm parameter order exactly.  Each Wasm value type must map to a concrete LLVM scalar type accepted by
    // this MVP emitter; a null mapping means the runtime type descriptor contains an unsupported or corrupted value kind.
    for(::std::size_t parameter_index{}; parameter_index != parameter_count; ++parameter_index)
    {
        auto llvm_parameter_type{
            get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
        if(llvm_parameter_type == nullptr) [[unlikely]] { return nullptr; }
        llvm_parameter_types.push_back(llvm_parameter_type);
    }

    auto llvm_result_type{get_llvm_result_type_from_wasm_result_range(llvm_context, result_begin, result_end)};
    if(llvm_result_type == nullptr) [[unlikely]] { return nullptr; }

    // Wasm function types have a fixed arity.  The final `false` explicitly disables LLVM varargs so verifier/type checks
    // catch any call-site arity mismatch instead of treating extra operands as native variadic arguments.
    return ::llvm::FunctionType::get(llvm_result_type, {llvm_parameter_types.data(), llvm_parameter_types.size()}, false);
}

// Runtime raw-call bridge signature.  Arguments are: module address, function index, result buffer address/size, and
// parameter buffer address/size.
[[nodiscard]] inline constexpr ::llvm::FunctionType* get_llvm_runtime_raw_call_bridge_function_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    return ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context),
                                     {llvm_intptr_type, llvm_i32_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type},
                                     false);
}

// Raw entry signature exported for compiled Wasm functions.  The first pointer-sized argument is an opaque context or
// target record supplied by the caller; the remaining arguments describe result and parameter byte buffers.
[[nodiscard]] inline constexpr ::llvm::FunctionType* get_llvm_runtime_raw_call_target_entry_function_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    return ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context),
                                     {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type},
                                     false);
}

// LLVM view of a lazy/raw target record: raw entry address, context address, canonical type id, and typed entry address.
// This must stay layout-compatible with the runtime storage used by lazy JIT target tables.
[[nodiscard]] inline constexpr ::llvm::StructType* get_llvm_runtime_raw_call_target_struct_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    return ::llvm::StructType::get(llvm_context, {llvm_intptr_type, llvm_intptr_type, llvm_i32_type, llvm_intptr_type}, false);
}

// LLVM view of a call_indirect table snapshot: target-record base address and current table size.
[[nodiscard]] inline constexpr ::llvm::StructType* get_llvm_runtime_call_indirect_table_view_struct_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    return ::llvm::StructType::get(llvm_context, {llvm_intptr_type, llvm_intptr_type}, false);
}

// Resolve a type-section function type by index.  `call_indirect` uses this to compare against table element type ids.
[[nodiscard]] inline constexpr ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const*
    resolve_runtime_type_section_function_type(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                               validation_module_traits_t::wasm_u32 type_index) noexcept
{
    auto type_begin{runtime_module.type_section_storage.type_section_begin};
    auto const type_count{get_runtime_type_section_count(runtime_module)};
    auto const type_index_uz{static_cast<::std::size_t>(type_index)};
    if(type_begin == nullptr || type_index_uz >= type_count) [[unlikely]] { return nullptr; }
    return type_begin + type_index_uz;
}

// Compute byte-buffer counts and sizes for a Wasm function type as used by the raw bridge ABI.
[[nodiscard]] inline constexpr llvm_jit_runtime_wasm_call_abi_layout_t
    get_runtime_wasm_call_abi_layout(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const parameter_end{wasm_function_type.parameter.end};
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};

    // Raw bridge layout uses the same pointer-range invariants as the typed LLVM signature conversion, but returns an
    // invalid layout object so callers can keep their own fallback/error policy.
    if(parameter_begin == nullptr && parameter_begin != parameter_end) [[unlikely]] { return {}; }
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return {}; }

    auto const parameter_count{parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(parameter_end - parameter_begin)};
    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};

    ::std::size_t parameter_bytes{};
    for(::std::size_t parameter_index{}; parameter_index != parameter_count; ++parameter_index)
    {
        // Check every scalar while accumulating byte counts.  A malformed value kind or size_t overflow would make the
        // raw byte-buffer ABI ambiguous, so the whole layout is rejected.
        auto const abi_size{get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
        if(abi_size == 0uz || parameter_bytes > ::std::numeric_limits<::std::size_t>::max() - abi_size) [[unlikely]] { return {}; }
        parameter_bytes += abi_size;
    }

    ::std::size_t result_bytes{};
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto const abi_size{get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(result_begin[result_index]))};
        if(abi_size == 0uz || result_bytes > ::std::numeric_limits<::std::size_t>::max() - abi_size) [[unlikely]] { return {}; }
        result_bytes += abi_size;
    }

    return llvm_jit_runtime_wasm_call_abi_layout_t{.valid = true,
                                                   .parameter_count = parameter_count,
                                                   .result_count = result_count,
                                                   .parameter_bytes = parameter_bytes,
                                                   .result_bytes = result_bytes};
}

[[nodiscard]] inline constexpr bool
    is_runtime_wasm_function_type_llvm_typed_entry_abi_supported(
        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const parameter_end{wasm_function_type.parameter.end};
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};

    if(parameter_begin == nullptr && parameter_begin != parameter_end) [[unlikely]] { return false; }
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return false; }

    auto const parameter_count{parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(parameter_end - parameter_begin)};
    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};

    for(::std::size_t parameter_index{}; parameter_index != parameter_count; ++parameter_index)
    {
        if(!is_runtime_wasm_value_type_llvm_typed_entry_abi_supported(
               static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index])))
        {
            return false;
        }
    }

    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        if(!is_runtime_wasm_value_type_llvm_typed_entry_abi_supported(
               static_cast<runtime_operand_stack_value_type>(result_begin[result_index])))
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] inline constexpr llvm_jit_runtime_wasm_call_abi_layout_t
    get_runtime_wasm_raw_buffer_abi_layout(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const parameter_end{wasm_function_type.parameter.end};
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};

    if(parameter_begin == nullptr && parameter_begin != parameter_end) [[unlikely]] { return {}; }
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return {}; }

    auto const parameter_count{parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(parameter_end - parameter_begin)};
    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};

    ::std::size_t parameter_bytes{};
    for(::std::size_t parameter_index{}; parameter_index != parameter_count; ++parameter_index)
    {
        auto const abi_size{get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
        if(abi_size == 0uz || parameter_bytes > ::std::numeric_limits<::std::size_t>::max() - abi_size) [[unlikely]] { return {}; }
        parameter_bytes += abi_size;
    }

    ::std::size_t result_bytes{};
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto const abi_size{get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(result_begin[result_index]))};
        if(abi_size == 0uz || result_bytes > ::std::numeric_limits<::std::size_t>::max() - abi_size) [[unlikely]] { return {}; }
        result_bytes += abi_size;
    }

    return llvm_jit_runtime_wasm_call_abi_layout_t{.valid = true,
                                                   .parameter_count = parameter_count,
                                                   .result_count = result_count,
                                                   .parameter_bytes = parameter_bytes,
                                                   .result_bytes = result_bytes};
}

// Materialize raw-call parameter and result buffers in the current LLVM function. Parameters and results are tightly
// packed in Wasm order with no native struct padding, matching the runtime raw-entry ABI for every result arity.
[[nodiscard]] inline constexpr llvm_jit_runtime_raw_call_buffers_t
    emit_runtime_raw_call_buffers(::llvm::IRBuilder<>& ir_builder,
                                  ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                  ::llvm::ArrayRef<::llvm::Value*> call_arguments,
                                  ::llvm::StringRef param_buffer_name,
                                  ::llvm::StringRef result_buffer_name) noexcept
{
    auto const abi_layout{get_runtime_wasm_call_abi_layout(wasm_function_type)};
    if(!abi_layout.valid || abi_layout.parameter_count != call_arguments.size()) [[unlikely]] { return {}; }

    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto& llvm_context{ir_builder.getContext()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};

    ::llvm::Value* param_buffer_address{::llvm::ConstantInt::get(llvm_intptr_type, 0u)};
    if(abi_layout.parameter_bytes != 0uz)
    {
        // The parameter buffer lives in the generated frame.  Its integer address is passed to the bridge, but ownership
        // stays entirely inside this LLVM function.
        auto param_buffer{create_llvm_jit_entry_block_alloca(ir_builder,
                                                             llvm_i8_type,
                                                             ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes),
                                                             param_buffer_name)};
        if(param_buffer == nullptr) [[unlikely]] { return {}; }

        ::std::size_t parameter_offset{};
        for(::std::size_t parameter_index{}; parameter_index != abi_layout.parameter_count; ++parameter_index)
        {
            // Pack each scalar at the next byte offset.  The bridge ABI is a tightly packed sequence of Wasm scalar
            // storage representations, independent of the native platform's C struct padding rules.
            auto const abi_size{get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
            auto argument{call_arguments[parameter_index]};
            if(abi_size == 0uz || argument == nullptr) [[unlikely]] { return {}; }

            auto store_address{ir_builder.CreateInBoundsGEP(llvm_i8_type, param_buffer, {::llvm::ConstantInt::get(llvm_intptr_type, parameter_offset)})};
            auto typed_store_address{ir_builder.CreateBitCast(store_address, get_llvm_pointer_type(argument->getType()))};
            auto packed_store{ir_builder.CreateStore(argument, typed_store_address)};
            packed_store->setAlignment(::llvm::Align{1u});
            parameter_offset += abi_size;
        }

        param_buffer_address = ir_builder.CreatePtrToInt(param_buffer, llvm_intptr_type);
    }

    ::llvm::AllocaInst* result_buffer{};
    ::llvm::Value* result_buffer_address{::llvm::ConstantInt::get(llvm_intptr_type, 0u)};
    if(abi_layout.result_bytes != 0uz)
    {
        result_buffer = create_llvm_jit_entry_block_alloca(ir_builder,
                                                           llvm_i8_type,
                                                           ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                                                           result_buffer_name);
        if(result_buffer == nullptr) [[unlikely]] { return {}; }
        result_buffer_address = ir_builder.CreatePtrToInt(result_buffer, llvm_intptr_type);
    }

    return llvm_jit_runtime_raw_call_buffers_t{.valid = true,
                                               .param_buffer_address = param_buffer_address,
                                               .result_buffer = result_buffer,
                                               .result_buffer_address = result_buffer_address};
}

// Load a complete typed Wasm result from a tightly packed raw result buffer. Multi-value results are reconstructed as the
// canonical LLVM struct used by typed entries; individual fields retain their exact scalar LLVM types.
[[nodiscard]] inline constexpr ::llvm::Value*
    emit_runtime_raw_call_result_value(::llvm::IRBuilder<>& ir_builder,
                                       ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                       llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers,
                                       ::llvm::StringRef result_name) noexcept
{
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return nullptr; }
    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};
    if(result_count == 0uz) { return nullptr; }
    if(raw_call_buffers.result_buffer == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{ir_builder.getContext()};
    auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_result_type{get_llvm_result_type_from_wasm_result_range(llvm_context, result_begin, result_end)};
    if(llvm_result_type == nullptr || llvm_result_type->isVoidTy()) [[unlikely]] { return nullptr; }

    ::llvm::Value* aggregate{result_count == 1uz ? nullptr : static_cast<::llvm::Value*>(::llvm::UndefValue::get(llvm_result_type))};
    ::std::size_t result_offset{};
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto const wasm_result_type{static_cast<runtime_operand_stack_value_type>(result_begin[result_index])};
        auto llvm_scalar_type{get_llvm_type_from_wasm_value_type(llvm_context, wasm_result_type)};
        auto const abi_size{get_runtime_wasm_value_type_abi_size(wasm_result_type)};
        if(llvm_scalar_type == nullptr || abi_size == 0uz) [[unlikely]] { return nullptr; }

        auto result_address{ir_builder.CreateInBoundsGEP(llvm_i8_type,
                                                         raw_call_buffers.result_buffer,
                                                         {::llvm::ConstantInt::get(llvm_intptr_type, result_offset)})};
        auto typed_result_address{ir_builder.CreateBitCast(result_address, get_llvm_pointer_type(llvm_scalar_type))};
        auto scalar_result{ir_builder.CreateLoad(llvm_scalar_type, typed_result_address, result_name)};
        scalar_result->setAlignment(::llvm::Align{1u});
        if(result_count == 1uz) { return scalar_result; }

        aggregate = ir_builder.CreateInsertValue(aggregate, scalar_result, {static_cast<unsigned>(result_index)}, result_name);
        result_offset += abi_size;
    }
    return aggregate;
}

// Store a typed scalar/struct Wasm result into the raw result buffer ABI. The integer buffer address has already been
// checked by the wrapper; stores are explicitly byte-aligned because adjacent Wasm values are tightly packed.
[[nodiscard]] inline constexpr bool
    emit_store_runtime_wasm_call_result_to_raw_buffer(::llvm::IRBuilder<>& ir_builder,
                                                      ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                      ::llvm::Value* typed_result,
                                                      ::llvm::Value* result_buffer_address,
                                                      ::llvm::StringRef result_name) noexcept
{
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return false; }
    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};
    if(result_count == 0uz) { return true; }
    if(typed_result == nullptr || result_buffer_address == nullptr) [[unlikely]] { return false; }

    auto& llvm_context{ir_builder.getContext()};
    auto canonical_result_type{get_llvm_result_type_from_wasm_result_range(llvm_context, result_begin, result_end)};
    if(canonical_result_type == nullptr || typed_result->getType() != canonical_result_type) [[unlikely]] { return false; }
    ::llvm::StructType* aggregate_result_type{};
    if(result_count > 1uz)
    {
        if(!canonical_result_type->isStructTy()) [[unlikely]] { return false; }
        aggregate_result_type = static_cast<::llvm::StructType*>(canonical_result_type);
        if(aggregate_result_type->getNumElements() != result_count) [[unlikely]] { return false; }
    }
    auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
    auto llvm_i8_ptr_type{get_llvm_pointer_type(llvm_i8_type)};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    if(llvm_i8_ptr_type == nullptr) [[unlikely]] { return false; }
    auto result_buffer_base{ir_builder.CreateIntToPtr(result_buffer_address, llvm_i8_ptr_type, result_name)};

    ::std::size_t result_offset{};
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto const wasm_result_type{static_cast<runtime_operand_stack_value_type>(result_begin[result_index])};
        auto llvm_scalar_type{get_llvm_type_from_wasm_value_type(llvm_context, wasm_result_type)};
        auto const abi_size{get_runtime_wasm_value_type_abi_size(wasm_result_type)};
        if(llvm_scalar_type == nullptr || abi_size == 0uz ||
           (aggregate_result_type != nullptr &&
            aggregate_result_type->getElementType(static_cast<unsigned>(result_index)) != llvm_scalar_type)) [[unlikely]]
        {
            return false;
        }

        auto scalar_result{result_count == 1uz
                               ? typed_result
                               : ir_builder.CreateExtractValue(typed_result, {static_cast<unsigned>(result_index)}, result_name)};
        if(scalar_result == nullptr || scalar_result->getType() != llvm_scalar_type) [[unlikely]] { return false; }
        auto result_address{ir_builder.CreateInBoundsGEP(llvm_i8_type,
                                                         result_buffer_base,
                                                         {::llvm::ConstantInt::get(llvm_intptr_type, result_offset)})};
        auto typed_result_address{ir_builder.CreateBitCast(result_address, get_llvm_pointer_type(llvm_scalar_type))};
        auto packed_store{ir_builder.CreateStore(scalar_result, typed_result_address)};
        packed_store->setAlignment(::llvm::Align{1u});
        result_offset += abi_size;
    }
    return true;
}

// Compare two Wasm function types structurally.  This is used for direct-call eligibility and canonical type-id
// computation, so the comparison is based on exact parameter/result byte values.
[[nodiscard]] inline constexpr bool runtime_wasm_function_types_equal(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& left,
                                                                      ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& right) noexcept
{
    auto const left_param_begin{left.parameter.begin};
    auto const left_param_end{left.parameter.end};
    auto const right_param_begin{right.parameter.begin};
    auto const right_param_end{right.parameter.end};
    auto const left_result_begin{left.result.begin};
    auto const left_result_end{left.result.end};
    auto const right_result_begin{right.result.begin};
    auto const right_result_end{right.result.end};

    if((left_param_begin == nullptr && left_param_begin != left_param_end) || (right_param_begin == nullptr && right_param_begin != right_param_end) ||
       (left_result_begin == nullptr && left_result_begin != left_result_end) || (right_result_begin == nullptr && right_result_begin != right_result_end))
        [[unlikely]]
    {
        return false;
    }

    auto const left_param_count{left_param_begin == nullptr ? 0uz : static_cast<::std::size_t>(left_param_end - left_param_begin)};
    auto const right_param_count{right_param_begin == nullptr ? 0uz : static_cast<::std::size_t>(right_param_end - right_param_begin)};
    auto const left_result_count{left_result_begin == nullptr ? 0uz : static_cast<::std::size_t>(left_result_end - left_result_begin)};
    auto const right_result_count{right_result_begin == nullptr ? 0uz : static_cast<::std::size_t>(right_result_end - right_result_begin)};

    if(left_param_count != right_param_count || left_result_count != right_result_count) { return false; }

    for(::std::size_t i{}; i != left_param_count; ++i)
    {
        // The runtime canonicalizes neither type objects nor duplicate declarations, so equality is structural by byte.
        if(left_param_begin[i] != right_param_begin[i]) { return false; }
    }

    for(::std::size_t i{}; i != left_result_count; ++i)
    {
        if(left_result_begin[i] != right_result_begin[i]) { return false; }
    }

    return true;
}

// Sentinel used when a type index cannot be mapped to a canonical type id for call_indirect checks.
[[nodiscard]] inline constexpr ::std::uint_least32_t invalid_runtime_canonical_type_id() noexcept
{ return (::std::numeric_limits<::std::uint_least32_t>::max)(); }

// Assign a canonical type id by finding the first structurally equal type in the module type section.  This makes
// `call_indirect` type checks stable even when the Wasm module has duplicate type declarations.
[[nodiscard]] inline constexpr ::std::uint_least32_t
    resolve_runtime_canonical_type_id(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                      validation_module_traits_t::wasm_u32 type_index) noexcept
{
    auto type_begin{runtime_module.type_section_storage.type_section_begin};
    auto type_end{runtime_module.type_section_storage.type_section_end};
    auto const type_index_uz{static_cast<::std::size_t>(type_index)};
    if(type_begin == nullptr || type_end == nullptr || type_begin > type_end) [[unlikely]] { return invalid_runtime_canonical_type_id(); }

    auto const total{static_cast<::std::size_t>(type_end - type_begin)};
    if(type_index_uz >= total) [[unlikely]] { return invalid_runtime_canonical_type_id(); }

    auto const& target_type{type_begin[type_index_uz]};
    ::std::size_t canonical_index{type_index_uz};
    for(::std::size_t i{}; i != type_index_uz; ++i)
    {
        // The first equal type wins.  This lets call_indirect compare compact ids while preserving Wasm's structural type
        // equality rule for duplicate entries in the type section.
        if(runtime_wasm_function_types_equal(type_begin[i], target_type))
        {
            canonical_index = i;
            break;
        }
    }

    if(canonical_index > static_cast<::std::size_t>((::std::numeric_limits<::std::uint_least32_t>::max)())) [[unlikely]]
    {
        return invalid_runtime_canonical_type_id();
    }
    return static_cast<::std::uint_least32_t>(canonical_index);
}

// Fully resolved information needed to emit a global.get/global.set.  A global can be directly addressable in current
// storage or reachable only through a local-imported module bridge.
struct runtime_global_access_info_t
{
    // Wasm scalar type stored by the global.
    runtime_operand_stack_value_type value_type{};

    // Whether `global.set` is permitted.
    bool is_mutable{};

    // Direct storage address for globals owned by this runtime instance.
    ::uwvm2::object::global::wasm_global_storage_t* storage_ptr{};

    // Provider module for local-imported globals that cannot be addressed directly.
    ::uwvm2::uwvm::wasm::type::local_imported_t* local_imported_module_ptr{};

    // Global index inside `local_imported_module_ptr`.
    ::std::size_t local_imported_global_index{};
};

// Resolve a global index through imported/local storage and any import forwarding chain.
[[nodiscard]] inline constexpr runtime_global_access_info_t
    resolve_runtime_global_access_info(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                       validation_module_traits_t::wasm_u32 global_index) noexcept
{
    runtime_global_access_info_t result{};

    auto const imported_global_count{runtime_module.imported_global_vec_storage.size()};
    auto const local_global_count{runtime_module.local_defined_global_vec_storage.size()};
    auto const global_index_uz{static_cast<::std::size_t>(global_index)};

    if(global_index_uz < imported_global_count)
    {
        using imported_global_storage_t = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t;
        using global_link_kind = imported_global_storage_t::imported_global_link_kind;

        auto const& imported_global_rec{runtime_module.imported_global_vec_storage.index_unchecked(global_index_uz)};
        auto import_type_ptr{imported_global_rec.import_type_ptr};
        if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::global) [[unlikely]] { return result; }

        result.value_type = static_cast<runtime_operand_stack_value_type>(import_type_ptr->imports.storage.global.type);
        result.is_mutable = import_type_ptr->imports.storage.global.is_mutable;

        imported_global_storage_t const* curr{::std::addressof(imported_global_rec)};
        for(;;)
        {
            // Runtime initialization owns cycle rejection for import chains.  This loop therefore follows links until a
            // concrete storage provider is found, while still treating null or unknown links as corrupted storage.
            if(curr == nullptr) [[unlikely]] { return {}; }

            switch(curr->link_kind)
            {
                case global_link_kind::imported:
                {
                    curr = curr->target.imported_ptr;
                    continue;
                }
                case global_link_kind::defined:
                {
                    auto defined_global{curr->target.defined_ptr};
                    if(defined_global == nullptr) [[unlikely]] { return {}; }
                    result.storage_ptr = const_cast<::uwvm2::object::global::wasm_global_storage_t*>(::std::addressof(defined_global->global));
                    return result;
                }
                case global_link_kind::local_imported:
                {
                    result.local_imported_module_ptr = curr->target.local_imported.module_ptr;
                    result.local_imported_global_index = curr->target.local_imported.index;
                    return result;
                }
                [[unlikely]] default:
                {
                    return {};
                }
            }
        }
    }

    auto const local_global_index{global_index_uz - imported_global_count};
    if(local_global_index >= local_global_count) [[unlikely]] { return result; }

    auto const& local_global_rec{runtime_module.local_defined_global_vec_storage.index_unchecked(local_global_index)};
    auto global_type_ptr{local_global_rec.global_type_ptr};
    if(global_type_ptr == nullptr) [[unlikely]] { return result; }

    result.value_type = static_cast<runtime_operand_stack_value_type>(global_type_ptr->type);
    result.is_mutable = global_type_ptr->is_mutable;
    result.storage_ptr = const_cast<::uwvm2::object::global::wasm_global_storage_t*>(::std::addressof(local_global_rec.global));
    return result;
}

// Build an LLVM external-symbol pointer to the concrete scalar field inside a directly addressable global storage record.
[[nodiscard]] inline constexpr ::llvm::Value* get_llvm_global_storage_pointer(::llvm::LLVMContext& llvm_context,
                                                                              ::llvm::IRBuilder<>& ir_builder,
                                                                              ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                              validation_module_traits_t::wasm_u32 global_index,
                                                                              ::uwvm2::object::global::wasm_global_storage_t* global_storage_ptr,
                                                                              runtime_operand_stack_value_type value_type) noexcept
{
    if(global_storage_ptr == nullptr) [[unlikely]] { return nullptr; }

    auto llvm_value_type{get_llvm_type_from_wasm_value_type(llvm_context, value_type)};
    if(llvm_value_type == nullptr) [[unlikely]] { return nullptr; }

    ::std::uintptr_t storage_address{};
    switch(value_type)
    {
        case runtime_operand_stack_value_type::i32:
        {
            // The global storage union is intentionally addressed at the active scalar member so LLVM sees the exact load
            // or store type and does not need to reason about the containing union object.
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.i32));
            break;
        }
        case runtime_operand_stack_value_type::i64:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.i64));
            break;
        }
        case runtime_operand_stack_value_type::f32:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.f32));
            break;
        }
        case runtime_operand_stack_value_type::f64:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.f64));
            break;
        }
        [[unlikely]] default:
        {
            return nullptr;
        }
    }

    auto const symbol_name{get_llvm_global_storage_symbol_name(runtime_module, global_index)};
    return get_llvm_external_host_object_pointer(ir_builder,
                                                 storage_address,
                                                 llvm_value_type,
                                                 ::uwvm2::utils::container::u8string_view{symbol_name.data(), symbol_name.size()});
}

// Host bridge used by generated code to read a local-imported global.  The template keeps the LLVM function signature
// strongly typed for each Wasm scalar kind.
template <typename ValueType>
[[nodiscard]] inline constexpr ValueType llvm_jit_local_imported_global_get_bridge(::std::uintptr_t local_imported_module_address,
                                                                                   ::std::size_t global_index) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    ValueType value{};
    local_imported_module->global_get_from_index(global_index, reinterpret_cast<::std::byte*>(::std::addressof(value)));
    return value;
}

// Host bridge used by generated code to write a local-imported global.  Failure is fatal because validated JIT code should
// only request globals that the runtime resolved successfully at emission time.
template <typename ValueType>
inline constexpr void
    llvm_jit_local_imported_global_set_bridge(::std::uintptr_t local_imported_module_address, ::std::size_t global_index, ValueType value) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    if(!local_imported_module->global_set_from_index(global_index, reinterpret_cast<::std::byte const*>(::std::addressof(value)))) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }
}

// Short aliases for runtime types used repeatedly by memory/table bridge templates below.
using runtime_native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
using runtime_wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using runtime_wasm_u32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32;
using runtime_wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
using runtime_wasm_u64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u64;
using runtime_wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
using runtime_wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
using runtime_wasm_v128 = ::uwvm2::parser::wasm::standard::wasm1p1::type::wasm_v128;
using runtime_table_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t;
using runtime_table_elem_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t;
using runtime_module_storage_t = ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t;
using runtime_data_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_data_storage_t;
using runtime_element_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_element_storage_t;
using runtime_wasm_global_ref = ::uwvm2::object::global::wasm_global_ref_t;
using runtime_wasm_funcref = ::uwvm2::object::global::wasm_funcref_t;
using runtime_wasm_externref = ::uwvm2::object::global::wasm_externref_t;

static_assert(sizeof(::std::uint_least32_t) == sizeof(runtime_wasm_u32));
static_assert(sizeof(::std::uint_least64_t) == sizeof(runtime_wasm_u64));
static_assert(sizeof(runtime_wasm_u32) == sizeof(runtime_wasm_f32));
static_assert(sizeof(runtime_wasm_u64) == sizeof(runtime_wasm_f64));
static_assert(sizeof(runtime_wasm_v128) == 16uz);
static_assert(sizeof(runtime_wasm_funcref) == sizeof(runtime_wasm_global_ref));
static_assert(sizeof(runtime_wasm_externref) == sizeof(runtime_wasm_global_ref));

[[nodiscard]] inline constexpr ::uwvm2::utils::container::u8string_view
    get_llvm_jit_bridge_value_type_name(runtime_operand_stack_value_type value_type) noexcept
{
    switch(value_type)
    {
        case runtime_operand_stack_value_type::i32: return ::uwvm2::utils::container::u8string_view{u8"i32"};
        case runtime_operand_stack_value_type::i64: return ::uwvm2::utils::container::u8string_view{u8"i64"};
        case runtime_operand_stack_value_type::f32: return ::uwvm2::utils::container::u8string_view{u8"f32"};
        case runtime_operand_stack_value_type::f64: return ::uwvm2::utils::container::u8string_view{u8"f64"};
        [[unlikely]] default: return ::uwvm2::utils::container::u8string_view{u8"unknown"};
    }
}

[[nodiscard]] inline ::uwvm2::utils::container::u8string make_llvm_jit_memory_bridge_symbol_discriminator(
    ::uwvm2::utils::container::u8string_view bridge_kind,
    runtime_operand_stack_value_type value_type,
    ::std::size_t access_bytes,
    bool signed_access = false) noexcept
{
    ::uwvm2::utils::container::u8string out{};
    ::uwvm2::utils::container::u8string_ref_uwvm out_ref{::std::addressof(out)};
    ::fast_io::io::print(out_ref,
                         bridge_kind,
                         u8":",
                         get_llvm_jit_bridge_value_type_name(value_type),
                         u8":",
                         access_bytes,
                         signed_access ? ::uwvm2::utils::container::u8string_view{u8":s"} : ::uwvm2::utils::container::u8string_view{u8":u"});
    return out;
}

// Result of resolving a table element for call_indirect.  This separates "table slot is empty" from "slot is present but
// cannot be converted to a same-module direct call".
struct runtime_call_indirect_callee_resolution_t
{
    // False when runtime table/function storage is inconsistent.
    bool state_valid{};

    // True when the table element contains a non-null function reference.
    bool present{};

    // True when `func_index` identifies a function in the current module and direct typed calls are possible.
    bool belongs_to_current_module{};

    // Current-module function index for direct calls.
    validation_module_traits_t::wasm_u32 func_index{};

    // Best-known Wasm type for the referenced function.
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr{};
};

// Resolve an imported or local table index to the concrete table storage backing call_indirect.
[[nodiscard]] inline constexpr runtime_table_storage_t const*
    resolve_runtime_table_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                  validation_module_traits_t::wasm_u32 table_index) noexcept
{
    using imported_table_storage_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t;
    using table_link_kind = imported_table_storage_t::imported_table_link_kind;

    auto const imported_table_count{runtime_module.imported_table_vec_storage.size()};
    auto const table_index_uz{static_cast<::std::size_t>(table_index)};

    if(table_index_uz < imported_table_count)
    {
        auto curr{::std::addressof(runtime_module.imported_table_vec_storage.index_unchecked(table_index_uz))};
        for(::std::size_t steps{};; ++steps)
        {
            // Table imports use the same forwarding model as globals/functions: a valid initialized graph ends at a
            // defined table, while null/unknown links are treated as runtime-storage corruption.  Initialization rejects
            // alias cycles; retain a hard bound here so corrupted embedding state cannot hang LLVM preparation.
            if(steps > 8192uz) [[unlikely]] { return nullptr; }
            if(curr == nullptr) [[unlikely]] { return nullptr; }

            switch(curr->link_kind)
            {
                case table_link_kind::defined:
                {
                    return curr->target.defined_ptr;
                }
                case table_link_kind::imported:
                {
                    curr = curr->target.imported_ptr;
                    continue;
                }
                [[unlikely]] default:
                {
                    return nullptr;
                }
            }
        }
    }

    auto const local_table_index{table_index_uz - imported_table_count};
    if(local_table_index >= runtime_module.local_defined_table_vec_storage.size()) [[unlikely]] { return nullptr; }
    return ::std::addressof(runtime_module.local_defined_table_vec_storage.index_unchecked(local_table_index));
}

enum class runtime_storage_pointer_membership : unsigned
{
    outside,
    element,
    invalid,
};

// Classify a possibly cross-module runtime pointer without relationally comparing or subtracting unrelated C++
// pointers.  An address outside this vector can legitimately name storage owned by the provider of an imported table;
// an address inside the byte range must land exactly on an element boundary.
template <typename Element>
[[nodiscard]] inline constexpr runtime_storage_pointer_membership
    classify_runtime_storage_pointer(Element const* begin, ::std::size_t count, Element const* element, ::std::size_t& index) noexcept
{
    index = 0uz;
    if(element == nullptr) { return runtime_storage_pointer_membership::outside; }
    if(begin == nullptr)
    {
        return count == 0uz ? runtime_storage_pointer_membership::outside : runtime_storage_pointer_membership::invalid;
    }
    if(count > (::std::numeric_limits<::std::uintptr_t>::max() / sizeof(Element))) [[unlikely]]
    {
        return runtime_storage_pointer_membership::invalid;
    }

    auto const begin_address{reinterpret_cast<::std::uintptr_t>(begin)};
    auto const element_address{reinterpret_cast<::std::uintptr_t>(element)};
    auto const storage_bytes{static_cast<::std::uintptr_t>(count * sizeof(Element))};
    if(begin_address > (::std::numeric_limits<::std::uintptr_t>::max() - storage_bytes)) [[unlikely]]
    {
        return runtime_storage_pointer_membership::invalid;
    }

    auto const end_address{begin_address + storage_bytes};
    if(element_address < begin_address || element_address >= end_address) { return runtime_storage_pointer_membership::outside; }

    auto const byte_offset{element_address - begin_address};
    if((byte_offset % sizeof(Element)) != 0u) [[unlikely]] { return runtime_storage_pointer_membership::invalid; }
    index = static_cast<::std::size_t>(byte_offset / sizeof(Element));
    if(index >= count) [[unlikely]] { return runtime_storage_pointer_membership::invalid; }
    return runtime_storage_pointer_membership::element;
}

// Resolve a table element to a function reference and decide whether it belongs to the current module.  This helper is
// used when building JIT table views and by paths that can pre-resolve indirect targets.
[[nodiscard]] inline constexpr runtime_call_indirect_callee_resolution_t
    resolve_runtime_call_indirect_callee(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                         runtime_table_elem_storage_t const& elem) noexcept
{
    using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

    runtime_call_indirect_callee_resolution_t result{.state_valid = true};

    auto const imported_func_count{runtime_module.imported_function_vec_storage.size()};
    auto const local_func_count{runtime_module.local_defined_function_vec_storage.size()};
    auto imported_func_begin{runtime_module.imported_function_vec_storage.data()};
    auto local_func_begin{runtime_module.local_defined_function_vec_storage.data()};

    switch(elem.type)
    {
        case table_elem_type::func_ref_imported:
        {
            auto imported_func_ptr{elem.storage.imported_ptr};
            if(imported_func_ptr == nullptr) { return result; }

            result.present = true;
            ::std::size_t func_index_uz{};
            auto const membership{
                classify_runtime_storage_pointer(imported_func_begin, imported_func_count, imported_func_ptr, func_index_uz)};
            if(membership == runtime_storage_pointer_membership::invalid) [[unlikely]] { return {}; }
            if(membership == runtime_storage_pointer_membership::outside) { return result; }

            // An imported function reference can be an empty table slot, a host/imported target, or a forwarding alias to
            // a current-module function.  Dereference only after proving that the pointer names an exact element in this
            // module's import vector; provider-module pointers intentionally remain non-direct targets here.
            auto import_type_ptr{imported_func_ptr->import_type_ptr};
            if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::func ||
               import_type_ptr->imports.storage.function == nullptr) [[unlikely]]
            {
                return {};
            }

            result.function_type_ptr = import_type_ptr->imports.storage.function;
            if(func_index_uz > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max())) [[unlikely]] { return {}; }

            auto const callee_resolution{resolve_runtime_direct_callee(runtime_module, static_cast<validation_module_traits_t::wasm_u32>(func_index_uz))};
            if(!callee_resolution.state_valid) [[unlikely]] { return {}; }

            if(callee_resolution.function_type_ptr != nullptr) { result.function_type_ptr = callee_resolution.function_type_ptr; }
            if(!callee_resolution.direct_callable) { return result; }

            result.belongs_to_current_module = true;
            result.func_index = callee_resolution.func_index;
            return result;
        }
        case table_elem_type::func_ref_defined:
        {
            auto defined_func_ptr{elem.storage.defined_ptr};
            if(defined_func_ptr == nullptr) { return result; }

            result.present = true;
            ::std::size_t local_func_index{};
            auto const membership{classify_runtime_storage_pointer(local_func_begin, local_func_count, defined_func_ptr, local_func_index)};
            if(membership == runtime_storage_pointer_membership::invalid) [[unlikely]] { return {}; }
            if(membership == runtime_storage_pointer_membership::outside) { return result; }

            if(defined_func_ptr->function_type_ptr == nullptr) [[unlikely]] { return {}; }
            result.function_type_ptr = defined_func_ptr->function_type_ptr;
            if(imported_func_count > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) ||
               local_func_index > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) - imported_func_count)
                [[unlikely]]
            {
                return {};
            }

            result.belongs_to_current_module = true;
            result.func_index = static_cast<validation_module_traits_t::wasm_u32>(imported_func_count + local_func_index);
            return result;
        }
        [[unlikely]] default:
        {
            return {};
        }
    }
}

// Resolved memory information for memory index 0.  The emitter currently caches only memory 0 because MVP memory
// instructions in this path target the default memory.
struct runtime_memory_access_info_t
{
    // Direct native memory storage, present for memories owned by this runtime instance.
    runtime_native_memory_t* memory_p{};

    // Provider module for local-imported memories that must be accessed through bridge calls.
    ::uwvm2::uwvm::wasm::type::local_imported_t* local_imported_module_ptr{};

    // Memory index inside `local_imported_module_ptr`.
    ::std::size_t local_imported_memory_index{};

    // Page size reported by the imported provider, used for memory.size and growth-limit calculations.
    ::std::uint_least64_t local_imported_page_size_bytes{};

    // Maximum byte length allowed by the Wasm limits and by backend-specific address-space constraints.
    ::std::size_t max_limit_memory_length{};

    // Stable base address for mmap-backed direct memory access.
    ::std::byte* stable_memory_begin{};

    // Number of reserved bytes addressable from stable_memory_begin. LLVM uses this as the external object's extent;
    // inaccessible guard pages remain part of the reservation and turn invalid Wasm accesses into hardware faults.
    ::std::size_t stable_memory_reserved_span_bytes{};

    // Atomic byte-length slot used by mmap-backed memories whose length can change concurrently.
    ::std::atomic_size_t* stable_memory_length_p{};

    // Non-atomic byte-length slot used by non-mmap memory backends.
    ::std::size_t const* stable_memory_length_value_p{};

    // log2(page size), allowing custom-page-size memories to compute page counts by shifting.
    unsigned custom_page_size_log2{};

    // True when mmap protection alone is not enough and every direct access must emit a dynamic bounds check.
    bool mmap_requires_dynamic_bounds{};

    // True when mmap protection covers only a prefix of the address space and high offsets need a dynamic slow check.
    bool mmap_uses_partial_protection{};

    // True only after resolving a concrete full wasm32 reservation with the unsigned 8-GiB address domain.
    bool mmap_covers_wasm32_effective_domain{};
};

// Return the largest byte length that the concrete memory backend can safely expose for direct addressing.  This is
// stricter than the Wasm declared max when mmap guard/protection strategy imposes a smaller usable range.
template <typename Memory>
[[nodiscard]] inline constexpr ::std::size_t get_runtime_memory_backend_max_limit_length_impl(Memory const& memory) noexcept
{
#if defined(UWVM_SUPPORT_MMAP)
    if constexpr(Memory::can_mmap)
    {
        if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
        {
            switch(memory.status)
            {
                case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm32:
                {
                    // Full guard/protection layouts reserve additional address space beyond the declared maximum.  Limit
                    // direct exposure to the portion the backend can protect without wraparound ambiguity.
                    constexpr auto max_full_protection_wasm32_length_half{::uwvm2::object::memory::linear::max_full_protection_wasm32_length / 2u};
                    return static_cast<::std::size_t>(max_full_protection_wasm32_length_half);
                }
                case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64:
                {
                    return static_cast<::std::size_t>(::uwvm2::object::memory::linear::max_partial_protection_wasm64_length);
                }
                [[unlikely]] default:
                {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                    return 0uz;
                }
            }
        }
        else
        {
            return static_cast<::std::size_t>(::uwvm2::object::memory::linear::max_partial_protection_wasm32_length);
        }
    }
    else
    {
        static_cast<void>(memory);
        return ::std::numeric_limits<::std::size_t>::max();
    }
#else
    static_cast<void>(memory);
    return ::std::numeric_limits<::std::size_t>::max();
#endif
}

// Copy mmap-specific direct-access fields from a concrete memory backend into the generic access-info record.
template <typename Memory>
inline constexpr void populate_runtime_memory_access_info_mmap_fields(runtime_memory_access_info_t& result, Memory& memory) noexcept
{
    if constexpr(requires { memory.memory_length; }) { result.stable_memory_length_value_p = ::std::addressof(memory.memory_length); }

#if defined(UWVM_SUPPORT_MMAP)
    if constexpr(Memory::can_mmap)
    {
        result.stable_memory_begin = memory.memory_begin;
        result.stable_memory_length_p = memory.memory_length_p;
        result.mmap_requires_dynamic_bounds = memory.require_dynamic_determination_memory_size();
        if(memory.memory_begin != nullptr && memory.reserved_begin != nullptr)
        {
            auto const reserved_span_bytes{memory.get_acquire_reserved_space_ceil()};
            auto const reserved_address{reinterpret_cast<::std::uintptr_t>(memory.reserved_begin)};
            auto const memory_address{reinterpret_cast<::std::uintptr_t>(memory.memory_begin)};
            if(memory_address >= reserved_address)
            {
                auto const memory_displacement{memory_address - reserved_address};
                if(memory_displacement <= reserved_span_bytes)
                {
                    result.stable_memory_reserved_span_bytes = reserved_span_bytes - static_cast<::std::size_t>(memory_displacement);
                }
            }
        }
        if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t) && sizeof(::std::uintptr_t) >= sizeof(::std::uint_least64_t))
        {
            constexpr auto required_unsigned_domain_span{
                ::uwvm2::object::memory::linear::wasm32_max_effective_offset + ::uwvm2::object::memory::linear::mmap_guard_max_access_size};
            result.mmap_covers_wasm32_effective_domain =
                !result.mmap_requires_dynamic_bounds && memory.status == ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm32 &&
                memory.memory_begin != nullptr && memory.memory_begin == memory.reserved_begin &&
                result.stable_memory_reserved_span_bytes >= required_unsigned_domain_span;
        }
        if(!result.mmap_requires_dynamic_bounds)
        {
            // If hardware protection covers the complete usable range, generated loads/stores can skip explicit dynamic
            // bounds checks for the common path.  Partial protection still needs a high-offset software check.
            if constexpr(sizeof(::std::uintptr_t) >= sizeof(::std::uint_least64_t))
            {
                result.mmap_uses_partial_protection = memory.status == ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
            }
            else
            {
                result.mmap_uses_partial_protection = true;
            }
        }
    }
    else
    {
        static_cast<void>(result);
        static_cast<void>(memory);
    }
#else
    static_cast<void>(result);
    static_cast<void>(memory);
#endif
}

// Convert Wasm memory limits from pages to bytes, saturating to size_t max when the limit is absent or overflows.
[[nodiscard]] inline constexpr ::std::size_t get_runtime_memory_max_limit_length_from_limits(auto const& limits) noexcept
{
    if(!limits.present_max) { return ::std::numeric_limits<::std::size_t>::max(); }

    // Wasm MVP memory limits are expressed in fixed 64 KiB pages.  If/when the custom-page-size proposal is enabled in
    // this lowering path, this conversion must use the memory's declared page size instead of the MVP constant.
    constexpr ::std::size_t wasm_page_bytes{65536uz};
    auto const max_pages{static_cast<::std::size_t>(limits.max)};
    if(max_pages > (::std::numeric_limits<::std::size_t>::max() / wasm_page_bytes)) { return ::std::numeric_limits<::std::size_t>::max(); }
    return max_pages * wasm_page_bytes;
}

// Wrapper for native memory backends; keeps call sites independent of the concrete memory template type.
[[nodiscard]] inline constexpr ::std::size_t get_runtime_memory_backend_max_limit_length(runtime_native_memory_t const& memory) noexcept
{ return get_runtime_memory_backend_max_limit_length_impl(memory); }

// Offset at which partial mmap protection can no longer guarantee that an invalid access traps in hardware.
[[nodiscard]] inline constexpr ::std::uint_least64_t get_runtime_partial_protection_limit_escape_offset() noexcept
{
#if defined(UWVM_SUPPORT_MMAP)
    if constexpr(sizeof(::std::uintptr_t) >= sizeof(::std::uint_least64_t))
    {
        return static_cast<::std::uint_least64_t>(1u) << ::uwvm2::object::memory::linear::max_partial_protection_wasm64_index;
    }
    else
    {
        return static_cast<::std::uint_least64_t>(1u) << ::uwvm2::object::memory::linear::max_partial_protection_wasm32_index;
    }
#else
    return 0u;
#endif
}

// Combine the Wasm/user limit with backend limits so memory.grow and direct-access checks agree.
[[nodiscard]] inline constexpr ::std::size_t refine_runtime_memory_max_limit_length(runtime_native_memory_t const& memory,
                                                                                    ::std::size_t max_limit_memory_length) noexcept
{
    auto const backend_max_limit_length{get_runtime_memory_backend_max_limit_length(memory)};
    return backend_max_limit_length < max_limit_memory_length ? backend_max_limit_length : max_limit_memory_length;
}

// Resolve memory index to either directly addressable native memory or a local-imported bridge provider.  The returned
// record also contains enough limit/page-size information for memory.size, memory.grow, and memory access checks.
[[nodiscard]] inline constexpr runtime_memory_access_info_t
    resolve_runtime_memory_access_info(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                       validation_module_traits_t::wasm_u32 memory_index) noexcept
{
    runtime_memory_access_info_t result{};

    auto const imported_memory_count{runtime_module.imported_memory_vec_storage.size()};
    auto const local_memory_count{runtime_module.local_defined_memory_vec_storage.size()};
    auto const memory_index_uz{static_cast<::std::size_t>(memory_index)};

    if(memory_index_uz < imported_memory_count)
    {
        using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
        using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

        auto const& imported_memory_rec{runtime_module.imported_memory_vec_storage.index_unchecked(memory_index_uz)};
        auto import_type_ptr{imported_memory_rec.import_type_ptr};
        if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::memory) [[unlikely]] { return result; }

        result.max_limit_memory_length = get_runtime_memory_max_limit_length_from_limits(imported_memory_rec.effective_limits);

        imported_memory_storage_t const* curr{::std::addressof(imported_memory_rec)};
        for(;;)
        {
            // Imported memories may resolve to native storage or to a local-imported provider.  The latter keeps all
            // actual memory accesses behind provider bridge calls because the provider owns locking and growth policy.
            if(curr == nullptr) [[unlikely]] { return {}; }

            switch(curr->link_kind)
            {
                case memory_link_kind::imported:
                {
                    curr = curr->target.imported_ptr;
                    continue;
                }
                case memory_link_kind::defined:
                {
                    auto defined_memory{curr->target.defined_ptr};
                    if(defined_memory == nullptr) [[unlikely]] { return {}; }
                    result.memory_p = ::std::addressof(defined_memory->memory);
                    result.max_limit_memory_length = get_runtime_memory_max_limit_length_from_limits(defined_memory->effective_limits);
                    result.max_limit_memory_length = refine_runtime_memory_max_limit_length(defined_memory->memory, result.max_limit_memory_length);
                    result.custom_page_size_log2 = defined_memory->memory.custom_page_size_log2;
                    populate_runtime_memory_access_info_mmap_fields(result, defined_memory->memory);
                    return result;
                }
                case memory_link_kind::local_imported:
                {
                    result.local_imported_module_ptr = curr->target.local_imported.module_ptr;
                    result.local_imported_memory_index = curr->target.local_imported.index;
                    if(result.local_imported_module_ptr == nullptr) [[unlikely]] { return {}; }
                    result.local_imported_page_size_bytes = result.local_imported_module_ptr->memory_page_size_from_index(result.local_imported_memory_index);
                    return result;
                }
                [[unlikely]] default:
                {
                    return {};
                }
            }
        }
    }

    auto const local_memory_index{memory_index_uz - imported_memory_count};
    if(local_memory_index >= local_memory_count) [[unlikely]] { return result; }

    auto const& local_memory_rec{runtime_module.local_defined_memory_vec_storage.index_unchecked(local_memory_index)};
    auto memory_type_ptr{local_memory_rec.memory_type_ptr};
    if(memory_type_ptr == nullptr) [[unlikely]] { return result; }

    result.memory_p = const_cast<runtime_native_memory_t*>(::std::addressof(local_memory_rec.memory));
    result.max_limit_memory_length = get_runtime_memory_max_limit_length_from_limits(local_memory_rec.effective_limits);
    result.max_limit_memory_length = refine_runtime_memory_max_limit_length(local_memory_rec.memory, result.max_limit_memory_length);
    result.custom_page_size_log2 = local_memory_rec.memory.custom_page_size_log2;
    populate_runtime_memory_access_info_mmap_fields(result, local_memory_rec.memory);
    return result;
}

// Result of adding a Wasm32 dynamic address and static memarg offset.  The extra boolean records whether the unsigned
// effective address escaped the 32-bit Wasm address range.
struct llvm_jit_wasm32_effective_offset_t
{
    // Low 64-bit effective byte offset used for diagnostics and in-bounds accesses.
    ::std::uint_least64_t offset{};

    // True when the computed address requires a 65th range bit and must be treated as out of bounds.
    bool offset_65_bit{};
};

// Portable checked addition used by the 32-bit fallback effective-offset computation.
template <::std::unsigned_integral I>
UWVM_ALWAYS_INLINE inline constexpr bool llvm_jit_add_overflow(I a, I b, I& result) noexcept
{
#if defined(_MSC_VER) && !defined(__clang__)
    if UWVM_IF_NOT_CONSTEVAL
    {
# if defined(_M_X64) && !(defined(_M_ARM64EC) || defined(__arm64ec__))
        if constexpr(::std::same_as<I, ::std::uint64_t>)
        {
            // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
            return ::fast_io::intrinsics::msvc::x86::_addcarry_u64(false, a, b, ::std::addressof(result));
        }
        else if constexpr(::std::same_as<I, ::std::uint32_t>)
        {
            // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
            return ::fast_io::intrinsics::msvc::x86::_addcarry_u32(false, a, b, ::std::addressof(result));
        }
        else if constexpr(::std::same_as<I, ::std::uint16_t>)
        {
            // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
            return ::fast_io::intrinsics::msvc::x86::_addcarry_u16(false, a, b, ::std::addressof(result));
        }
        else if constexpr(::std::same_as<I, ::std::uint8_t>)
        {
            // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
            return ::fast_io::intrinsics::msvc::x86::_addcarry_u8(false, a, b, ::std::addressof(result));
        }
        else
        {
            // msvc's `_addcarry_u32` does not specialize for `unsigned long`
            result = static_cast<I>(a + b);
            return result < a;
        }

# elif defined(_M_X32)
        if constexpr(::std::same_as<I, ::std::uint32_t>)
        {
            // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
            return ::fast_io::intrinsics::msvc::x86::_addcarry_u32(false, a, b, ::std::addressof(result));
        }
        else if constexpr(::std::same_as<I, ::std::uint16_t>)
        {
            // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
            return ::fast_io::intrinsics::msvc::x86::_addcarry_u16(false, a, b, ::std::addressof(result));
        }
        else if constexpr(::std::same_as<I, ::std::uint8_t>)
        {
            // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
            return ::fast_io::intrinsics::msvc::x86::_addcarry_u8(false, a, b, ::std::addressof(result));
        }
        else
        {
            result = static_cast<I>(a + b);
            return result < a;
        }

# else  // ARM, ARM64, ARM64ec
        result = static_cast<I>(a + b);
        return result < a;
# endif
    }
    else
    {
        result = static_cast<I>(a + b);
        return result < a;
    }

#elif UWVM_HAS_BUILTIN(__builtin_add_overflow)
    // Parameters can't be filled with ::std::addressof(i), to ensure determinism of exceptions.
    return __builtin_add_overflow(a, b, ::std::addressof(result));

#else
    result = static_cast<I>(a + b);
    return result < a;
#endif
}

// Compute the effective offset for a Wasm32 memory access.  Keeping the overflow bit separate lets bridge traps report
// both the wrapped low bits and the fact that the access was outside the representable Wasm32 range.
[[nodiscard]] inline constexpr llvm_jit_wasm32_effective_offset_t
    llvm_jit_compute_wasm32_effective_offset(runtime_wasm_i32 address, validation_module_traits_t::wasm_u32 static_offset) noexcept
{
    if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
    {
        auto const dynamic_offset{static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(address))};
        auto const static_offset_u64{static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(static_offset))};
        auto const sum_u64{dynamic_offset + static_offset_u64};
        return {.offset = sum_u64, .offset_65_bit = sum_u64 > 0xffffffffull};
    }
    else
    {
        auto const dynamic_offset_u32{static_cast<::std::uint_least32_t>(address)};
        auto const static_offset_u32{static_cast<::std::uint_least32_t>(static_offset)};
        ::std::uint_least32_t low{};
        auto const out_of_range{llvm_jit_add_overflow(dynamic_offset_u32, static_offset_u32, low)};

        return {.offset = static_cast<::std::uint_least64_t>(low), .offset_65_bit = out_of_range};
    }
}

// Emit the same unsigned Wasm32 effective-address calculation used by the checked bridge helpers.  Keeping this as a
// named helper lets direct mmap lowering and focused IR tests share the zero-extension invariant.
[[nodiscard]] inline constexpr ::llvm::Value*
    emit_llvm_wasm32_effective_offset(::llvm::IRBuilder<>& ir_builder,
                                      ::llvm::Value* address_value,
                                      validation_module_traits_t::wasm_u32 static_offset) noexcept
{
    if(address_value == nullptr || !address_value->getType()->isIntegerTy(32)) [[unlikely]] { return nullptr; }

    auto llvm_i64_type{::llvm::Type::getInt64Ty(ir_builder.getContext())};
    return ir_builder.CreateAdd(ir_builder.CreateZExt(address_value, llvm_i64_type),
                                ::llvm::ConstantInt::get(llvm_i64_type, static_cast<::std::uint_least32_t>(static_offset)));
}

// Wasm32 defines the effective address in an unbounded intermediate domain and traps when the dynamic address plus the
// memarg offset exceeds the maximum 4-GiB logical length. Checked/partial backends use this explicit predicate; the full
// unsigned-domain mmap reservation can let hardware protection reject the same invalid access without emitting a branch.
[[nodiscard]] inline constexpr ::llvm::Value*
    emit_llvm_wasm32_effective_offset_out_of_range(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* effective_offset) noexcept
{
    if(effective_offset == nullptr || !effective_offset->getType()->isIntegerTy(64)) [[unlikely]] { return nullptr; }
    return ir_builder.CreateICmpUGT(effective_offset, ::llvm::ConstantInt::get(effective_offset->getType(), 0xffffffffull));
}

// A Wasm memarg alignment exponent is only an optimization hint; it does not constrain the runtime address.  Direct
// mmap loads and stores therefore must advertise byte alignment to LLVM unless the effective pointer is proven aligned.
[[nodiscard]] inline constexpr ::llvm::Align
    get_llvm_wasm_memory_access_alignment(::std::size_t, validation_module_traits_t::wasm_u32) noexcept
{
    return ::llvm::Align{1u};
}

// Load an integer from Wasm memory in little-endian order without assuming host alignment.
template <typename UInt>
[[nodiscard]] inline constexpr UInt llvm_jit_load_little_endian_integer(::std::byte const* memory_ptr) noexcept
{
    UInt value{};
    ::std::memcpy(::std::addressof(value), memory_ptr, sizeof(UInt));
    return ::fast_io::little_endian(value);
}

// Store an integer to Wasm memory in little-endian order without assuming host alignment.
template <typename UInt>
inline constexpr void llvm_jit_store_little_endian_integer(::std::byte* memory_ptr, UInt value) noexcept
{
    value = ::fast_io::little_endian(value);
    ::std::memcpy(memory_ptr, ::std::addressof(value), sizeof(UInt));
}

// Generic memory trap bridge used when detailed access metadata is unavailable.
inline constexpr void llvm_jit_memory_bridge_trap() noexcept
{ ::uwvm2::runtime::lib::llvm_jit_runtime_trap(::uwvm2::runtime::lib::llvm_jit_trap_kind::memory_out_of_bounds, 0u, 0u); }

// Detailed memory trap bridge for native checked-memory access fallback paths.
inline constexpr void llvm_jit_memory_bridge_trap(::std::size_t memory_idx,
                                                  validation_module_traits_t::wasm_u32 static_offset,
                                                  llvm_jit_wasm32_effective_offset_t effective_offset,
                                                  ::std::size_t memory_length,
                                                  ::std::size_t access_size) noexcept
{
    ::uwvm2::runtime::lib::llvm_jit_memory_out_of_bounds_trap(memory_idx,
                                                              static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(static_offset)),
                                                              effective_offset.offset,
                                                              effective_offset.offset_65_bit ? 1u : 0u,
                                                              static_cast<::std::uint_least64_t>(memory_length),
                                                              access_size,
                                                              0u,
                                                              0u);
    ::fast_io::fast_terminate();
}

// Run a memory access under the backend's required snapshot/guard discipline and call `fn` only after bounds checks pass.
template <typename MemoryT, typename Fn>
[[nodiscard]] inline constexpr bool llvm_jit_try_checked_memory_access(MemoryT const& memory,
                                                                       llvm_jit_wasm32_effective_offset_t effective_offset,
                                                                       ::std::size_t access_size,
                                                                       ::std::size_t& memory_length_out,
                                                                       Fn&& fn) noexcept
{
    // The common checked path records the length used for diagnostics, validates the effective range, and then executes
    // the caller's load/store lambda on a raw byte pointer.
    auto const checked_access{[&](::std::byte* memory_begin, ::std::size_t memory_length) constexpr noexcept
                              {
                                  memory_length_out = memory_length;
                                  if(memory_begin == nullptr || effective_offset.offset_65_bit || access_size > memory_length ||
                                     effective_offset.offset > static_cast<::std::uint_least64_t>(memory_length - access_size))
                                  {
                                      return false;
                                  }

                                  fn(memory_begin, static_cast<::std::size_t>(effective_offset.offset));
                                  return true;
                              }};

    if constexpr(MemoryT::can_mmap)
    {
        return ::uwvm2::object::memory::linear::with_memory_access_snapshot(memory,
                                                                            [&](::std::byte* memory_begin, ::std::size_t memory_length) constexpr noexcept
                                                                            { return checked_access(memory_begin, memory_length); });
    }
    else if constexpr(MemoryT::support_multi_thread)
    {
#if __cpp_lib_atomic_wait >= 201907L
        [[maybe_unused]] ::uwvm2::object::memory::linear::memory_operation_guard_t guard{memory.growing_flag_p, memory.active_ops_p};
#else
        static_assert(!MemoryT::support_multi_thread);
#endif
        return checked_access(memory.memory_begin, memory.memory_length);
    }
    else
    {
        return checked_access(memory.memory_begin, memory.memory_length);
    }
}

// Convenience wrapper around checked native memory access that decodes the memory pointer address and emits a detailed
// trap on failure.
template <typename Fn>
inline constexpr void llvm_jit_with_checked_memory_access(::std::uintptr_t memory_address,
                                                          validation_module_traits_t::wasm_u32 static_offset,
                                                          runtime_wasm_i32 address,
                                                          ::std::size_t access_size,
                                                          Fn&& fn) noexcept
{
    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(0uz, static_offset, effective_offset, 0uz, access_size); }

    ::std::size_t memory_length{};
    if(!llvm_jit_try_checked_memory_access(*memory_p, effective_offset, access_size, memory_length, ::std::forward<Fn>(fn))) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(0uz, static_offset, effective_offset, memory_length, access_size);
    }
}

// Native memory load bridge used when direct LLVM loads are not legal or profitable.  It receives the runtime native-memory
// object address, the static memarg offset, and the Wasm32 dynamic address.  The bridge centralizes the exact fallback
// semantics that direct IR must match: checked effective-address computation, snapshot/guard-aware bounds checking,
// unaligned little-endian byte loads, integer sign/zero extension, and bit-preserving floating-point reconstruction.
template <typename ResultType, ::std::size_t LoadBytes, bool Signed = false>
[[nodiscard]] inline constexpr ResultType
    llvm_jit_memory_load_bridge(::std::uintptr_t memory_address, validation_module_traits_t::wasm_u32 static_offset, runtime_wasm_i32 address) noexcept
{
    // The checked-access wrapper either invokes the lambda with an in-bounds byte pointer or reports a memory trap that
    // terminates execution.  Keeping a default-initialized result makes the function structurally total for the compiler,
    // while successful accesses overwrite it before returning.
    ResultType result{};
    llvm_jit_with_checked_memory_access(
        memory_address,
        static_offset,
        address,
        LoadBytes,
        [&](::std::byte* memory_begin, ::std::size_t effective_offset) constexpr noexcept
        {
            // At this point the runtime memory snapshot/guard is active and the range `[effective_offset, +LoadBytes)` is
            // known in-bounds.  The helper still reads through `std::byte` + memcpy so host alignment never leaks into Wasm
            // semantics.
            auto load_ptr{memory_begin + effective_offset};

            if constexpr(::std::same_as<ResultType, runtime_wasm_i32>)
            {
                // i32.load8/16_s and i32.load8/16_u load fewer bytes than the result type.  Wasm defines the extension
                // explicitly, so choose signed or unsigned widening from the loaded little-endian payload.
                if constexpr(LoadBytes == 1uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
                    if constexpr(Signed) { result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(value))); }
                    else
                    {
                        result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 2uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
                    if constexpr(Signed)
                    {
                        result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(value)));
                    }
                    else
                    {
                        result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 4uz)
                {
                    // Full-width integer loads are bit-preserving: read the four little-endian bytes and reinterpret them
                    // as the runtime i32 payload without an arithmetic conversion step.
                    result = ::std::bit_cast<runtime_wasm_i32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
                }
            }
            else if constexpr(::std::same_as<ResultType, runtime_wasm_i64>)
            {
                // i64.load8/16/32_s and i64.load8/16/32_u mirror the i32 path but widen to 64 bits.  The intermediate
                // signed type is chosen to the loaded width so sign extension happens exactly once and at the Wasm width.
                if constexpr(LoadBytes == 1uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
                    if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least8_t>(value))); }
                    else
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 2uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
                    if constexpr(Signed)
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least16_t>(value)));
                    }
                    else
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 4uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr)};
                    if constexpr(Signed)
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(value)));
                    }
                    else
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 8uz)
                {
                    // Full-width i64 load is also bit-preserving; no signedness is involved for the 8-byte form.
                    result = ::std::bit_cast<runtime_wasm_i64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
                }
            }
            else if constexpr(::std::same_as<ResultType, runtime_wasm_f32>)
            {
                // Floating-point loads are not numeric conversions.  WebAssembly loads raw IEEE-754 payload bytes from
                // memory, so NaN payloads, sign bits, and signed zero must survive unchanged.
                static_assert(LoadBytes == 4uz);
                result = ::std::bit_cast<runtime_wasm_f32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
            }
            else if constexpr(::std::same_as<ResultType, runtime_wasm_f64>)
            {
                // Same payload-preserving rule for f64; only the loaded byte width changes.
                static_assert(LoadBytes == 8uz);
                result = ::std::bit_cast<runtime_wasm_f64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
            }
        });
    return result;
}

// Local-imported memory load bridge.  The provider module performs the actual read so its own locking/snapshot rules
// remain active for the duration of the access.
template <typename ResultType, ::std::size_t LoadBytes, bool Signed = false>
[[nodiscard]] inline constexpr ResultType llvm_jit_local_imported_memory_load_bridge(::std::uintptr_t local_imported_module_address,
                                                                                     ::std::size_t memory_index,
                                                                                     validation_module_traits_t::wasm_u32 static_offset,
                                                                                     runtime_wasm_i32 address) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    if(effective_offset.offset_65_bit) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    ::std::byte load_buffer[LoadBytes]{};
    if(!local_imported_module->memory_read_from_index(memory_index, effective_offset.offset, load_buffer, LoadBytes)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }

    ResultType result{};
    auto load_ptr{load_buffer};

    if constexpr(::std::same_as<ResultType, runtime_wasm_i32>)
    {
        if constexpr(LoadBytes == 1uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
            }
        }
        else if constexpr(LoadBytes == 2uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
            }
        }
        else if constexpr(LoadBytes == 4uz)
        {
            result = ::std::bit_cast<runtime_wasm_i32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
        }
    }
    else if constexpr(::std::same_as<ResultType, runtime_wasm_i64>)
    {
        if constexpr(LoadBytes == 1uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least8_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
            }
        }
        else if constexpr(LoadBytes == 2uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least16_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
            }
        }
        else if constexpr(LoadBytes == 4uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
            }
        }
        else if constexpr(LoadBytes == 8uz)
        {
            result = ::std::bit_cast<runtime_wasm_i64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
        }
    }
    else if constexpr(::std::same_as<ResultType, runtime_wasm_f32>)
    {
        static_assert(LoadBytes == 4uz);
        result = ::std::bit_cast<runtime_wasm_f32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
    }
    else if constexpr(::std::same_as<ResultType, runtime_wasm_f64>)
    {
        static_assert(LoadBytes == 8uz);
        result = ::std::bit_cast<runtime_wasm_f64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
    }

    return result;
}

// Native memory store bridge used when direct LLVM stores are unavailable.  Integer stores truncate according to the Wasm
// opcode width, and float stores preserve the IEEE bit pattern.
template <typename ValueType, ::std::size_t StoreBytes>
inline constexpr void llvm_jit_memory_store_bridge(::std::uintptr_t memory_address,
                                                   validation_module_traits_t::wasm_u32 static_offset,
                                                   runtime_wasm_i32 address,
                                                   ValueType value) noexcept
{
    llvm_jit_with_checked_memory_access(memory_address,
                                        static_offset,
                                        address,
                                        StoreBytes,
                                        [&](::std::byte* memory_begin, ::std::size_t effective_offset) constexpr noexcept
                                        {
                                            auto store_ptr{memory_begin + effective_offset};

                                            if constexpr(::std::same_as<ValueType, runtime_wasm_i32>)
                                            {
                                                auto const unsigned_value{::std::bit_cast<::std::uint_least32_t>(value)};
                                                if constexpr(StoreBytes == 1uz)
                                                {
                                                    auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
                                                    ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
                                                }
                                                else if constexpr(StoreBytes == 2uz)
                                                {
                                                    llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value));
                                                }
                                                else if constexpr(StoreBytes == 4uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
                                            }
                                            else if constexpr(::std::same_as<ValueType, runtime_wasm_i64>)
                                            {
                                                auto const unsigned_value{::std::bit_cast<::std::uint_least64_t>(value)};
                                                if constexpr(StoreBytes == 1uz)
                                                {
                                                    auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
                                                    ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
                                                }
                                                else if constexpr(StoreBytes == 2uz)
                                                {
                                                    llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value));
                                                }
                                                else if constexpr(StoreBytes == 4uz)
                                                {
                                                    llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least32_t>(unsigned_value));
                                                }
                                                else if constexpr(StoreBytes == 8uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
                                            }
                                            else if constexpr(::std::same_as<ValueType, runtime_wasm_f32>)
                                            {
                                                static_assert(StoreBytes == 4uz);
                                                llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least32_t>(value));
                                            }
                                            else if constexpr(::std::same_as<ValueType, runtime_wasm_f64>)
                                            {
                                                static_assert(StoreBytes == 8uz);
                                                llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least64_t>(value));
                                            }
                                        });
}

// Local-imported memory store bridge.  The value is serialized into a fixed byte buffer before calling the provider.
template <typename ValueType, ::std::size_t StoreBytes>
inline constexpr void llvm_jit_local_imported_memory_store_bridge(::std::uintptr_t local_imported_module_address,
                                                                  ::std::size_t memory_index,
                                                                  validation_module_traits_t::wasm_u32 static_offset,
                                                                  runtime_wasm_i32 address,
                                                                  ValueType value) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    if(effective_offset.offset_65_bit) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    ::std::byte store_buffer[StoreBytes]{};
    auto store_ptr{store_buffer};

    if constexpr(::std::same_as<ValueType, runtime_wasm_i32>)
    {
        auto const unsigned_value{::std::bit_cast<::std::uint_least32_t>(value)};
        if constexpr(StoreBytes == 1uz)
        {
            auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
            ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
        }
        else if constexpr(StoreBytes == 2uz) { llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value)); }
        else if constexpr(StoreBytes == 4uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
    }
    else if constexpr(::std::same_as<ValueType, runtime_wasm_i64>)
    {
        auto const unsigned_value{::std::bit_cast<::std::uint_least64_t>(value)};
        if constexpr(StoreBytes == 1uz)
        {
            auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
            ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
        }
        else if constexpr(StoreBytes == 2uz) { llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value)); }
        else if constexpr(StoreBytes == 4uz) { llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least32_t>(unsigned_value)); }
        else if constexpr(StoreBytes == 8uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
    }
    else if constexpr(::std::same_as<ValueType, runtime_wasm_f32>)
    {
        static_assert(StoreBytes == 4uz);
        llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least32_t>(value));
    }
    else if constexpr(::std::same_as<ValueType, runtime_wasm_f64>)
    {
        static_assert(StoreBytes == 8uz);
        llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least64_t>(value));
    }

    if(!local_imported_module->memory_write_to_index(memory_index, effective_offset.offset, store_buffer, StoreBytes)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }
}

// SIMD bridges deliberately exchange v128 through explicitly addressed 16-byte buffers. This makes the generated ABI
// independent of each platform's native vector calling convention while reusing the exact uwvm-int SIMD semantics.
namespace llvm_jit_simd_details = ::uwvm2::runtime::compiler::shared::wasm1p1_simd_details;
using llvm_jit_simd_code = llvm_jit_simd_details::simd_code;

template <typename ValueType>
[[nodiscard]] UWVM_ALWAYS_INLINE inline constexpr ValueType llvm_jit_simd_read_bridge_value(::std::uintptr_t address) noexcept
{
    auto ptr{reinterpret_cast<void const*>(address)};
    if(ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    ValueType value{};
    ::std::memcpy(::std::addressof(value), ptr, sizeof(value));
    return value;
}

template <typename ValueType>
UWVM_ALWAYS_INLINE inline constexpr void llvm_jit_simd_write_bridge_value(::std::uintptr_t address, ValueType const& value) noexcept
{
    auto ptr{reinterpret_cast<void*>(address)};
    if(ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    ::std::memcpy(ptr, ::std::addressof(value), sizeof(value));
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_unary_bridge(::std::uintptr_t result_address, ::std::uintptr_t operand_address) noexcept
{
    auto const operand{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(operand_address)};
    auto const result{llvm_jit_simd_details::eval_full_unop<Op>(operand)};
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_binary_bridge(::std::uintptr_t result_address,
                                                  ::std::uintptr_t lhs_address,
                                                  ::std::uintptr_t rhs_address) noexcept
{
    auto const lhs{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(lhs_address)};
    auto const rhs{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(rhs_address)};
    auto const result{llvm_jit_simd_details::eval_full_binop<Op>(lhs, rhs)};
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_ternary_bridge(::std::uintptr_t result_address,
                                                   ::std::uintptr_t lhs_address,
                                                   ::std::uintptr_t rhs_address,
                                                   ::std::uintptr_t mask_address) noexcept
{
    static_assert(Op == llvm_jit_simd_code::v128_bitselect);
    auto const lhs{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(lhs_address)};
    auto const rhs{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(rhs_address)};
    auto const mask{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(mask_address)};
    auto const result{llvm_jit_simd_details::eval_bitselect(lhs, rhs, mask)};
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op>
[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_simd_test_bridge(::std::uintptr_t operand_address) noexcept
{
    auto const operand{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(operand_address)};
    return llvm_jit_simd_details::eval_full_test<Op>(operand);
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_shift_bridge(::std::uintptr_t result_address,
                                                 ::std::uintptr_t operand_address,
                                                 runtime_wasm_i32 count) noexcept
{
    auto const operand{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(operand_address)};
    auto const result{llvm_jit_simd_details::eval_full_shift<Op>(operand, count)};
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op, typename ScalarType>
inline constexpr void llvm_jit_simd_splat_bridge(::std::uintptr_t result_address, ScalarType scalar) noexcept
{
    runtime_wasm_v128 result{};
    if constexpr(::std::same_as<ScalarType, runtime_wasm_i32>) { result = llvm_jit_simd_details::eval_full_splat_i32<Op>(scalar); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_i64>) { result = llvm_jit_simd_details::eval_full_splat_i64<Op>(scalar); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_f32>) { result = llvm_jit_simd_details::eval_full_splat_f32<Op>(scalar); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_f64>) { result = llvm_jit_simd_details::eval_full_splat_f64<Op>(scalar); }
    else { static_assert(!::std::same_as<ScalarType, ScalarType>, "unsupported SIMD splat scalar"); }
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op, typename ScalarType>
[[nodiscard]] inline constexpr ScalarType llvm_jit_simd_extract_lane_bridge(::std::uintptr_t operand_address,
                                                                            runtime_wasm_u32 lane) noexcept
{
    auto const operand{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(operand_address)};
    auto const lane_u8{static_cast<llvm_jit_simd_details::u8>(lane)};
    if constexpr(::std::same_as<ScalarType, runtime_wasm_i32>) { return llvm_jit_simd_details::eval_extract_lane_i32<Op>(operand, lane_u8); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_i64>) { return llvm_jit_simd_details::eval_extract_lane_i64<Op>(operand, lane_u8); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_f32>) { return llvm_jit_simd_details::eval_extract_lane_f32<Op>(operand, lane_u8); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_f64>) { return llvm_jit_simd_details::eval_extract_lane_f64<Op>(operand, lane_u8); }
    else { static_assert(!::std::same_as<ScalarType, ScalarType>, "unsupported SIMD extract-lane scalar"); }
}

template <llvm_jit_simd_code Op, typename ScalarType>
inline constexpr void llvm_jit_simd_replace_lane_bridge(::std::uintptr_t result_address,
                                                        ::std::uintptr_t operand_address,
                                                        ScalarType scalar,
                                                        runtime_wasm_u32 lane) noexcept
{
    auto const operand{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(operand_address)};
    auto const lane_u8{static_cast<llvm_jit_simd_details::u8>(lane)};
    runtime_wasm_v128 result{};
    if constexpr(::std::same_as<ScalarType, runtime_wasm_i32>) { result = llvm_jit_simd_details::eval_replace_lane_i32<Op>(operand, scalar, lane_u8); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_i64>) { result = llvm_jit_simd_details::eval_replace_lane_i64<Op>(operand, scalar, lane_u8); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_f32>) { result = llvm_jit_simd_details::eval_replace_lane_f32<Op>(operand, scalar, lane_u8); }
    else if constexpr(::std::same_as<ScalarType, runtime_wasm_f64>) { result = llvm_jit_simd_details::eval_replace_lane_f64<Op>(operand, scalar, lane_u8); }
    else { static_assert(!::std::same_as<ScalarType, ScalarType>, "unsupported SIMD replace-lane scalar"); }
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_shuffle_bridge(::std::uintptr_t result_address,
                                                   ::std::uintptr_t lhs_address,
                                                   ::std::uintptr_t rhs_address,
                                                   ::std::uintptr_t lane_address) noexcept
{
    static_assert(Op == llvm_jit_simd_code::i8x16_shuffle);
    auto const lhs{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(lhs_address)};
    auto const rhs{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(rhs_address)};
    auto lane_ptr{reinterpret_cast<llvm_jit_simd_details::u8 const*>(lane_address)};
    if(lane_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    llvm_jit_simd_details::u8 lanes[16]{};
    ::std::memcpy(lanes, lane_ptr, sizeof(lanes));
    auto const controls{llvm_jit_simd_details::make_shuffle_controls(lanes)};
    auto const result{llvm_jit_simd_details::eval_shuffle(lhs, rhs, controls)};
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_memory_load_bridge(::std::uintptr_t memory_address,
                                                       runtime_wasm_u32 static_offset,
                                                       runtime_wasm_i32 address,
                                                       ::std::uintptr_t old_value_address,
                                                       runtime_wasm_u32 lane,
                                                       ::std::uintptr_t result_address) noexcept
{
    runtime_wasm_v128 old_value{};
    if constexpr(Op == llvm_jit_simd_code::v128_load8_lane || Op == llvm_jit_simd_code::v128_load16_lane ||
                 Op == llvm_jit_simd_code::v128_load32_lane || Op == llvm_jit_simd_code::v128_load64_lane)
    {
        old_value = llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(old_value_address);
    }
    runtime_wasm_v128 result{};
    constexpr auto access_size{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    llvm_jit_with_checked_memory_access(memory_address,
                                        static_offset,
                                        address,
                                        access_size,
                                        [&](::std::byte* memory_begin, ::std::size_t effective_offset) constexpr noexcept
                                        {
                                            result = llvm_jit_simd_details::eval_memory_load<Op>(
                                                memory_begin + effective_offset,
                                                old_value,
                                                static_cast<llvm_jit_simd_details::u8>(lane));
                                        });
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_local_imported_memory_load_bridge(::std::uintptr_t local_imported_module_address,
                                                                      ::std::size_t memory_index,
                                                                      runtime_wasm_u32 static_offset,
                                                                      runtime_wasm_i32 address,
                                                                      ::std::uintptr_t old_value_address,
                                                                      runtime_wasm_u32 lane,
                                                                      ::std::uintptr_t result_address) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }
    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    if(effective_offset.offset_65_bit) [[unlikely]] { llvm_jit_memory_bridge_trap(); }
    constexpr auto access_size{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    ::std::byte bytes[16]{};
    if(!local_imported_module->memory_read_from_index(memory_index, effective_offset.offset, bytes, access_size)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }
    runtime_wasm_v128 old_value{};
    if constexpr(Op == llvm_jit_simd_code::v128_load8_lane || Op == llvm_jit_simd_code::v128_load16_lane ||
                 Op == llvm_jit_simd_code::v128_load32_lane || Op == llvm_jit_simd_code::v128_load64_lane)
    {
        old_value = llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(old_value_address);
    }
    auto const result{llvm_jit_simd_details::eval_memory_load<Op>(bytes, old_value, static_cast<llvm_jit_simd_details::u8>(lane))};
    llvm_jit_simd_write_bridge_value(result_address, result);
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_memory_store_bridge(::std::uintptr_t memory_address,
                                                        runtime_wasm_u32 static_offset,
                                                        runtime_wasm_i32 address,
                                                        ::std::uintptr_t value_address,
                                                        runtime_wasm_u32 lane) noexcept
{
    auto const value{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(value_address)};
    constexpr auto access_size{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    llvm_jit_with_checked_memory_access(memory_address,
                                        static_offset,
                                        address,
                                        access_size,
                                        [&](::std::byte* memory_begin, ::std::size_t effective_offset) constexpr noexcept
                                        {
                                            llvm_jit_simd_details::eval_memory_store<Op>(
                                                memory_begin + effective_offset,
                                                value,
                                                static_cast<llvm_jit_simd_details::u8>(lane));
                                        });
}

template <llvm_jit_simd_code Op>
inline constexpr void llvm_jit_simd_local_imported_memory_store_bridge(::std::uintptr_t local_imported_module_address,
                                                                       ::std::size_t memory_index,
                                                                       runtime_wasm_u32 static_offset,
                                                                       runtime_wasm_i32 address,
                                                                       ::std::uintptr_t value_address,
                                                                       runtime_wasm_u32 lane) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }
    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    if(effective_offset.offset_65_bit) [[unlikely]] { llvm_jit_memory_bridge_trap(); }
    auto const value{llvm_jit_simd_read_bridge_value<runtime_wasm_v128>(value_address)};
    constexpr auto access_size{llvm_jit_simd_details::simd_memory_access_size<Op>()};
    ::std::byte bytes[16]{};
    llvm_jit_simd_details::eval_memory_store<Op>(bytes, value, static_cast<llvm_jit_simd_details::u8>(lane));
    if(!local_imported_module->memory_write_to_index(memory_index, effective_offset.offset, bytes, access_size)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }
}

[[nodiscard]] inline constexpr ::std::uint_least32_t llvm_jit_wasm_i32_bits_to_u32(runtime_wasm_i32 value) noexcept
{
    using unsigned_wasm_i32 = ::std::uint32_t;
    static_assert(sizeof(unsigned_wasm_i32) == sizeof(runtime_wasm_i32));
    return ::std::bit_cast<unsigned_wasm_i32>(value);
}

[[nodiscard]] inline constexpr bool llvm_jit_bulk_memory_range_oob(::std::size_t begin, ::std::size_t len, ::std::size_t bound) noexcept
{ return begin > bound || len > bound - begin; }

template <typename MemoryT, typename Fn>
[[nodiscard]] inline constexpr bool llvm_jit_with_bulk_memory_snapshot(MemoryT const& memory, Fn&& fn) noexcept
{
    if constexpr(MemoryT::can_mmap)
    {
        return ::uwvm2::object::memory::linear::with_memory_access_snapshot(memory,
                                                                            [&](::std::byte* memory_begin, ::std::size_t memory_length) constexpr noexcept
                                                                            { return fn(memory_begin, memory_length); });
    }
    else if constexpr(MemoryT::support_multi_thread)
    {
#if __cpp_lib_atomic_wait >= 201907L
        [[maybe_unused]] ::uwvm2::object::memory::linear::memory_operation_guard_t guard{memory.growing_flag_p, memory.active_ops_p};
#else
        static_assert(!MemoryT::support_multi_thread);
#endif
        return fn(memory.memory_begin, memory.memory_length);
    }
    else
    {
        return fn(memory.memory_begin, memory.memory_length);
    }
}

// Resolve the current byte length of a local-imported memory without retaining the provider's raw snapshot pointer.
// Local-imported memories can relocate while growing, so bulk operations validate the complete range from this snapshot
// and then perform the actual transfer through provider-owned read/write calls.  Memory can grow but cannot shrink,
// therefore an in-bounds range remains valid after this check without exposing a stale allocation address.
[[nodiscard]] inline constexpr bool
    llvm_jit_local_imported_memory_byte_length(::uwvm2::uwvm::wasm::type::local_imported_t* local_imported_module,
                                               ::std::size_t memory_index,
                                               ::std::size_t& byte_length_out) noexcept
{
    if(local_imported_module == nullptr) [[unlikely]] { return false; }

    ::uwvm2::uwvm::wasm::type::memory_access_snapshot_result_t snapshot{};
    if(!local_imported_module->memory_access_snapshot_from_index(memory_index, snapshot)) [[unlikely]] { return false; }

    auto const page_size_bytes_u64{local_imported_module->memory_page_size_from_index(memory_index)};
    if(page_size_bytes_u64 == 0u) [[unlikely]] { return false; }
    if constexpr(::std::numeric_limits<::std::size_t>::digits < ::std::numeric_limits<::std::uint_least64_t>::digits)
    {
        if(page_size_bytes_u64 > static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::size_t>::max)())) [[unlikely]] { return false; }
    }

    auto const page_size_bytes{static_cast<::std::size_t>(page_size_bytes_u64)};
    auto const max_page_count{(::std::numeric_limits<::std::size_t>::max)() / page_size_bytes};
    if constexpr(::std::numeric_limits<::std::size_t>::digits < ::std::numeric_limits<::std::uint_least64_t>::digits)
    {
        if(snapshot.page_count > static_cast<::std::uint_least64_t>(max_page_count)) [[unlikely]] { return false; }
    }
    else
    {
        if(static_cast<::std::size_t>(snapshot.page_count) > max_page_count) [[unlikely]] { return false; }
    }

    byte_length_out = static_cast<::std::size_t>(snapshot.page_count) * page_size_bytes;
    return true;
}

inline constexpr void llvm_jit_memory_copy_bridge(::std::uintptr_t memory_address,
                                                  runtime_wasm_i32 dst_i32,
                                                  runtime_wasm_i32 src_i32,
                                                  runtime_wasm_i32 len_i32) noexcept
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const src{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(src_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};

    if(memory_p == nullptr) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(0uz, 0u, {.offset = dst, .offset_65_bit = false}, 0uz, len);
    }

    static_cast<void>(llvm_jit_with_bulk_memory_snapshot(*memory_p,
                                                         [&](::std::byte* memory_begin, ::std::size_t memory_length) constexpr noexcept
                                                         {
                                                             if(llvm_jit_bulk_memory_range_oob(src, len, memory_length)) [[unlikely]]
                                                             {
                                                                 llvm_jit_memory_bridge_trap(0uz,
                                                                                             0u,
                                                                                             {.offset = src, .offset_65_bit = false},
                                                                                             memory_length,
                                                                                             len);
                                                             }
                                                             if(llvm_jit_bulk_memory_range_oob(dst, len, memory_length)) [[unlikely]]
                                                             {
                                                                 llvm_jit_memory_bridge_trap(0uz,
                                                                                             0u,
                                                                                             {.offset = dst, .offset_65_bit = false},
                                                                                             memory_length,
                                                                                             len);
                                                             }
                                                             if(len != 0uz)
                                                             {
                                                                 if(memory_begin == nullptr) [[unlikely]]
                                                                 {
                                                                     llvm_jit_memory_bridge_trap(0uz,
                                                                                                 0u,
                                                                                                 {.offset = dst, .offset_65_bit = false},
                                                                                                 memory_length,
                                                                                                 len);
                                                                 }
                                                                 ::std::memmove(memory_begin + dst, memory_begin + src, len);
                                                             }
                                                             return true;
                                                         }));
}

inline constexpr void llvm_jit_memory_fill_bridge(::std::uintptr_t memory_address,
                                                  runtime_wasm_i32 dst_i32,
                                                  runtime_wasm_i32 value_i32,
                                                  runtime_wasm_i32 len_i32) noexcept
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const value{static_cast<int>(static_cast<unsigned char>(llvm_jit_wasm_i32_bits_to_u32(value_i32)))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};

    if(memory_p == nullptr) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(0uz, 0u, {.offset = dst, .offset_65_bit = false}, 0uz, len);
    }

    static_cast<void>(llvm_jit_with_bulk_memory_snapshot(*memory_p,
                                                         [&](::std::byte* memory_begin, ::std::size_t memory_length) constexpr noexcept
                                                         {
                                                             if(llvm_jit_bulk_memory_range_oob(dst, len, memory_length)) [[unlikely]]
                                                             {
                                                                 llvm_jit_memory_bridge_trap(0uz,
                                                                                             0u,
                                                                                             {.offset = dst, .offset_65_bit = false},
                                                                                             memory_length,
                                                                                             len);
                                                             }
                                                             if(len != 0uz)
                                                             {
                                                                 if(memory_begin == nullptr) [[unlikely]]
                                                                 {
                                                                     llvm_jit_memory_bridge_trap(0uz,
                                                                                                 0u,
                                                                                                 {.offset = dst, .offset_65_bit = false},
                                                                                                 memory_length,
                                                                                                 len);
                                                                 }
                                                                 ::std::memset(memory_begin + dst, value, len);
                                                             }
                                                             return true;
                                                         }));
}

// Local-imported memory.copy bridge.  A fixed staging buffer keeps the provider's lock/snapshot discipline inside each
// read/write call.  Copy direction is selected exactly like memmove so overlapping ranges retain WebAssembly semantics.
inline constexpr void llvm_jit_local_imported_memory_copy_bridge(::std::uintptr_t local_imported_module_address,
                                                                 ::std::size_t memory_index,
                                                                 runtime_wasm_i32 dst_i32,
                                                                 runtime_wasm_i32 src_i32,
                                                                 runtime_wasm_i32 len_i32) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const src{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(src_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};

    ::std::size_t memory_length{};
    if(!llvm_jit_local_imported_memory_byte_length(local_imported_module, memory_index, memory_length) ||
       llvm_jit_bulk_memory_range_oob(src, len, memory_length) || llvm_jit_bulk_memory_range_oob(dst, len, memory_length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
        return;
    }
    if(len == 0uz) { return; }

    constexpr ::std::size_t staging_capacity{4096uz};
    ::std::byte staging[staging_capacity]{};
    if(dst > src && dst - src < len)
    {
        // The destination begins inside the source range: walk backwards so a completed write cannot clobber a later read.
        ::std::size_t remaining{len};
        while(remaining != 0uz)
        {
            auto const chunk_size{remaining < staging_capacity ? remaining : staging_capacity};
            auto const chunk_offset{remaining - chunk_size};
            if(!local_imported_module->memory_read_from_index(memory_index, src + chunk_offset, staging, chunk_size) ||
               !local_imported_module->memory_write_to_index(memory_index, dst + chunk_offset, staging, chunk_size)) [[unlikely]]
            {
                llvm_jit_memory_bridge_trap();
                return;
            }
            remaining = chunk_offset;
        }
        return;
    }

    // Forward copying covers non-overlap and the case where the destination begins before the source.
    ::std::size_t copied{};
    while(copied != len)
    {
        auto const remaining{len - copied};
        auto const chunk_size{remaining < staging_capacity ? remaining : staging_capacity};
        if(!local_imported_module->memory_read_from_index(memory_index, src + copied, staging, chunk_size) ||
           !local_imported_module->memory_write_to_index(memory_index, dst + copied, staging, chunk_size)) [[unlikely]]
        {
            llvm_jit_memory_bridge_trap();
            return;
        }
        copied += chunk_size;
    }
}

// Local-imported memory.fill bridge.  Validate the complete destination range before the first provider write so an
// out-of-bounds instruction cannot leave a partially filled memory.
inline constexpr void llvm_jit_local_imported_memory_fill_bridge(::std::uintptr_t local_imported_module_address,
                                                                 ::std::size_t memory_index,
                                                                 runtime_wasm_i32 dst_i32,
                                                                 runtime_wasm_i32 value_i32,
                                                                 runtime_wasm_i32 len_i32) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const value{static_cast<int>(static_cast<unsigned char>(llvm_jit_wasm_i32_bits_to_u32(value_i32)))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};

    ::std::size_t memory_length{};
    if(!llvm_jit_local_imported_memory_byte_length(local_imported_module, memory_index, memory_length) ||
       llvm_jit_bulk_memory_range_oob(dst, len, memory_length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
        return;
    }
    if(len == 0uz) { return; }

    constexpr ::std::size_t staging_capacity{4096uz};
    ::std::byte staging[staging_capacity]{};
    ::std::memset(staging, value, staging_capacity);
    ::std::size_t filled{};
    while(filled != len)
    {
        auto const remaining{len - filled};
        auto const chunk_size{remaining < staging_capacity ? remaining : staging_capacity};
        if(!local_imported_module->memory_write_to_index(memory_index, dst + filled, staging, chunk_size)) [[unlikely]]
        {
            llvm_jit_memory_bridge_trap();
            return;
        }
        filled += chunk_size;
    }
}

// Drop one data instance without routing the complete function through the interpreter.  The module address is stable
// for the lifetime of generated code and the data index has already been checked by the authoritative wasm2 validator.
inline constexpr void llvm_jit_data_drop_bridge(::std::uintptr_t runtime_module_address, runtime_wasm_u32 data_index) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr || static_cast<::std::size_t>(data_index) >= runtime_module->local_defined_data_vec_storage.size()) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }

    auto& data{runtime_module->local_defined_data_vec_storage.index_unchecked(static_cast<::std::size_t>(data_index)).data};
    ::uwvm2::uwvm::runtime::storage::drop_wasm_data_segment_payload(data);
}

// Copy a range from a data instance into memory 0.  Both the data-source range and destination range trap exactly like
// the WebAssembly memory.init instruction; a dropped data instance therefore has length zero.
inline constexpr void llvm_jit_memory_init_bridge(::std::uintptr_t memory_address,
                                                  ::std::uintptr_t runtime_module_address,
                                                  runtime_wasm_u32 data_index,
                                                  runtime_wasm_i32 dst_i32,
                                                  runtime_wasm_i32 src_i32,
                                                  runtime_wasm_i32 len_i32) noexcept
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const src{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(src_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};

    if(memory_p == nullptr || runtime_module == nullptr ||
       static_cast<::std::size_t>(data_index) >= runtime_module->local_defined_data_vec_storage.size()) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(0uz, 0u, {.offset = dst, .offset_65_bit = false}, 0uz, len);
    }

    auto const& data{runtime_module->local_defined_data_vec_storage.index_unchecked(static_cast<::std::size_t>(data_index)).data};
    auto const data_begin{data.byte_begin};
    auto const data_end{data.byte_end};
    if((data_begin == nullptr) != (data_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const data_length{data_begin == nullptr ? 0uz : static_cast<::std::size_t>(data_end - data_begin)};

    if(llvm_jit_bulk_memory_range_oob(src, len, data_length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap(0uz, 0u, {.offset = src, .offset_65_bit = false}, data_length, len);
    }

    static_cast<void>(llvm_jit_with_bulk_memory_snapshot(*memory_p,
                                                         [&](::std::byte* memory_begin, ::std::size_t memory_length) constexpr noexcept
                                                         {
                                                             if(llvm_jit_bulk_memory_range_oob(dst, len, memory_length)) [[unlikely]]
                                                             {
                                                                 llvm_jit_memory_bridge_trap(0uz,
                                                                                             0u,
                                                                                             {.offset = dst, .offset_65_bit = false},
                                                                                             memory_length,
                                                                                             len);
                                                             }
                                                             if(len != 0uz)
                                                             {
                                                                 if(memory_begin == nullptr || data_begin == nullptr) [[unlikely]]
                                                                 {
                                                                     llvm_jit_memory_bridge_trap(0uz,
                                                                                                 0u,
                                                                                                 {.offset = dst, .offset_65_bit = false},
                                                                                                 memory_length,
                                                                                                 len);
                                                                 }
                                                                 ::std::memcpy(memory_begin + dst, data_begin + src, len);
                                                             }
                                                             return true;
                                                         }));
}

// Local-imported memory.init bridge.  The passive data instance remains owned by the importing runtime module, while the
// destination provider owns the actual memory write.  Source and destination ranges are both validated before the
// provider is called; after data.drop the source length is zero, preserving the WebAssembly data-instance semantics.
inline constexpr void llvm_jit_local_imported_memory_init_bridge(::std::uintptr_t local_imported_module_address,
                                                                 ::std::size_t memory_index,
                                                                 ::std::uintptr_t runtime_module_address,
                                                                 runtime_wasm_u32 data_index,
                                                                 runtime_wasm_i32 dst_i32,
                                                                 runtime_wasm_i32 src_i32,
                                                                 runtime_wasm_i32 len_i32) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const src{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(src_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};

    if(local_imported_module == nullptr || runtime_module == nullptr ||
       static_cast<::std::size_t>(data_index) >= runtime_module->local_defined_data_vec_storage.size()) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
        return;
    }

    auto const& data{runtime_module->local_defined_data_vec_storage.index_unchecked(static_cast<::std::size_t>(data_index)).data};
    auto const data_begin{data.byte_begin};
    auto const data_end{data.byte_end};
    if((data_begin == nullptr) != (data_end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const data_length{data_begin == nullptr ? 0uz : static_cast<::std::size_t>(data_end - data_begin)};
    if(llvm_jit_bulk_memory_range_oob(src, len, data_length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
        return;
    }

    ::std::size_t memory_length{};
    if(!llvm_jit_local_imported_memory_byte_length(local_imported_module, memory_index, memory_length) ||
       llvm_jit_bulk_memory_range_oob(dst, len, memory_length)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
        return;
    }
    if(len == 0uz) { return; }

    if(data_begin == nullptr || !local_imported_module->memory_write_to_index(memory_index, dst, data_begin + src, len)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
        return;
    }
}

// Reference/table bridge helpers keep the generated ABI target-independent. LLVM holds a reference as opaque integer
// bits, while every C++ bridge reads or writes the real runtime object through an explicitly passed buffer address.
[[noreturn]] inline constexpr void llvm_jit_table_out_of_bounds_bridge_trap() noexcept
{
    ::uwvm2::runtime::lib::llvm_jit_runtime_trap(::uwvm2::runtime::lib::llvm_jit_trap_kind::table_out_of_bounds, 0u, 0u);
    ::fast_io::fast_terminate();
}

[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_wasm_u32_bits_to_i32(::std::uint_least32_t value) noexcept
{
    static_assert(sizeof(value) == sizeof(runtime_wasm_i32));
    return ::std::bit_cast<runtime_wasm_i32>(value);
}

[[nodiscard]] inline constexpr runtime_table_storage_t*
    llvm_jit_resolve_mutable_runtime_table(runtime_module_storage_t& runtime_module, runtime_wasm_u32 table_index) noexcept
{
    return const_cast<runtime_table_storage_t*>(resolve_runtime_table_storage(runtime_module, table_index));
}

[[nodiscard]] inline constexpr bool llvm_jit_runtime_table_is_funcref(runtime_table_storage_t const& table) noexcept
{
    return table.table_type_ptr != nullptr &&
           table.table_type_ptr->reftype == ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::funcref;
}

[[nodiscard]] inline constexpr bool llvm_jit_runtime_table_is_externref(runtime_table_storage_t const& table) noexcept
{
    return table.table_type_ptr != nullptr &&
           table.table_type_ptr->reftype == ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type::externref;
}

[[nodiscard]] inline constexpr runtime_table_elem_storage_t
    llvm_jit_resolve_table_elem_from_func_index(runtime_module_storage_t const& module, runtime_wasm_u32 func_index) noexcept
{
    using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

    auto const index{static_cast<::std::size_t>(func_index)};
    auto const imported_count{module.imported_function_vec_storage.size()};
    auto const local_count{module.local_defined_function_vec_storage.size()};
    if(index >= imported_count + local_count) [[unlikely]] { ::fast_io::fast_terminate(); }

    runtime_table_elem_storage_t result{};
    if(index < imported_count)
    {
        result.storage.imported_ptr = ::std::addressof(module.imported_function_vec_storage.index_unchecked(index));
        result.type = elem_type::func_ref_imported;
    }
    else
    {
        result.storage.defined_ptr = ::std::addressof(module.local_defined_function_vec_storage.index_unchecked(index - imported_count));
        result.type = elem_type::func_ref_defined;
    }
    return result;
}

[[nodiscard]] inline constexpr runtime_wasm_funcref llvm_jit_funcref_from_table_elem(runtime_table_elem_storage_t const& elem) noexcept
{
    using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;

    runtime_wasm_funcref result{};
    switch(elem.type)
    {
        case elem_type::func_ref_imported:
            if(elem.storage.imported_ptr == nullptr)
            {
                result.ref.kind = ref_kind::wasm_null;
            }
            else
            {
                result.ref.storage.ptr = const_cast<void*>(static_cast<void const*>(elem.storage.imported_ptr));
                result.ref.kind = ref_kind::wasm_func_imported;
            }
            return result;
        case elem_type::func_ref_defined:
            if(elem.storage.defined_ptr == nullptr)
            {
                result.ref.kind = ref_kind::wasm_null;
            }
            else
            {
                result.ref.storage.ptr = const_cast<void*>(static_cast<void const*>(elem.storage.defined_ptr));
                result.ref.kind = ref_kind::wasm_func_defined;
            }
            return result;
        [[unlikely]] default:
            ::fast_io::fast_terminate();
    }
}

[[nodiscard]] inline constexpr runtime_table_elem_storage_t
llvm_jit_table_elem_from_funcref(runtime_module_storage_t const& module, runtime_wasm_funcref const& ref) noexcept
{
    static_cast<void>(module);
    using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;

    runtime_table_elem_storage_t result{};
    switch(ref.ref.kind)
    {
        case ref_kind::wasm_null:
            return result;
        case ref_kind::wasm_func:
            // Bare indices are initializer-only staging values and have no owner after crossing a module boundary.
            ::fast_io::fast_terminate();
        case ref_kind::wasm_func_imported:
            result.storage.imported_ptr =
                static_cast<::uwvm2::uwvm::runtime::storage::imported_function_storage_t const*>(ref.ref.storage.ptr);
            if(result.storage.imported_ptr == nullptr) { return {}; }
            result.type = elem_type::func_ref_imported;
            return result;
        case ref_kind::wasm_func_defined:
            result.storage.defined_ptr =
                static_cast<::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const*>(ref.ref.storage.ptr);
            if(result.storage.defined_ptr == nullptr) { return {}; }
            result.type = elem_type::func_ref_defined;
            return result;
        [[unlikely]] default:
            ::fast_io::fast_terminate();
    }
}

[[nodiscard]] inline constexpr runtime_wasm_externref llvm_jit_externref_from_table_elem(runtime_table_elem_storage_t const& elem) noexcept
{
    using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;

    runtime_wasm_externref result{};
    if(elem.type != elem_type::extern_ref) [[unlikely]]
    {
        // Zero-initialized legacy slots are valid null externrefs; initialized externref tables use the explicit tag.
        if(elem.storage.imported_ptr == nullptr)
        {
            result.ref.kind = ref_kind::wasm_null;
            return result;
        }
        ::fast_io::fast_terminate();
    }

    result.ref.storage.ptr = elem.storage.extern_ptr;
    result.ref.kind = elem.storage.extern_ptr == nullptr ? ref_kind::wasm_null : ref_kind::wasm_extern;
    return result;
}

[[nodiscard]] inline constexpr runtime_table_elem_storage_t llvm_jit_table_elem_from_externref(runtime_wasm_externref const& ref) noexcept
{
    using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;

    runtime_table_elem_storage_t result{};
    result.type = elem_type::extern_ref;
    switch(ref.ref.kind)
    {
        case ref_kind::wasm_null:
            result.storage.extern_ptr = nullptr;
            return result;
        case ref_kind::wasm_extern:
            result.storage.extern_ptr = ref.ref.storage.ptr;
            return result;
        [[unlikely]] default:
            ::fast_io::fast_terminate();
    }
}

inline constexpr void llvm_jit_ref_func_bridge(::std::uintptr_t runtime_module_address,
                                               runtime_wasm_u32 func_index,
                                               ::std::uintptr_t result_address) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t const*>(runtime_module_address)};
    auto result_ptr{reinterpret_cast<void*>(result_address)};
    if(runtime_module == nullptr || result_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const result{llvm_jit_funcref_from_table_elem(llvm_jit_resolve_table_elem_from_func_index(*runtime_module, func_index))};
    ::std::memcpy(result_ptr, ::std::addressof(result), sizeof(result));
}

[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_ref_is_null_bridge(::std::uintptr_t ref_address) noexcept
{
    auto ref_ptr{reinterpret_cast<void const*>(ref_address)};
    if(ref_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    runtime_wasm_global_ref ref{};
    ::std::memcpy(::std::addressof(ref), ref_ptr, sizeof(ref));
    return ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ? runtime_wasm_i32{1} : runtime_wasm_i32{};
}

inline constexpr void llvm_jit_table_get_bridge(::std::uintptr_t runtime_module_address,
                                                runtime_wasm_u32 table_index,
                                                runtime_wasm_i32 element_index_i32,
                                                ::std::uintptr_t result_address) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto result_ptr{reinterpret_cast<void*>(result_address)};
    if(runtime_module == nullptr || result_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const element_index{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(element_index_i32))};
    if(element_index >= table->elems.size()) [[unlikely]] { llvm_jit_table_out_of_bounds_bridge_trap(); }

    if(llvm_jit_runtime_table_is_funcref(*table))
    {
        auto const result{llvm_jit_funcref_from_table_elem(table->elems.index_unchecked(element_index))};
        ::std::memcpy(result_ptr, ::std::addressof(result), sizeof(result));
    }
    else if(llvm_jit_runtime_table_is_externref(*table))
    {
        auto const result{llvm_jit_externref_from_table_elem(table->elems.index_unchecked(element_index))};
        ::std::memcpy(result_ptr, ::std::addressof(result), sizeof(result));
    }
    else
    {
        ::fast_io::fast_terminate();
    }
}

inline constexpr void llvm_jit_table_set_bridge(::std::uintptr_t runtime_module_address,
                                                runtime_wasm_u32 table_index,
                                                runtime_wasm_i32 element_index_i32,
                                                ::std::uintptr_t value_address) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto value_ptr{reinterpret_cast<void const*>(value_address)};
    if(runtime_module == nullptr || value_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const element_index{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(element_index_i32))};
    if(element_index >= table->elems.size()) [[unlikely]] { llvm_jit_table_out_of_bounds_bridge_trap(); }

    if(llvm_jit_runtime_table_is_funcref(*table))
    {
        runtime_wasm_funcref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        table->elems.index_unchecked(element_index) = llvm_jit_table_elem_from_funcref(*runtime_module, value);
        ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views();
    }
    else if(llvm_jit_runtime_table_is_externref(*table))
    {
        runtime_wasm_externref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        table->elems.index_unchecked(element_index) = llvm_jit_table_elem_from_externref(value);
    }
    else
    {
        ::fast_io::fast_terminate();
    }
}

inline constexpr void llvm_jit_elem_drop_bridge(::std::uintptr_t runtime_module_address, runtime_wasm_u32 element_index) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr || static_cast<::std::size_t>(element_index) >= runtime_module->local_defined_element_vec_storage.size()) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }

    auto& element{runtime_module->local_defined_element_vec_storage.index_unchecked(static_cast<::std::size_t>(element_index)).element};
    ::uwvm2::uwvm::runtime::storage::drop_wasm_element_segment_payload(element);
}

inline constexpr void llvm_jit_table_init_bridge(::std::uintptr_t runtime_module_address,
                                                 runtime_wasm_u32 element_index,
                                                 runtime_wasm_u32 table_index,
                                                 runtime_wasm_i32 dst_i32,
                                                 runtime_wasm_i32 src_i32,
                                                 runtime_wasm_i32 len_i32) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr || static_cast<::std::size_t>(element_index) >= runtime_module->local_defined_element_vec_storage.size()) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const& element{runtime_module->local_defined_element_vec_storage.index_unchecked(static_cast<::std::size_t>(element_index)).element};
    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const src{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(src_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};

    if(llvm_jit_runtime_table_is_funcref(*table))
    {
        auto const funcidx_begin{element.funcidx_begin};
        auto const funcidx_end{element.funcidx_end};
        auto const funcref_begin{element.funcref_begin};
        auto const funcref_end{element.funcref_end};
        if((funcidx_begin == nullptr) != (funcidx_end == nullptr) ||
           (funcref_begin == nullptr) != (funcref_end == nullptr) ||
           (funcidx_begin != nullptr && funcref_begin != nullptr)) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        auto const source_size{funcref_begin == nullptr
                                   ? (funcidx_begin == nullptr ? 0uz : static_cast<::std::size_t>(funcidx_end - funcidx_begin))
                                   : static_cast<::std::size_t>(funcref_end - funcref_begin)};
        if(llvm_jit_bulk_memory_range_oob(src, len, source_size) || llvm_jit_bulk_memory_range_oob(dst, len, table->elems.size())) [[unlikely]]
        {
            llvm_jit_table_out_of_bounds_bridge_trap();
        }

        if(funcref_begin != nullptr)
        {
            for(::std::size_t i{}; i != len; ++i) { table->elems.index_unchecked(dst + i) = funcref_begin[src + i]; }
        }
        else
        {
            for(::std::size_t i{}; i != len; ++i)
            {
                auto const func_index{funcidx_begin[src + i]};
                table->elems.index_unchecked(dst + i) = func_index == (::std::numeric_limits<runtime_wasm_u32>::max)()
                                                            ? runtime_table_elem_storage_t{}
                                                            : llvm_jit_resolve_table_elem_from_func_index(*runtime_module, func_index);
            }
        }
        if(len != 0uz) { ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(); }
    }
    else if(llvm_jit_runtime_table_is_externref(*table))
    {
        auto const begin{element.externref_begin};
        auto const end{element.externref_end};
        if((begin == nullptr) != (end == nullptr)) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const source_size{begin == nullptr ? 0uz : static_cast<::std::size_t>(end - begin)};
        if(llvm_jit_bulk_memory_range_oob(src, len, source_size) || llvm_jit_bulk_memory_range_oob(dst, len, table->elems.size())) [[unlikely]]
        {
            llvm_jit_table_out_of_bounds_bridge_trap();
        }

        using elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;
        for(::std::size_t i{}; i != len; ++i)
        {
            auto& slot{table->elems.index_unchecked(dst + i)};
            slot.storage.extern_ptr = begin[src + i];
            slot.type = elem_type::extern_ref;
        }
    }
    else
    {
        ::fast_io::fast_terminate();
    }
}

inline constexpr void llvm_jit_table_copy_bridge(::std::uintptr_t runtime_module_address,
                                                 runtime_wasm_u32 dst_table_index,
                                                 runtime_wasm_u32 src_table_index,
                                                 runtime_wasm_i32 dst_i32,
                                                 runtime_wasm_i32 src_i32,
                                                 runtime_wasm_i32 len_i32) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto dst_table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, dst_table_index)};
    auto src_table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, src_table_index)};
    if(dst_table == nullptr || src_table == nullptr || dst_table->table_type_ptr == nullptr || src_table->table_type_ptr == nullptr ||
       dst_table->table_type_ptr->reftype != src_table->table_type_ptr->reftype) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }

    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const src{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(src_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};
    if(llvm_jit_bulk_memory_range_oob(src, len, src_table->elems.size()) ||
       llvm_jit_bulk_memory_range_oob(dst, len, dst_table->elems.size())) [[unlikely]]
    {
        llvm_jit_table_out_of_bounds_bridge_trap();
    }
    if(len != 0uz)
    {
        ::std::memmove(dst_table->elems.data() + dst, src_table->elems.data() + src, len * sizeof(runtime_table_elem_storage_t));
        if(llvm_jit_runtime_table_is_funcref(*dst_table)) { ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(); }
    }
}

[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_table_grow_bridge(::std::uintptr_t runtime_module_address,
                                                                           runtime_wasm_u32 table_index,
                                                                           ::std::uintptr_t value_address,
                                                                           runtime_wasm_i32 delta_i32) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto value_ptr{reinterpret_cast<void const*>(value_address)};
    if(runtime_module == nullptr || value_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const delta{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(delta_i32))};
    auto const old_size{table->elems.size()};
    auto result{llvm_jit_wasm_u32_bits_to_i32((::std::numeric_limits<::std::uint_least32_t>::max)())};
    auto const max_size{static_cast<::std::size_t>(table->table_type_ptr->limits.max)};
    if(old_size <= max_size && delta <= max_size - old_size)
    {
        runtime_table_elem_storage_t fill_element{};
        bool funcref_table{};
        if(llvm_jit_runtime_table_is_funcref(*table))
        {
            runtime_wasm_funcref value{};
            ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
            fill_element = llvm_jit_table_elem_from_funcref(*runtime_module, value);
            funcref_table = true;
        }
        else if(llvm_jit_runtime_table_is_externref(*table))
        {
            runtime_wasm_externref value{};
            ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
            fill_element = llvm_jit_table_elem_from_externref(value);
        }
        else
        {
            ::fast_io::fast_terminate();
        }

        auto const new_size{old_size + delta};
        table->elems.resize(new_size);
        for(::std::size_t i{old_size}; i != new_size; ++i) { table->elems.index_unchecked(i) = fill_element; }
        result = llvm_jit_wasm_u32_bits_to_i32(static_cast<::std::uint_least32_t>(old_size));
        if(funcref_table && delta != 0uz) { ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(); }
    }
    return result;
}

[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_table_size_bridge(::std::uintptr_t runtime_module_address,
                                                                           runtime_wasm_u32 table_index) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    if(runtime_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    return llvm_jit_wasm_u32_bits_to_i32(static_cast<::std::uint_least32_t>(table->elems.size()));
}

inline constexpr void llvm_jit_table_fill_bridge(::std::uintptr_t runtime_module_address,
                                                 runtime_wasm_u32 table_index,
                                                 runtime_wasm_i32 dst_i32,
                                                 ::std::uintptr_t value_address,
                                                 runtime_wasm_i32 len_i32) noexcept
{
    auto runtime_module{reinterpret_cast<runtime_module_storage_t*>(runtime_module_address)};
    auto value_ptr{reinterpret_cast<void const*>(value_address)};
    if(runtime_module == nullptr || value_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
    auto table{llvm_jit_resolve_mutable_runtime_table(*runtime_module, table_index)};
    if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    auto const dst{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(dst_i32))};
    auto const len{static_cast<::std::size_t>(llvm_jit_wasm_i32_bits_to_u32(len_i32))};
    if(llvm_jit_bulk_memory_range_oob(dst, len, table->elems.size())) [[unlikely]] { llvm_jit_table_out_of_bounds_bridge_trap(); }

    runtime_table_elem_storage_t fill_element{};
    bool funcref_table{};
    if(llvm_jit_runtime_table_is_funcref(*table))
    {
        runtime_wasm_funcref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        fill_element = llvm_jit_table_elem_from_funcref(*runtime_module, value);
        funcref_table = true;
    }
    else if(llvm_jit_runtime_table_is_externref(*table))
    {
        runtime_wasm_externref value{};
        ::std::memcpy(::std::addressof(value), value_ptr, sizeof(value));
        fill_element = llvm_jit_table_elem_from_externref(value);
    }
    else
    {
        ::fast_io::fast_terminate();
    }

    for(::std::size_t i{}; i != len; ++i) { table->elems.index_unchecked(dst + i) = fill_element; }
    if(funcref_table && len != 0uz) { ::uwvm2::runtime::lib::llvm_jit_refresh_call_indirect_table_views(); }
}

// Acquire a best-effort address/length snapshot for a local-imported memory.  This is used only for memory.size-style
// queries; actual loads/stores stay on bridge functions so provider locks do not outlive the snapshot.
[[nodiscard]] inline constexpr bool llvm_jit_local_imported_memory_snapshot_bridge(::std::uintptr_t local_imported_module_address,
                                                                                   ::std::size_t memory_index,
                                                                                   ::std::uintptr_t* memory_begin_address_out,
                                                                                   ::std::size_t* byte_length_out) noexcept
{
    if(memory_begin_address_out == nullptr || byte_length_out == nullptr) [[unlikely]] { return false; }

    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { return false; }

    ::uwvm2::uwvm::wasm::type::memory_access_snapshot_result_t snapshot{};
    if(!local_imported_module->memory_access_snapshot_from_index(memory_index, snapshot)) [[unlikely]] { return false; }

    auto const page_size_bytes{local_imported_module->memory_page_size_from_index(memory_index)};

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
    if(!::std::has_single_bit(page_size_bytes)) [[unlikely]]
    {
        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        return false;
    }
#endif

    auto const page_size_shift{static_cast<unsigned>(::std::countr_zero(page_size_bytes))};
    if(page_size_shift >= ::std::numeric_limits<::std::size_t>::digits) [[unlikely]] { return false; }
    if(snapshot.page_count > static_cast<::std::uint_least64_t>(::std::numeric_limits<::std::size_t>::max() >> page_size_shift)) [[unlikely]] { return false; }

    *memory_begin_address_out = reinterpret_cast<::std::uintptr_t>(snapshot.memory_begin);
    *byte_length_out = static_cast<::std::size_t>(snapshot.page_count) << page_size_shift;
    return true;
}

// Native memory.grow bridge.  It returns the old page count on success and -1 on Wasm-visible failure, matching the Wasm
// instruction contract.
[[nodiscard]] inline constexpr runtime_wasm_i32
    llvm_jit_memory_grow_bridge(::std::uintptr_t memory_address, ::std::size_t max_limit_memory_length, runtime_wasm_i32 delta_i32) noexcept
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }

    auto const delta_pages{static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(delta_i32))};
    ::std::size_t old_pages{};

    // Strict mode is the observable host-allocation failure path: failed growth is reported to
    // Wasm as `-1` and execution may continue. Without this flag, the default fail-fast/silent
    // policy is used: requests beyond the configured Wasm/user limit still return `-1`, but host
    // allocation failure is silent with respect to the host result and may terminate the process
    // through `grow_silently()`/`try_grow_silently()`.
    //
    // On overcommit systems, especially common Linux configurations, allocation admission can
    // succeed up to the architecture/user-VA limit (for example, about 128 TiB on common x86-64
    // layouts). The real OOM may happen only on later guest writes, where the kernel can kill the
    // process without a recoverable runtime error. Do not replace the silent path with strict
    // growth merely to synthesize an OOM diagnostic.
    if(::uwvm2::object::memory::flags::grow_strict)
    {
        return memory_p->grow_strictly(delta_pages, max_limit_memory_length, ::std::addressof(old_pages)) ? static_cast<runtime_wasm_i32>(old_pages)
                                                                                                          : static_cast<runtime_wasm_i32>(-1);
    }

    if constexpr(runtime_native_memory_t::support_multi_thread)
    {
        // Keep the check inside the backend critical section for concurrent memories. A false result here is a
        // Wasm -1 max/fit failure, not a request to synthesize a diagnostic OOM trap.
        return memory_p->try_grow_silently(delta_pages, max_limit_memory_length, ::std::addressof(old_pages)) ? static_cast<runtime_wasm_i32>(old_pages)
                                                                                                              : static_cast<runtime_wasm_i32>(-1);
    }
    else
    {
        old_pages = static_cast<::std::size_t>(memory_p->get_page_size());

        auto const limit_pages{max_limit_memory_length >> memory_p->custom_page_size_log2};
        if(old_pages > limit_pages || delta_pages > (limit_pages - old_pages)) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }

        memory_p->grow_silently(delta_pages, max_limit_memory_length);
        return static_cast<runtime_wasm_i32>(old_pages);
    }
}

// Native memory.size bridge, used when the JIT cannot read the current page count directly.
[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_memory_size_bridge(::std::uintptr_t memory_address) noexcept
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    return static_cast<runtime_wasm_i32>(memory_p->get_page_size());
}

// Local-imported memory.grow bridge.  The provider owns the memory growth policy and returns the old page count or -1.
[[nodiscard]] inline constexpr runtime_wasm_i32 llvm_jit_local_imported_memory_grow_bridge(::std::uintptr_t local_imported_module_address,
                                                                                           ::std::size_t memory_index,
                                                                                           ::std::size_t max_limit_memory_length,
                                                                                           runtime_wasm_i32 delta_i32) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }

    auto const delta_pages{static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(delta_i32))};
    ::std::uint_least64_t old_pages{};
    return local_imported_module->memory_try_grow_from_index(memory_index, delta_pages, max_limit_memory_length, ::std::addressof(old_pages))
               ? static_cast<runtime_wasm_i32>(old_pages)
               : static_cast<runtime_wasm_i32>(-1);
}

// Emit a runtime call-stack push for trap diagnostics.  This is used for public entries that represent a logical Wasm
// function frame.
[[nodiscard]] inline constexpr bool
    emit_runtime_local_func_llvm_jit_call_stack_push(::llvm::IRBuilder<>& ir_builder, ::std::size_t module_id, ::std::size_t function_index) noexcept
{
    auto& llvm_context{ir_builder.getContext()};
    auto llvm_size_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::size_t) * 8u))};
    auto function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {llvm_size_type, llvm_size_type}, false)};
    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<::uwvm2::runtime::lib::llvm_jit_push_call_stack_frame>(ir_builder, function_type)};
    if(bridge_pointer == nullptr) [[unlikely]] { return false; }

    apply_llvm_jit_host_calling_conv(
        ir_builder.CreateCall(function_type,
                              bridge_pointer,
                              {::llvm::ConstantInt::get(llvm_size_type, module_id), ::llvm::ConstantInt::get(llvm_size_type, function_index)}));
    return true;
}

// Emit the matching runtime call-stack pop before returning from a generated Wasm frame.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_call_stack_pop(::llvm::IRBuilder<>& ir_builder) noexcept
{
    auto& llvm_context{ir_builder.getContext()};
    auto function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), false)};
    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<::uwvm2::runtime::lib::llvm_jit_pop_call_stack_frame>(ir_builder, function_type)};
    if(bridge_pointer == nullptr) [[unlikely]] { return false; }

    apply_llvm_jit_host_calling_conv(ir_builder.CreateCall(function_type, bridge_pointer, {}));
    return true;
}

// Parse a LEB128 immediate from the current instruction slice and advance `code_curr` on success.
template <typename Immediate>
[[nodiscard]] inline constexpr bool parse_wasm_leb128_immediate(::std::byte const*& code_curr, ::std::byte const* code_end, Immediate& immediate) noexcept
{
    // fast_io's scanner operates on character pointers.  The may-alias char8_t view lets us parse the byte slice
    // directly without copying while preserving the original std::byte cursor API used by the dispatcher.
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(immediate))};
    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }

    code_curr = reinterpret_cast<::std::byte const*>(imm_next);
    return true;
}

// Parse the literal reserved 0x00 memory-immediate byte used by the supported memory.size/grow and
// bulk-memory encodings.
// Entry (the immediate may already be section_end):
// reserved_zero ...
// unsafe (could be the section_end)
// ^^ code_curr
// A missing/nonzero byte fails transactionally and leaves code_curr at this entry position.
[[nodiscard]] inline constexpr bool parse_wasm_reserved_zero_byte(::std::byte const*& code_curr, ::std::byte const* code_end) noexcept
{
    if(code_curr == code_end || *code_curr != ::std::byte{}) [[unlikely]] { return false; }
    ++code_curr;

    // reserved_zero ...
    // [    safe   ] unsafe (could be the section_end)
    //               ^^ code_curr
    return true;
}

// Parse a fixed-width little-endian immediate, used by f32.const and f64.const.
template <typename UInt>
[[nodiscard]] inline constexpr bool parse_wasm_little_endian_immediate(::std::byte const*& code_curr, ::std::byte const* code_end, UInt& immediate) noexcept
{
    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(UInt)) [[unlikely]] { return false; }

    immediate = 0;
    for(::std::size_t byte_index{}; byte_index != sizeof(UInt); ++byte_index)
    {
        // Wasm immediates are always little-endian, independent of the host.  Assemble by bytes so f32/f64 constants keep
        // their exact IEEE payload bits.
        immediate |= static_cast<UInt>(::std::to_integer<::std::uint_least8_t>(code_curr[byte_index])) << (byte_index * 8u);
    }

    code_curr += sizeof(UInt);
    return true;
}

// Static single-result arrays used by inline blocktype parsing. Type-index signatures borrow full ranges from runtime
// type-section storage.
inline constexpr runtime_operand_stack_value_type llvm_jit_i32_block_result_arr[]{runtime_operand_stack_value_type::i32};
inline constexpr runtime_operand_stack_value_type llvm_jit_i64_block_result_arr[]{runtime_operand_stack_value_type::i64};
inline constexpr runtime_operand_stack_value_type llvm_jit_f32_block_result_arr[]{runtime_operand_stack_value_type::f32};
inline constexpr runtime_operand_stack_value_type llvm_jit_f64_block_result_arr[]{runtime_operand_stack_value_type::f64};
inline constexpr runtime_operand_stack_value_type llvm_jit_v128_block_result_arr[]{runtime_operand_stack_value_type::v128};
inline constexpr runtime_operand_stack_value_type llvm_jit_funcref_block_result_arr[]{runtime_operand_stack_value_type::funcref};
inline constexpr runtime_operand_stack_value_type llvm_jit_externref_block_result_arr[]{runtime_operand_stack_value_type::externref};

// Fully resolved Wasm blocktype. `params` are the block-start types and `results` are the end types. Inline blocktypes have
// no params; a non-negative s33 type index borrows both ranges from the runtime type section.
struct runtime_block_signature_type
{
    runtime_block_result_type params{};
    runtime_block_result_type results{};
};

// Kind of structured Wasm control context currently active in the lowering stack.
enum class llvm_jit_control_context_type : unsigned
{
    // Implicit outer context for the function return label.
    function,

    // Wasm block context; branches target the block end.
    block,

    // Then arm of a Wasm if before an else has been seen.
    if_then,

    // Else arm of a Wasm if after the else boundary.
    if_else,

    // Wasm loop context; branches to label depth zero target the loop body.
    loop
};

// One entry on the structured-control stack used while lowering a Wasm function body.
struct llvm_jit_control_context_t
{
    // Structured construct represented by this stack entry.
    llvm_jit_control_context_type type{};

    // Parameter tuple visible at construct entry.
    runtime_block_result_type params{};

    // Result tuple expected at the construct's end label.
    runtime_block_result_type result{};

    // LLVM continuation block for the construct.
    ::llvm::BasicBlock* end_block{};

    // One PHI per result in `end_block`, in Wasm source order.
    ::uwvm2::utils::container::vector<::llvm::PHINode*> end_phis{};

    // Else block for `if`; null for all other context types.
    ::llvm::BasicBlock* else_block{};

    // Operand stack height before entering the construct.
    ::std::size_t outer_stack_size{};

    // Original block-start values. If/else restores this tuple for the else arm; other contexts leave it empty.
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> entry_params{};

    // Branch-target stack height before adding labels for this construct.
    ::std::size_t outer_branch_target_stack_size{};

    // Whether the current insertion point inside the construct is reachable.
    bool is_reachable{true};

    // True once at least one branch/fallthrough reaches `end_block`.
    bool end_block_has_incoming{};
};

// Normalized branch target for br/br_if/br_table/return.  For blocks and ifs this targets the end block; for loops it
// targets the loop body and carries loop parameters.
struct llvm_jit_branch_target_t
{
    // Values that must be available on the operand stack before taking the branch.
    runtime_block_result_type params{};

    // Destination block for the branch edge.
    ::llvm::BasicBlock* block{};

    // One PHI per branch argument, in Wasm source order.
    ::uwvm2::utils::container::vector<::llvm::PHINode*> phis{};

    // Index of the owning control-stack entry so incoming edges can mark the owner reachable.
    ::std::size_t control_stack_index{};
};

// Count the result values described by a runtime block result pointer pair.
[[nodiscard]] inline constexpr ::std::size_t get_runtime_block_result_count(runtime_block_result_type result) noexcept
{
    if(result.begin == nullptr || result.end == nullptr) { return 0uz; }
    return static_cast<::std::size_t>(result.end - result.begin);
}

// Compare two Wasm type tuples exactly in source order.
[[nodiscard]] inline constexpr bool runtime_block_result_types_equal(runtime_block_result_type left, runtime_block_result_type right) noexcept
{
    auto const left_count{get_runtime_block_result_count(left)};
    if(left_count != get_runtime_block_result_count(right)) { return false; }
    for(::std::size_t i{}; i != left_count; ++i)
    {
        if(left.begin[i] != right.begin[i]) { return false; }
    }
    return true;
}

// Parse and resolve a Wasm blocktype. Direct value forms describe an empty-parameter, zero/one-result signature. All other
// valid forms are non-negative s33 type indices and borrow their parameter/result ranges from the runtime type section.
// A LEB decode failure leaves code_curr at the blocktype start; later grammar/resolution failures occur after the complete
// checked immediate has been committed, and this scanner helper does not roll it back.
[[nodiscard]] inline constexpr bool
    parse_wasm_block_signature_type(::std::byte const*& code_curr,
                                    ::std::byte const* code_end,
                                    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                    runtime_block_signature_type& block_signature) noexcept
{
    // control_op blocktype ...
    // [  safe  ] unsafe (could be the section_end)
    //            ^^ code_curr

    if(code_curr == code_end) [[unlikely]] { return false; }

    auto const blocktype_begin{code_curr};
    ::std::int_least64_t blocktype{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, blocktype)) [[unlikely]] { return false; }
    auto const blocktype_encoded_size{static_cast<::std::size_t>(code_curr - blocktype_begin)};

    // control_op blocktype ...
    // [       safe       ] unsafe (could be the section_end)
    //                      ^^ code_curr

    if(blocktype_encoded_size > 5uz || (blocktype < 0 && blocktype_encoded_size != 1uz)) [[unlikely]] { return false; }

    switch(blocktype)
    {
        case -64:
        {
            block_signature = {};
            return true;
        }
        case -1:
        {
            block_signature = {.params = {}, .results = {llvm_jit_i32_block_result_arr, llvm_jit_i32_block_result_arr + 1u}};
            return true;
        }
        case -2:
        {
            block_signature = {.params = {}, .results = {llvm_jit_i64_block_result_arr, llvm_jit_i64_block_result_arr + 1u}};
            return true;
        }
        case -3:
        {
            block_signature = {.params = {}, .results = {llvm_jit_f32_block_result_arr, llvm_jit_f32_block_result_arr + 1u}};
            return true;
        }
        case -4:
        {
            block_signature = {.params = {}, .results = {llvm_jit_f64_block_result_arr, llvm_jit_f64_block_result_arr + 1u}};
            return true;
        }
        case -5:
        {
            block_signature = {.params = {}, .results = {llvm_jit_v128_block_result_arr, llvm_jit_v128_block_result_arr + 1u}};
            return true;
        }
        case -16:
        {
            block_signature = {.params = {}, .results = {llvm_jit_funcref_block_result_arr, llvm_jit_funcref_block_result_arr + 1u}};
            return true;
        }
        case -17:
        {
            block_signature = {.params = {}, .results = {llvm_jit_externref_block_result_arr, llvm_jit_externref_block_result_arr + 1u}};
            return true;
        }
        default:
        {
            if(blocktype < 0 || static_cast<::std::uint_least64_t>(blocktype) >
                   static_cast<::std::uint_least64_t>((::std::numeric_limits<validation_module_traits_t::wasm_u32>::max)())) [[unlikely]]
            {
                return false;
            }
            auto function_type{resolve_runtime_type_section_function_type(
                runtime_module,
                static_cast<validation_module_traits_t::wasm_u32>(blocktype))};
            if(function_type == nullptr) [[unlikely]] { return false; }
            block_signature.params = {function_type->parameter.begin, function_type->parameter.end};
            block_signature.results = {function_type->result.begin, function_type->result.end};
            return true;
        }
    }
}

// The enclosing validation/translation dispatcher supplies exactly one instruction slice after accepting its binary grammar.
// Unreachable instructions contribute no immediate-dependent LLVM state, so consume that established slice boundary
// instead of maintaining a second partial decoder that can drift when proposal opcodes gain new immediate forms.
// Entry is the validated instruction start; an empty slice fails without moving code_curr:
// instruction ...
// unsafe (could be the section_end)
// ^^ code_curr
// On success the sole cursor commit assigns code_end after the non-empty check.
[[nodiscard]] inline constexpr bool consume_validated_wasm_instruction_slice(::std::byte const*& code_curr,
                                                                              ::std::byte const* code_end) noexcept
{
    if(code_curr == code_end) [[unlikely]] { return false; }
    code_curr = code_end;

    // instruction ...
    // [  safe   ] unsafe (could be the section_end)
    //             ^^ code_curr / code_end
    return true;
}

// Mutable state for lowering one local Wasm function to LLVM IR.  This object intentionally stores all transient stacks
// and LLVM handles so opcode include files can share a compact emitter API without global state.
struct runtime_local_func_llvm_jit_emit_state_t
{
    // True after preparation succeeds and before finalization consumes the control stack.
    bool valid{};

    // Enables expensive LLVM verifier checks on generated functions/modules.
    bool verify_llvm_jit_ir{default_verify_llvm_jit_ir};

    // Forces Wasm calls through the runtime raw bridge instead of direct typed declarations.
    bool route_wasm_calls_through_runtime_bridge{};

    // Base address/count of lazy raw-call target records for locally defined functions.
    ::std::uintptr_t lazy_defined_raw_call_target_base_address{};
    ::std::size_t lazy_defined_raw_call_target_count{};

    // Base address/count of lazy typed-entry pointers for locally defined functions.
    ::std::uintptr_t lazy_defined_typed_entry_target_base_address{};
    ::std::size_t lazy_defined_typed_entry_target_count{};

    // Whether lazy target table loads must use acquire atomics.
    bool lazy_defined_targets_are_atomic{};

    // Enables OSR/tiered loop reentry wrapper generation.
    bool emit_tiered_loop_reentry_entries{};

    // Enables runtime logical call-stack push/pop around public Wasm entries.
    bool emit_call_stack_frames{true};

    // Enables native unwind metadata so concrete generated frames can be mapped back to Wasm frames.
    bool emit_unwind_call_stack_frames{};

    // Runtime local-function storage being compiled.
    local_func_storage_t const* local_func_storage_ptr{};

    // Flattened local types: function parameters first, then declared locals in declaration order.
    ::uwvm2::utils::container::vector<runtime_operand_stack_value_type> local_types{};

    // Raw ABI byte offsets for locals, used by tiered OSR local snapshots.
    ::uwvm2::utils::container::vector<::std::size_t> local_offsets{};

    // LLVM context/module handles owned by the module storage object.
    ::llvm::LLVMContext* llvm_context_holder{};
    ::llvm::Module* llvm_module{};

    // Function currently receiving the body.  In tiered mode this is the internal core function.
    ::llvm::Function* llvm_function{};

    // Public typed entry function.  In non-tiered mode this is the same as `llvm_function`.
    ::llvm::Function* llvm_public_entry_function{};

    // Entry and normal-init blocks used by the tiered core dispatch.
    ::llvm::BasicBlock* tiered_core_entry_block{};
    ::llvm::BasicBlock* tiered_core_normal_init_block{};

    // Extra tiered-core arguments: reentry id and pointer to serialized local storage.
    ::llvm::Argument* tiered_core_entry_id_arg{};
    ::llvm::Argument* tiered_core_local_base_arg{};

    // Primary IRBuilder for body emission.
    ::uwvm2::utils::container::delete_owned_ptr<::llvm::IRBuilder<>> ir_builder{};

    // Entry-block allocas for every local slot.
    ::uwvm2::utils::container::vector<::llvm::AllocaInst*> local_pointers{};

    // Cached resolution for default memory 0.
    runtime_memory_access_info_t memory0_access_info{};
    bool memory0_access_info_resolved{};

    // Function result signature and return join block.
    runtime_block_result_type function_result{};
    ::std::size_t func_result_count_uz{};
    ::llvm::BasicBlock* return_block{};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> return_phis{};

    // Transient Wasm operand stack represented as typed LLVM SSA values.
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> operand_stack{};

    // Structured control stack and its flattened branch-target view.
    ::uwvm2::utils::container::vector<llvm_jit_control_context_t> control_stack{};
    ::uwvm2::utils::container::vector<llvm_jit_branch_target_t> branch_target_stack{};

    // Recorded OSR loop reentry metadata and target blocks.
    ::uwvm2::utils::container::vector<tiered_loop_reentry_storage_t> tiered_loop_reentries{};
    ::uwvm2::utils::container::vector<::llvm::BasicBlock*> tiered_loop_reentry_blocks{};

    // Byte offset of the instruction currently being emitted, or SIZE_MAX when unavailable.
    ::std::size_t current_wasm_op_offset{SIZE_MAX};

    // Nested structured-control depth being skipped inside an unreachable context.
    ::std::size_t unreachable_control_depth{};
};

// Create one PHI per Wasm tuple field at the beginning of `block`.
[[nodiscard]] inline constexpr bool create_runtime_local_func_llvm_jit_result_phis(
    ::llvm::LLVMContext& llvm_context,
    ::llvm::BasicBlock* block,
    runtime_block_result_type result_types,
    ::llvm::StringRef name,
    ::uwvm2::utils::container::vector<::llvm::PHINode*>& phis) noexcept
{
    phis.clear();
    auto const result_count{get_runtime_block_result_count(result_types)};
    if(result_count == 0uz) { return true; }
    if(block == nullptr) [[unlikely]] { return false; }

    phis.reserve(result_count);
    ::llvm::IRBuilder<> phi_builder(block);
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto phi_type{get_llvm_type_from_wasm_value_type(llvm_context, result_types.begin[result_index])};
        if(phi_type == nullptr) [[unlikely]] { return false; }
        auto phi{phi_builder.CreatePHI(phi_type, 0u, name)};
        if(phi == nullptr) [[unlikely]] { return false; }
        phis.push_back(phi);
    }
    return true;
}

// Pack a vector of scalar LLVM values into the canonical typed Wasm result representation.
[[nodiscard]] inline constexpr ::llvm::Value* emit_pack_runtime_wasm_tuple(
    ::llvm::IRBuilder<>& ir_builder,
    runtime_block_result_type result_types,
    ::llvm::ArrayRef<::llvm::Value*> values,
    ::llvm::StringRef name) noexcept
{
    auto const result_count{get_runtime_block_result_count(result_types)};
    if(result_count == 0uz || values.size() != result_count) [[unlikely]] { return nullptr; }
    auto llvm_result_type{get_llvm_result_type_from_wasm_result_range(ir_builder.getContext(), result_types.begin, result_types.end)};
    if(llvm_result_type == nullptr) [[unlikely]] { return nullptr; }
    if(result_count == 1uz)
    {
        auto value{values[0]};
        return value != nullptr && value->getType() == llvm_result_type ? value : nullptr;
    }
    if(!llvm_result_type->isStructTy()) [[unlikely]] { return nullptr; }
    auto llvm_struct_type{static_cast<::llvm::StructType*>(llvm_result_type)};
    if(llvm_struct_type->getNumElements() != result_count) [[unlikely]] { return nullptr; }
    ::llvm::Value* aggregate{::llvm::UndefValue::get(llvm_result_type)};
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto value{values[result_index]};
        auto expected_type{get_llvm_type_from_wasm_value_type(ir_builder.getContext(), result_types.begin[result_index])};
        if(value == nullptr || expected_type == nullptr || value->getType() != expected_type ||
           llvm_struct_type->getElementType(static_cast<unsigned>(result_index)) != expected_type) [[unlikely]]
        {
            return nullptr;
        }
        aggregate = ir_builder.CreateInsertValue(aggregate, value, {static_cast<unsigned>(result_index)}, name);
    }
    return aggregate;
}

// Allocate LLVM context/module storage for a runtime module.
[[nodiscard]] inline constexpr bool try_prepare_runtime_llvm_jit_module_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                                llvm_jit_module_storage_t& module_storage,
                                                                                bool emit_unwind_call_stack_frames = false) noexcept
{
    static_cast<void>(emit_unwind_call_stack_frames);
    module_storage = {};
    module_storage.llvm_context_holder = ::uwvm2::utils::container::make_delete_owned<::llvm::LLVMContext>();
    if(module_storage.llvm_context_holder == nullptr) [[unlikely]] { return false; }

    auto const llvm_module_name{get_llvm_wasm_ir_module_name(runtime_module)};
    module_storage.llvm_module =
        ::uwvm2::utils::container::make_delete_owned<::llvm::Module>(get_llvm_string_ref(llvm_module_name), *module_storage.llvm_context_holder);
    if(module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

    return true;
}

// Initialize the full per-function emit state, create typed entry/core functions, allocate locals, and seed the control
// stack with the implicit function return label.
[[nodiscard]] inline constexpr bool try_prepare_runtime_local_func_llvm_jit_emit_state(local_func_storage_t const& local_func_storage,
                                                                                       llvm_jit_module_storage_t& module_storage,
                                                                                       runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                       bool verify_llvm_jit_ir = default_verify_llvm_jit_ir,
                                                                                       bool route_wasm_calls_through_runtime_bridge = false,
                                                                                       ::std::uintptr_t lazy_defined_raw_call_target_base_address = 0u,
                                                                                       ::std::size_t lazy_defined_raw_call_target_count = 0uz,
                                                                                       ::std::uintptr_t lazy_defined_typed_entry_target_base_address = 0u,
                                                                                       ::std::size_t lazy_defined_typed_entry_target_count = 0uz,
                                                                                       bool lazy_defined_targets_are_atomic = false,
                                                                                       bool emit_tiered_loop_reentry_entries = false,
                                                                                       bool emit_call_stack_frames = true,
                                                                                       bool emit_unwind_call_stack_frames = false) noexcept
{
    state = {};
    state.verify_llvm_jit_ir = verify_llvm_jit_ir;
    state.route_wasm_calls_through_runtime_bridge = route_wasm_calls_through_runtime_bridge;
    state.lazy_defined_raw_call_target_base_address = lazy_defined_raw_call_target_base_address;
    state.lazy_defined_raw_call_target_count = lazy_defined_raw_call_target_count;
    state.lazy_defined_typed_entry_target_base_address = lazy_defined_typed_entry_target_base_address;
    state.lazy_defined_typed_entry_target_count = lazy_defined_typed_entry_target_count;
    state.lazy_defined_targets_are_atomic = lazy_defined_targets_are_atomic;
    state.emit_tiered_loop_reentry_entries = emit_tiered_loop_reentry_entries;
    state.emit_call_stack_frames = emit_call_stack_frames;
    state.emit_unwind_call_stack_frames = emit_unwind_call_stack_frames;

    auto function_type_ptr{local_func_storage.function_type_ptr};
    auto wasm_code_ptr{local_func_storage.wasm_code_ptr};
    if(function_type_ptr == nullptr || wasm_code_ptr == nullptr) [[unlikely]] { return false; }

    auto const func_parameter_begin{function_type_ptr->parameter.begin};
    auto const func_parameter_end{function_type_ptr->parameter.end};
    auto const func_result_begin{function_type_ptr->result.begin};
    auto const func_result_end{function_type_ptr->result.end};

    if(func_parameter_begin == nullptr && func_parameter_begin != func_parameter_end) [[unlikely]] { return false; }
    if(func_result_begin == nullptr && func_result_begin != func_result_end) [[unlikely]] { return false; }

    auto const func_parameter_count_uz{func_parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(func_parameter_end - func_parameter_begin)};
    auto const func_result_count_uz{func_result_begin == nullptr ? 0uz : static_cast<::std::size_t>(func_result_end - func_result_begin)};
    auto const defined_local_count_uz{static_cast<::std::size_t>(wasm_code_ptr->all_local_count)};
    // This total drives vector reservations, alloca creation, and OSR local snapshot offsets; reject wraparound before
    // any of those layouts can diverge from the validated Wasm local count.
    if(func_parameter_count_uz > ::std::numeric_limits<::std::size_t>::max() - defined_local_count_uz) [[unlikely]] { return false; }
    auto const all_local_count_uz{func_parameter_count_uz + defined_local_count_uz};
    using wasm_u32 = validation_module_traits_t::wasm_u32;
    if(local_func_storage.function_index > static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())) [[unlikely]] { return false; }
    auto const function_index{static_cast<wasm_u32>(local_func_storage.function_index)};
    state.local_offsets.reserve(all_local_count_uz);
    ::std::size_t local_bytes{};
    state.local_types.reserve(all_local_count_uz);
    // Build the flattened local layout once.  The same type list drives LLVM allocas, while byte offsets define the raw
    // serialized-local ABI used by tiered OSR reentry wrappers.
    for(::std::size_t i{}; i != func_parameter_count_uz; ++i)
    {
        auto const vt{static_cast<runtime_operand_stack_value_type>(func_parameter_begin[i])};
        auto const abi_size{get_runtime_wasm_value_type_abi_size(vt)};
        if(abi_size == 0uz || abi_size > (::std::numeric_limits<::std::size_t>::max() - local_bytes)) [[unlikely]] { return false; }
        state.local_offsets.push_back(local_bytes);
        state.local_types.push_back(vt);
        local_bytes += abi_size;
    }
    for(auto const& local_part: wasm_code_ptr->locals)
    {
        for(validation_module_traits_t::wasm_u32 i{}; i != local_part.count; ++i)
        {
            auto const vt{static_cast<runtime_operand_stack_value_type>(local_part.type)};
            auto const abi_size{get_runtime_wasm_value_type_abi_size(vt)};
            if(abi_size == 0uz || abi_size > (::std::numeric_limits<::std::size_t>::max() - local_bytes)) [[unlikely]] { return false; }
            state.local_offsets.push_back(local_bytes);
            state.local_types.push_back(vt);
            local_bytes += abi_size;
        }
    }

    if(state.local_types.size() != all_local_count_uz || state.local_offsets.size() != all_local_count_uz) [[unlikely]] { return false; }

    auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }
    if(module_storage.llvm_context_holder == nullptr || module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

    state.local_func_storage_ptr = ::std::addressof(local_func_storage);
    state.func_result_count_uz = func_result_count_uz;
    state.function_result = runtime_block_result_type{func_result_begin, func_result_end};

    // Borrow the LLVM objects from module storage. The emit state does not own these handles.
    state.llvm_context_holder = module_storage.llvm_context_holder.get();
    auto& llvm_context{*state.llvm_context_holder};
    state.llvm_module = module_storage.llvm_module.get();

    ::uwvm2::utils::container::vector<::llvm::Type*> llvm_parameter_types{};
    llvm_parameter_types.reserve(func_parameter_count_uz);
    for(::std::size_t i{}; i != func_parameter_count_uz; ++i)
    {
        auto llvm_parameter_type{get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(func_parameter_begin[i]))};
        if(llvm_parameter_type == nullptr) [[unlikely]] { return false; }
        llvm_parameter_types.push_back(llvm_parameter_type);
    }

    auto llvm_function_type{get_llvm_function_type_from_wasm_function_type(llvm_context, *function_type_ptr)};
    if(llvm_function_type == nullptr) [[unlikely]] { return false; }
    auto llvm_result_type{llvm_function_type->getReturnType()};
    // Keep every LLVM symbol name on the same checked Wasm32 function index; silent truncation here would alias
    // two runtime functions to the same generated declaration.
    auto const function_name{get_llvm_wasm_function_name(*runtime_module_ptr, function_index)};
    state.llvm_public_entry_function = state.llvm_module->getFunction(get_llvm_string_ref(function_name));
    if(state.llvm_public_entry_function == nullptr)
    {
        state.llvm_public_entry_function =
            ::llvm::Function::Create(llvm_function_type, ::llvm::Function::ExternalLinkage, get_llvm_string_ref(function_name), state.llvm_module);
    }
    else
    {
        // Preparation may run after declaration pre-creation.  Reusing an already defined body would merge two emissions
        // into one LLVM function, so only empty declarations are accepted.
        if(state.llvm_public_entry_function->getFunctionType() != llvm_function_type || !state.llvm_public_entry_function->empty()) [[unlikely]]
        {
            return false;
        }
        state.llvm_public_entry_function->setLinkage(::llvm::Function::ExternalLinkage);
    }
    if(state.llvm_public_entry_function == nullptr) [[unlikely]] { return false; }
    apply_llvm_jit_wasm_calling_conv(*state.llvm_public_entry_function);
    if(state.emit_unwind_call_stack_frames) { apply_llvm_jit_unwind_call_stack_function_attrs(*state.llvm_public_entry_function); }
    if(emit_tiered_loop_reentry_entries)
    {
        // Tiered mode splits the function into a public typed wrapper and an internal core.  The core receives two hidden
        // arguments before the Wasm parameters: reentry id and serialized-local base address.
        auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
        ::uwvm2::utils::container::vector<::llvm::Type*> core_parameter_types{};
        core_parameter_types.reserve(llvm_parameter_types.size() + 2uz);
        core_parameter_types.push_back(::llvm::Type::getInt32Ty(llvm_context));
        core_parameter_types.push_back(llvm_intptr_type);
        for(auto param_type: llvm_parameter_types) { core_parameter_types.push_back(param_type); }

        auto core_function_type{::llvm::FunctionType::get(llvm_result_type, {core_parameter_types.data(), core_parameter_types.size()}, false)};
        auto const core_function_name{get_llvm_wasm_tiered_core_function_name(*runtime_module_ptr, function_index)};
        state.llvm_function = state.llvm_module->getFunction(get_llvm_string_ref(core_function_name));
        if(state.llvm_function == nullptr)
        {
            state.llvm_function =
                ::llvm::Function::Create(core_function_type, ::llvm::Function::InternalLinkage, get_llvm_string_ref(core_function_name), state.llvm_module);
        }
        else
        {
            if(state.llvm_function->getFunctionType() != core_function_type || !state.llvm_function->empty()) [[unlikely]] { return false; }
            state.llvm_function->setLinkage(::llvm::Function::InternalLinkage);
        }
        if(state.llvm_function == nullptr) [[unlikely]] { return false; }
        apply_llvm_jit_wasm_calling_conv(*state.llvm_function);
        if(state.emit_unwind_call_stack_frames) { apply_llvm_jit_unwind_call_stack_function_attrs(*state.llvm_function); }

        state.tiered_core_entry_id_arg = state.llvm_function->getArg(0u);
        state.tiered_core_local_base_arg = state.llvm_function->getArg(1u);
        if(state.tiered_core_entry_id_arg == nullptr || state.tiered_core_local_base_arg == nullptr) [[unlikely]] { return false; }
    }
    else
    {
        state.llvm_function = state.llvm_public_entry_function;
    }

    auto entry_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"entry"), state.llvm_function)};
    if(entry_block == nullptr) [[unlikely]] { return false; }
    state.tiered_core_entry_block = entry_block;

    ::llvm::BasicBlock* body_init_block{entry_block};
    if(emit_tiered_loop_reentry_entries)
    {
        body_init_block = ::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"tiered.normal.init"), state.llvm_function);
        if(body_init_block == nullptr) [[unlikely]] { return false; }
        state.tiered_core_normal_init_block = body_init_block;
        // The real entry block is reserved for the OSR dispatch switch finalized later; ordinary function initialization
        // starts in this separate block.
    }

    state.ir_builder = ::uwvm2::utils::container::make_delete_owned<::llvm::IRBuilder<>>(body_init_block);
    if(state.emit_call_stack_frames && !emit_tiered_loop_reentry_entries &&
       !emit_runtime_local_func_llvm_jit_call_stack_push(*state.ir_builder, local_func_storage.module_id, local_func_storage.function_index)) [[unlikely]]
    {
        return false;
    }
    // In tiered mode the public wrapper owns logical call-stack push/pop.  The internal core may be entered through OSR,
    // so pushing inside the core would create duplicate or unbalanced logical frames.

    state.local_pointers.reserve(state.local_types.size());
    for(::std::size_t local_index{}; local_index != state.local_types.size(); ++local_index)
    {
        // Every local is represented by an entry-block alloca.  Parameters are stored into their local slots first, and
        // declared locals are initialized to the Wasm zero value.
        auto const local_type{state.local_types[local_index]};
        auto llvm_local_type{get_llvm_type_from_wasm_value_type(llvm_context, local_type)};
        if(llvm_local_type == nullptr) [[unlikely]] { return false; }

        auto local_pointer{create_llvm_jit_entry_block_alloca(*state.ir_builder, llvm_local_type, nullptr, get_llvm_string_ref(u8""))};
        if(local_pointer == nullptr) [[unlikely]] { return false; }
        state.local_pointers.push_back(local_pointer);

        if(local_index < func_parameter_count_uz)
        {
            auto const core_arg_index{emit_tiered_loop_reentry_entries ? local_index + 2uz : local_index};
            state.ir_builder->CreateStore(state.llvm_function->getArg(core_arg_index), local_pointer);
        }
        else
        {
            auto zero_constant{get_llvm_zero_constant_from_wasm_value_type(llvm_context, local_type)};
            if(zero_constant == nullptr) [[unlikely]] { return false; }
            state.ir_builder->CreateStore(zero_constant, local_pointer);
        }
    }

    state.return_block = ::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"return"), state.llvm_function);
    if(!create_runtime_local_func_llvm_jit_result_phis(llvm_context,
                                                       state.return_block,
                                                       state.function_result,
                                                       get_llvm_string_ref(u8"return.phi"),
                                                       state.return_phis)) [[unlikely]]
    {
        return false;
    }

    state.control_stack.push_back({.type = llvm_jit_control_context_type::function,
                                   .params = {},
                                   .result = state.function_result,
                                   .end_block = state.return_block,
                                   .end_phis = state.return_phis,
                                   .else_block = nullptr,
                                   .outer_stack_size = 0uz,
                                   .entry_params = {},
                                   .outer_branch_target_stack_size = 0uz,
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = state.function_result, .block = state.return_block, .phis = state.return_phis, .control_stack_index = 0uz});
    // The implicit function label sits at branch depth equal to the outermost target.  `return` reuses the same branch
    // machinery as `br` by selecting this first branch-target entry.
    state.valid = true;
    return true;
}

// Complete the current function after all instructions have been emitted.  This seals the return block, generates any
// tiered/raw wrappers, verifies generated functions when requested, and leaves the module ready for optimization/JIT use.
[[nodiscard]] inline constexpr bool finalize_runtime_local_func_llvm_jit_emit_state(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                    llvm_jit_module_storage_t&) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_module == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr)
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.empty()) [[unlikely]] { return false; }

    auto& ir_builder{*state.ir_builder};

    // Seal the synthetic function return block.  If there are no incoming edges for a result-returning function, the body
    // was unreachable and the return block must become unreachable rather than returning an undef value.
    // Finalization runs only after the implicit function context has been closed by `end`, so no more branch incoming
    // edges can be added after this point.
    ir_builder.SetInsertPoint(state.return_block);
    if(state.func_result_count_uz == 0uz)
    {
        if(state.emit_call_stack_frames && !state.emit_tiered_loop_reentry_entries && !emit_runtime_local_func_llvm_jit_call_stack_pop(ir_builder)) [[unlikely]]
        {
            return false;
        }
        ir_builder.CreateRetVoid();
    }
    else if(state.return_phis.size() == state.func_result_count_uz && !state.return_phis.empty() &&
            state.return_phis.index_unchecked(0uz)->getNumIncomingValues() != 0u)
    {
        if(state.emit_call_stack_frames && !state.emit_tiered_loop_reentry_entries && !emit_runtime_local_func_llvm_jit_call_stack_pop(ir_builder)) [[unlikely]]
        {
            return false;
        }
        ::uwvm2::utils::container::vector<::llvm::Value*> return_values{};
        return_values.reserve(state.return_phis.size());
        for(auto phi: state.return_phis)
        {
            if(phi == nullptr || phi->getNumIncomingValues() != state.return_phis.index_unchecked(0uz)->getNumIncomingValues()) [[unlikely]] { return false; }
            return_values.push_back(phi);
        }
        auto packed_return{emit_pack_runtime_wasm_tuple(ir_builder,
                                                        state.function_result,
                                                        {return_values.data(), return_values.size()},
                                                        get_llvm_string_ref(u8"return.value"))};
        if(packed_return == nullptr || packed_return->getType() != state.llvm_function->getReturnType()) [[unlikely]] { return false; }
        ir_builder.CreateRet(packed_return);
    }
    else
    {
        for(auto phi: state.return_phis)
        {
            if(phi != nullptr && phi->getNumIncomingValues() == 0u) { phi->eraseFromParent(); }
        }
        ir_builder.CreateUnreachable();
    }

    auto const emit_runtime_local_func_llvm_jit_tiered_core_dispatch{
        [&]() constexpr noexcept -> bool
        {
            // The core entry dispatches entry_id 0 to normal initialization and recorded non-zero ids to OSR load blocks.
            // Each OSR load block restores locals from the raw local snapshot before branching to the recorded loop block.
            if(!state.emit_tiered_loop_reentry_entries) { return true; }
            if(state.llvm_context_holder == nullptr || state.llvm_module == nullptr || state.llvm_function == nullptr ||
               state.tiered_core_entry_block == nullptr || state.tiered_core_normal_init_block == nullptr || state.tiered_core_entry_id_arg == nullptr ||
               state.tiered_core_local_base_arg == nullptr) [[unlikely]]
            {
                return false;
            }
            if(state.tiered_loop_reentries.size() != state.tiered_loop_reentry_blocks.size()) [[unlikely]] { return false; }

            auto& llvm_context{*state.llvm_context_holder};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
            auto llvm_i8_ptr_type{get_llvm_pointer_type(llvm_i8_type)};
            if(llvm_i8_ptr_type == nullptr) [[unlikely]] { return false; }

            ::llvm::IRBuilder<> entry_builder(state.tiered_core_entry_block);
            // The normal entry id is zero; OSR ids start at one so a default switch target naturally represents the
            // ordinary function entry path.
            auto switch_inst{entry_builder.CreateSwitch(state.tiered_core_entry_id_arg,
                                                        state.tiered_core_normal_init_block,
                                                        static_cast<unsigned>(state.tiered_loop_reentries.size()))};

            for(::std::size_t i{}; i != state.tiered_loop_reentries.size(); ++i)
            {
                auto const& reentry{state.tiered_loop_reentries.index_unchecked(i)};
                auto target_block{state.tiered_loop_reentry_blocks.index_unchecked(i)};
                if(target_block == nullptr) [[unlikely]] { return false; }

                auto load_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"tiered.osr.load"), state.llvm_function)};
                if(load_block == nullptr) [[unlikely]] { return false; }

                switch_inst->addCase(::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), reentry.entry_id), load_block);

                ::llvm::IRBuilder<> load_builder(load_block);
                auto local_base_ptr{
                    load_builder.CreateIntToPtr(state.tiered_core_local_base_arg, llvm_i8_ptr_type, get_llvm_string_ref(u8"tiered.local.base"))};
                for(::std::size_t local_index{}; local_index != state.local_types.size(); ++local_index)
                {
                    // Locals are serialized using the same offsets computed during preparation.  They are reloaded into
                    // the normal local allocas so the rest of the body is identical to normal-entry execution.
                    auto llvm_local_type{get_llvm_type_from_wasm_value_type(llvm_context, state.local_types.index_unchecked(local_index))};
                    auto local_pointer{state.local_pointers.index_unchecked(local_index)};
                    if(llvm_local_type == nullptr || local_pointer == nullptr) [[unlikely]] { return false; }

                    auto local_address{
                        load_builder.CreateInBoundsGEP(llvm_i8_type,
                                                       local_base_ptr,
                                                       {::llvm::ConstantInt::get(llvm_intptr_type, state.local_offsets.index_unchecked(local_index))},
                                                       get_llvm_string_ref(u8"tiered.local.addr"))};
                    auto typed_local_address{
                        load_builder.CreateBitCast(local_address, get_llvm_pointer_type(llvm_local_type), get_llvm_string_ref(u8"tiered.local.typed.addr"))};
                    auto packed_local_load{load_builder.CreateLoad(llvm_local_type, typed_local_address, get_llvm_string_ref(u8"tiered.local"))};
                    packed_local_load->setAlignment(::llvm::Align{1u});
                    load_builder.CreateStore(packed_local_load, local_pointer);
                }
                load_builder.CreateBr(target_block);
            }

            return true;
        }};

    auto const emit_runtime_local_func_llvm_jit_tiered_public_entry_wrapper{
        [&]() constexpr noexcept -> bool
        {
            // The public typed entry is kept small in tiered mode: it pushes the logical call-stack frame, calls core
            // entry id 0, pops the frame, and returns the core result.
            if(!state.emit_tiered_loop_reentry_entries) { return true; }
            auto const local_func_storage_ptr{state.local_func_storage_ptr};
            auto const llvm_module{state.llvm_module};
            auto const core_function{state.llvm_function};
            auto public_function{state.llvm_public_entry_function};
            auto const llvm_context_holder{state.llvm_context_holder};
            if(local_func_storage_ptr == nullptr || llvm_module == nullptr || core_function == nullptr || public_function == nullptr ||
               llvm_context_holder == nullptr) [[unlikely]]
            {
                return false;
            }
            if(!public_function->empty()) [[unlikely]] { return false; }

            auto& llvm_context{*llvm_context_holder};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto entry_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"entry"), public_function)};
            if(entry_block == nullptr) [[unlikely]] { return false; }
            ::llvm::IRBuilder<> public_builder(entry_block);

            if(state.emit_call_stack_frames &&
               !emit_runtime_local_func_llvm_jit_call_stack_push(public_builder, local_func_storage_ptr->module_id, local_func_storage_ptr->function_index))
                [[unlikely]]
            {
                return false;
            }

            ::uwvm2::utils::container::vector<::llvm::Value*> core_arguments{};
            core_arguments.reserve(public_function->arg_size() + 2uz);
            core_arguments.push_back(::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), 0u));
            core_arguments.push_back(::llvm::ConstantInt::get(llvm_intptr_type, 0u));
            // Hidden core arguments come first, followed by the public Wasm parameters unchanged.  Entry id zero tells
            // the core to run normal local initialization instead of OSR local restore.
            for(auto& arg: public_function->args()) { core_arguments.push_back(::std::addressof(arg)); }

            auto core_call{apply_llvm_jit_wasm_calling_conv(public_builder.CreateCall(core_function, {core_arguments.data(), core_arguments.size()}))};
            if(state.emit_call_stack_frames && !emit_runtime_local_func_llvm_jit_call_stack_pop(public_builder)) [[unlikely]] { return false; }

            if(public_function->getReturnType()->isVoidTy()) { public_builder.CreateRetVoid(); }
            else
            {
                public_builder.CreateRet(core_call);
            }

            return verify_llvm_jit_function(*public_function, state.verify_llvm_jit_ir);
        }};

    // Compute the serialized-local byte span required by tiered OSR raw-entry wrappers.
    auto const get_tiered_osr_local_bytes{[&]() constexpr noexcept -> ::std::size_t
                                          {
                                              // Compute the byte span that OSR wrappers must receive for serialized
                                              // locals.  Returning zero for a non-empty local set indicates a bad layout.
                                              ::std::size_t local_bytes{};
                                              for(::std::size_t local_index{}; local_index != state.local_types.size(); ++local_index)
                                              {
                                                  auto const abi_size{get_runtime_wasm_value_type_abi_size(state.local_types.index_unchecked(local_index))};
                                                  auto const offset{state.local_offsets.index_unchecked(local_index)};
                                                  if(abi_size == 0uz || offset > (::std::numeric_limits<::std::size_t>::max() - abi_size)) [[unlikely]]
                                                  {
                                                      return 0uz;
                                                  }
                                                  auto const end{offset + abi_size};
                                                  if(end > local_bytes) { local_bytes = end; }
                                              }
                                              return local_bytes;
                                          }};

    auto const emit_runtime_local_func_llvm_jit_tiered_loop_reentry_wrappers{
        [&]() constexpr noexcept -> bool
        {
            // Each OSR wrapper uses the raw ABI expected by the tiered runtime.  Parameters are not read from a parameter
            // buffer; the wrapper provides zero/default Wasm arguments and restores live locals from the local snapshot.
            if(!state.emit_tiered_loop_reentry_entries) { return true; }
            auto const local_func_storage_ptr{state.local_func_storage_ptr};
            auto const llvm_module{state.llvm_module};
            auto const core_function{state.llvm_function};
            auto const llvm_context_holder{state.llvm_context_holder};
            if(local_func_storage_ptr == nullptr || llvm_module == nullptr || core_function == nullptr || llvm_context_holder == nullptr) [[unlikely]]
            {
                return false;
            }

            auto const runtime_module_ptr{local_func_storage_ptr->runtime_module_ptr};
            auto const function_type_ptr{local_func_storage_ptr->function_type_ptr};
            if(runtime_module_ptr == nullptr || function_type_ptr == nullptr) [[unlikely]] { return false; }

            using wasm_u32 = validation_module_traits_t::wasm_u32;
            auto const function_index_uz{local_func_storage_ptr->function_index};
            if(function_index_uz > static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())) [[unlikely]] { return false; }
            auto const function_index{static_cast<wasm_u32>(function_index_uz)};

            auto& llvm_context{*llvm_context_holder};
            auto raw_entry_function_type{get_llvm_runtime_raw_call_target_entry_function_type(llvm_context)};
            if(raw_entry_function_type == nullptr) [[unlikely]] { return false; }

            auto const abi_layout{get_runtime_wasm_call_abi_layout(*function_type_ptr)};
            if(!abi_layout.valid) [[unlikely]] { return false; }

            auto const local_bytes{get_tiered_osr_local_bytes()};
            if(!state.local_types.empty() && local_bytes == 0uz) [[unlikely]] { return false; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
            auto llvm_i8_ptr_type{get_llvm_pointer_type(llvm_i8_type)};
            if(llvm_i8_ptr_type == nullptr) [[unlikely]] { return false; }

            for(auto const& reentry: state.tiered_loop_reentries)
            {
                // The wrapper validates buffer sizes/nullability at runtime before jumping into the core.  Invalid runtime
                // contracts are reported as invariant traps instead of silently corrupting locals/results.
                auto const reentry_function_name{
                    get_llvm_wasm_tiered_loop_reentry_raw_function_name(*runtime_module_ptr, function_index, reentry.wasm_code_offset)};
                auto reentry_function{llvm_module->getFunction(get_llvm_string_ref(reentry_function_name))};
                if(reentry_function == nullptr)
                {
                    reentry_function = ::llvm::Function::Create(raw_entry_function_type,
                                                                ::llvm::Function::ExternalLinkage,
                                                                get_llvm_string_ref(reentry_function_name),
                                                                llvm_module);
                }
                else
                {
                    if(reentry_function->getFunctionType() != raw_entry_function_type || !reentry_function->empty()) [[unlikely]] { return false; }
                    reentry_function->setLinkage(::llvm::Function::ExternalLinkage);
                }
                if(reentry_function == nullptr) [[unlikely]] { return false; }
                // OSR reentry wrappers are raw Wasm-entry targets.  Keep their LLVM calling convention synchronized with
                // the C++ `UWVM2_RUNTIME_LLVM_JIT_RAW_ENTRY_PTR_ABI` function pointer used by the tiered runtime.  On
                // Windows x86_64 that means SysV, matching uwvm-int opfuncs; using the host Win64 ABI here silently
                // corrupts mixed tiered reentry calls.
                apply_llvm_jit_raw_entry_calling_conv(*reentry_function);
                if(state.emit_unwind_call_stack_frames) { apply_llvm_jit_unwind_call_stack_function_attrs(*reentry_function); }

                auto entry_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"entry"), reentry_function)};
                if(entry_block == nullptr) [[unlikely]] { return false; }
                ::llvm::IRBuilder<> osr_builder(entry_block);

                auto const result_buffer_address{reentry_function->getArg(1u)};
                auto const result_bytes{reentry_function->getArg(2u)};
                auto const local_base_address{reentry_function->getArg(3u)};
                auto const local_base_bytes{reentry_function->getArg(4u)};
                if(result_buffer_address == nullptr || result_bytes == nullptr || local_base_address == nullptr || local_base_bytes == nullptr) [[unlikely]]
                {
                    return false;
                }

                emit_llvm_conditional_trap(*llvm_module,
                                           osr_builder,
                                           osr_builder.CreateICmpNE(local_base_bytes, ::llvm::ConstantInt::get(llvm_intptr_type, local_bytes)));
                emit_llvm_conditional_trap(*llvm_module,
                                           osr_builder,
                                           osr_builder.CreateICmpNE(result_bytes, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes)));
                if(local_bytes != 0uz)
                {
                    emit_llvm_conditional_trap(*llvm_module,
                                               osr_builder,
                                               osr_builder.CreateICmpEQ(local_base_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)));
                }
                if(abi_layout.result_bytes != 0uz)
                {
                    emit_llvm_conditional_trap(*llvm_module,
                                               osr_builder,
                                               osr_builder.CreateICmpEQ(result_buffer_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)));
                }

                ::uwvm2::utils::container::vector<::llvm::Value*> core_arguments{};
                core_arguments.reserve(abi_layout.parameter_count + 2uz);
                core_arguments.push_back(::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), reentry.entry_id));
                core_arguments.push_back(local_base_address);
                auto const parameter_begin{function_type_ptr->parameter.begin};
                for(::std::size_t parameter_index{}; parameter_index != abi_layout.parameter_count; ++parameter_index)
                {
                    // OSR reentry restores execution state from serialized locals, not from the original function
                    // parameters.  Default parameter values satisfy the core signature while the restored locals provide
                    // the live values used after the target loop point.
                    auto llvm_param_type{
                        get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
                    if(llvm_param_type == nullptr) [[unlikely]] { return false; }
                    core_arguments.push_back(::llvm::Constant::getNullValue(llvm_param_type));
                }

                auto core_call{apply_llvm_jit_wasm_calling_conv(osr_builder.CreateCall(core_function, {core_arguments.data(), core_arguments.size()}))};
                if(abi_layout.result_count != 0uz)
                {
                    auto llvm_result_type{get_llvm_result_type_from_wasm_result_range(
                        llvm_context,
                        function_type_ptr->result.begin,
                        function_type_ptr->result.end)};
                    if(llvm_result_type == nullptr || core_function->getReturnType() != llvm_result_type ||
                       !emit_store_runtime_wasm_call_result_to_raw_buffer(osr_builder,
                                                                          *function_type_ptr,
                                                                          core_call,
                                                                          result_buffer_address,
                                                                          get_llvm_string_ref(u8"tiered.result"))) [[unlikely]]
                    {
                        return false;
                    }
                }

                osr_builder.CreateRetVoid();
                if(!verify_llvm_jit_function(*reentry_function, state.verify_llvm_jit_ir)) [[unlikely]] { return false; }
            }

            return true;
        }};

    if(!emit_runtime_local_func_llvm_jit_tiered_core_dispatch() || !emit_runtime_local_func_llvm_jit_tiered_public_entry_wrapper() ||
       !emit_runtime_local_func_llvm_jit_tiered_loop_reentry_wrappers()) [[unlikely]]
    {
        return false;
    }

    auto const emit_runtime_local_func_llvm_jit_raw_entry_wrapper{
        [&]() constexpr noexcept -> bool
        {
            // The raw entry wrapper adapts the generic byte-buffer ABI to the typed public Wasm entry.  This is the stable
            // boundary used by lazy targets, imported host calls, and call_indirect fallback paths.
            auto const local_func_storage_ptr{state.local_func_storage_ptr};
            auto const llvm_module{state.llvm_module};
            auto const llvm_function{state.llvm_public_entry_function};
            auto const llvm_context_holder{state.llvm_context_holder};
            if(local_func_storage_ptr == nullptr || llvm_module == nullptr || llvm_function == nullptr || llvm_context_holder == nullptr) [[unlikely]]
            {
                return false;
            }

            auto const runtime_module_ptr{local_func_storage_ptr->runtime_module_ptr};
            auto const function_type_ptr{local_func_storage_ptr->function_type_ptr};
            if(runtime_module_ptr == nullptr || function_type_ptr == nullptr) [[unlikely]] { return false; }

            using wasm_u32 = validation_module_traits_t::wasm_u32;
            auto const function_index_uz{local_func_storage_ptr->function_index};
            if(function_index_uz > static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())) [[unlikely]] { return false; }

            auto const function_index{static_cast<wasm_u32>(function_index_uz)};
            auto& llvm_context{*llvm_context_holder};
            auto raw_entry_function_type{get_llvm_runtime_raw_call_target_entry_function_type(llvm_context)};
            if(raw_entry_function_type == nullptr) [[unlikely]] { return false; }

            auto const raw_entry_function_name{get_llvm_wasm_raw_function_name(*runtime_module_ptr, function_index)};
            auto raw_entry_function{llvm_module->getFunction(get_llvm_string_ref(raw_entry_function_name))};
            if(raw_entry_function == nullptr)
            {
                raw_entry_function = ::llvm::Function::Create(raw_entry_function_type,
                                                              ::llvm::Function::ExternalLinkage,
                                                              get_llvm_string_ref(raw_entry_function_name),
                                                              llvm_module);
            }
            else
            {
                if(raw_entry_function->getFunctionType() != raw_entry_function_type || !raw_entry_function->empty()) [[unlikely]] { return false; }
                raw_entry_function->setLinkage(::llvm::Function::ExternalLinkage);
            }
            if(raw_entry_function == nullptr) [[unlikely]] { return false; }
            apply_llvm_jit_raw_entry_calling_conv(*raw_entry_function);
            if(state.emit_unwind_call_stack_frames) { apply_llvm_jit_unwind_call_stack_function_attrs(*raw_entry_function); }
            auto const abi_layout{get_runtime_wasm_call_abi_layout(*function_type_ptr)};
            if(!abi_layout.valid) [[unlikely]] { return false; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
            auto llvm_i8_ptr_type{get_llvm_pointer_type(llvm_i8_type)};
            if(llvm_i8_ptr_type == nullptr) [[unlikely]] { return false; }

            auto entry_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"entry"), raw_entry_function)};
            if(entry_block == nullptr) [[unlikely]] { return false; }
            ::llvm::IRBuilder<> raw_ir_builder(entry_block);
            auto const result_buffer_address{raw_entry_function->getArg(1u)};
            auto const result_bytes{raw_entry_function->getArg(2u)};
            auto const param_buffer_address{raw_entry_function->getArg(3u)};
            auto const param_bytes{raw_entry_function->getArg(4u)};
            if(result_buffer_address == nullptr || result_bytes == nullptr || param_buffer_address == nullptr || param_bytes == nullptr) [[unlikely]]
            {
                return false;
            }

            // Raw callers are outside LLVM's typed verifier.  Validate buffer sizes and required non-null addresses in IR
            // before unpacking bytes so a bad runtime contract becomes a trap instead of unchecked memory access.
            emit_llvm_conditional_trap(*llvm_module,
                                       raw_ir_builder,
                                       raw_ir_builder.CreateICmpNE(param_bytes, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)));
            emit_llvm_conditional_trap(*llvm_module,
                                       raw_ir_builder,
                                       raw_ir_builder.CreateICmpNE(result_bytes, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes)));

            if(abi_layout.parameter_bytes != 0uz)
            {
                emit_llvm_conditional_trap(*llvm_module,
                                           raw_ir_builder,
                                           raw_ir_builder.CreateICmpEQ(param_buffer_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)));
            }
            if(abi_layout.result_bytes != 0uz)
            {
                emit_llvm_conditional_trap(*llvm_module,
                                           raw_ir_builder,
                                           raw_ir_builder.CreateICmpEQ(result_buffer_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)));
            }

            auto const param_begin{function_type_ptr->parameter.begin};
            auto param_buffer_base{raw_ir_builder.CreateIntToPtr(param_buffer_address, llvm_i8_ptr_type, get_llvm_string_ref(u8"raw.param.base"))};

            ::uwvm2::utils::container::vector<::llvm::Value*> call_arguments{};
            call_arguments.reserve(abi_layout.parameter_count);

            ::std::size_t param_offset{};
            for(::std::size_t parameter_index{}; parameter_index != abi_layout.parameter_count; ++parameter_index)
            {
                // Unpack parameters from the tightly packed raw buffer and pass them to the typed public entry in normal
                // Wasm parameter order.
                auto const wasm_value_type{static_cast<runtime_operand_stack_value_type>(param_begin[parameter_index])};
                auto llvm_param_type{get_llvm_type_from_wasm_value_type(llvm_context, wasm_value_type)};
                if(llvm_param_type == nullptr) [[unlikely]] { return false; }

                auto const abi_size{get_runtime_wasm_value_type_abi_size(wasm_value_type)};
                if(abi_size == 0uz) [[unlikely]] { return false; }

                auto parameter_address{raw_ir_builder.CreateInBoundsGEP(llvm_i8_type,
                                                                        param_buffer_base,
                                                                        {::llvm::ConstantInt::get(llvm_intptr_type, param_offset)},
                                                                        get_llvm_string_ref(u8"raw.param.addr"))};
                auto typed_parameter_address{
                    raw_ir_builder.CreateBitCast(parameter_address, get_llvm_pointer_type(llvm_param_type), get_llvm_string_ref(u8"raw.param.typed.addr"))};
                auto packed_load{raw_ir_builder.CreateLoad(llvm_param_type, typed_parameter_address, get_llvm_string_ref(u8"raw.param"))};
                packed_load->setAlignment(::llvm::Align{1u});
                call_arguments.push_back(packed_load);
                param_offset += abi_size;
            }

            auto typed_call{apply_llvm_jit_wasm_calling_conv(raw_ir_builder.CreateCall(llvm_function, {call_arguments.data(), call_arguments.size()}))};
            if(abi_layout.result_count != 0uz)
            {
                auto llvm_result_type{get_llvm_result_type_from_wasm_result_range(
                    llvm_context,
                    function_type_ptr->result.begin,
                    function_type_ptr->result.end)};
                if(llvm_result_type == nullptr || llvm_function->getReturnType() != llvm_result_type ||
                   !emit_store_runtime_wasm_call_result_to_raw_buffer(raw_ir_builder,
                                                                      *function_type_ptr,
                                                                      typed_call,
                                                                      result_buffer_address,
                                                                      get_llvm_string_ref(u8"raw.result"))) [[unlikely]]
                {
                    return false;
                }
            }

            raw_ir_builder.CreateRetVoid();

            if(!verify_llvm_jit_function(*raw_entry_function, state.verify_llvm_jit_ir)) [[unlikely]] { return false; }
            return true;
        }};

    if(!emit_runtime_local_func_llvm_jit_raw_entry_wrapper()) [[unlikely]] { return false; }

    if(!verify_llvm_jit_function(*state.llvm_function, state.verify_llvm_jit_ir)) [[unlikely]] { return false; }
    return true;
}

// Translate a Wasm label depth to the corresponding branch target.  Depth zero means the innermost active label.
[[nodiscard]] inline constexpr llvm_jit_branch_target_t const*
    get_runtime_local_func_llvm_jit_branch_target_by_depth(runtime_local_func_llvm_jit_emit_state_t const& state,
                                                           validation_module_traits_t::wasm_u32 label_depth) noexcept
{
    auto const depth{static_cast<::std::size_t>(label_depth)};
    auto const label_count{state.branch_target_stack.size()};
    if(depth >= label_count) { return nullptr; }
    return ::std::addressof(state.branch_target_stack.index_unchecked(label_count - 1uz - depth));
}

// Mark the owning control context as having at least one incoming edge to its end block.
inline constexpr void mark_runtime_local_func_llvm_jit_branch_target_has_incoming(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                  llvm_jit_branch_target_t const& target) noexcept
{
    if(target.control_stack_index >= state.control_stack.size()) [[unlikely]] { return; }

    auto& target_context{state.control_stack.index_unchecked(target.control_stack_index)};
    if(target_context.end_block == target.block) { target_context.end_block_has_incoming = true; }
}

// Read a branch argument tuple without mutating the operand stack. Values are returned in Wasm source order even though
// the last tuple field is the top stack operand.
[[nodiscard]] inline constexpr bool try_get_runtime_local_func_llvm_jit_branch_values(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    runtime_block_result_type params,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t>& branch_values) noexcept
{
    auto const arity{get_runtime_block_result_count(params)};
    branch_values.clear();
    if(state.operand_stack.size() < arity) [[unlikely]] { return false; }
    branch_values.reserve(arity);
    auto const first_value_index{state.operand_stack.size() - arity};
    for(::std::size_t value_index{}; value_index != arity; ++value_index)
    {
        auto const branch_value{state.operand_stack.index_unchecked(first_value_index + value_index)};
        if(branch_value.value == nullptr || branch_value.type != params.begin[value_index]) [[unlikely]] { return false; }
        branch_values.push_back(branch_value);
    }
    return true;
}

// Add one predecessor's complete tuple to a normalized target PHI vector.
[[nodiscard]] inline constexpr bool try_add_runtime_local_func_llvm_jit_branch_target_incoming(
    runtime_local_func_llvm_jit_emit_state_t& state,
    llvm_jit_branch_target_t const& target,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> const& branch_values,
    ::llvm::BasicBlock* predecessor) noexcept
{
    auto const arity{get_runtime_block_result_count(target.params)};
    if(predecessor == nullptr || target.block == nullptr || target.phis.size() != arity || branch_values.size() != arity) [[unlikely]] { return false; }
    for(::std::size_t value_index{}; value_index != arity; ++value_index)
    {
        auto phi{target.phis.index_unchecked(value_index)};
        auto const& branch_value{branch_values.index_unchecked(value_index)};
        if(phi == nullptr || branch_value.value == nullptr || branch_value.type != target.params.begin[value_index] ||
           branch_value.value->getType() != phi->getType()) [[unlikely]]
        {
            return false;
        }
        phi->addIncoming(branch_value.value, predecessor);
    }
    mark_runtime_local_func_llvm_jit_branch_target_has_incoming(state, target);
    return true;
}

// Emit an unconditional branch to a normalized target and wire every tuple PHI.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_branch_to_target(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                          llvm_jit_branch_target_t const& target) noexcept
{
    if(!state.valid || state.ir_builder == nullptr) [[unlikely]] { return false; }

    auto& ir_builder{*state.ir_builder};
    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }

    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> branch_values{};
    if(!try_get_runtime_local_func_llvm_jit_branch_values(state, target.params, branch_values) ||
       !try_add_runtime_local_func_llvm_jit_branch_target_incoming(state, target, branch_values, current_block)) [[unlikely]]
    {
        return false;
    }

    ir_builder.CreateBr(target.block);
    return true;
}

// Enter Wasm's polymorphic/unreachable control state after an instruction such as unreachable, br, br_table, or return.
// The concrete operand stack is truncated to the surrounding block height.
inline constexpr void enter_runtime_local_func_llvm_jit_unreachable_control_context(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(state.control_stack.empty()) [[unlikely]] { return; }

    auto& current_context{state.control_stack.back()};
    while(state.operand_stack.size() > current_context.outer_stack_size) { state.operand_stack.pop_back(); }
    // Wasm unreachable code is stack-polymorphic.  Keep only the operands visible outside the current construct and let
    // subsequent non-control instructions be skipped until a structural boundary is reached.
    current_context.is_reachable = false;
    state.unreachable_control_depth = 0uz;
}

// Drop transient operands above a known structured-control stack height.
inline constexpr void truncate_runtime_local_func_llvm_jit_operand_stack_to(runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t target_size) noexcept
{
    while(state.operand_stack.size() > target_size) { state.operand_stack.pop_back(); }
}

// Snapshot and validate the block-start tuple currently at the top of the operand stack.
[[nodiscard]] inline constexpr bool try_get_runtime_local_func_llvm_jit_block_entry_params(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    runtime_block_result_type param_types,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t>& params,
    ::std::size_t& outer_stack_size) noexcept
{
    auto const param_count{get_runtime_block_result_count(param_types)};
    if(state.operand_stack.size() < param_count) [[unlikely]] { return false; }
    outer_stack_size = state.operand_stack.size() - param_count;
    params.clear();
    params.reserve(param_count);
    for(::std::size_t param_index{}; param_index != param_count; ++param_index)
    {
        auto const param{state.operand_stack.index_unchecked(outer_stack_size + param_index)};
        if(param.value == nullptr || param.type != param_types.begin[param_index]) [[unlikely]] { return false; }
        params.push_back(param);
    }
    return true;
}

inline constexpr void push_runtime_local_func_llvm_jit_tuple(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> const& values) noexcept
{
    for(auto const& value: values) { state.operand_stack.push_back(value); }
}

// Record a tiered loop reentry point when the current instruction starts a loop/block body with an empty operand stack.
// Duplicate Wasm offsets are ignored so each hot loop has at most one OSR entry id.
[[nodiscard]] inline constexpr bool try_record_runtime_local_func_llvm_jit_tiered_reentry(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                          ::llvm::BasicBlock* target_block) noexcept
{
    if(!state.emit_tiered_loop_reentry_entries || !state.operand_stack.empty() || state.current_wasm_op_offset == SIZE_MAX) { return true; }
    if(target_block == nullptr) [[unlikely]] { return false; }
    if(state.tiered_loop_reentries.size() != state.tiered_loop_reentry_blocks.size()) [[unlikely]] { return false; }

    auto const next_entry_id{state.tiered_loop_reentries.size() + 1uz};
    if(next_entry_id > static_cast<::std::size_t>((::std::numeric_limits<::std::uint_least32_t>::max)())) { return true; }

    for(auto const& reentry: state.tiered_loop_reentries)
    {
        if(reentry.wasm_code_offset == state.current_wasm_op_offset) { return true; }
    }

    state.tiered_loop_reentries.push_back({.wasm_code_offset = state.current_wasm_op_offset, .entry_id = static_cast<::std::uint_least32_t>(next_entry_id)});
    state.tiered_loop_reentry_blocks.push_back(target_block);
    return true;
}

// Emit the Wasm `unreachable` opcode as a runtime trap followed by LLVM unreachable.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_unreachable(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.valid || state.llvm_module == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }

    auto& ir_builder{*state.ir_builder};
    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }

    emit_llvm_runtime_trap(ir_builder, ::uwvm2::runtime::lib::llvm_jit_trap_kind::unreachable);
    ir_builder.CreateUnreachable();
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

// Emit Wasm `nop`.  It has no IR effect but still validates that the emitter is in a usable structured context.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_nop(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{ return state.valid && !state.control_stack.empty(); }

// Begin lowering a Wasm `block`. A block label targets the end block, whose PHI vector receives the complete result tuple.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_block(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                               runtime_block_signature_type block_signature) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }

    if(!state.control_stack.back().is_reachable)
    {
        ++state.unreachable_control_depth;
        return true;
    }

    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> entry_params{};
    ::std::size_t outer_stack_size{};
    if(!try_get_runtime_local_func_llvm_jit_block_entry_params(state, block_signature.params, entry_params, outer_stack_size)) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"block.end"), state.llvm_function)};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> end_phis{};
    if(!create_runtime_local_func_llvm_jit_result_phis(
           llvm_context,
           end_block,
           block_signature.results,
           get_llvm_string_ref(u8"block.result"),
           end_phis)) [[unlikely]]
    {
        return false;
    }

    if(state.emit_tiered_loop_reentry_entries && state.operand_stack.empty() && state.current_wasm_op_offset != SIZE_MAX)
    {
        // Create a real body block for OSR so a reentry wrapper can branch to a stable target before the block's first
        // instruction, rather than into the middle of an existing predecessor block.
        if(state.ir_builder == nullptr) [[unlikely]] { return false; }
        auto& ir_builder{*state.ir_builder};
        auto current_block{ir_builder.GetInsertBlock()};
        if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }

        auto block_body_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"block.body"), state.llvm_function)};
        if(block_body_block == nullptr) [[unlikely]] { return false; }

        ir_builder.CreateBr(block_body_block);
        ir_builder.SetInsertPoint(block_body_block);
        if(!try_record_runtime_local_func_llvm_jit_tiered_reentry(state, block_body_block)) [[unlikely]] { return false; }
    }

    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::block,
                                   .params = block_signature.params,
                                   .result = block_signature.results,
                                   .end_block = end_block,
                                   .end_phis = end_phis,
                                   .else_block = nullptr,
                                   .outer_stack_size = outer_stack_size,
                                   .entry_params = {},
                                   .outer_branch_target_stack_size = state.branch_target_stack.size(),
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = block_signature.results, .block = end_block, .phis = end_phis, .control_stack_index = control_stack_index});
    return true;
}

// Begin lowering a Wasm `loop`.  A loop label targets the loop body, while fallthrough/branch-to-end values still merge at
// the loop end block.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_loop(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                              runtime_block_signature_type block_signature) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr || state.control_stack.empty())
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable)
    {
        ++state.unreachable_control_depth;
        return true;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }

    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> entry_params{};
    ::std::size_t outer_stack_size{};
    if(!try_get_runtime_local_func_llvm_jit_block_entry_params(state, block_signature.params, entry_params, outer_stack_size)) [[unlikely]] { return false; }

    auto loop_body_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"loop.body"), state.llvm_function)};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"loop.end"), state.llvm_function)};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> loop_param_phis{};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> end_phis{};
    if(!create_runtime_local_func_llvm_jit_result_phis(
           llvm_context,
           loop_body_block,
           block_signature.params,
           get_llvm_string_ref(u8"loop.param"),
           loop_param_phis) ||
       !create_runtime_local_func_llvm_jit_result_phis(
           llvm_context,
           end_block,
           block_signature.results,
           get_llvm_string_ref(u8"loop.result"),
           end_phis)) [[unlikely]]
    {
        return false;
    }
    for(::std::size_t param_index{}; param_index != entry_params.size(); ++param_index)
    {
        auto phi{loop_param_phis.index_unchecked(param_index)};
        auto const& param{entry_params.index_unchecked(param_index)};
        if(phi == nullptr || param.value == nullptr || phi->getType() != param.value->getType()) [[unlikely]] { return false; }
        phi->addIncoming(param.value, current_block);
    }

    ir_builder.CreateBr(loop_body_block);
    ir_builder.SetInsertPoint(loop_body_block);

    truncate_runtime_local_func_llvm_jit_operand_stack_to(state, outer_stack_size);
    for(::std::size_t param_index{}; param_index != loop_param_phis.size(); ++param_index)
    {
        state.operand_stack.push_back({.type = block_signature.params.begin[param_index],
                                       .value = loop_param_phis.index_unchecked(param_index)});
    }

    if(!try_record_runtime_local_func_llvm_jit_tiered_reentry(state, loop_body_block)) [[unlikely]] { return false; }

    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::loop,
                                   .params = block_signature.params,
                                   .result = block_signature.results,
                                   .end_block = end_block,
                                   .end_phis = end_phis,
                                   .else_block = nullptr,
                                   .outer_stack_size = outer_stack_size,
                                   .entry_params = {},
                                   .outer_branch_target_stack_size = state.branch_target_stack.size(),
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = block_signature.params, .block = loop_body_block, .phis = loop_param_phis, .control_stack_index = control_stack_index});
    return true;
}

// Begin lowering a Wasm `if`.  The i32 condition is consumed and converted to an LLVM i1 comparison against zero.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_if(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                            runtime_block_signature_type block_signature) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr || state.control_stack.empty())
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable)
    {
        ++state.unreachable_control_depth;
        return true;
    }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const condition{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(condition.type != runtime_operand_stack_value_type::i32 || condition.value == nullptr) [[unlikely]] { return false; }

    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> entry_params{};
    ::std::size_t outer_stack_size{};
    if(!try_get_runtime_local_func_llvm_jit_block_entry_params(state, block_signature.params, entry_params, outer_stack_size)) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }

    auto then_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"if.then"), state.llvm_function)};
    auto else_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"if.else"), state.llvm_function)};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"if.end"), state.llvm_function)};
    ::uwvm2::utils::container::vector<::llvm::PHINode*> end_phis{};
    if(!create_runtime_local_func_llvm_jit_result_phis(
           llvm_context,
           end_block,
           block_signature.results,
           get_llvm_string_ref(u8"if.result"),
           end_phis)) [[unlikely]]
    {
        return false;
    }

    auto cond_i1{ir_builder.CreateICmpNE(condition.value, ::llvm::ConstantInt::get(condition.value->getType(), 0u))};
    ir_builder.CreateCondBr(cond_i1, then_block, else_block);
    ir_builder.SetInsertPoint(then_block);

    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::if_then,
                                   .params = block_signature.params,
                                   .result = block_signature.results,
                                   .end_block = end_block,
                                   .end_phis = end_phis,
                                   .else_block = else_block,
                                   .outer_stack_size = outer_stack_size,
                                   .entry_params = entry_params,
                                   .outer_branch_target_stack_size = state.branch_target_stack.size(),
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = block_signature.results, .block = end_block, .phis = end_phis, .control_stack_index = control_stack_index});
    return true;
}

// Lower Wasm `else`.  The reachable then branch falls through by branching to the if end block, then emission continues
// in the else block with the operand stack restored to the if entry height.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_else(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }

    if(!state.control_stack.back().is_reachable && state.unreachable_control_depth != 0uz) { return true; }

    auto const current_control_stack_index{state.control_stack.size() - 1uz};
    auto& current_context{state.control_stack.back()};
    if(current_context.type != llvm_jit_control_context_type::if_then || current_context.else_block == nullptr) [[unlikely]] { return false; }

    auto const branch_target{
        llvm_jit_branch_target_t{.params = current_context.result,
                                 .block = current_context.end_block,
                                 .phis = current_context.end_phis,
                                 .control_stack_index = current_control_stack_index}
    };
    if(current_context.is_reachable && !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, branch_target)) [[unlikely]] { return false; }

    truncate_runtime_local_func_llvm_jit_operand_stack_to(state, current_context.outer_stack_size);
    push_runtime_local_func_llvm_jit_tuple(state, current_context.entry_params);
    current_context.type = llvm_jit_control_context_type::if_else;
    current_context.is_reachable = true;
    state.ir_builder->SetInsertPoint(current_context.else_block);
    return true;
}

// Lower Wasm `end` for blocks, loops, ifs, and the implicit function context.  This seals the current structured context,
// restores outer stacks, and moves the builder to the continuation block when reachable.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_end(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }

    if(!state.control_stack.back().is_reachable && state.unreachable_control_depth != 0uz)
    {
        --state.unreachable_control_depth;
        return true;
    }

    auto const current_control_stack_index{state.control_stack.size() - 1uz};
    auto& current_context{state.control_stack.back()};

    auto const branch_target{
        llvm_jit_branch_target_t{.params = current_context.result,
                                 .block = current_context.end_block,
                                 .phis = current_context.end_phis,
                                 .control_stack_index = current_control_stack_index}
    };

    if(current_context.type == llvm_jit_control_context_type::if_then)
    {
        // A missing else is the identity function over the original block parameters. Validation requires the parameter
        // and result tuples to be identical; check it again at the IR boundary before wiring those captured SSA values
        // into the false edge's result PHIs.
        auto const result_count{get_runtime_block_result_count(current_context.result)};
        if(!runtime_block_result_types_equal(current_context.params, current_context.result) || current_context.entry_params.size() != result_count ||
           current_context.end_phis.size() != result_count || current_context.else_block == nullptr) [[unlikely]]
        {
            return false;
        }
        if(current_context.is_reachable && !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, branch_target)) [[unlikely]] { return false; }

        for(::std::size_t result_index{}; result_index != result_count; ++result_index)
        {
            auto const& false_result{current_context.entry_params.index_unchecked(result_index)};
            auto false_result_phi{current_context.end_phis.index_unchecked(result_index)};
            if(false_result.value == nullptr || false_result.type != current_context.result.begin[result_index] || false_result_phi == nullptr ||
               false_result_phi->getType() != false_result.value->getType()) [[unlikely]]
            {
                return false;
            }
            false_result_phi->addIncoming(false_result.value, current_context.else_block);
        }
        ::llvm::IRBuilder<> else_builder(current_context.else_block);
        else_builder.CreateBr(current_context.end_block);
        current_context.end_block_has_incoming = true;
    }
    else if(current_context.is_reachable && !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, branch_target)) [[unlikely]] { return false; }

    auto const block_result{current_context.result};
    auto end_block{current_context.end_block};
    auto end_phis{current_context.end_phis};
    auto const outer_stack_size{current_context.outer_stack_size};
    auto const outer_branch_target_stack_size{current_context.outer_branch_target_stack_size};
    auto const continuation_reachable{current_context.end_block_has_incoming};

    state.control_stack.pop_back();
    while(state.branch_target_stack.size() > outer_branch_target_stack_size) { state.branch_target_stack.pop_back(); }
    truncate_runtime_local_func_llvm_jit_operand_stack_to(state, outer_stack_size);

    if(state.control_stack.empty()) { return true; }

    state.control_stack.back().is_reachable = continuation_reachable;
    if(!continuation_reachable)
    {
        // LLVM still requires a terminator in the merge block even when no edge reaches it.  Remove an unused PHI first
        // so the block is structurally valid.
        if(end_block != nullptr && !llvm_jit_basic_block_has_terminator(end_block))
        {
            for(auto phi: end_phis)
            {
                if(phi != nullptr && phi->getNumIncomingValues() == 0u) { phi->eraseFromParent(); }
            }
            ::llvm::IRBuilder<> unreachable_builder(end_block);
            unreachable_builder.CreateUnreachable();
        }
        return true;
    }
    if(end_block == nullptr) [[unlikely]] { return false; }

    state.ir_builder->SetInsertPoint(end_block);
    auto const result_count{get_runtime_block_result_count(block_result)};
    if(end_phis.size() != result_count) [[unlikely]] { return false; }
    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto phi{end_phis.index_unchecked(result_index)};
        if(phi == nullptr || phi->getNumIncomingValues() == 0u) [[unlikely]] { return false; }
        state.operand_stack.push_back({.type = block_result.begin[result_index], .value = phi});
    }
    return true;
}

// Lower Wasm `br` to an unconditional branch and enter unreachable/polymorphic state for the remainder of the source
// block until a matching structural boundary is encountered.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_br(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                            validation_module_traits_t::wasm_u32 label_index) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr || state.control_stack.empty())
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto branch_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, label_index)};
    if(branch_target == nullptr || !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, *branch_target)) [[unlikely]] { return false; }

    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

// Lower Wasm `br_if`.  The branch value is copied into the target PHI before the conditional branch, while the fallthrough
// path continues with the original branch value still on the operand stack as required by Wasm semantics.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_br_if(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                               validation_module_traits_t::wasm_u32 label_index) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr || state.control_stack.empty())
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto branch_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, label_index)};
    if(branch_target == nullptr) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_function{state.llvm_function};
    auto& ir_builder{*state.ir_builder};

    auto const condition{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(condition.type != runtime_operand_stack_value_type::i32 || condition.value == nullptr) [[unlikely]] { return false; }

    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> branch_values{};
    if(!try_get_runtime_local_func_llvm_jit_branch_values(state, branch_target->params, branch_values)) [[unlikely]] { return false; }

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }

    if(!try_add_runtime_local_func_llvm_jit_branch_target_incoming(state, *branch_target, branch_values, current_block)) [[unlikely]] { return false; }

    auto fallthrough_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"br_if.cont"), llvm_function)};
    auto cond_i1{ir_builder.CreateICmpNE(condition.value, ::llvm::ConstantInt::get(condition.value->getType(), 0u))};
    ir_builder.CreateCondBr(cond_i1, branch_target->block, fallthrough_block);
    ir_builder.SetInsertPoint(fallthrough_block);
    return true;
}

// Lower Wasm `br_table` to an LLVM switch.  All branch targets must have the same arity/type, and duplicate destinations
// still receive duplicate PHI incoming edges because LLVM models edges, not unique predecessor/target pairs.
[[nodiscard]] inline constexpr bool
    try_emit_runtime_local_func_llvm_jit_br_table(runtime_local_func_llvm_jit_emit_state_t& state,
                                                  ::uwvm2::utils::container::vector<validation_module_traits_t::wasm_u32> const& label_indices,
                                                  validation_module_traits_t::wasm_u32 default_label_index) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr || state.control_stack.empty())
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};

    ::uwvm2::utils::container::vector<llvm_jit_branch_target_t const*> branch_targets{};
    branch_targets.reserve(label_indices.size() + 1uz);

    runtime_block_result_type expected_signature{};
    bool have_expected_signature{};

    for(auto const label_index: label_indices)
    {
        auto branch_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, label_index)};
        if(branch_target == nullptr) [[unlikely]] { return false; }

        if(!have_expected_signature)
        {
            have_expected_signature = true;
            expected_signature = branch_target->params;
        }
        else if(!runtime_block_result_types_equal(branch_target->params, expected_signature)) [[unlikely]] { return false; }

        branch_targets.push_back(branch_target);
    }

    auto default_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, default_label_index)};
    if(default_target == nullptr) [[unlikely]] { return false; }

    if(!have_expected_signature)
    {
        have_expected_signature = true;
        expected_signature = default_target->params;
    }
    else if(!runtime_block_result_types_equal(default_target->params, expected_signature)) [[unlikely]] { return false; }

    auto const condition{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(condition.type != runtime_operand_stack_value_type::i32 || condition.value == nullptr) [[unlikely]] { return false; }

    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> branch_values{};
    if(!try_get_runtime_local_func_llvm_jit_branch_values(state, expected_signature, branch_values)) [[unlikely]] { return false; }

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || llvm_jit_basic_block_has_terminator(current_block)) [[unlikely]] { return false; }

    // Add PHI/control-context incoming state for one br_table destination while preserving duplicate switch edges.
    auto const add_target_incoming{[&](llvm_jit_branch_target_t const& branch_target) constexpr noexcept
                                   {
                                       return try_add_runtime_local_func_llvm_jit_branch_target_incoming(
                                           state,
                                           branch_target,
                                           branch_values,
                                           current_block);
                                   }};

    if(!add_target_incoming(*default_target)) [[unlikely]] { return false; }
    for(auto branch_target: branch_targets)
    {
        if(branch_target == nullptr || !add_target_incoming(*branch_target)) [[unlikely]] { return false; }
    }

    auto switch_inst{ir_builder.CreateSwitch(condition.value, default_target->block, static_cast<unsigned>(label_indices.size()))};
    for(::std::size_t target_index{}; target_index != label_indices.size(); ++target_index)
    {
        switch_inst->addCase(::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), target_index),
                             branch_targets.index_unchecked(target_index)->block);
    }

    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

// Lower Wasm `return` as a branch to the implicit function label.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_return(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr || state.control_stack.empty() ||
       state.branch_target_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const& return_target{state.branch_target_stack.index_unchecked(0u)};
    if(!try_emit_runtime_local_func_llvm_jit_branch_to_target(state, return_target)) [[unlikely]] { return false; }

    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

// Pop and type-check call arguments from the operand stack.  Arguments are popped in reverse stack order and written back
// into source parameter order for LLVM call creation.
[[nodiscard]] inline constexpr llvm_jit_prepared_wasm_call_operands_t prepare_runtime_local_func_llvm_jit_wasm_call_operands(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    llvm_jit_prepared_wasm_call_operands_t prepared{};
    prepared.abi_layout = get_runtime_wasm_call_abi_layout(wasm_function_type);
    if(!prepared.abi_layout.valid || state.operand_stack.size() < prepared.abi_layout.parameter_count) [[unlikely]] { return prepared; }

    auto const parameter_begin{wasm_function_type.parameter.begin};
    prepared.arguments.resize(prepared.abi_layout.parameter_count);

    // The Wasm operand stack places the last argument on top.  Fill the LLVM argument vector from the back so the final
    // call operands are back in source-order parameter layout.
    for(::std::size_t parameter_index{prepared.abi_layout.parameter_count}; parameter_index != 0uz; --parameter_index)
    {
        auto const argument{state.operand_stack.back()};
        state.operand_stack.pop_back();

        auto const expected_type{static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index - 1uz])};
        if(argument.type != expected_type || argument.value == nullptr) [[unlikely]] { return {}; }

        prepared.arguments[parameter_index - 1uz] = argument.value;
    }

    prepared.results = runtime_block_result_type{wasm_function_type.result.begin, wasm_function_type.result.end};

    prepared.valid = true;
    return prepared;
}

// Push a typed call result tuple back onto the Wasm operand stack. LLVM returns multiple values as a struct in source
// order, so each field is extracted and pushed independently to preserve normal Wasm stack behavior.
[[nodiscard]] inline constexpr bool push_runtime_local_func_llvm_jit_wasm_call_result(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                      llvm_jit_prepared_wasm_call_operands_t const& prepared,
                                                                                      ::llvm::Value* value) noexcept
{
    auto const result_count{get_runtime_block_result_count(prepared.results)};
    if(result_count == 0uz) { return true; }
    if(value == nullptr || state.ir_builder == nullptr) [[unlikely]] { return false; }
    auto canonical_result_type{get_llvm_result_type_from_wasm_result_range(
        state.ir_builder->getContext(),
        prepared.results.begin,
        prepared.results.end)};
    if(canonical_result_type == nullptr || value->getType() != canonical_result_type) [[unlikely]] { return false; }
    ::llvm::StructType* aggregate_result_type{};
    if(result_count > 1uz)
    {
        if(!canonical_result_type->isStructTy()) [[unlikely]] { return false; }
        aggregate_result_type = static_cast<::llvm::StructType*>(canonical_result_type);
        if(aggregate_result_type->getNumElements() != result_count) [[unlikely]] { return false; }
        for(::std::size_t result_index{}; result_index != result_count; ++result_index)
        {
            auto expected_type{get_llvm_type_from_wasm_value_type(state.ir_builder->getContext(), prepared.results.begin[result_index])};
            if(expected_type == nullptr || aggregate_result_type->getElementType(static_cast<unsigned>(result_index)) != expected_type) [[unlikely]]
            {
                return false;
            }
        }
    }

    for(::std::size_t result_index{}; result_index != result_count; ++result_index)
    {
        auto const result_type{prepared.results.begin[result_index]};
        auto llvm_result_type{get_llvm_type_from_wasm_value_type(state.ir_builder->getContext(), result_type)};
        auto result_value{result_count == 1uz
                              ? value
                              : state.ir_builder->CreateExtractValue(value,
                                                                     {static_cast<unsigned>(result_index)},
                                                                     get_llvm_string_ref(u8"call.result"))};
        if(result_value == nullptr || llvm_result_type == nullptr || result_value->getType() != llvm_result_type) [[unlikely]] { return false; }
        state.operand_stack.push_back({.type = result_type, .value = result_value});
    }
    return true;
}

// Emit a direct typed Wasm-to-Wasm call through an LLVM function declaration in the current module.
[[nodiscard]] inline constexpr ::llvm::CallInst*
    emit_runtime_local_func_llvm_jit_direct_wasm_call_value(runtime_local_func_llvm_jit_emit_state_t& state,
                                                            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                            validation_module_traits_t::wasm_u32 func_index,
                                                            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                            ::llvm::ArrayRef<::llvm::Value*> call_arguments) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_module == nullptr || state.ir_builder == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_module{state.llvm_module};
    auto& ir_builder{*state.ir_builder};

    auto callee_function{get_or_create_llvm_wasm_function_declaration(*llvm_module, llvm_context, runtime_module, func_index, wasm_function_type)};
    if(callee_function == nullptr) [[unlikely]] { return nullptr; }
    return apply_llvm_jit_wasm_calling_conv(ir_builder.CreateCall(callee_function, call_arguments));
}

// Emit a call to a C++ runtime bridge function using the host calling convention.
template <auto BridgeFunction>
[[nodiscard]] inline constexpr ::llvm::CallInst* emit_runtime_local_func_llvm_jit_runtime_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                                      ::llvm::FunctionType* bridge_function_type,
                                                                                                      ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || bridge_function_type == nullptr) [[unlikely]] { return nullptr; }

    auto& ir_builder{*state.ir_builder};

    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<BridgeFunction>(ir_builder, bridge_function_type)};
    if(bridge_pointer == nullptr) [[unlikely]] { return nullptr; }
    return apply_llvm_jit_host_calling_conv(ir_builder.CreateCall(bridge_function_type, bridge_pointer, arguments));
}

// Shared raw-call helper: pack typed arguments into byte buffers, ask the caller to emit the bridge call, then load the
// typed result back from the result buffer if present.
template <typename EmitBridgeCallFromBuffers>
[[nodiscard]] inline constexpr llvm_jit_runtime_raw_bridge_emit_result_t
    emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                  ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                                  ::llvm::ArrayRef<::llvm::Value*> call_arguments,
                                                                  ::llvm::StringRef param_buffer_name,
                                                                  ::llvm::StringRef result_buffer_name,
                                                                  EmitBridgeCallFromBuffers&& emit_bridge_call_from_buffers) noexcept
{
    if(!state.valid || state.ir_builder == nullptr) [[unlikely]] { return {}; }

    auto& ir_builder{*state.ir_builder};

    auto const raw_call_buffers{emit_runtime_raw_call_buffers(ir_builder, wasm_function_type, call_arguments, param_buffer_name, result_buffer_name)};
    if(!raw_call_buffers.valid) [[unlikely]] { return {}; }

    auto bridge_call{emit_bridge_call_from_buffers(raw_call_buffers)};
    if(bridge_call == nullptr) [[unlikely]] { return {}; }

    auto result_value{emit_runtime_raw_call_result_value(ir_builder,
                                                        wasm_function_type,
                                                        raw_call_buffers,
                                                        get_llvm_string_ref(u8"call.raw.result"))};
    if(get_runtime_wasm_call_abi_layout(wasm_function_type).result_count != 0uz && result_value == nullptr) [[unlikely]] { return {}; }

    return llvm_jit_runtime_raw_bridge_emit_result_t{.valid = true, .bridge_call = bridge_call, .result_value = result_value};
}

// Emit a raw runtime call for an imported or otherwise non-direct Wasm callee.  The runtime resolves the target from the
// module address and function index.
[[nodiscard]] inline constexpr llvm_jit_runtime_raw_bridge_emit_result_t
    emit_runtime_local_func_llvm_jit_raw_host_wasm_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                        validation_module_traits_t::wasm_u32 func_index,
                                                        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                        llvm_jit_prepared_wasm_call_operands_t const& prepared_call,
                                                        ::llvm::StringRef param_buffer_name,
                                                        ::llvm::StringRef result_buffer_name) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr) [[unlikely]] { return {}; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    auto const abi_layout{prepared_call.abi_layout};

    return emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(
        state,
        wasm_function_type,
        {prepared_call.arguments.data(), prepared_call.arguments.size()},
        param_buffer_name,
        result_buffer_name,
        [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) constexpr noexcept -> ::llvm::CallInst*
        {
            // Pass the module address and original module function index to the runtime.  The runtime owns import
            // resolution for host/dynamic targets that cannot be represented by a typed LLVM declaration.
            auto bridge_function_type{get_llvm_runtime_raw_call_bridge_function_type(llvm_context)};
            auto const module_symbol_name{get_llvm_runtime_module_object_symbol_name(runtime_module)};
            auto module_address{
                get_llvm_external_host_object_address(ir_builder,
                                                      reinterpret_cast<::std::uintptr_t>(::std::addressof(runtime_module)),
                                                      ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()})};
            if(module_address == nullptr) [[unlikely]] { return nullptr; }

            return emit_runtime_local_func_llvm_jit_runtime_bridge_call<::uwvm2::runtime::lib::llvm_jit_call_raw_host_api>(
                state,
                bridge_function_type,
                {module_address,
                 ::llvm::ConstantInt::get(llvm_i32_type, func_index),
                 raw_call_buffers.result_buffer_address,
                 ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                 raw_call_buffers.param_buffer_address,
                 ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)});
        });
}

// Emit a raw call through the lazy-defined target table for a local defined function whose typed entry is not available.
[[nodiscard]] inline constexpr llvm_jit_runtime_raw_bridge_emit_result_t
    emit_runtime_local_func_llvm_jit_raw_target_wasm_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                          ::std::size_t local_function_index,
                                                          ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                          llvm_jit_prepared_wasm_call_operands_t const& prepared_call,
                                                          ::llvm::StringRef param_buffer_name,
                                                          ::llvm::StringRef result_buffer_name) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_module == nullptr || state.ir_builder == nullptr) [[unlikely]] { return {}; }
    if(state.lazy_defined_raw_call_target_base_address == 0u || local_function_index >= state.lazy_defined_raw_call_target_count) [[unlikely]] { return {}; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_module{state.llvm_module};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto const abi_layout{prepared_call.abi_layout};

    return emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(
        state,
        wasm_function_type,
        {prepared_call.arguments.data(), prepared_call.arguments.size()},
        param_buffer_name,
        result_buffer_name,
        [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) constexpr noexcept -> ::llvm::CallInst*
        {
            // Lazy target records are read at runtime and may be patched after this LLVM function has been optimized.
            // Keep the loads volatile even in non-atomic modes: otherwise Win64 tiered/lazy code can constant-fold the
            // initial zero entry and emit a permanent call-through-null before the first materialization.
            auto raw_entry_function_type{get_llvm_runtime_raw_call_target_entry_function_type(llvm_context)};
            auto raw_target_struct_type{get_llvm_runtime_raw_call_target_struct_type(llvm_context)};
            if(raw_entry_function_type == nullptr || raw_target_struct_type == nullptr) [[unlikely]] { return nullptr; }

            auto runtime_module_ptr{state.local_func_storage_ptr == nullptr ? nullptr : state.local_func_storage_ptr->runtime_module_ptr};
            if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }
            auto const target_table_symbol_name{get_llvm_lazy_raw_target_table_symbol_name(*runtime_module_ptr)};
            auto target_base_ptr{get_llvm_external_host_object_pointer(
                ir_builder,
                state.lazy_defined_raw_call_target_base_address,
                raw_target_struct_type,
                ::uwvm2::utils::container::u8string_view{target_table_symbol_name.data(), target_table_symbol_name.size()})};
            if(target_base_ptr == nullptr) [[unlikely]] { return nullptr; }

            auto target_ptr{ir_builder.CreateInBoundsGEP(raw_target_struct_type,
                                                         target_base_ptr,
                                                         {::llvm::ConstantInt::get(llvm_intptr_type, local_function_index)},
                                                         get_llvm_string_ref(u8"call.lazy.target.ptr"))};
            auto entry_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 0u, get_llvm_string_ref(u8"call.lazy.entry.addr.ptr"))};
            auto context_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 1u, get_llvm_string_ref(u8"call.lazy.context.addr.ptr"))};
            auto entry_address{ir_builder.CreateLoad(llvm_intptr_type, entry_address_ptr, get_llvm_string_ref(u8"call.lazy.entry.addr"))};
            auto context_address{ir_builder.CreateLoad(llvm_intptr_type, context_address_ptr, get_llvm_string_ref(u8"call.lazy.context.addr"))};
            entry_address->setVolatile(true);
            context_address->setVolatile(true);
            entry_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
            context_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
            if(state.lazy_defined_targets_are_atomic)
            {
                entry_address->setAtomic(::llvm::AtomicOrdering::Acquire);
                context_address->setAtomic(::llvm::AtomicOrdering::Acquire);
            }

            emit_llvm_conditional_trap(*llvm_module,
                                       ir_builder,
                                       ir_builder.CreateICmpEQ(entry_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)),
                                       ::uwvm2::runtime::lib::llvm_jit_trap_kind::runtime_invariant_failure);

            auto raw_entry_function_ptr{
                ir_builder.CreateIntToPtr(entry_address, get_llvm_pointer_type(raw_entry_function_type), get_llvm_string_ref(u8"call.lazy.entry.ptr"))};
            return apply_llvm_jit_raw_entry_calling_conv(ir_builder.CreateCall(raw_entry_function_type,
                                                                               raw_entry_function_ptr,
                                                                               {context_address,
                                                                                raw_call_buffers.result_buffer_address,
                                                                                ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                                                                                raw_call_buffers.param_buffer_address,
                                                                                ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)}));
        });
}

// Result of attempting to emit the lazy typed-entry fast path.
struct llvm_jit_lazy_typed_target_emit_result_t
{
    // True when either the known typed entry or fast/slow split was emitted successfully.
    bool valid{};

    // Typed result value, or null for void callees.
    ::llvm::Value* result_value{};
};

// Emit a local call through the typed-entry lazy target table when possible.  If the typed entry is missing at runtime,
// the generated code falls back to the raw target entry and merges results with a PHI.
[[nodiscard]] inline constexpr llvm_jit_lazy_typed_target_emit_result_t
    emit_runtime_local_func_llvm_jit_lazy_typed_target_wasm_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                 ::std::size_t local_function_index,
                                                                 ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                                 llvm_jit_prepared_wasm_call_operands_t const& prepared_call,
                                                                 ::llvm::StringRef param_buffer_name,
                                                                 ::llvm::StringRef result_buffer_name) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr) [[unlikely]] { return {}; }
    if(state.lazy_defined_typed_entry_target_base_address == 0u || state.lazy_defined_raw_call_target_base_address == 0u ||
       local_function_index >= state.lazy_defined_typed_entry_target_count || local_function_index >= state.lazy_defined_raw_call_target_count) [[unlikely]]
    {
        return {};
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto const curr_block{ir_builder.GetInsertBlock()};
    if(curr_block == nullptr || curr_block->getParent() == nullptr) [[unlikely]] { return {}; }

    auto llvm_function{curr_block->getParent()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto typed_entry_ptr_type{get_llvm_pointer_type(llvm_intptr_type)};
    auto callee_function_type{get_llvm_function_type_from_wasm_function_type(llvm_context, wasm_function_type)};
    if(typed_entry_ptr_type == nullptr || callee_function_type == nullptr) [[unlikely]] { return {}; }

    auto runtime_module_ptr{state.local_func_storage_ptr == nullptr ? nullptr : state.local_func_storage_ptr->runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return {}; }
    auto const target_table_symbol_name{get_llvm_lazy_typed_entry_target_table_symbol_name(*runtime_module_ptr)};
    auto target_base_ptr{
        get_llvm_external_host_object_pointer(ir_builder,
                                              state.lazy_defined_typed_entry_target_base_address,
                                              llvm_intptr_type,
                                              ::uwvm2::utils::container::u8string_view{target_table_symbol_name.data(), target_table_symbol_name.size()})};
    if(target_base_ptr == nullptr) [[unlikely]] { return {}; }

    auto const known_typed_entry_targets{reinterpret_cast<::std::uintptr_t const*>(state.lazy_defined_typed_entry_target_base_address)};
    auto const known_typed_entry_address{known_typed_entry_targets[local_function_index]};
    if(!state.lazy_defined_targets_are_atomic && known_typed_entry_address != 0u)
    {
        // If the typed entry was already known while building IR, bake it into a direct constant-pointer call and skip the
        // runtime fast/slow control-flow split.
        auto typed_entry_function_ptr{get_llvm_host_pointer_value(ir_builder, known_typed_entry_address, get_llvm_pointer_type(callee_function_type))};
        if(typed_entry_function_ptr == nullptr) [[unlikely]] { return {}; }
        auto typed_call{apply_llvm_jit_wasm_calling_conv(
            ir_builder.CreateCall(callee_function_type, typed_entry_function_ptr, {prepared_call.arguments.data(), prepared_call.arguments.size()}))};
        if(typed_call == nullptr) [[unlikely]] { return {}; }
        return {.valid = true, .result_value = typed_call};
    }

    auto target_ptr{ir_builder.CreateInBoundsGEP(llvm_intptr_type,
                                                 target_base_ptr,
                                                 {::llvm::ConstantInt::get(llvm_intptr_type, local_function_index)},
                                                 get_llvm_string_ref(u8"call.lazy.typed.target.ptr"))};
    auto typed_entry_address{ir_builder.CreateLoad(llvm_intptr_type, target_ptr, get_llvm_string_ref(u8"call.lazy.typed.entry.addr"))};
    typed_entry_address->setVolatile(true);
    typed_entry_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    if(state.lazy_defined_targets_are_atomic) { typed_entry_address->setAtomic(::llvm::AtomicOrdering::Acquire); }

    // The typed entry pointer may be published after this function is compiled.  Split at runtime: use the typed ABI when
    // available, otherwise fall back to the raw target record for lazy materialization.
    auto fast_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"call.lazy.typed.fast"), llvm_function)};
    auto slow_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"call.lazy.typed.slow"), llvm_function)};
    auto merge_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"call.lazy.typed.merge"), llvm_function)};
    if(fast_block == nullptr || slow_block == nullptr || merge_block == nullptr) [[unlikely]] { return {}; }

    ir_builder.CreateCondBr(ir_builder.CreateICmpNE(typed_entry_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)), fast_block, slow_block);

    ir_builder.SetInsertPoint(fast_block);
    auto typed_entry_function_ptr{
        ir_builder.CreateIntToPtr(typed_entry_address, get_llvm_pointer_type(callee_function_type), get_llvm_string_ref(u8"call.lazy.typed.entry.ptr"))};
    auto fast_call{apply_llvm_jit_wasm_calling_conv(
        ir_builder.CreateCall(callee_function_type, typed_entry_function_ptr, {prepared_call.arguments.data(), prepared_call.arguments.size()}))};
    if(fast_call == nullptr) [[unlikely]] { return {}; }
    auto fast_end_block{ir_builder.GetInsertBlock()};
    ir_builder.CreateBr(merge_block);

    ir_builder.SetInsertPoint(slow_block);
    auto const raw_target_result{emit_runtime_local_func_llvm_jit_raw_target_wasm_call(state,
                                                                                       local_function_index,
                                                                                       wasm_function_type,
                                                                                       prepared_call,
                                                                                       param_buffer_name,
                                                                                       result_buffer_name)};
    if(!raw_target_result.valid) [[unlikely]] { return {}; }
    auto slow_end_block{ir_builder.GetInsertBlock()};
    ir_builder.CreateBr(merge_block);

    ir_builder.SetInsertPoint(merge_block);
    ::llvm::Value* result_value{};
    if(get_runtime_block_result_count(prepared_call.results) != 0uz)
    {
        auto canonical_result_type{get_llvm_result_type_from_wasm_result_range(
            llvm_context,
            prepared_call.results.begin,
            prepared_call.results.end)};
        if(fast_call == nullptr || raw_target_result.result_value == nullptr || canonical_result_type == nullptr ||
           fast_call->getType() != canonical_result_type || raw_target_result.result_value->getType() != canonical_result_type) [[unlikely]]
        {
            return {};
        }
        // Both paths implement the same Wasm call.  Merge their typed results so the caller sees one SSA value regardless
        // of whether the target had already been tiered.
        auto result_phi{ir_builder.CreatePHI(fast_call->getType(), 2u, get_llvm_string_ref(u8"call.lazy.typed.result"))};
        result_phi->addIncoming(fast_call, fast_end_block);
        result_phi->addIncoming(raw_target_result.result_value, slow_end_block);
        result_value = result_phi;
    }

    return {.valid = true, .result_value = result_value};
}

// Dispatch to the scalar bridge function matching a Wasm value type while keeping each bridge call's C++ type exact.
template <auto I32BridgeFunction, auto I64BridgeFunction, auto F32BridgeFunction, auto F64BridgeFunction>
[[nodiscard]] inline constexpr ::llvm::CallInst*
    emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                runtime_operand_stack_value_type value_type,
                                                                ::llvm::FunctionType* bridge_function_type,
                                                                ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
{
    switch(value_type)
    {
        case runtime_operand_stack_value_type::i32:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call<I32BridgeFunction>(state, bridge_function_type, arguments);
        case runtime_operand_stack_value_type::i64:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call<I64BridgeFunction>(state, bridge_function_type, arguments);
        case runtime_operand_stack_value_type::f32:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call<F32BridgeFunction>(state, bridge_function_type, arguments);
        case runtime_operand_stack_value_type::f64:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call<F64BridgeFunction>(state, bridge_function_type, arguments);
        [[unlikely]] default:
            return nullptr;
    }
}

// Emit the local-imported global.get bridge call for the resolved scalar type.
[[nodiscard]] inline constexpr ::llvm::CallInst*
    emit_runtime_local_func_llvm_jit_local_imported_global_get_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                           ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                           validation_module_traits_t::wasm_u32 global_index,
                                                                           runtime_global_access_info_t const& global_access_info,
                                                                           ::llvm::Type* llvm_global_type) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || global_access_info.local_imported_module_ptr == nullptr ||
       llvm_global_type == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto bridge_function_type{::llvm::FunctionType::get(llvm_global_type, {llvm_intptr_type, llvm_intptr_type}, false)};

    auto const module_symbol_name{get_llvm_local_imported_global_module_symbol_name(runtime_module, global_index)};
    auto module_pointer{get_llvm_external_host_object_pointer(ir_builder,
                                                              reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr),
                                                              ::llvm::Type::getInt8Ty(llvm_context),
                                                              ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()})};
    if(module_pointer == nullptr) [[unlikely]] { return nullptr; }

    auto module_address{ir_builder.CreatePtrToInt(module_pointer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.module.addr"))};
    ::llvm::Value* bridge_arguments_array[]{
        module_address,
        ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index),
    };
    auto const bridge_arguments{::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array}};

    return emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call<llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_f32>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_f64>>(
        state,
        global_access_info.value_type,
        bridge_function_type,
        bridge_arguments);
}

// Emit the local-imported global.set bridge call for the resolved scalar type.
[[nodiscard]] inline constexpr ::llvm::CallInst*
    emit_runtime_local_func_llvm_jit_local_imported_global_set_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                           ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                           validation_module_traits_t::wasm_u32 global_index,
                                                                           runtime_global_access_info_t const& global_access_info,
                                                                           ::llvm::Type* llvm_value_type,
                                                                           ::llvm::Value* value) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || global_access_info.local_imported_module_ptr == nullptr ||
       llvm_value_type == nullptr || value == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto bridge_function_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_value_type}, false)};

    auto const module_symbol_name{get_llvm_local_imported_global_module_symbol_name(runtime_module, global_index)};
    auto module_pointer{get_llvm_external_host_object_pointer(ir_builder,
                                                              reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr),
                                                              ::llvm::Type::getInt8Ty(llvm_context),
                                                              ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()})};
    if(module_pointer == nullptr) [[unlikely]] { return nullptr; }

    auto module_address{ir_builder.CreatePtrToInt(module_pointer, llvm_intptr_type, get_llvm_string_ref(u8"global.local_imported.module.addr"))};
    ::llvm::Value* bridge_arguments_array[]{
        module_address,
        ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index),
        value,
    };
    auto const bridge_arguments{::llvm::ArrayRef<::llvm::Value*>{bridge_arguments_array}};

    return emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call<llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_f32>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_f64>>(
        state,
        global_access_info.value_type,
        bridge_function_type,
        bridge_arguments);
}

// Lower Wasm `drop` by removing one value from the JIT operand stack.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_drop(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.valid || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    state.operand_stack.pop_back();
    return true;
}

// Lower WebAssembly 1.0/MVP untyped `select` as an LLVM select over an i32 non-zero condition.  Typed select/reference
// types require extra immediate decoding and value-tag/LLVM-type support before this lowering can be reused.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_select(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.size() < 3uz) [[unlikely]] { return false; }

    auto const selector{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const false_value{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const true_value{state.operand_stack.back()};
    state.operand_stack.pop_back();

    if(selector.type != runtime_operand_stack_value_type::i32 || true_value.type != false_value.type || selector.value == nullptr ||
       true_value.value == nullptr || false_value.value == nullptr) [[unlikely]]
    {
        return false;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    if(selector.value->getType() != llvm_i32_type || true_value.value->getType() != false_value.value->getType()) [[unlikely]] { return false; }

    auto selector_is_nonzero{ir_builder.CreateICmpNE(selector.value, ::llvm::ConstantInt::get(llvm_i32_type, 0u))};
    auto selected_value{ir_builder.CreateSelect(selector_is_nonzero, true_value.value, false_value.value)};
    if(selected_value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = true_value.type, .value = selected_value});
    return true;
}

// Lower Wasm `local.get` by loading from the corresponding entry-block local alloca.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_local_get(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                   validation_module_traits_t::wasm_u32 local_index) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const local_index_uz{static_cast<::std::size_t>(local_index)};
    if(local_index_uz >= state.local_types.size() || local_index_uz >= state.local_pointers.size()) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto const local_type{state.local_types[local_index_uz]};
    auto llvm_local_type{get_llvm_type_from_wasm_value_type(llvm_context, local_type)};
    auto local_pointer{state.local_pointers[local_index_uz]};
    if(llvm_local_type == nullptr || local_pointer == nullptr) [[unlikely]] { return false; }

    auto loaded_value{ir_builder.CreateLoad(llvm_local_type, local_pointer, get_llvm_string_ref(u8"local.get"))};
    if(loaded_value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = local_type, .value = loaded_value});
    return true;
}

// Lower Wasm `local.set` by storing the top operand into the local slot and consuming it.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_local_set(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                   validation_module_traits_t::wasm_u32 local_index) noexcept
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const local_index_uz{static_cast<::std::size_t>(local_index)};
    if(local_index_uz >= state.local_types.size() || local_index_uz >= state.local_pointers.size()) [[unlikely]] { return false; }

    auto const value{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const local_type{state.local_types[local_index_uz]};
    auto local_pointer{state.local_pointers[local_index_uz]};
    if(value.type != local_type || value.value == nullptr || local_pointer == nullptr) [[unlikely]] { return false; }

    state.ir_builder->CreateStore(value.value, local_pointer);
    return true;
}

// Lower Wasm `local.tee` by storing the top operand into the local slot without consuming it.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_local_tee(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                   validation_module_traits_t::wasm_u32 local_index) noexcept
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const local_index_uz{static_cast<::std::size_t>(local_index)};
    if(local_index_uz >= state.local_types.size() || local_index_uz >= state.local_pointers.size()) [[unlikely]] { return false; }

    auto const value{state.operand_stack.back()};
    auto const local_type{state.local_types[local_index_uz]};
    auto local_pointer{state.local_pointers[local_index_uz]};
    if(value.type != local_type || value.value == nullptr || local_pointer == nullptr) [[unlikely]] { return false; }

    state.ir_builder->CreateStore(value.value, local_pointer);
    return true;
}

// Lower Wasm `global.get`, preferring direct storage loads and falling back to local-imported bridge calls.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_global_get(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                    validation_module_traits_t::wasm_u32 global_index) noexcept
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto runtime_module_ptr{state.local_func_storage_ptr->runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto const global_access_info{resolve_runtime_global_access_info(*runtime_module_ptr, global_index)};
    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_global_type{get_llvm_type_from_wasm_value_type(llvm_context, global_access_info.value_type)};
    if(llvm_global_type == nullptr) [[unlikely]] { return false; }

    if(global_access_info.storage_ptr != nullptr)
    {
        auto global_pointer{get_llvm_global_storage_pointer(llvm_context,
                                                            ir_builder,
                                                            *runtime_module_ptr,
                                                            global_index,
                                                            global_access_info.storage_ptr,
                                                            global_access_info.value_type)};
        if(global_pointer == nullptr) [[unlikely]] { return false; }

        auto loaded_value{ir_builder.CreateLoad(llvm_global_type, global_pointer, get_llvm_string_ref(u8"global.get"))};
        if(loaded_value == nullptr) [[unlikely]] { return false; }

        state.operand_stack.push_back({.type = global_access_info.value_type, .value = loaded_value});
        return true;
    }

    auto bridge_call{
        emit_runtime_local_func_llvm_jit_local_imported_global_get_bridge_call(state, *runtime_module_ptr, global_index, global_access_info, llvm_global_type)};
    if(bridge_call == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = global_access_info.value_type, .value = bridge_call});
    return true;
}

// Lower Wasm `global.set`, preferring direct storage stores and falling back to local-imported bridge calls.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_global_set(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                    validation_module_traits_t::wasm_u32 global_index) noexcept
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto runtime_module_ptr{state.local_func_storage_ptr->runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto const global_access_info{resolve_runtime_global_access_info(*runtime_module_ptr, global_index)};
    if(!global_access_info.is_mutable) [[unlikely]] { return false; }

    auto const value{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(value.type != global_access_info.value_type || value.value == nullptr) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    if(global_access_info.storage_ptr != nullptr)
    {
        auto global_pointer{get_llvm_global_storage_pointer(llvm_context,
                                                            *state.ir_builder,
                                                            *runtime_module_ptr,
                                                            global_index,
                                                            global_access_info.storage_ptr,
                                                            global_access_info.value_type)};
        if(global_pointer == nullptr) [[unlikely]] { return false; }

        state.ir_builder->CreateStore(value.value, global_pointer);
        return true;
    }

    auto llvm_value_type{get_llvm_type_from_wasm_value_type(llvm_context, global_access_info.value_type)};
    if(llvm_value_type == nullptr) [[unlikely]] { return false; }

    return emit_runtime_local_func_llvm_jit_local_imported_global_set_bridge_call(state,
                                                                                  *runtime_module_ptr,
                                                                                  global_index,
                                                                                  global_access_info,
                                                                                  llvm_value_type,
                                                                                  value.value) != nullptr;
}

// Lower Wasm `call`.  The emitter chooses, in order, direct typed JIT calls, lazy typed/raw target calls, or the raw
// runtime host bridge depending on import status and compilation mode.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                              validation_module_traits_t::wasm_u32 func_index) noexcept
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const& local_func_storage{*state.local_func_storage_ptr};
    auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto callee_type_ptr{resolve_runtime_callee_function_type(*runtime_module_ptr, func_index)};
    if(callee_type_ptr == nullptr) [[unlikely]] { return false; }

    auto const prepared_call{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *callee_type_ptr)};
    if(!prepared_call.valid) [[unlikely]] { return false; }

    auto const has_lazy_defined_target_tables{state.lazy_defined_raw_call_target_base_address != 0u &&
                                              state.lazy_defined_typed_entry_target_base_address != 0u && state.lazy_defined_raw_call_target_count != 0uz &&
                                              state.lazy_defined_typed_entry_target_count != 0uz};
    auto const emit_lazy_defined_target_call{[&](::std::size_t local_function_index,
                                                 ::llvm::StringRef param_buffer_name,
                                                 ::llvm::StringRef result_buffer_name) constexpr noexcept -> llvm_jit_runtime_raw_bridge_emit_result_t
                                             {
                                                 auto const typed_target_result{
                                                     emit_runtime_local_func_llvm_jit_lazy_typed_target_wasm_call(state,
                                                                                                                  local_function_index,
                                                                                                                  *callee_type_ptr,
                                                                                                                  prepared_call,
                                                                                                                  param_buffer_name,
                                                                                                                  result_buffer_name)};
                                                 if(typed_target_result.valid)
                                                 {
                                                     return {.valid = true, .bridge_call = nullptr, .result_value = typed_target_result.result_value};
                                                 }

                                                 return emit_runtime_local_func_llvm_jit_raw_target_wasm_call(state,
                                                                                                              local_function_index,
                                                                                                              *callee_type_ptr,
                                                                                                              prepared_call,
                                                                                                              param_buffer_name,
                                                                                                              result_buffer_name);
                                             }};

    if(state.route_wasm_calls_through_runtime_bridge)
    {
        // In routed mode, local functions are still allowed to use lazy target tables before falling back to the generic
        // runtime raw-host bridge.  This keeps tiered/lazy replacement possible without direct declarations.
        auto const import_func_count{runtime_module_ptr->imported_function_vec_storage.size()};
        if(static_cast<::std::size_t>(func_index) >= import_func_count)
        {
            auto const local_function_index{static_cast<::std::size_t>(func_index) - import_func_count};
            auto const lazy_target_result{
                emit_lazy_defined_target_call(local_function_index, get_llvm_string_ref(u8"call.params"), get_llvm_string_ref(u8"call.result.buf"))};
            if(lazy_target_result.valid) { return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, lazy_target_result.result_value); }
        }

        auto const raw_bridge_result{emit_runtime_local_func_llvm_jit_raw_host_wasm_call(state,
                                                                                         *runtime_module_ptr,
                                                                                         func_index,
                                                                                         *callee_type_ptr,
                                                                                         prepared_call,
                                                                                         get_llvm_string_ref(u8"call.params"),
                                                                                         get_llvm_string_ref(u8"call.result.buf"))};
        if(!raw_bridge_result.valid) [[unlikely]] { return false; }

        return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, raw_bridge_result.result_value);
    }

    auto const import_func_count{runtime_module_ptr->imported_function_vec_storage.size()};
    if(static_cast<::std::size_t>(func_index) < import_func_count)
    {
        // Imported functions may still resolve to local defined functions through import forwarding.  When that happens
        // and the type matches exactly, emit a direct typed call; otherwise use the raw bridge.
        auto const callee_resolution{resolve_runtime_direct_callee(*runtime_module_ptr, func_index)};
        if(!callee_resolution.state_valid) [[unlikely]] { return false; }

        if(callee_resolution.direct_callable && callee_resolution.function_type_ptr != nullptr &&
           runtime_wasm_function_types_equal(*callee_resolution.function_type_ptr, *callee_type_ptr))
        {
            if(has_lazy_defined_target_tables && static_cast<::std::size_t>(callee_resolution.func_index) >= import_func_count)
            {
                auto const local_function_index{static_cast<::std::size_t>(callee_resolution.func_index) - import_func_count};
                auto const lazy_target_result{
                    emit_lazy_defined_target_call(local_function_index, get_llvm_string_ref(u8"call.params"), get_llvm_string_ref(u8"call.result.buf"))};
                if(lazy_target_result.valid)
                {
                    return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, lazy_target_result.result_value);
                }
            }

            auto call_value{emit_runtime_local_func_llvm_jit_direct_wasm_call_value(state,
                                                                                    *runtime_module_ptr,
                                                                                    callee_resolution.func_index,
                                                                                    *callee_resolution.function_type_ptr,
                                                                                    {prepared_call.arguments.data(), prepared_call.arguments.size()})};
            // `push_runtime_local_func_llvm_jit_wasm_call_result` intentionally accepts null for void callees, so direct
            // call emission must be checked here or a failed void call would be silently dropped from the IR.
            if(call_value == nullptr) [[unlikely]] { return false; }
            return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, call_value);
        }

        auto const raw_bridge_result{emit_runtime_local_func_llvm_jit_raw_host_wasm_call(state,
                                                                                         *runtime_module_ptr,
                                                                                         func_index,
                                                                                         *callee_type_ptr,
                                                                                         prepared_call,
                                                                                         get_llvm_string_ref(u8"call.params"),
                                                                                         get_llvm_string_ref(u8"call.result.buf"))};
        if(!raw_bridge_result.valid) [[unlikely]] { return false; }

        return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, raw_bridge_result.result_value);
    }

    if(has_lazy_defined_target_tables)
    {
        // Lazy MCJIT modules do not necessarily contain every direct callee.  Emitting an ordinary external declaration
        // lets COFF/RuntimeDyld resolve the missing Wasm symbol to zero on Windows.  Route through the runtime target
        // table whenever lazy tables are present; eager/full-module JIT, where all definitions are in the module, keeps
        // the direct typed call below.
        auto const local_function_index{static_cast<::std::size_t>(func_index) - import_func_count};
        auto const lazy_target_result{
            emit_lazy_defined_target_call(local_function_index, get_llvm_string_ref(u8"call.params"), get_llvm_string_ref(u8"call.result.buf"))};
        if(lazy_target_result.valid) { return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, lazy_target_result.result_value); }
    }

    auto call_value{emit_runtime_local_func_llvm_jit_direct_wasm_call_value(state,
                                                                            *runtime_module_ptr,
                                                                            func_index,
                                                                            *callee_type_ptr,
                                                                            {prepared_call.arguments.data(), prepared_call.arguments.size()})};
    if(call_value == nullptr) [[unlikely]] { return false; }
    return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, call_value);
}

// Lower Wasm `call_indirect`.  The generated code checks table bounds, null element, and canonical type id before choosing
// a typed entry fast path or raw-entry fallback for the selected table element.  WebAssembly 1.0/MVP normally selects the
// default table; reference-types/multi-table support must keep the decoded table index, runtime table storage, and this
// selected-table lowering in sync.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_call_indirect(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                       validation_module_traits_t::wasm_u32 type_index,
                                                                                       validation_module_traits_t::wasm_u32 table_index) noexcept
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const& local_func_storage{*state.local_func_storage_ptr};
    auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto const all_table_count{runtime_module_ptr->imported_table_vec_storage.size() + runtime_module_ptr->local_defined_table_vec_storage.size()};
    if(static_cast<::std::size_t>(table_index) >= all_table_count) [[unlikely]] { return false; }

    auto callee_type_ptr{resolve_runtime_type_section_function_type(*runtime_module_ptr, type_index)};
    if(callee_type_ptr == nullptr) [[unlikely]] { return false; }

    auto const abi_layout{get_runtime_wasm_call_abi_layout(*callee_type_ptr)};
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    if(!abi_layout.valid || abi_layout.parameter_count == max_operand_stack_requirement || state.operand_stack.size() < abi_layout.parameter_count + 1uz)
        [[unlikely]]
    {
        return false;
    }

    // In Wasm, call_indirect's table selector is evaluated after the call arguments, so it is the top stack operand.  Pop
    // it first, then reuse the normal call-operand preparation for the remaining arguments.
    auto const selector{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(selector.type != runtime_operand_stack_value_type::i32 || selector.value == nullptr) [[unlikely]] { return false; }

    auto const prepared_call{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *callee_type_ptr)};
    if(!prepared_call.valid) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_module{state.llvm_module};
    auto& ir_builder{*state.ir_builder};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

    auto const expected_type_id{resolve_runtime_canonical_type_id(*runtime_module_ptr, type_index)};
    if(expected_type_id == invalid_runtime_canonical_type_id()) [[unlikely]] { return false; }

    auto table_view_begin{runtime_module_ptr->llvm_jit_call_indirect_table_views.data()};
    if(table_view_begin == nullptr) [[unlikely]] { return false; }

    // The table view is a compact runtime-maintained snapshot: one entry per table with a target-record base address and
    // current size.  The selected target record then carries raw and optional typed entry addresses.
    auto raw_entry_function_type{get_llvm_runtime_raw_call_target_entry_function_type(llvm_context)};
    auto raw_target_struct_type{get_llvm_runtime_raw_call_target_struct_type(llvm_context)};
    auto table_view_struct_type{get_llvm_runtime_call_indirect_table_view_struct_type(llvm_context)};
    auto typed_entry_function_type{get_llvm_function_type_from_wasm_function_type(llvm_context, *callee_type_ptr)};
    if(raw_entry_function_type == nullptr || raw_target_struct_type == nullptr || table_view_struct_type == nullptr || typed_entry_function_type == nullptr)
        [[unlikely]]
    {
        return false;
    }

    auto const table_view_symbol_name{get_llvm_call_indirect_table_view_symbol_name(*runtime_module_ptr)};
    auto table_view_base_ptr{
        get_llvm_external_host_object_pointer(ir_builder,
                                              reinterpret_cast<::std::uintptr_t>(table_view_begin),
                                              table_view_struct_type,
                                              ::uwvm2::utils::container::u8string_view{table_view_symbol_name.data(), table_view_symbol_name.size()})};
    if(table_view_base_ptr == nullptr) [[unlikely]] { return false; }

    auto selector_index{ir_builder.CreateZExt(selector.value, llvm_intptr_type, get_llvm_string_ref(u8"call_indirect.selector.index"))};
    auto table_view_ptr{ir_builder.CreateInBoundsGEP(table_view_struct_type,
                                                     table_view_base_ptr,
                                                     {::llvm::ConstantInt::get(llvm_intptr_type, table_index)},
                                                     get_llvm_string_ref(u8"call_indirect.table_view.ptr"))};
    auto table_data_address_ptr{
        ir_builder.CreateStructGEP(table_view_struct_type, table_view_ptr, 0u, get_llvm_string_ref(u8"call_indirect.table.data.addr.ptr"))};
    auto table_size_ptr{ir_builder.CreateStructGEP(table_view_struct_type, table_view_ptr, 1u, get_llvm_string_ref(u8"call_indirect.table.size.ptr"))};
    auto table_data_address{ir_builder.CreateLoad(llvm_intptr_type, table_data_address_ptr, get_llvm_string_ref(u8"call_indirect.table.data.addr"))};
    auto table_size{ir_builder.CreateLoad(llvm_intptr_type, table_size_ptr, get_llvm_string_ref(u8"call_indirect.table.size"))};
    table_data_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    table_size->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});

    emit_llvm_conditional_trap(*llvm_module,
                               ir_builder,
                               ir_builder.CreateICmpUGE(selector_index, table_size),
                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_table_out_of_bounds);
    emit_llvm_conditional_trap(*llvm_module,
                               ir_builder,
                               ir_builder.CreateICmpEQ(table_data_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)),
                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_null_element);

    auto target_base_ptr{
        ir_builder.CreateIntToPtr(table_data_address, get_llvm_pointer_type(raw_target_struct_type), get_llvm_string_ref(u8"call_indirect.target.base.ptr"))};
    auto target_ptr{ir_builder.CreateInBoundsGEP(raw_target_struct_type, target_base_ptr, selector_index, get_llvm_string_ref(u8"call_indirect.target.ptr"))};
    auto entry_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 0u, get_llvm_string_ref(u8"call_indirect.entry.addr.ptr"))};
    auto context_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 1u, get_llvm_string_ref(u8"call_indirect.context.addr.ptr"))};
    auto encoded_type_id_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 2u, get_llvm_string_ref(u8"call_indirect.type.id.ptr"))};
    auto typed_entry_address_ptr{
        ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 3u, get_llvm_string_ref(u8"call_indirect.typed.entry.addr.ptr"))};
    auto entry_address{ir_builder.CreateLoad(llvm_intptr_type, entry_address_ptr, get_llvm_string_ref(u8"call_indirect.entry.addr"))};
    auto context_address{ir_builder.CreateLoad(llvm_intptr_type, context_address_ptr, get_llvm_string_ref(u8"call_indirect.context.addr"))};
    auto encoded_type_id{ir_builder.CreateLoad(llvm_i32_type, encoded_type_id_ptr, get_llvm_string_ref(u8"call_indirect.type.id"))};
    auto typed_entry_address{ir_builder.CreateLoad(llvm_intptr_type, typed_entry_address_ptr, get_llvm_string_ref(u8"call_indirect.typed.entry.addr"))};
    entry_address->setVolatile(true);
    context_address->setVolatile(true);
    typed_entry_address->setVolatile(true);
    entry_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    context_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    typed_entry_address->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    if(state.lazy_defined_targets_are_atomic)
    {
        // Table views may be patched by lazy compilation.  Acquire ordering pairs with the runtime's publication of entry
        // and context addresses.
        entry_address->setAtomic(::llvm::AtomicOrdering::Acquire);
        context_address->setAtomic(::llvm::AtomicOrdering::Acquire);
        typed_entry_address->setAtomic(::llvm::AtomicOrdering::Acquire);
    }

    emit_llvm_conditional_trap(*llvm_module,
                               ir_builder,
                               ir_builder.CreateICmpEQ(entry_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)),
                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_null_element);
    emit_llvm_conditional_trap(*llvm_module,
                               ir_builder,
                               ir_builder.CreateICmpNE(encoded_type_id, ::llvm::ConstantInt::get(llvm_i32_type, expected_type_id)),
                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::call_indirect_type_mismatch);

    // After bounds/null/type checks, the raw entry is always callable.  The typed entry is an optimization for targets
    // whose exact LLVM signature is already available.
    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getParent() == nullptr) [[unlikely]] { return false; }

    auto llvm_function{current_block->getParent()};
    auto typed_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"call_indirect.typed"), llvm_function)};
    auto raw_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"call_indirect.raw"), llvm_function)};
    auto merge_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"call_indirect.merge"), llvm_function)};
    if(typed_block == nullptr || raw_block == nullptr || merge_block == nullptr) [[unlikely]] { return false; }

    ir_builder.CreateCondBr(ir_builder.CreateICmpNE(typed_entry_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)), typed_block, raw_block);

    ir_builder.SetInsertPoint(typed_block);
    auto typed_entry_function_ptr{ir_builder.CreateIntToPtr(typed_entry_address,
                                                            get_llvm_pointer_type(typed_entry_function_type),
                                                            get_llvm_string_ref(u8"call_indirect.typed.entry.ptr"))};
    auto typed_call{apply_llvm_jit_wasm_calling_conv(
        ir_builder.CreateCall(typed_entry_function_type, typed_entry_function_ptr, {prepared_call.arguments.data(), prepared_call.arguments.size()}))};
    if(typed_call == nullptr) [[unlikely]] { return false; }
    auto typed_end_block{ir_builder.GetInsertBlock()};
    ir_builder.CreateBr(merge_block);

    ir_builder.SetInsertPoint(raw_block);
    auto const raw_bridge_result{emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(
        state,
        *callee_type_ptr,
        {prepared_call.arguments.data(), prepared_call.arguments.size()},
        get_llvm_string_ref(u8"call_indirect.params"),
        get_llvm_string_ref(u8"call_indirect.result.buf"),
        [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) constexpr noexcept -> ::llvm::CallInst*
        {
            auto raw_entry_function_ptr{
                ir_builder.CreateIntToPtr(entry_address, get_llvm_pointer_type(raw_entry_function_type), get_llvm_string_ref(u8"call_indirect.entry.ptr"))};
            return apply_llvm_jit_raw_entry_calling_conv(ir_builder.CreateCall(raw_entry_function_type,
                                                                               raw_entry_function_ptr,
                                                                               {context_address,
                                                                                raw_call_buffers.result_buffer_address,
                                                                                ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                                                                                raw_call_buffers.param_buffer_address,
                                                                                ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)}));
        })};
    if(!raw_bridge_result.valid) [[unlikely]] { return false; }
    auto raw_end_block{ir_builder.GetInsertBlock()};
    ir_builder.CreateBr(merge_block);

    ir_builder.SetInsertPoint(merge_block);
    ::llvm::Value* result_value{};
    if(get_runtime_block_result_count(prepared_call.results) != 0uz)
    {
        auto canonical_result_type{get_llvm_result_type_from_wasm_result_range(
            llvm_context,
            prepared_call.results.begin,
            prepared_call.results.end)};
        if(typed_call == nullptr || raw_bridge_result.result_value == nullptr || canonical_result_type == nullptr ||
           typed_call->getType() != canonical_result_type || raw_bridge_result.result_value->getType() != canonical_result_type) [[unlikely]]
        {
            return false;
        }
        auto result_phi{ir_builder.CreatePHI(typed_call->getType(), 2u, get_llvm_string_ref(u8"call_indirect.result"))};
        result_phi->addIncoming(typed_call, typed_end_block);
        result_phi->addIncoming(raw_bridge_result.result_value, raw_end_block);
        result_value = result_phi;
    }

    return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, result_value);
}

// Generic constant emitter used by numeric opcode case files.  The supplied callable builds the LLVM constant lazily in
// the current context.
template <typename CreateValue>
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_constant(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                  runtime_operand_stack_value_type result_type,
                                                                                  CreateValue&& create_value) noexcept
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }

    auto value{create_value(*state.llvm_context_holder)};
    if(value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = result_type, .value = value});
    return true;
}

// Generic unary emitter used by numeric conversion and arithmetic opcodes.
template <typename CreateValue>
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_unary(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                               runtime_operand_stack_value_type expected_type,
                                                                               runtime_operand_stack_value_type result_type,
                                                                               CreateValue&& create_value) noexcept
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const operand{state.operand_stack.back()};
    state.operand_stack.pop_back();

    if(operand.type != expected_type || operand.value == nullptr) [[unlikely]] { return false; }

    auto value{create_value(*state.ir_builder, operand)};
    if(value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = result_type, .value = value});
    return true;
}

// Generic binary emitter used by numeric arithmetic/comparison opcode case files.
template <typename CreateValue>
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_binary(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                runtime_operand_stack_value_type expected_type,
                                                                                runtime_operand_stack_value_type result_type,
                                                                                CreateValue&& create_value) noexcept
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.size() < 2uz) [[unlikely]] { return false; }

    auto const right{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const left{state.operand_stack.back()};
    state.operand_stack.pop_back();

    if(left.type != expected_type || right.type != expected_type || left.value == nullptr || right.value == nullptr) [[unlikely]] { return false; }

    auto value{create_value(*state.ir_builder, left, right)};
    if(value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = result_type, .value = value});
    return true;
}

template <::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind ScalarKind>
struct llvm_jit_simd_scalar_traits;

template <>
struct llvm_jit_simd_scalar_traits<::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind::i32>
{
    using type = runtime_wasm_i32;
    inline static constexpr runtime_operand_stack_value_type value_type{runtime_operand_stack_value_type::i32};
};

template <>
struct llvm_jit_simd_scalar_traits<::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind::i64>
{
    using type = runtime_wasm_i64;
    inline static constexpr runtime_operand_stack_value_type value_type{runtime_operand_stack_value_type::i64};
};

template <>
struct llvm_jit_simd_scalar_traits<::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind::f32>
{
    using type = runtime_wasm_f32;
    inline static constexpr runtime_operand_stack_value_type value_type{runtime_operand_stack_value_type::f32};
};

template <>
struct llvm_jit_simd_scalar_traits<::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind::f64>
{
    using type = runtime_wasm_f64;
    inline static constexpr runtime_operand_stack_value_type value_type{runtime_operand_stack_value_type::f64};
};

// Lower one already-decoded SIMD instruction through its opcode-specialized typed bridge. v128 never crosses the C++
// ABI by value: i128 is the internal LLVM storage form and entry-block buffers carry the exact 16 payload bytes.
template <llvm_jit_simd_code Op,
          ::uwvm2::runtime::compiler::shared::wasm1p1_simd_instruction_kind Kind,
          ::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind ScalarKind,
          ::std::size_t LaneCount,
          ::std::uint_least32_t MaxAlign>
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_typed_simd_instruction(
    runtime_local_func_llvm_jit_emit_state_t& state,
    runtime_wasm_u32 static_offset,
    runtime_wasm_u32 lane,
    ::std::byte const* immediate_bytes) noexcept
{
    using simd_kind = ::uwvm2::runtime::compiler::shared::wasm1p1_simd_instruction_kind;
    using simd_scalar_kind = ::uwvm2::runtime::compiler::shared::wasm1p1_simd_scalar_kind;

    static_cast<void>(MaxAlign);
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_module == nullptr || state.ir_builder == nullptr ||
       state.local_func_storage_ptr == nullptr) [[unlikely]]
    {
        return false;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto& operand_stack{state.operand_stack};
    auto llvm_v128_type{get_llvm_type_from_wasm_value_type(llvm_context, runtime_operand_stack_value_type::v128)};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
    if(llvm_v128_type == nullptr) [[unlikely]] { return false; }

    // Clang may render distinct non-type function-template arguments identically in __PRETTY_FUNCTION__ when their
    // signatures match. Keep the external MCJIT symbol stable and opcode-specific instead of letting, for example,
    // i32x4.add and i32x4.eq register the same symbol name.
    auto const bridge_discriminator{
        ::uwvm2::utils::container::u8concat_uwvm(u8"simd_",
                                                 ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least32_t>(Op)),
                                                 u8"_",
                                                 ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(Kind)),
                                                 u8"_",
                                                 ::fast_io::mnp::hex<false, true>(static_cast<::std::uint_least8_t>(ScalarKind)),
                                                 u8"_",
                                                 ::fast_io::mnp::hex<false, true>(LaneCount),
                                                 u8"_",
                                                 ::fast_io::mnp::hex<false, true>(MaxAlign))};

    auto const create_v128_buffer{[&](::llvm::Value* value, ::llvm::StringRef name) constexpr noexcept -> ::llvm::AllocaInst*
                                  {
                                      auto buffer{create_llvm_jit_entry_block_alloca(ir_builder, llvm_v128_type, nullptr, name)};
                                      if(buffer == nullptr) [[unlikely]] { return nullptr; }
                                      if(value != nullptr)
                                      {
                                          if(value->getType() != llvm_v128_type) [[unlikely]] { return nullptr; }
                                          ir_builder.CreateStore(value, buffer);
                                      }
                                      return buffer;
                                  }};
    auto const get_buffer_address{[&](::llvm::AllocaInst* buffer, ::llvm::StringRef name) constexpr noexcept -> ::llvm::Value*
                                  {
                                      if(buffer == nullptr) [[unlikely]] { return nullptr; }
                                      return ir_builder.CreatePtrToInt(buffer, llvm_intptr_type, name);
                                  }};
    auto const emit_bridge_call{[&]<auto BridgeFunction>(::llvm::FunctionType* function_type,
                                ::llvm::ArrayRef<::llvm::Value*> arguments) constexpr noexcept -> ::llvm::CallInst*
                                {
                                    auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<BridgeFunction>(
                                        ir_builder,
                                        function_type,
                                        ::uwvm2::utils::container::u8string_view{bridge_discriminator.data(), bridge_discriminator.size()})};
                                    if(bridge_pointer == nullptr) [[unlikely]] { return nullptr; }
                                    return apply_llvm_jit_host_calling_conv(ir_builder.CreateCall(function_type, bridge_pointer, arguments));
                                }};
    auto const push_v128_result{[&](::llvm::AllocaInst* result_buffer) constexpr noexcept -> bool
                                {
                                    if(result_buffer == nullptr) [[unlikely]] { return false; }
                                    auto result{ir_builder.CreateLoad(llvm_v128_type, result_buffer, get_llvm_string_ref(u8"simd.result"))};
                                    if(result == nullptr) [[unlikely]] { return false; }
                                    operand_stack.push_back({.type = runtime_operand_stack_value_type::v128, .value = result});
                                    return true;
                                }};
    auto const make_v128_constant{[&](::std::byte const* bytes) constexpr noexcept -> ::llvm::Constant*
                                  {
                                      if(bytes == nullptr) [[unlikely]] { return nullptr; }
                                      ::std::uint64_t words[2]{};
                                      for(::std::size_t byte_index{}; byte_index != 16uz; ++byte_index)
                                      {
                                          // APInt words are least-significant first, while an i128 store follows the
                                          // native target byte order. Choose the numeric bit position that makes the
                                          // alloca contain the original Wasm byte sequence on either endian.
                                          auto const numeric_byte_index{
                                              ::std::endian::native == ::std::endian::little ? byte_index : 15uz - byte_index};
                                          auto const word_index{numeric_byte_index / 8uz};
                                          auto const bit_offset{static_cast<unsigned>((numeric_byte_index % 8uz) * 8uz)};
                                          words[word_index] |=
                                              static_cast<::std::uint64_t>(::std::to_integer<::std::uint_least8_t>(bytes[byte_index])) << bit_offset;
                                      }
                                      return ::llvm::ConstantInt::get(llvm_context,
                                                                      ::llvm::APInt{128u, ::llvm::ArrayRef<::std::uint64_t>{words, 2uz}});
                                  }};

    if constexpr(Kind == simd_kind::constant)
    {
        auto value{make_v128_constant(immediate_bytes)};
        if(value == nullptr) [[unlikely]] { return false; }
        operand_stack.push_back({.type = runtime_operand_stack_value_type::v128, .value = value});
        return true;
    }
    else if constexpr(Kind == simd_kind::unary || Kind == simd_kind::test)
    {
        if(operand_stack.empty()) [[unlikely]] { return false; }
        auto operand{operand_stack.back()};
        operand_stack.pop_back();
        if(operand.type != runtime_operand_stack_value_type::v128 || operand.value == nullptr) [[unlikely]] { return false; }
        auto operand_buffer{create_v128_buffer(operand.value, get_llvm_string_ref(u8"simd.operand"))};
        auto operand_address{get_buffer_address(operand_buffer, get_llvm_string_ref(u8"simd.operand.addr"))};
        if(operand_address == nullptr) [[unlikely]] { return false; }
        if constexpr(Kind == simd_kind::test)
        {
            auto function_type{::llvm::FunctionType::get(llvm_i32_type, {llvm_intptr_type}, false)};
            auto result{emit_bridge_call.template operator()<llvm_jit_simd_test_bridge<Op>>(function_type, {operand_address})};
            if(result == nullptr) [[unlikely]] { return false; }
            operand_stack.push_back({.type = runtime_operand_stack_value_type::i32, .value = result});
            return true;
        }
        else
        {
            auto result_buffer{create_v128_buffer(nullptr, get_llvm_string_ref(u8"simd.result.buffer"))};
            auto result_address{get_buffer_address(result_buffer, get_llvm_string_ref(u8"simd.result.addr"))};
            auto function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type}, false)};
            if(result_address == nullptr ||
               emit_bridge_call.template operator()<llvm_jit_simd_unary_bridge<Op>>(function_type, {result_address, operand_address}) == nullptr) [[unlikely]]
            {
                return false;
            }
            return push_v128_result(result_buffer);
        }
    }
    else if constexpr(Kind == simd_kind::binary || Kind == simd_kind::shuffle)
    {
        if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
        auto rhs{operand_stack.back()};
        operand_stack.pop_back();
        auto lhs{operand_stack.back()};
        operand_stack.pop_back();
        if(lhs.type != runtime_operand_stack_value_type::v128 || rhs.type != runtime_operand_stack_value_type::v128 ||
           lhs.value == nullptr || rhs.value == nullptr) [[unlikely]]
        {
            return false;
        }
        auto lhs_buffer{create_v128_buffer(lhs.value, get_llvm_string_ref(u8"simd.lhs"))};
        auto rhs_buffer{create_v128_buffer(rhs.value, get_llvm_string_ref(u8"simd.rhs"))};
        auto result_buffer{create_v128_buffer(nullptr, get_llvm_string_ref(u8"simd.result.buffer"))};
        auto lhs_address{get_buffer_address(lhs_buffer, get_llvm_string_ref(u8"simd.lhs.addr"))};
        auto rhs_address{get_buffer_address(rhs_buffer, get_llvm_string_ref(u8"simd.rhs.addr"))};
        auto result_address{get_buffer_address(result_buffer, get_llvm_string_ref(u8"simd.result.addr"))};
        if(lhs_address == nullptr || rhs_address == nullptr || result_address == nullptr) [[unlikely]] { return false; }
        if constexpr(Kind == simd_kind::shuffle)
        {
            auto lane_constant{make_v128_constant(immediate_bytes)};
            auto lane_buffer{create_v128_buffer(lane_constant, get_llvm_string_ref(u8"simd.shuffle.lanes"))};
            auto lane_address{get_buffer_address(lane_buffer, get_llvm_string_ref(u8"simd.shuffle.lanes.addr"))};
            auto function_type{
                ::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
            if(lane_address == nullptr || emit_bridge_call.template operator()<llvm_jit_simd_shuffle_bridge<Op>>(
                                              function_type,
                                              {result_address, lhs_address, rhs_address, lane_address}) == nullptr) [[unlikely]]
            {
                return false;
            }
        }
        else
        {
            auto function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
            if(emit_bridge_call.template operator()<llvm_jit_simd_binary_bridge<Op>>(
                   function_type,
                   {result_address, lhs_address, rhs_address}) == nullptr) [[unlikely]]
            {
                return false;
            }
        }
        return push_v128_result(result_buffer);
    }
    else if constexpr(Kind == simd_kind::ternary)
    {
        if(operand_stack.size() < 3uz) [[unlikely]] { return false; }
        auto mask{operand_stack.back()};
        operand_stack.pop_back();
        auto rhs{operand_stack.back()};
        operand_stack.pop_back();
        auto lhs{operand_stack.back()};
        operand_stack.pop_back();
        if(lhs.type != runtime_operand_stack_value_type::v128 || rhs.type != runtime_operand_stack_value_type::v128 ||
           mask.type != runtime_operand_stack_value_type::v128 || lhs.value == nullptr || rhs.value == nullptr || mask.value == nullptr) [[unlikely]]
        {
            return false;
        }
        auto lhs_buffer{create_v128_buffer(lhs.value, get_llvm_string_ref(u8"simd.lhs"))};
        auto rhs_buffer{create_v128_buffer(rhs.value, get_llvm_string_ref(u8"simd.rhs"))};
        auto mask_buffer{create_v128_buffer(mask.value, get_llvm_string_ref(u8"simd.mask"))};
        auto result_buffer{create_v128_buffer(nullptr, get_llvm_string_ref(u8"simd.result.buffer"))};
        auto lhs_address{get_buffer_address(lhs_buffer, get_llvm_string_ref(u8"simd.lhs.addr"))};
        auto rhs_address{get_buffer_address(rhs_buffer, get_llvm_string_ref(u8"simd.rhs.addr"))};
        auto mask_address{get_buffer_address(mask_buffer, get_llvm_string_ref(u8"simd.mask.addr"))};
        auto result_address{get_buffer_address(result_buffer, get_llvm_string_ref(u8"simd.result.addr"))};
        auto function_type{
            ::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type}, false)};
        if(lhs_address == nullptr || rhs_address == nullptr || mask_address == nullptr || result_address == nullptr ||
           emit_bridge_call.template operator()<llvm_jit_simd_ternary_bridge<Op>>(
               function_type,
               {result_address, lhs_address, rhs_address, mask_address}) == nullptr) [[unlikely]]
        {
            return false;
        }
        return push_v128_result(result_buffer);
    }
    else if constexpr(Kind == simd_kind::shift)
    {
        if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
        auto count{operand_stack.back()};
        operand_stack.pop_back();
        auto operand{operand_stack.back()};
        operand_stack.pop_back();
        if(count.type != runtime_operand_stack_value_type::i32 || operand.type != runtime_operand_stack_value_type::v128 ||
           count.value == nullptr || operand.value == nullptr) [[unlikely]]
        {
            return false;
        }
        auto operand_buffer{create_v128_buffer(operand.value, get_llvm_string_ref(u8"simd.operand"))};
        auto result_buffer{create_v128_buffer(nullptr, get_llvm_string_ref(u8"simd.result.buffer"))};
        auto operand_address{get_buffer_address(operand_buffer, get_llvm_string_ref(u8"simd.operand.addr"))};
        auto result_address{get_buffer_address(result_buffer, get_llvm_string_ref(u8"simd.result.addr"))};
        auto function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type, llvm_i32_type}, false)};
        if(operand_address == nullptr || result_address == nullptr ||
           emit_bridge_call.template operator()<llvm_jit_simd_shift_bridge<Op>>(
               function_type,
               {result_address, operand_address, count.value}) == nullptr) [[unlikely]]
        {
            return false;
        }
        return push_v128_result(result_buffer);
    }
    else if constexpr(Kind == simd_kind::splat || Kind == simd_kind::extract_lane || Kind == simd_kind::replace_lane)
    {
        static_assert(ScalarKind != simd_scalar_kind::none);
        using scalar_traits = llvm_jit_simd_scalar_traits<ScalarKind>;
        using scalar_type = typename scalar_traits::type;
        constexpr auto scalar_value_type{scalar_traits::value_type};
        auto llvm_scalar_type{get_llvm_type_from_wasm_value_type(llvm_context, scalar_value_type)};
        if(llvm_scalar_type == nullptr) [[unlikely]] { return false; }

        if constexpr(Kind == simd_kind::splat)
        {
            if(operand_stack.empty()) [[unlikely]] { return false; }
            auto scalar{operand_stack.back()};
            operand_stack.pop_back();
            if(scalar.type != scalar_value_type || scalar.value == nullptr) [[unlikely]] { return false; }
            auto result_buffer{create_v128_buffer(nullptr, get_llvm_string_ref(u8"simd.result.buffer"))};
            auto result_address{get_buffer_address(result_buffer, get_llvm_string_ref(u8"simd.result.addr"))};
            auto function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_scalar_type}, false)};
            if(result_address == nullptr || emit_bridge_call.template operator()<llvm_jit_simd_splat_bridge<Op, scalar_type>>(
                                                function_type,
                                                {result_address, scalar.value}) == nullptr) [[unlikely]]
            {
                return false;
            }
            return push_v128_result(result_buffer);
        }
        else if constexpr(Kind == simd_kind::extract_lane)
        {
            static_assert(LaneCount != 0uz);
            if(operand_stack.empty()) [[unlikely]] { return false; }
            auto operand{operand_stack.back()};
            operand_stack.pop_back();
            if(operand.type != runtime_operand_stack_value_type::v128 || operand.value == nullptr) [[unlikely]] { return false; }
            auto operand_buffer{create_v128_buffer(operand.value, get_llvm_string_ref(u8"simd.operand"))};
            auto operand_address{get_buffer_address(operand_buffer, get_llvm_string_ref(u8"simd.operand.addr"))};
            auto function_type{::llvm::FunctionType::get(llvm_scalar_type, {llvm_intptr_type, llvm_i32_type}, false)};
            auto result{operand_address == nullptr
                            ? nullptr
                            : emit_bridge_call.template operator()<llvm_jit_simd_extract_lane_bridge<Op, scalar_type>>(
                                  function_type,
                                  {operand_address, ::llvm::ConstantInt::get(llvm_i32_type, lane)})};
            if(result == nullptr) [[unlikely]] { return false; }
            operand_stack.push_back({.type = scalar_value_type, .value = result});
            return true;
        }
        else
        {
            static_assert(LaneCount != 0uz);
            if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
            auto scalar{operand_stack.back()};
            operand_stack.pop_back();
            auto operand{operand_stack.back()};
            operand_stack.pop_back();
            if(scalar.type != scalar_value_type || operand.type != runtime_operand_stack_value_type::v128 ||
               scalar.value == nullptr || operand.value == nullptr) [[unlikely]]
            {
                return false;
            }
            auto operand_buffer{create_v128_buffer(operand.value, get_llvm_string_ref(u8"simd.operand"))};
            auto result_buffer{create_v128_buffer(nullptr, get_llvm_string_ref(u8"simd.result.buffer"))};
            auto operand_address{get_buffer_address(operand_buffer, get_llvm_string_ref(u8"simd.operand.addr"))};
            auto result_address{get_buffer_address(result_buffer, get_llvm_string_ref(u8"simd.result.addr"))};
            auto function_type{
                ::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type, llvm_scalar_type, llvm_i32_type}, false)};
            if(operand_address == nullptr || result_address == nullptr ||
               emit_bridge_call.template operator()<llvm_jit_simd_replace_lane_bridge<Op, scalar_type>>(
                   function_type,
                   {result_address, operand_address, scalar.value, ::llvm::ConstantInt::get(llvm_i32_type, lane)}) == nullptr) [[unlikely]]
            {
                return false;
            }
            return push_v128_result(result_buffer);
        }
    }
    else if constexpr(Kind == simd_kind::memory_load || Kind == simd_kind::memory_store)
    {
        auto const& local_func_storage{*state.local_func_storage_ptr};
        auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
        if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }
        if(!state.memory0_access_info_resolved)
        {
            state.memory0_access_info = resolve_runtime_memory_access_info(*runtime_module_ptr, 0u);
            state.memory0_access_info_resolved = true;
        }
        auto const& memory_info{state.memory0_access_info};
        if(memory_info.memory_p == nullptr && memory_info.local_imported_module_ptr == nullptr) [[unlikely]] { return false; }

        llvm_jit_stack_value_t address{};
        llvm_jit_stack_value_t vector_value{};
        if constexpr(Kind == simd_kind::memory_load && LaneCount == 0uz)
        {
            if(operand_stack.empty()) [[unlikely]] { return false; }
            address = operand_stack.back();
            operand_stack.pop_back();
        }
        else
        {
            if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
            vector_value = operand_stack.back();
            operand_stack.pop_back();
            address = operand_stack.back();
            operand_stack.pop_back();
            if(vector_value.type != runtime_operand_stack_value_type::v128 || vector_value.value == nullptr) [[unlikely]] { return false; }
        }
        if(address.type != runtime_operand_stack_value_type::i32 || address.value == nullptr) [[unlikely]] { return false; }

        auto zero_intptr{::llvm::ConstantInt::get(llvm_intptr_type, 0u)};
        auto lane_value{::llvm::ConstantInt::get(llvm_i32_type, lane)};
        ::llvm::AllocaInst* vector_buffer{};
        ::llvm::Value* vector_address{zero_intptr};
        if constexpr(Kind == simd_kind::memory_store || LaneCount != 0uz)
        {
            vector_buffer = create_v128_buffer(vector_value.value, get_llvm_string_ref(u8"simd.memory.value"));
            vector_address = get_buffer_address(vector_buffer, get_llvm_string_ref(u8"simd.memory.value.addr"));
            if(vector_address == nullptr) [[unlikely]] { return false; }
        }

        ::llvm::AllocaInst* result_buffer{};
        ::llvm::Value* result_address{};
        if constexpr(Kind == simd_kind::memory_load)
        {
            result_buffer = create_v128_buffer(nullptr, get_llvm_string_ref(u8"simd.memory.result"));
            result_address = get_buffer_address(result_buffer, get_llvm_string_ref(u8"simd.memory.result.addr"));
            if(result_address == nullptr) [[unlikely]] { return false; }
        }

        auto static_offset_value{::llvm::ConstantInt::get(llvm_i32_type, static_offset)};
        ::llvm::CallInst* call{};
        if(memory_info.memory_p != nullptr)
        {
            auto const symbol_name{get_llvm_native_memory_object_symbol_name(*runtime_module_ptr, 0u)};
            auto memory_address{get_llvm_external_host_object_address(
                ir_builder,
                reinterpret_cast<::std::uintptr_t>(memory_info.memory_p),
                ::uwvm2::utils::container::u8string_view{symbol_name.data(), symbol_name.size()})};
            if(memory_address == nullptr) [[unlikely]] { return false; }
            if constexpr(Kind == simd_kind::memory_load)
            {
                auto function_type{::llvm::FunctionType::get(
                    llvm_void_type,
                    {llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_intptr_type, llvm_i32_type, llvm_intptr_type},
                    false)};
                call = emit_bridge_call.template operator()<llvm_jit_simd_memory_load_bridge<Op>>(
                    function_type,
                    {memory_address, static_offset_value, address.value, vector_address, lane_value, result_address});
            }
            else
            {
                auto function_type{::llvm::FunctionType::get(
                    llvm_void_type,
                    {llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_intptr_type, llvm_i32_type},
                    false)};
                call = emit_bridge_call.template operator()<llvm_jit_simd_memory_store_bridge<Op>>(
                    function_type,
                    {memory_address, static_offset_value, address.value, vector_address, lane_value});
            }
        }
        else
        {
            auto const symbol_name{get_llvm_local_imported_memory_module_symbol_name(
                *runtime_module_ptr,
                static_cast<runtime_wasm_u32>(memory_info.local_imported_memory_index))};
            auto memory_module_address{get_llvm_external_host_object_address(
                ir_builder,
                reinterpret_cast<::std::uintptr_t>(memory_info.local_imported_module_ptr),
                ::uwvm2::utils::container::u8string_view{symbol_name.data(), symbol_name.size()})};
            if(memory_module_address == nullptr) [[unlikely]] { return false; }
            auto memory_index_value{::llvm::ConstantInt::get(llvm_intptr_type, memory_info.local_imported_memory_index)};
            if constexpr(Kind == simd_kind::memory_load)
            {
                auto function_type{::llvm::FunctionType::get(
                    llvm_void_type,
                    {llvm_intptr_type, llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_intptr_type, llvm_i32_type, llvm_intptr_type},
                    false)};
                call = emit_bridge_call.template operator()<llvm_jit_simd_local_imported_memory_load_bridge<Op>>(
                    function_type,
                    {memory_module_address, memory_index_value, static_offset_value, address.value, vector_address, lane_value, result_address});
            }
            else
            {
                auto function_type{::llvm::FunctionType::get(
                    llvm_void_type,
                    {llvm_intptr_type, llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_intptr_type, llvm_i32_type},
                    false)};
                call = emit_bridge_call.template operator()<llvm_jit_simd_local_imported_memory_store_bridge<Op>>(
                    function_type,
                    {memory_module_address, memory_index_value, static_offset_value, address.value, vector_address, lane_value});
            }
        }
        if(call == nullptr) [[unlikely]] { return false; }
        if constexpr(Kind == simd_kind::memory_load) { return push_v128_result(result_buffer); }
        else { return true; }
    }
    else
    {
        return false;
    }
}

// Emit exactly one Wasm instruction from `instruction_begin..instruction_end`.  The caller slices instructions before
// calling this function; returning true means the slice was fully consumed and the emit state remains usable.
[[nodiscard]] inline constexpr bool try_emit_runtime_local_func_llvm_jit_instruction(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                     ::std::byte const* instruction_begin,
                                                                                     ::std::byte const* instruction_end) noexcept
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.llvm_function == nullptr || state.ir_builder == nullptr || instruction_begin == nullptr || instruction_end == nullptr ||
       instruction_begin > instruction_end) [[unlikely]]
    {
        return false;
    }

    // Local references intentionally mirror the names used by opcode include files.  They keep the included case bodies
    // compact while still making all dependencies explicit in this dispatcher scope.
    auto const& local_func_storage{*state.local_func_storage_ptr};
    [[maybe_unused]] auto const& local_types{state.local_types};
    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_module{state.llvm_module};
    [[maybe_unused]] auto llvm_function{state.llvm_function};
    auto& ir_builder{*state.ir_builder};
    [[maybe_unused]] auto const& local_pointers{state.local_pointers};
    auto& memory0_access_info{state.memory0_access_info};
    auto& memory0_access_info_resolved{state.memory0_access_info_resolved};
    auto& operand_stack{state.operand_stack};
    auto& control_stack{state.control_stack};
    auto& unreachable_control_depth{state.unreachable_control_depth};
    auto code_curr{instruction_begin};
    auto const code_end{instruction_end};
    constexpr bool result{};
    using wasm1p1_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_basic;
    using wasm1p1_numeric_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_numeric;
    using wasm1p1_simd_code = ::uwvm2::parser::wasm::standard::wasm1p1::opcode::op_simd;

    // Push a typed LLVM value onto the transient operand stack.
    auto const push_operand{[&](runtime_operand_stack_value_type type, ::llvm::Value* value) constexpr noexcept
                            { operand_stack.push_back({.type = type, .value = value}); }};

    // Resolve and cache memory 0 on first use.  WebAssembly 1.0/MVP memory instructions target the default memory, so a
    // single cached record is sufficient here.  Multi-memory must replace this with selected-memory resolution, and
    // memory64 must also widen the effective-address/data-path assumptions used by the memory emit helpers.
    auto const ensure_memory0_access_info{[&]() constexpr noexcept
                                          {
                                              if(memory0_access_info_resolved)
                                              {
                                                  return memory0_access_info.memory_p != nullptr || memory0_access_info.local_imported_module_ptr != nullptr;
                                              }

                                              auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                                              if(runtime_module_ptr == nullptr) [[unlikely]]
                                              {
                                                  memory0_access_info_resolved = true;
                                                  return false;
                                              }

                                              memory0_access_info = resolve_runtime_memory_access_info(*runtime_module_ptr, 0u);
                                              memory0_access_info_resolved = true;
                                              return memory0_access_info.memory_p != nullptr || memory0_access_info.local_imported_module_ptr != nullptr;
                                          }};

    // Local bridge-call helper for dispatcher-only fallbacks.  Higher-level helpers above use the same host convention,
    // but the opcode include files need this compact lambda for memory/global cases.
    auto const emit_runtime_bridge_call_with_discriminator{
        [&]<auto bridge_function>(::llvm::FunctionType* bridge_function_type,
                                  ::llvm::ArrayRef<::llvm::Value*> arguments,
                                  ::uwvm2::utils::container::u8string_view discriminator) constexpr noexcept -> ::llvm::CallInst*
        {
            auto bridge_pointer{get_llvm_runtime_bridge_function_symbol_value<bridge_function>(ir_builder, bridge_function_type, discriminator)};
            if(bridge_pointer == nullptr) [[unlikely]] { return nullptr; }
            return apply_llvm_jit_host_calling_conv(ir_builder.CreateCall(bridge_function_type, bridge_pointer, arguments));
        }};

    auto const emit_runtime_bridge_call{[&]<auto bridge_function>(::llvm::FunctionType* bridge_function_type,
                                                                  ::llvm::ArrayRef<::llvm::Value*> arguments,
                                                                  ::uwvm2::utils::container::u8string_view discriminator = {}) constexpr noexcept
                                                                  -> ::llvm::CallInst*
                                        {
                                            return emit_runtime_bridge_call_with_discriminator.template operator()<bridge_function>(bridge_function_type,
                                                                                                                                    arguments,
                                                                                                                                    discriminator);
                                        }};

    // Select one of four scalar bridge functions based on the Wasm value type.  This keeps bridge signatures exact for
    // integer and floating-point globals/memory operations.
    auto const emit_runtime_scalar_bridge_call{
        [&]<auto i32_bridge_function, auto i64_bridge_function, auto f32_bridge_function, auto f64_bridge_function>(
            runtime_operand_stack_value_type value_type,
            ::llvm::FunctionType* bridge_function_type,
            ::llvm::ArrayRef<::llvm::Value*> bridge_arguments) constexpr noexcept -> ::llvm::CallInst*
        {
            switch(value_type)
            {
                case runtime_operand_stack_value_type::i32:
                    return emit_runtime_bridge_call.template operator()<i32_bridge_function>(bridge_function_type, bridge_arguments);
                case runtime_operand_stack_value_type::i64:
                    return emit_runtime_bridge_call.template operator()<i64_bridge_function>(bridge_function_type, bridge_arguments);
                case runtime_operand_stack_value_type::f32:
                    return emit_runtime_bridge_call.template operator()<f32_bridge_function>(bridge_function_type, bridge_arguments);
                case runtime_operand_stack_value_type::f64:
                    return emit_runtime_bridge_call.template operator()<f64_bridge_function>(bridge_function_type, bridge_arguments);
                [[unlikely]] default:
                    return nullptr;
            }
        }};

    auto const emit_local_imported_memory_module_address{
        [&]() constexpr noexcept -> ::llvm::Value*
        {
            if(memory0_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return nullptr; }

            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }

            auto const module_symbol_name{get_llvm_local_imported_memory_module_symbol_name(
                *runtime_module_ptr,
                static_cast<validation_module_traits_t::wasm_u32>(memory0_access_info.local_imported_memory_index))};
            return get_llvm_external_host_object_address(ir_builder,
                                                         reinterpret_cast<::std::uintptr_t>(memory0_access_info.local_imported_module_ptr),
                                                         ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()});
        }};

    auto const emit_native_memory_object_address{[&]() constexpr noexcept -> ::llvm::Value*
                                                 {
                                                     if(memory0_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

                                                     auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                                                     if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }

                                                     auto const memory_symbol_name{get_llvm_native_memory_object_symbol_name(*runtime_module_ptr, 0u)};
                                                     return get_llvm_external_host_object_address(
                                                         ir_builder,
                                                         reinterpret_cast<::std::uintptr_t>(memory0_access_info.memory_p),
                                                         ::uwvm2::utils::container::u8string_view{memory_symbol_name.data(), memory_symbol_name.size()});
                                                 }};

    auto const emit_runtime_module_object_address{[&]() constexpr noexcept -> ::llvm::Value*
                                                  {
                                                      auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                                                      if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }

                                                      auto const module_symbol_name{get_llvm_runtime_module_object_symbol_name(*runtime_module_ptr)};
                                                      return get_llvm_external_host_object_address(
                                                          ir_builder,
                                                          reinterpret_cast<::std::uintptr_t>(runtime_module_ptr),
                                                          ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()});
                                                  }};

    auto const get_runtime_table_reference_value_type{
        [&](validation_module_traits_t::wasm_u32 table_index, runtime_operand_stack_value_type& result_type) constexpr noexcept -> bool
        {
            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }
            auto table{resolve_runtime_table_storage(*runtime_module_ptr, table_index)};
            if(table == nullptr || table->table_type_ptr == nullptr) [[unlikely]] { return false; }

            using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
            switch(table->table_type_ptr->reftype)
            {
                case reference_type::funcref:
                    result_type = runtime_operand_stack_value_type::funcref;
                    return true;
                case reference_type::externref:
                    result_type = runtime_operand_stack_value_type::externref;
                    return true;
                [[unlikely]] default:
                    return false;
            }
        }};

    auto const create_reference_buffer{[&](::llvm::Value* initial_value, ::llvm::StringRef name) constexpr noexcept -> ::llvm::AllocaInst*
                                       {
                                           auto reference_type{::llvm::Type::getIntNTy(
                                               llvm_context,
                                               static_cast<unsigned>(sizeof(runtime_wasm_global_ref) * CHAR_BIT))};
                                           auto buffer{create_llvm_jit_entry_block_alloca(ir_builder, reference_type, nullptr, name)};
                                           if(buffer == nullptr) [[unlikely]] { return nullptr; }
                                           if(initial_value != nullptr)
                                           {
                                               if(initial_value->getType() != reference_type) [[unlikely]] { return nullptr; }
                                               ir_builder.CreateStore(initial_value, buffer);
                                           }
                                           return buffer;
                                       }};

    auto const reference_buffer_address{[&](::llvm::AllocaInst* buffer, ::llvm::StringRef name) constexpr noexcept -> ::llvm::Value*
                                        {
                                            if(buffer == nullptr) [[unlikely]] { return nullptr; }
                                            auto llvm_intptr_type{
                                                ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                            return ir_builder.CreatePtrToInt(buffer, llvm_intptr_type, name);
                                        }};

    auto const emit_ref_null_call{[&](runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                  {
                                      auto zero{get_llvm_zero_constant_from_wasm_value_type(llvm_context, reference_value_type)};
                                      if(zero == nullptr) [[unlikely]] { return false; }
                                      push_operand(reference_value_type, zero);
                                      return true;
                                  }};

    auto const emit_ref_func_call{[&](validation_module_traits_t::wasm_u32 function_index) constexpr noexcept -> bool
                                  {
                                      auto module_address{emit_runtime_module_object_address()};
                                      auto result_buffer{create_reference_buffer(nullptr, get_llvm_string_ref(u8"ref.func.result"))};
                                      auto result_address{reference_buffer_address(result_buffer, get_llvm_string_ref(u8"ref.func.result.addr"))};
                                      if(module_address == nullptr || result_address == nullptr) [[unlikely]] { return false; }

                                      auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                      auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                      auto llvm_intptr_type{
                                          ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                      auto bridge_function_type{
                                          ::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_i32_type, llvm_intptr_type}, false)};
                                      ::llvm::Value* bridge_arguments[]{
                                          module_address,
                                          ::llvm::ConstantInt::get(llvm_i32_type, function_index),
                                          result_address};
                                      if(emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_ref_func_bridge>(
                                             state,
                                             bridge_function_type,
                                             {bridge_arguments}) == nullptr) [[unlikely]]
                                      {
                                          return false;
                                      }

                                      auto reference_type{get_llvm_type_from_wasm_value_type(llvm_context, runtime_operand_stack_value_type::funcref)};
                                      if(reference_type == nullptr) [[unlikely]] { return false; }
                                      auto result{ir_builder.CreateLoad(reference_type, result_buffer, get_llvm_string_ref(u8"ref.func"))};
                                      if(result == nullptr) [[unlikely]] { return false; }
                                      push_operand(runtime_operand_stack_value_type::funcref, result);
                                      return true;
                                  }};

    auto const emit_ref_is_null_call{[&]() constexpr noexcept -> bool
                                     {
                                         if(operand_stack.empty()) [[unlikely]] { return false; }
                                         auto const reference{operand_stack.back()};
                                         operand_stack.pop_back();
                                         if((reference.type != runtime_operand_stack_value_type::funcref &&
                                             reference.type != runtime_operand_stack_value_type::externref) ||
                                            reference.value == nullptr) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         auto value_buffer{create_reference_buffer(reference.value, get_llvm_string_ref(u8"ref.is_null.value"))};
                                         auto value_address{reference_buffer_address(value_buffer, get_llvm_string_ref(u8"ref.is_null.value.addr"))};
                                         if(value_address == nullptr) [[unlikely]] { return false; }
                                         auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                         auto llvm_intptr_type{
                                             ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                         auto bridge_function_type{::llvm::FunctionType::get(llvm_i32_type, {llvm_intptr_type}, false)};
                                         ::llvm::Value* bridge_arguments[]{value_address};
                                         auto result{emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_ref_is_null_bridge>(
                                             state,
                                             bridge_function_type,
                                             {bridge_arguments})};
                                         if(result == nullptr) [[unlikely]] { return false; }
                                         push_operand(runtime_operand_stack_value_type::i32, result);
                                         return true;
                                     }};

    auto const emit_table_get_call{[&](validation_module_traits_t::wasm_u32 table_index,
                                       runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                   {
                                       if(operand_stack.empty()) [[unlikely]] { return false; }
                                       auto const index{operand_stack.back()};
                                       operand_stack.pop_back();
                                       if(index.type != runtime_operand_stack_value_type::i32 || index.value == nullptr) [[unlikely]] { return false; }

                                       auto module_address{emit_runtime_module_object_address()};
                                       auto result_buffer{create_reference_buffer(nullptr, get_llvm_string_ref(u8"table.get.result"))};
                                       auto result_address{reference_buffer_address(result_buffer, get_llvm_string_ref(u8"table.get.result.addr"))};
                                       if(module_address == nullptr || result_address == nullptr) [[unlikely]] { return false; }
                                       auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                       auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                       auto llvm_intptr_type{
                                           ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                       auto bridge_function_type{::llvm::FunctionType::get(
                                           llvm_void_type,
                                           {llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_intptr_type},
                                           false)};
                                       ::llvm::Value* bridge_arguments[]{
                                           module_address,
                                           ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                           index.value,
                                           result_address};
                                       if(emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_table_get_bridge>(
                                              state,
                                              bridge_function_type,
                                              {bridge_arguments}) == nullptr) [[unlikely]]
                                       {
                                           return false;
                                       }

                                       auto reference_type{get_llvm_type_from_wasm_value_type(llvm_context, reference_value_type)};
                                       if(reference_type == nullptr) [[unlikely]] { return false; }
                                       auto result{ir_builder.CreateLoad(reference_type, result_buffer, get_llvm_string_ref(u8"table.get"))};
                                       if(result == nullptr) [[unlikely]] { return false; }
                                       push_operand(reference_value_type, result);
                                       return true;
                                   }};

    auto const emit_table_set_call{[&](validation_module_traits_t::wasm_u32 table_index,
                                       runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                   {
                                       if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
                                       auto const value{operand_stack.back()};
                                       operand_stack.pop_back();
                                       auto const index{operand_stack.back()};
                                       operand_stack.pop_back();
                                       if(value.type != reference_value_type || value.value == nullptr ||
                                          index.type != runtime_operand_stack_value_type::i32 || index.value == nullptr) [[unlikely]]
                                       {
                                           return false;
                                       }

                                       auto module_address{emit_runtime_module_object_address()};
                                       auto value_buffer{create_reference_buffer(value.value, get_llvm_string_ref(u8"table.set.value"))};
                                       auto value_address{reference_buffer_address(value_buffer, get_llvm_string_ref(u8"table.set.value.addr"))};
                                       if(module_address == nullptr || value_address == nullptr) [[unlikely]] { return false; }
                                       auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                       auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                       auto llvm_intptr_type{
                                           ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                       auto bridge_function_type{::llvm::FunctionType::get(
                                           llvm_void_type,
                                           {llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_intptr_type},
                                           false)};
                                       ::llvm::Value* bridge_arguments[]{
                                           module_address,
                                           ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                           index.value,
                                           value_address};
                                       return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_table_set_bridge>(
                                                  state,
                                                  bridge_function_type,
                                                  {bridge_arguments}) != nullptr;
                                   }};

    // Dispatcher-local local-imported global getter.  Some opcode case files use this directly instead of the standalone
    // helper when all required LLVM locals are already in scope.
    [[maybe_unused]] auto const emit_local_imported_global_get_bridge_call{
        [&](runtime_global_access_info_t const& global_access_info, ::llvm::Type* llvm_global_type) constexpr noexcept -> ::llvm::CallInst*
        {
            if(global_access_info.local_imported_module_ptr == nullptr || llvm_global_type == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(llvm_global_type, {llvm_intptr_type, llvm_intptr_type}, false)};
            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }
            auto const module_symbol_name{get_llvm_local_imported_global_module_symbol_name(
                *runtime_module_ptr,
                static_cast<validation_module_traits_t::wasm_u32>(global_access_info.local_imported_global_index))};
            auto module_address{
                get_llvm_external_host_object_address(ir_builder,
                                                      reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr),
                                                      ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()})};
            if(module_address == nullptr) [[unlikely]] { return nullptr; }
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{module_address, ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index)}
            };

            return emit_runtime_scalar_bridge_call.template operator()<llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_f32>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_f64>>(
                global_access_info.value_type,
                bridge_function_type,
                bridge_arguments);
        }};

    // Dispatcher-local local-imported global setter matching the getter above.
    [[maybe_unused]] auto const emit_local_imported_global_set_bridge_call{
        [&](runtime_global_access_info_t const& global_access_info, ::llvm::Type* llvm_value_type, ::llvm::Value* value) constexpr noexcept -> ::llvm::CallInst*
        {
            if(global_access_info.local_imported_module_ptr == nullptr || llvm_value_type == nullptr || value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_value_type}, false)};
            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }
            auto const module_symbol_name{get_llvm_local_imported_global_module_symbol_name(
                *runtime_module_ptr,
                static_cast<validation_module_traits_t::wasm_u32>(global_access_info.local_imported_global_index))};
            auto module_address{
                get_llvm_external_host_object_address(ir_builder,
                                                      reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr),
                                                      ::uwvm2::utils::container::u8string_view{module_symbol_name.data(), module_symbol_name.size()})};
            if(module_address == nullptr) [[unlikely]] { return nullptr; }
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{module_address,
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index),
                                                 value}
            };

            return emit_runtime_scalar_bridge_call.template operator()<llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_f32>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_f64>>(
                global_access_info.value_type,
                bridge_function_type,
                bridge_arguments);
        }};

    // Emit a load of the current direct-memory byte length.  mmap-backed memory uses an acquire atomic length load because
    // growth may publish a new length concurrently with JIT code execution.
    auto const emit_direct_memory_byte_length_value{
        [&]() constexpr noexcept -> ::llvm::Value*
        {
            if(!ensure_memory0_access_info()) [[unlikely]] { return nullptr; }
            if(memory0_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

            if constexpr(runtime_native_memory_t::can_mmap)
            {
                auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }
                auto const memory_length_symbol_name{
                    ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*runtime_module_ptr), u8"_memory0_length")};
                auto memory_length_ptr{get_llvm_external_host_object_pointer(
                    ir_builder,
                    reinterpret_cast<::std::uintptr_t>(memory0_access_info.stable_memory_length_p),
                    llvm_intptr_type,
                    ::uwvm2::utils::container::u8string_view{memory_length_symbol_name.data(), memory_length_symbol_name.size()})};
                if(memory_length_ptr == nullptr) [[unlikely]] { return nullptr; }

                auto load_inst{ir_builder.CreateLoad(llvm_intptr_type, memory_length_ptr, get_llvm_string_ref(u8"memory.length"))};
                load_inst->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
                load_inst->setAtomic(::llvm::AtomicOrdering::Acquire);
                return load_inst;
            }
            else
            {
                if(memory0_access_info.stable_memory_length_value_p == nullptr) [[unlikely]] { return nullptr; }
                auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }
                auto const memory_length_symbol_name{
                    ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*runtime_module_ptr), u8"_memory0_length_value")};
                auto memory_length_ptr{get_llvm_external_host_object_pointer(
                    ir_builder,
                    reinterpret_cast<::std::uintptr_t>(memory0_access_info.stable_memory_length_value_p),
                    llvm_intptr_type,
                    ::uwvm2::utils::container::u8string_view{memory_length_symbol_name.data(), memory_length_symbol_name.size()})};
                if(memory_length_ptr == nullptr) [[unlikely]] { return nullptr; }
                auto load_inst{ir_builder.CreateLoad(llvm_intptr_type, memory_length_ptr, get_llvm_string_ref(u8"memory.length"))};
                load_inst->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
                return load_inst;
            }
        }};

    // Convert a byte length to a Wasm i32 page count using either a shift for power-of-two page sizes or division for
    // custom page sizes.
    auto const emit_direct_memory_page_count_from_byte_length{
        [&](::llvm::Value* memory_length_load, ::std::size_t page_size_bytes) constexpr noexcept -> ::llvm::Value*
        {
            if(memory_length_load == nullptr || page_size_bytes == 0uz) [[unlikely]] { return nullptr; }

            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            ::llvm::Value* page_count{};
            if(::std::has_single_bit(page_size_bytes))
            {
                page_count = ir_builder.CreateLShr(memory_length_load, ::llvm::ConstantInt::get(llvm_intptr_type, ::std::countr_zero(page_size_bytes)));
            }
            else
            {
                page_count = ir_builder.CreateUDiv(memory_length_load,
                                                   ::llvm::ConstantInt::get(llvm_intptr_type, page_size_bytes),
                                                   get_llvm_string_ref(u8"memory.page_count"));
            }
            return ir_builder.CreateTrunc(page_count, llvm_i32_type);
        }};

    // Emit memory.size for a directly addressable memory.
    auto const emit_direct_memory_page_count_value{[&]() constexpr noexcept -> ::llvm::Value*
                                                   {
                                                       auto memory_length_load{emit_direct_memory_byte_length_value()};
                                                       if(memory_length_load == nullptr) [[unlikely]] { return nullptr; }
                                                       return emit_direct_memory_page_count_from_byte_length(memory_length_load,
                                                                                                             static_cast<::std::size_t>(1uz)
                                                                                                                 << memory0_access_info.custom_page_size_log2);
                                                   }};

    // Try to compute a direct byte pointer for a memory access.  This path is available only for little-endian mmap-backed
    // memories, where the generated LLVM load/store can use the host representation directly.
    auto const emit_direct_mmap_memory_byte_pointer{
        [&](validation_module_traits_t::wasm_u32 static_offset, ::std::size_t access_size, ::llvm::Value* address_value) constexpr noexcept -> ::llvm::Value*
        {
            if constexpr(!(runtime_native_memory_t::can_mmap && ::std::endian::native == ::std::endian::little))
            {
                static_cast<void>(static_offset);
                static_cast<void>(access_size);
                static_cast<void>(address_value);
                return nullptr;
            }
            else
            {
                if(!ensure_memory0_access_info() || address_value == nullptr) [[unlikely]] { return nullptr; }

                auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
                auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};

                // Wasm32 addresses are unsigned i32 bit patterns.  Widen before adding the static memarg offset so a
                // high address cannot alias low memory (for example, 0xffffffff + 1 must overflow instead of becoming 0).
                auto effective_offset{emit_llvm_wasm32_effective_offset(ir_builder, address_value, static_offset)};
                if(effective_offset == nullptr) [[unlikely]] { return nullptr; }
                auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                if(runtime_module_ptr == nullptr) [[unlikely]] { return nullptr; }
                auto const memory_begin_symbol_name{
                    ::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*runtime_module_ptr), u8"_memory0_begin")};
                auto stable_memory_begin{get_llvm_external_host_byte_span_pointer(
                    ir_builder,
                    reinterpret_cast<::std::uintptr_t>(memory0_access_info.stable_memory_begin),
                    memory0_access_info.stable_memory_reserved_span_bytes,
                    ::uwvm2::utils::container::u8string_view{memory_begin_symbol_name.data(), memory_begin_symbol_name.size()})};
                if(stable_memory_begin == nullptr) [[unlikely]] { return nullptr; }

                if(memory0_access_info.mmap_requires_dynamic_bounds
#if defined(UWVM_SUPPORT_MMAP)
                   || access_size > ::uwvm2::object::memory::linear::mmap_guard_max_access_size
#endif
                )
                {
                    // Full dynamic-bounds mode checks every access before forming the final address.  The diagnostic trap
                    // receives both the effective offset and the current memory length.
                    auto memory_length_load{emit_direct_memory_byte_length_value()};
                    if(memory_length_load == nullptr) [[unlikely]] { return nullptr; }

                    auto effective_too_large{emit_llvm_wasm32_effective_offset_out_of_range(ir_builder, effective_offset)};
                    auto memory_length_i64{ir_builder.CreateZExt(memory_length_load, llvm_i64_type)};
                    auto access_size_i64{::llvm::ConstantInt::get(llvm_i64_type, access_size)};
                    auto memory_too_small{ir_builder.CreateICmpULT(memory_length_i64, access_size_i64)};
                    // Compute max offset in IR even when memory is smaller than the access width; the separate
                    // `memory_too_small` predicate keeps the underflowed value from making the access look valid.
                    auto max_access_offset{ir_builder.CreateSub(memory_length_i64, access_size_i64)};
                    auto access_oob{ir_builder.CreateICmpUGT(effective_offset, max_access_offset)};

                    emit_llvm_conditional_memory_out_of_bounds_trap(*llvm_module,
                                                                    ir_builder,
                                                                    ir_builder.CreateOr(effective_too_large, ir_builder.CreateOr(memory_too_small, access_oob)),
                                                                    0uz,
                                                                    static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(static_offset)),
                                                                    effective_offset,
                                                                    effective_too_large,
                                                                    memory_length_load,
                                                                    access_size);
                }
                else if(memory0_access_info.mmap_uses_partial_protection)
                {
                    // Partial mmap protection lets low in-range accesses fault naturally, but high offsets must still be
                    // checked against the current length in generated IR.
                    ::llvm::Value* partial_limit_escape{};
                    partial_limit_escape =
                        ir_builder.CreateICmpUGE(effective_offset,
                                                 ::llvm::ConstantInt::get(llvm_i64_type, get_runtime_partial_protection_limit_escape_offset()));

                    auto needs_dynamic_bounds_check{partial_limit_escape};
                    auto current_block{ir_builder.GetInsertBlock()};
                    auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
                    if(current_function == nullptr) [[unlikely]] { return nullptr; }

                    // Low offsets stay on the hardware-protected fast path.  Only offsets outside that protected prefix
                    // split to a software bounds check before rejoining at the same pointer-formation logic.
                    auto partial_check_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"memory.partial.check"), current_function)};
                    auto partial_continue_block{::llvm::BasicBlock::Create(llvm_context, get_llvm_string_ref(u8"memory.partial.cont"), current_function)};
                    ir_builder.CreateCondBr(needs_dynamic_bounds_check, partial_check_block, partial_continue_block);

                    ir_builder.SetInsertPoint(partial_check_block);
                    auto memory_length_load{emit_direct_memory_byte_length_value()};
                    if(memory_length_load == nullptr) [[unlikely]] { return nullptr; }

                    auto effective_too_large{emit_llvm_wasm32_effective_offset_out_of_range(ir_builder, effective_offset)};
                    auto memory_length_i64{ir_builder.CreateZExt(memory_length_load, llvm_i64_type)};
                    auto access_size_i64{::llvm::ConstantInt::get(llvm_i64_type, access_size)};
                    auto memory_too_small{ir_builder.CreateICmpULT(memory_length_i64, access_size_i64)};
                    // Same underflow-safe max-offset pattern as the full dynamic-bounds path.
                    auto max_access_offset{ir_builder.CreateSub(memory_length_i64, access_size_i64)};
                    auto access_oob{ir_builder.CreateICmpUGT(effective_offset, max_access_offset)};

                    emit_llvm_conditional_memory_out_of_bounds_trap(*llvm_module,
                                                                    ir_builder,
                                                                    ir_builder.CreateOr(effective_too_large, ir_builder.CreateOr(memory_too_small, access_oob)),
                                                                    0uz,
                                                                    static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(static_offset)),
                                                                    effective_offset,
                                                                    effective_too_large,
                                                                    memory_length_load,
                                                                    access_size);
                    ir_builder.CreateBr(partial_continue_block);
                    ir_builder.SetInsertPoint(partial_continue_block);
                }
                else if(!memory0_access_info.mmap_covers_wasm32_effective_domain)
                {
                    // A mapping without the complete unsigned-domain reservation proof still needs the conservative
                    // overflow gate. Never infer this proof merely from an absent dynamic-length check.
                    emit_llvm_conditional_trap(*llvm_module,
                                               ir_builder,
                                               emit_llvm_wasm32_effective_offset_out_of_range(ir_builder, effective_offset),
                                               ::uwvm2::runtime::lib::llvm_jit_trap_kind::memory_out_of_bounds);
                }

                // Form the final byte pointer only after any required software checks have dominated this point.  For
                // proven full wasm32 mappings, every u32+u32 offset and supported access width fits the reservation;
                // logical OOB and u32 overflow are both intentionally left to the permanently inaccessible guard pages.
                // Keep this GEP non-inbounds: logical OOB values that deliberately land in a guard page must remain
                // ordinary IR values rather than becoming LLVM poison before the hardware fault is observed.
                return ir_builder.CreateGEP(llvm_i8_type, stable_memory_begin, effective_offset, get_llvm_string_ref(u8"memory.addr"));
            }
        }};

    // Ask a local-imported memory provider for a snapshot and return it as LLVM values.  This is safe for size queries but
    // not used for actual loads/stores because the provider's access lock/snapshot lifetime is not represented in LLVM IR.
    auto const emit_local_imported_memory_snapshot{
        [&]() constexpr noexcept -> llvm_jit_memory_snapshot_values_t
        {
            llvm_jit_memory_snapshot_values_t result{};
            if(!ensure_memory0_access_info() || memory0_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return result; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            // The provider writes snapshot outputs through host bridge pointer arguments.  Entry-block allocas give LLVM
            // stable addresses for those out-parameters and make the following loads explicit in IR.
            auto memory_begin_slot{
                create_llvm_jit_entry_block_alloca(ir_builder, llvm_intptr_type, nullptr, get_llvm_string_ref(u8"local_imported.memory.begin.addr.slot"))};
            auto byte_length_slot{
                create_llvm_jit_entry_block_alloca(ir_builder, llvm_intptr_type, nullptr, get_llvm_string_ref(u8"local_imported.memory.byte_length.slot"))};
            if(memory_begin_slot == nullptr || byte_length_slot == nullptr) [[unlikely]] { return result; }
            auto bridge_function_type{::llvm::FunctionType::get(
                ::llvm::Type::getInt1Ty(llvm_context),
                {llvm_intptr_type, llvm_intptr_type, get_llvm_pointer_type(llvm_intptr_type), get_llvm_pointer_type(llvm_intptr_type)},
                false)};
            auto module_address{emit_local_imported_memory_module_address()};
            if(module_address == nullptr) [[unlikely]] { return result; }
            auto snapshot_ok{emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_snapshot_bridge>(
                bridge_function_type,
                {module_address,
                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                 memory_begin_slot,
                 byte_length_slot})};
            if(snapshot_ok == nullptr) [[unlikely]] { return result; }

            emit_llvm_conditional_trap(*llvm_module,
                                       ir_builder,
                                       ir_builder.CreateNot(snapshot_ok),
                                       ::uwvm2::runtime::lib::llvm_jit_trap_kind::memory_out_of_bounds);
            result.memory_begin_address = ir_builder.CreateLoad(llvm_intptr_type, memory_begin_slot, get_llvm_string_ref(u8"local_imported.memory.begin.addr"));
            result.byte_length = ir_builder.CreateLoad(llvm_intptr_type, byte_length_slot, get_llvm_string_ref(u8"local_imported.memory.byte_length"));
            return result;
        }};

    // Emit memory.size for local-imported memories by snapshotting byte length and converting it to pages.
    auto const emit_local_imported_memory_page_count_value{
        [&]() constexpr noexcept -> ::llvm::Value*
        {
            if(!ensure_memory0_access_info() || memory0_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return nullptr; }
            if(memory0_access_info.local_imported_page_size_bytes == 0u) [[unlikely]] { return nullptr; }

            auto const snapshot{emit_local_imported_memory_snapshot()};
            if(snapshot.byte_length == nullptr) [[unlikely]] { return nullptr; }

            return emit_direct_memory_page_count_from_byte_length(snapshot.byte_length,
                                                                  static_cast<::std::size_t>(memory0_access_info.local_imported_page_size_bytes));
        }};

    // Select the correct templated local-imported memory load bridge for a scalar type, access width, and signedness.
    auto const emit_local_imported_memory_load_bridge_call_for_scalar{
        [&]<typename ScalarType>(::llvm::FunctionType* bridge_function_type,
                                 ::llvm::ArrayRef<::llvm::Value*> bridge_arguments,
                                 ::std::size_t load_bytes,
                                 bool signed_load,
                                 runtime_operand_stack_value_type scalar_value_type) constexpr noexcept -> ::llvm::CallInst*
        {
            auto discriminator_storage{
                make_llvm_jit_memory_bridge_symbol_discriminator(u8"local-imported-memory-load", scalar_value_type, load_bytes, signed_load)};
            auto discriminator{
                ::uwvm2::utils::container::u8string_view{discriminator_storage.data(), discriminator_storage.size()}};
            if constexpr(::std::same_as<ScalarType, runtime_wasm_i32>)
            {
                switch(load_bytes)
                {
                    case 1uz:
                        if(signed_load)
                        {
                            return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 1uz, true>>(
                                bridge_function_type,
                                bridge_arguments,
                                discriminator);
                        }
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 1uz, false>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 2uz:
                        if(signed_load)
                        {
                            return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 2uz, true>>(
                                bridge_function_type,
                                bridge_arguments,
                                discriminator);
                        }
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 2uz, false>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 4uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 4uz, false>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_i64>)
            {
                switch(load_bytes)
                {
                    case 1uz:
                        if(signed_load)
                        {
                            return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 1uz, true>>(
                                bridge_function_type,
                                bridge_arguments,
                                discriminator);
                        }
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 1uz, false>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 2uz:
                        if(signed_load)
                        {
                            return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 2uz, true>>(
                                bridge_function_type,
                                bridge_arguments,
                                discriminator);
                        }
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 2uz, false>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 4uz:
                        if(signed_load)
                        {
                            return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 4uz, true>>(
                                bridge_function_type,
                                bridge_arguments,
                                discriminator);
                        }
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 4uz, false>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 8uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 8uz, false>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f32>)
            {
                if(load_bytes != 4uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_f32, 4uz, false>>(
                    bridge_function_type,
                    bridge_arguments,
                    discriminator);
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f64>)
            {
                if(load_bytes != 8uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_load_bridge<runtime_wasm_f64, 8uz, false>>(
                    bridge_function_type,
                    bridge_arguments,
                    discriminator);
            }
            else
            {
                return nullptr;
            }
        }};

    // Select the correct templated local-imported memory store bridge for a scalar type and access width.
    auto const emit_local_imported_memory_store_bridge_call_for_scalar{
        [&]<typename ScalarType>(::llvm::FunctionType* bridge_function_type,
                                 ::llvm::ArrayRef<::llvm::Value*> bridge_arguments,
                                 ::std::size_t store_bytes,
                                 runtime_operand_stack_value_type scalar_value_type) constexpr noexcept -> ::llvm::CallInst*
        {
            auto discriminator_storage{make_llvm_jit_memory_bridge_symbol_discriminator(u8"local-imported-memory-store", scalar_value_type, store_bytes)};
            auto discriminator{
                ::uwvm2::utils::container::u8string_view{discriminator_storage.data(), discriminator_storage.size()}};
            if constexpr(::std::same_as<ScalarType, runtime_wasm_i32>)
            {
                switch(store_bytes)
                {
                    case 1uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i32, 1uz>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 2uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i32, 2uz>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 4uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i32, 4uz>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_i64>)
            {
                switch(store_bytes)
                {
                    case 1uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 1uz>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 2uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 2uz>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 4uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 4uz>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    case 8uz:
                        return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 8uz>>(
                            bridge_function_type,
                            bridge_arguments,
                            discriminator);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f32>)
            {
                if(store_bytes != 4uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_f32, 4uz>>(bridge_function_type,
                                                                                                                                        bridge_arguments,
                                                                                                                                        discriminator);
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f64>)
            {
                if(store_bytes != 8uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_store_bridge<runtime_wasm_f64, 8uz>>(bridge_function_type,
                                                                                                                                        bridge_arguments,
                                                                                                                                        discriminator);
            }
            else
            {
                return nullptr;
            }
        }};

    // Emit the bridge call for one local-imported memory load after the opcode case has decoded memarg and result type.
    auto const emit_local_imported_memory_load_bridge_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            runtime_operand_stack_value_type result_type,
            ::llvm::Type* llvm_result_type,
            ::std::size_t load_bytes,
            bool signed_load,
            ::llvm::Value* address_value) constexpr noexcept -> ::llvm::CallInst*
        {
            if(memory0_access_info.local_imported_module_ptr == nullptr || llvm_result_type == nullptr || address_value == nullptr) [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(llvm_result_type,
                                          {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context)},
                                          false)};
            auto module_address{emit_local_imported_memory_module_address()};
            if(module_address == nullptr) [[unlikely]] { return nullptr; }
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{module_address,
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                                                 ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                                                 address_value}
            };

            switch(result_type)
            {
                case runtime_operand_stack_value_type::i32:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_i32>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load,
                                                                                                                        result_type);
                case runtime_operand_stack_value_type::i64:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_i64>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load,
                                                                                                                        result_type);
                case runtime_operand_stack_value_type::f32:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_f32>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load,
                                                                                                                        result_type);
                case runtime_operand_stack_value_type::f64:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_f64>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load,
                                                                                                                        result_type);
                [[unlikely]] default:
                {
                    return nullptr;
                }
            }
        }};

    // Emit the bridge call for one local-imported memory store after the opcode case has decoded memarg and value type.
    auto const emit_local_imported_memory_store_bridge_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            runtime_operand_stack_value_type value_type,
            ::llvm::Type* llvm_value_type,
            ::std::size_t store_bytes,
            ::llvm::Value* address_value,
            ::llvm::Value* value) constexpr noexcept -> ::llvm::CallInst*
        {
            if(memory0_access_info.local_imported_module_ptr == nullptr || llvm_value_type == nullptr || address_value == nullptr || value == nullptr)
                [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(
                ::llvm::Type::getVoidTy(llvm_context),
                {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context), llvm_value_type},
                false)};
            auto module_address{emit_local_imported_memory_module_address()};
            if(module_address == nullptr) [[unlikely]] { return nullptr; }
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{module_address,
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                                                 ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                                                 address_value, value}
            };

            switch(value_type)
            {
                case runtime_operand_stack_value_type::i32:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_i32>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes,
                                                                                                                         value_type);
                case runtime_operand_stack_value_type::i64:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_i64>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes,
                                                                                                                         value_type);
                case runtime_operand_stack_value_type::f32:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_f32>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes,
                                                                                                                         value_type);
                case runtime_operand_stack_value_type::f64:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_f64>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes,
                                                                                                                         value_type);
                [[unlikely]] default:
                {
                    return nullptr;
                }
            }
        }};

    // Choose the direct-memory address path when possible.  Local-imported memories deliberately return null here so the
    // caller falls back to provider-owned bridge calls.
    auto const emit_direct_memory_byte_pointer{
        [&](validation_module_traits_t::wasm_u32 static_offset, ::std::size_t access_size, ::llvm::Value* address_value) constexpr noexcept -> ::llvm::Value*
        {
            if(memory0_access_info.memory_p != nullptr)
            {
                if constexpr(runtime_native_memory_t::can_mmap) { return emit_direct_mmap_memory_byte_pointer(static_offset, access_size, address_value); }
            }
            else if(memory0_access_info.local_imported_module_ptr != nullptr)
            {
                // A local-imported memory snapshot does not keep allocator-backed grow locks alive across the actual load/store.
                // Keep imported memories on the bridge path; the bridge performs the access while the provider's snapshot/lock is active.
                static_cast<void>(static_offset);
                static_cast<void>(access_size);
                static_cast<void>(address_value);
                return nullptr;
            }
            return nullptr;
        }};

    // Emit a native-memory load bridge call for fallback paths.
    auto const emit_native_memory_load_bridge_call{
        [&]<auto bridge_function>(validation_module_traits_t::wasm_u32 static_offset,
                                  ::llvm::Type* llvm_result_type,
                                  ::llvm::Value* address_value,
                                  ::uwvm2::utils::container::u8string_view discriminator) constexpr noexcept -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p == nullptr || llvm_result_type == nullptr || address_value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(llvm_result_type,
                                          {llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context)},
                                          false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<bridge_function>(
                bridge_function_type,
                {memory_address, ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset), address_value},
                discriminator);
        }};

    // Emit a native-memory store bridge call for fallback paths.
    auto const emit_native_memory_store_bridge_call{
        [&]<auto bridge_function>(validation_module_traits_t::wasm_u32 static_offset,
                                  ::llvm::Type* llvm_value_type,
                                  ::llvm::Value* address_value,
                                  ::llvm::Value* value,
                                  ::uwvm2::utils::container::u8string_view discriminator) constexpr noexcept -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p == nullptr || llvm_value_type == nullptr || address_value == nullptr || value == nullptr) [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context),
                                          {llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context), llvm_value_type},
                                          false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<bridge_function>(
                bridge_function_type,
                {memory_address, ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset), address_value, value},
                discriminator);
        }};

    // Emit a native-memory memory.size bridge call when direct length loads are unavailable.
    auto const emit_native_memory_page_count_bridge_call{
        [&]() constexpr noexcept -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(llvm_context), {llvm_intptr_type}, false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<llvm_jit_memory_size_bridge>(bridge_function_type, {memory_address});
        }};

    // Pick the correct load fallback path: native bridge for directly owned memory, provider bridge for local-imported
    // memory.
    auto const emit_memory_load_bridge_fallback_call{
        [&]<auto native_bridge_function>(validation_module_traits_t::wasm_u32 static_offset,
                                         runtime_operand_stack_value_type result_type,
                                         ::llvm::Type* llvm_result_type,
                                         ::std::size_t load_bytes,
                                         bool signed_load,
                                         ::llvm::Value* address_value) constexpr noexcept -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p != nullptr)
            {
                auto discriminator_storage{
                    make_llvm_jit_memory_bridge_symbol_discriminator(u8"native-memory-load", result_type, load_bytes, signed_load)};
                auto discriminator{
                    ::uwvm2::utils::container::u8string_view{discriminator_storage.data(), discriminator_storage.size()}};
                return emit_native_memory_load_bridge_call.template operator()<native_bridge_function>(static_offset,
                                                                                                      llvm_result_type,
                                                                                                      address_value,
                                                                                                      discriminator);
            }
            return emit_local_imported_memory_load_bridge_call(static_offset, result_type, llvm_result_type, load_bytes, signed_load, address_value);
        }};

    // Pick the correct store fallback path: native bridge for directly owned memory, provider bridge for local-imported
    // memory.
    auto const emit_memory_store_bridge_fallback_call{
        [&]<auto native_bridge_function>(validation_module_traits_t::wasm_u32 static_offset,
                                         runtime_operand_stack_value_type value_type,
                                         ::llvm::Type* llvm_value_type,
                                         ::std::size_t store_bytes,
                                         ::llvm::Value* address_value,
                                         ::llvm::Value* value) constexpr noexcept -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p != nullptr)
            {
                auto discriminator_storage{make_llvm_jit_memory_bridge_symbol_discriminator(u8"native-memory-store", value_type, store_bytes)};
                auto discriminator{
                    ::uwvm2::utils::container::u8string_view{discriminator_storage.data(), discriminator_storage.size()}};
                return emit_native_memory_store_bridge_call.template operator()<native_bridge_function>(static_offset,
                                                                                                       llvm_value_type,
                                                                                                       address_value,
                                                                                                       value,
                                                                                                       discriminator);
            }
            return emit_local_imported_memory_store_bridge_call(static_offset, value_type, llvm_value_type, store_bytes, address_value, value);
        }};

    // Emit a direct LLVM memory load once a checked byte pointer has been produced.  Integer extension/truncation follows
    // the exact Wasm load opcode width and signedness.
    auto const emit_direct_memory_load_value{
        [&](::llvm::Value* direct_memory_pointer,
            runtime_operand_stack_value_type result_type,
            ::std::size_t load_bytes,
            bool signed_load,
            ::llvm::Align memory_alignment) constexpr noexcept -> ::llvm::Value*
        {
            if(direct_memory_pointer == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
            auto llvm_i16_type{::llvm::Type::getInt16Ty(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};
            if(result_type == runtime_operand_stack_value_type::i32)
            {
                ::llvm::Type* llvm_load_type{};
                switch(load_bytes)
                {
                    case 1uz: llvm_load_type = llvm_i8_type; break;
                    case 2uz: llvm_load_type = llvm_i16_type; break;
                    case 4uz:
                        llvm_load_type = llvm_i32_type;
                        break;
                    [[unlikely]] default:
                        return nullptr;
                }

                auto load_inst{ir_builder.CreateLoad(llvm_load_type,
                                                     ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_load_type)),
                                                     get_llvm_string_ref(u8"memory.load"))};
                load_inst->setAlignment(memory_alignment);
                // Guest memory can be externally observed or invalidated through runtime growth/trap mechanics.  Mark the
                // direct load volatile so LLVM does not invent or eliminate memory traffic around those checks.
                load_inst->setVolatile(true);

                if(load_bytes == 4uz) { return load_inst; }
                return signed_load ? ir_builder.CreateSExt(load_inst, llvm_i32_type) : ir_builder.CreateZExt(load_inst, llvm_i32_type);
            }

            if(result_type == runtime_operand_stack_value_type::i64)
            {
                ::llvm::Type* llvm_load_type{};
                switch(load_bytes)
                {
                    case 1uz: llvm_load_type = llvm_i8_type; break;
                    case 2uz: llvm_load_type = llvm_i16_type; break;
                    case 4uz: llvm_load_type = llvm_i32_type; break;
                    case 8uz:
                        llvm_load_type = llvm_i64_type;
                        break;
                    [[unlikely]] default:
                        return nullptr;
                }

                auto load_inst{ir_builder.CreateLoad(llvm_load_type,
                                                     ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_load_type)),
                                                     get_llvm_string_ref(u8"memory.load"))};
                load_inst->setAlignment(memory_alignment);
                load_inst->setVolatile(true);

                if(load_bytes == 8uz) { return load_inst; }
                return signed_load ? ir_builder.CreateSExt(load_inst, llvm_i64_type) : ir_builder.CreateZExt(load_inst, llvm_i64_type);
            }

            if(result_type == runtime_operand_stack_value_type::f32)
            {
                if(load_bytes != 4uz) [[unlikely]] { return nullptr; }

                auto load_inst{ir_builder.CreateLoad(llvm_i32_type,
                                                     ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i32_type)),
                                                     get_llvm_string_ref(u8"memory.load"))};
                load_inst->setAlignment(memory_alignment);
                load_inst->setVolatile(true);
                return ir_builder.CreateBitCast(load_inst, ::llvm::Type::getFloatTy(llvm_context));
            }

            if(result_type == runtime_operand_stack_value_type::f64)
            {
                if(load_bytes != 8uz) [[unlikely]] { return nullptr; }

                auto load_inst{ir_builder.CreateLoad(llvm_i64_type,
                                                     ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i64_type)),
                                                     get_llvm_string_ref(u8"memory.load"))};
                load_inst->setAlignment(memory_alignment);
                load_inst->setVolatile(true);
                return ir_builder.CreateBitCast(load_inst, ::llvm::Type::getDoubleTy(llvm_context));
            }

            return nullptr;
        }};

    // Emit a direct LLVM memory store once a checked byte pointer has been produced.  Narrow integer stores explicitly
    // truncate before writing.
    auto const emit_direct_memory_store_value{
        [&](::llvm::Value* direct_memory_pointer,
            runtime_operand_stack_value_type value_type,
            ::llvm::Value* value,
            ::std::size_t store_bytes,
            ::llvm::Align memory_alignment) constexpr noexcept -> ::llvm::StoreInst*
        {
            if(direct_memory_pointer == nullptr || value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
            auto llvm_i16_type{::llvm::Type::getInt16Ty(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};
            if(value_type == runtime_operand_stack_value_type::i32)
            {
                switch(store_bytes)
                {
                    case 1uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i8_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i8_type)))};
                        return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
                    }
                    case 2uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i16_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i16_type)))};
                        return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
                    }
                    case 4uz:
                    {
                        auto store_inst{
                            ir_builder.CreateStore(value, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i32_type)))};
                        return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
                    }
                    [[unlikely]] default:
                        return nullptr;
                }
            }

            if(value_type == runtime_operand_stack_value_type::i64)
            {
                switch(store_bytes)
                {
                    case 1uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i8_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i8_type)))};
                        return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
                    }
                    case 2uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i16_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i16_type)))};
                        return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
                    }
                    case 4uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i32_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i32_type)))};
                        return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
                    }
                    case 8uz:
                    {
                        auto store_inst{
                            ir_builder.CreateStore(value, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i64_type)))};
                        return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
                    }
                    [[unlikely]] default:
                        return nullptr;
                }
            }

            if(value_type == runtime_operand_stack_value_type::f32)
            {
                if(store_bytes != 4uz) [[unlikely]] { return nullptr; }

                auto store_inst{
                    ir_builder.CreateStore(ir_builder.CreateBitCast(value, llvm_i32_type),
                                           ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i32_type)))};
                return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
            }

            if(value_type == runtime_operand_stack_value_type::f64)
            {
                if(store_bytes != 8uz) [[unlikely]] { return nullptr; }

                auto store_inst{ir_builder.CreateStore(ir_builder.CreateBitCast(value, llvm_i64_type),
                                                       ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i64_type)))};
                return finalize_llvm_jit_direct_memory_store(store_inst, memory_alignment);
            }

            return nullptr;
        }};

    // Emit memory.size for whichever default-memory representation was resolved.
    auto const emit_memory_page_count_value{[&]() constexpr noexcept -> ::llvm::Value*
                                            {
                                                if(!ensure_memory0_access_info()) [[unlikely]] { return nullptr; }
                                                if(memory0_access_info.local_imported_module_ptr != nullptr)
                                                {
                                                    return emit_local_imported_memory_page_count_value();
                                                }
                                                if constexpr(!runtime_native_memory_t::can_mmap && runtime_native_memory_t::support_multi_thread)
                                                {
                                                    return emit_native_memory_page_count_bridge_call();
                                                }
                                                return emit_direct_memory_page_count_value();
                                            }};

    // Build the control flow for memory.grow.  A zero delta returns the current size without a bridge call; requests that
    // statically exceed the limit return -1; all other requests call the runtime/provider grow bridge.
    auto const emit_memory_grow_result_value{
        [&](::llvm::Value* delta_value,
            ::llvm::Value* current_page_count,
            ::llvm::Value* definitely_fail,
            bool local_imported_path,
            auto&& emit_bridge_call) constexpr noexcept -> ::llvm::Value*
        {
            if(delta_value == nullptr || current_page_count == nullptr) [[unlikely]] { return nullptr; }

            auto current_block{ir_builder.GetInsertBlock()};
            auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
            if(current_function == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto delta_is_zero{ir_builder.CreateICmpEQ(delta_value, ::llvm::ConstantInt::get(llvm_i32_type, 0u))};
            auto grow_zero_block{::llvm::BasicBlock::Create(llvm_context,
                                                            local_imported_path ? get_llvm_string_ref(u8"memory.grow.local_imported.zero")
                                                                                : get_llvm_string_ref(u8"memory.grow.zero"),
                                                            current_function)};
            auto grow_fail_block{::llvm::BasicBlock::Create(llvm_context,
                                                            local_imported_path ? get_llvm_string_ref(u8"memory.grow.local_imported.fail")
                                                                                : get_llvm_string_ref(u8"memory.grow.fail"),
                                                            current_function)};
            auto grow_runtime_block{::llvm::BasicBlock::Create(llvm_context,
                                                               local_imported_path ? get_llvm_string_ref(u8"memory.grow.local_imported.runtime")
                                                                                   : get_llvm_string_ref(u8"memory.grow.runtime"),
                                                               current_function)};
            auto grow_merge_block{::llvm::BasicBlock::Create(llvm_context,
                                                             local_imported_path ? get_llvm_string_ref(u8"memory.grow.local_imported.merge")
                                                                                 : get_llvm_string_ref(u8"memory.grow.merge"),
                                                             current_function)};
            // Some callers may choose to skip a static fail pre-check.  In that case the non-zero edge jumps directly to
            // the runtime bridge, while the block structure stays uniform for the merge logic below.
            auto non_zero_target{definitely_fail == nullptr
                                     ? grow_runtime_block
                                     : ::llvm::BasicBlock::Create(llvm_context,
                                                                  local_imported_path ? get_llvm_string_ref(u8"memory.grow.local_imported.nonzero")
                                                                                      : get_llvm_string_ref(u8"memory.grow.nonzero"),
                                                                  current_function)};

            ir_builder.CreateCondBr(delta_is_zero, grow_zero_block, non_zero_target);

            ir_builder.SetInsertPoint(grow_zero_block);
            ir_builder.CreateBr(grow_merge_block);

            if(non_zero_target != grow_runtime_block)
            {
                ir_builder.SetInsertPoint(non_zero_target);
                ir_builder.CreateCondBr(definitely_fail, grow_fail_block, grow_runtime_block);
            }

            ir_builder.SetInsertPoint(grow_fail_block);
            ir_builder.CreateBr(grow_merge_block);

            ir_builder.SetInsertPoint(grow_runtime_block);
            auto bridge_call{emit_bridge_call()};
            if(bridge_call == nullptr) [[unlikely]] { return nullptr; }
            ir_builder.CreateBr(grow_merge_block);

            ir_builder.SetInsertPoint(grow_merge_block);
            // The PHI exactly mirrors Wasm's three observable outcomes: zero-delta returns the old/current page count,
            // pre-known limit failure returns -1, and runtime/provider growth returns its bridge result.
            auto grow_result_phi{ir_builder.CreatePHI(llvm_i32_type,
                                                      3u,
                                                      local_imported_path ? get_llvm_string_ref(u8"memory.grow.local_imported.result")
                                                                          : get_llvm_string_ref(u8"memory.grow.result"))};
            grow_result_phi->addIncoming(current_page_count, grow_zero_block);
            grow_result_phi->addIncoming(::llvm::ConstantInt::getSigned(llvm_i32_type, -1), grow_fail_block);
            grow_result_phi->addIncoming(bridge_call, grow_runtime_block);
            return grow_result_phi;
        }};

    // Return the effective page size for the resolved default memory.
    auto const get_memory_page_size_bytes{[&]() constexpr noexcept -> ::std::size_t
                                          {
                                              if(memory0_access_info.local_imported_module_ptr != nullptr)
                                              {
                                                  return static_cast<::std::size_t>(memory0_access_info.local_imported_page_size_bytes);
                                              }
                                              return static_cast<::std::size_t>(1uz) << memory0_access_info.custom_page_size_log2;
                                          }};

    // Emit a conservative pre-check for memory.grow requests that cannot fit under the configured byte limit.
    auto const emit_memory_grow_definitely_fail_value{
        [&](::llvm::Value* current_page_count, ::llvm::Value* delta_pages_unsigned) constexpr noexcept -> ::llvm::Value*
        {
            if(current_page_count == nullptr || delta_pages_unsigned == nullptr) [[unlikely]] { return nullptr; }

            auto const page_size_bytes{get_memory_page_size_bytes()};
            if(memory0_access_info.max_limit_memory_length == ::std::numeric_limits<::std::size_t>::max() || page_size_bytes == 0uz)
            {
                return ::llvm::ConstantInt::getFalse(llvm_context);
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto current_page_count_unsigned{ir_builder.CreateZExt(current_page_count, llvm_intptr_type)};
            auto const limit_pages{memory0_access_info.max_limit_memory_length / page_size_bytes};
            auto limit_pages_value{::llvm::ConstantInt::get(llvm_intptr_type, limit_pages)};
            auto current_exceeds_limit{ir_builder.CreateICmpUGT(current_page_count_unsigned, limit_pages_value)};
            auto remaining_pages{ir_builder.CreateSelect(current_exceeds_limit,
                                                         ::llvm::ConstantInt::get(llvm_intptr_type, 0u),
                                                         ir_builder.CreateSub(limit_pages_value, current_page_count_unsigned))};
            return ir_builder.CreateOr(current_exceeds_limit, ir_builder.CreateICmpUGT(delta_pages_unsigned, remaining_pages));
        }};

    // Emit the actual runtime/provider memory.grow bridge call.
    auto const emit_memory_grow_bridge_call{
        [&](::llvm::Value* delta_value) constexpr noexcept -> ::llvm::CallInst*
        {
            if(delta_value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

            if(memory0_access_info.local_imported_module_ptr != nullptr)
            {
                auto grow_bridge_function_type{
                    ::llvm::FunctionType::get(::llvm::Type::getInt32Ty(llvm_context),
                                              {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context)},
                                              false)};
                auto module_address{emit_local_imported_memory_module_address()};
                if(module_address == nullptr) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_grow_bridge>(
                    grow_bridge_function_type,
                    {module_address,
                     ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                     ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.max_limit_memory_length),
                     delta_value});
            }

            if(memory0_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

            auto bridge_function_type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(llvm_context),
                                                                {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context)},
                                                                false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<llvm_jit_memory_grow_bridge>(
                bridge_function_type,
                {memory_address, ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.max_limit_memory_length), delta_value});
        }};

    // Complete a Wasm memory load instruction after the opcode case provides static offset, alignment, result type, width,
    // signedness, and the native bridge function.
    auto const emit_memory_load_call{
        [&]<auto bridge_function>(validation_module_traits_t::wasm_u32 static_offset,
                                  validation_module_traits_t::wasm_u32 memarg_align,
                                  runtime_operand_stack_value_type result_type,
                                  ::llvm::Type* llvm_result_type,
                                  ::std::size_t load_bytes,
                                  bool signed_load) constexpr noexcept -> bool
        {
            if(!ensure_memory0_access_info() || llvm_result_type == nullptr || operand_stack.empty()) [[unlikely]] { return false; }

            auto const address{operand_stack.back()};
            operand_stack.pop_back();
            if(address.type != runtime_operand_stack_value_type::i32 || address.value == nullptr) [[unlikely]] { return false; }

            if constexpr(::std::endian::native == ::std::endian::little)
            {
                auto direct_memory_pointer{emit_direct_memory_byte_pointer(static_offset, load_bytes, address.value)};

                if(direct_memory_pointer != nullptr)
                {
                    auto const memory_alignment{get_llvm_wasm_memory_access_alignment(load_bytes, memarg_align)};
                    auto direct_value{emit_direct_memory_load_value(direct_memory_pointer, result_type, load_bytes, signed_load, memory_alignment)};
                    if(direct_value == nullptr) [[unlikely]] { return false; }

                    push_operand(result_type, direct_value);
                    return true;
                }
            }

            auto bridge_call{emit_memory_load_bridge_fallback_call
                                 .template operator()<bridge_function>(static_offset, result_type, llvm_result_type, load_bytes, signed_load, address.value)};
            if(bridge_call == nullptr) [[unlikely]] { return false; }

            push_operand(result_type, bridge_call);
            return true;
        }};

    // Complete a Wasm memory store instruction after the opcode case provides static offset, alignment, value type, width,
    // and the native bridge function.
    auto const emit_memory_store_call{
        [&]<auto bridge_function>(validation_module_traits_t::wasm_u32 static_offset,
                                  validation_module_traits_t::wasm_u32 memarg_align,
                                  runtime_operand_stack_value_type value_type,
                                  ::llvm::Type* llvm_value_type,
                                  ::std::size_t store_bytes) constexpr noexcept -> bool
        {
            if(!ensure_memory0_access_info() || llvm_value_type == nullptr || operand_stack.size() < 2uz) [[unlikely]] { return false; }

            auto const value{operand_stack.back()};
            operand_stack.pop_back();
            auto const address{operand_stack.back()};
            operand_stack.pop_back();

            if(value.type != value_type || address.type != runtime_operand_stack_value_type::i32 || value.value == nullptr || address.value == nullptr)
                [[unlikely]]
            {
                return false;
            }

            if constexpr(::std::endian::native == ::std::endian::little)
            {
                auto direct_memory_pointer{emit_direct_memory_byte_pointer(static_offset, store_bytes, address.value)};

                if(direct_memory_pointer != nullptr)
                {
                    auto const memory_alignment{get_llvm_wasm_memory_access_alignment(store_bytes, memarg_align)};
                    return emit_direct_memory_store_value(direct_memory_pointer, value_type, value.value, store_bytes, memory_alignment) != nullptr;
                }
            }

            return emit_memory_store_bridge_fallback_call
                       .template operator()<bridge_function>(static_offset, value_type, llvm_value_type, store_bytes, address.value, value.value) != nullptr;
        }};

    auto const emit_native_memory_copy_bridge_call{
        [&](::llvm::Value* dst, ::llvm::Value* src, ::llvm::Value* len) constexpr noexcept -> ::llvm::CallInst*
        {
            if(!ensure_memory0_access_info() || memory0_access_info.memory_p == nullptr || dst == nullptr || src == nullptr || len == nullptr) [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_i32_type}, false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<llvm_jit_memory_copy_bridge>(bridge_function_type, {memory_address, dst, src, len});
        }};

    auto const emit_native_memory_fill_bridge_call{
        [&](::llvm::Value* dst, ::llvm::Value* value, ::llvm::Value* len) constexpr noexcept -> ::llvm::CallInst*
        {
            if(!ensure_memory0_access_info() || memory0_access_info.memory_p == nullptr || dst == nullptr || value == nullptr || len == nullptr) [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_i32_type}, false)};
            auto memory_address{emit_native_memory_object_address()};
            if(memory_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<llvm_jit_memory_fill_bridge>(bridge_function_type, {memory_address, dst, value, len});
        }};

    auto const emit_local_imported_memory_copy_bridge_call{
        [&](::llvm::Value* dst, ::llvm::Value* src, ::llvm::Value* len) constexpr noexcept -> ::llvm::CallInst*
        {
            if(!ensure_memory0_access_info() || memory0_access_info.local_imported_module_ptr == nullptr || dst == nullptr || src == nullptr || len == nullptr)
                [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_i32_type}, false)};
            auto module_address{emit_local_imported_memory_module_address()};
            if(module_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_copy_bridge>(
                bridge_function_type,
                {module_address,
                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                 dst,
                 src,
                 len});
        }};

    auto const emit_local_imported_memory_fill_bridge_call{
        [&](::llvm::Value* dst, ::llvm::Value* value, ::llvm::Value* len) constexpr noexcept -> ::llvm::CallInst*
        {
            if(!ensure_memory0_access_info() || memory0_access_info.local_imported_module_ptr == nullptr || dst == nullptr || value == nullptr || len == nullptr)
                [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_i32_type}, false)};
            auto module_address{emit_local_imported_memory_module_address()};
            if(module_address == nullptr) [[unlikely]] { return nullptr; }
            return emit_runtime_bridge_call.template operator()<llvm_jit_local_imported_memory_fill_bridge>(
                bridge_function_type,
                {module_address,
                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                 dst,
                 value,
                 len});
        }};

    auto const emit_memory_copy_call{[&]() constexpr noexcept -> bool
                                     {
                                         if(operand_stack.size() < 3uz) [[unlikely]] { return false; }

                                         auto const len{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const src{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const dst{operand_stack.back()};
                                         operand_stack.pop_back();

                                         if(dst.type != runtime_operand_stack_value_type::i32 || src.type != runtime_operand_stack_value_type::i32 ||
                                            len.type != runtime_operand_stack_value_type::i32) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         if(!ensure_memory0_access_info()) [[unlikely]] { return false; }
                                         if(memory0_access_info.memory_p != nullptr)
                                         {
                                             return emit_native_memory_copy_bridge_call(dst.value, src.value, len.value) != nullptr;
                                         }
                                         return emit_local_imported_memory_copy_bridge_call(dst.value, src.value, len.value) != nullptr;
                                     }};

    auto const emit_memory_fill_call{[&]() constexpr noexcept -> bool
                                     {
                                         if(operand_stack.size() < 3uz) [[unlikely]] { return false; }

                                         auto const len{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const value{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const dst{operand_stack.back()};
                                         operand_stack.pop_back();

                                         if(dst.type != runtime_operand_stack_value_type::i32 || value.type != runtime_operand_stack_value_type::i32 ||
                                            len.type != runtime_operand_stack_value_type::i32) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         if(!ensure_memory0_access_info()) [[unlikely]] { return false; }
                                         if(memory0_access_info.memory_p != nullptr)
                                         {
                                             return emit_native_memory_fill_bridge_call(dst.value, value.value, len.value) != nullptr;
                                         }
                                         return emit_local_imported_memory_fill_bridge_call(dst.value, value.value, len.value) != nullptr;
                                     }};

    auto const emit_data_drop_call{[&](validation_module_traits_t::wasm_u32 data_index) constexpr noexcept -> bool
                                   {
                                       auto module_address{emit_runtime_module_object_address()};
                                       if(module_address == nullptr) [[unlikely]] { return false; }

                                       auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                       auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                       auto llvm_intptr_type{
                                           ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
                                       auto bridge_function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_i32_type}, false)};
                                       ::llvm::Value* bridge_arguments[]{
                                           module_address,
                                           ::llvm::ConstantInt::get(llvm_i32_type, data_index)};
                                       auto bridge_call{emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_data_drop_bridge>(
                                           state,
                                           bridge_function_type,
                                           {bridge_arguments})};
                                       return bridge_call != nullptr;
                                   }};

    auto const emit_memory_init_call{[&](validation_module_traits_t::wasm_u32 data_index) constexpr noexcept -> bool
                                     {
                                         if(!ensure_memory0_access_info() || operand_stack.size() < 3uz) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         auto const len{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const src{operand_stack.back()};
                                         operand_stack.pop_back();
                                         auto const dst{operand_stack.back()};
                                         operand_stack.pop_back();
                                         if(dst.type != runtime_operand_stack_value_type::i32 || src.type != runtime_operand_stack_value_type::i32 ||
                                            len.type != runtime_operand_stack_value_type::i32 || dst.value == nullptr || src.value == nullptr ||
                                            len.value == nullptr) [[unlikely]]
                                         {
                                             return false;
                                         }

                                         auto module_address{emit_runtime_module_object_address()};
                                         if(module_address == nullptr) [[unlikely]] { return false; }

                                         auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                         auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                         auto llvm_intptr_type{
                                             ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

                                         if(memory0_access_info.local_imported_module_ptr != nullptr)
                                         {
                                             auto local_imported_module_address{emit_local_imported_memory_module_address()};
                                             if(local_imported_module_address == nullptr) [[unlikely]] { return false; }

                                             auto bridge_function_type{::llvm::FunctionType::get(
                                                 llvm_void_type,
                                                 {llvm_intptr_type,
                                                  llvm_intptr_type,
                                                  llvm_intptr_type,
                                                  llvm_i32_type,
                                                  llvm_i32_type,
                                                  llvm_i32_type,
                                                  llvm_i32_type},
                                                 false)};
                                             ::llvm::Value* bridge_arguments[]{
                                                 local_imported_module_address,
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                                                 module_address,
                                                 ::llvm::ConstantInt::get(llvm_i32_type, data_index),
                                                 dst.value,
                                                 src.value,
                                                 len.value};
                                             return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_local_imported_memory_init_bridge>(
                                                        state,
                                                        bridge_function_type,
                                                        {bridge_arguments}) != nullptr;
                                         }

                                         auto memory_address{emit_native_memory_object_address()};
                                         if(memory_address == nullptr) [[unlikely]] { return false; }
                                         auto bridge_function_type{::llvm::FunctionType::get(
                                             llvm_void_type,
                                             {llvm_intptr_type, llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_i32_type, llvm_i32_type},
                                             false)};
                                         ::llvm::Value* bridge_arguments[]{
                                             memory_address,
                                             module_address,
                                             ::llvm::ConstantInt::get(llvm_i32_type, data_index),
                                             dst.value,
                                             src.value,
                                             len.value};
                                         auto bridge_call{emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_memory_init_bridge>(
                                             state,
                                             bridge_function_type,
                                             {bridge_arguments})};
                                         return bridge_call != nullptr;
                                     }};

    auto const emit_elem_drop_call{[&](validation_module_traits_t::wasm_u32 element_index) constexpr noexcept -> bool
                                   {
                                       auto module_address{emit_runtime_module_object_address()};
                                       if(module_address == nullptr) [[unlikely]] { return false; }
                                       auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                       auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                       auto llvm_intptr_type{
                                           ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                       auto bridge_function_type{::llvm::FunctionType::get(llvm_void_type, {llvm_intptr_type, llvm_i32_type}, false)};
                                       ::llvm::Value* bridge_arguments[]{
                                           module_address,
                                           ::llvm::ConstantInt::get(llvm_i32_type, element_index)};
                                       return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_elem_drop_bridge>(
                                                  state,
                                                  bridge_function_type,
                                                  {bridge_arguments}) != nullptr;
                                   }};

    auto const emit_table_init_call{[&](validation_module_traits_t::wasm_u32 element_index,
                                        validation_module_traits_t::wasm_u32 table_index) constexpr noexcept -> bool
                                    {
                                        if(operand_stack.size() < 3uz) [[unlikely]] { return false; }
                                        auto const len{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const src{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const dst{operand_stack.back()};
                                        operand_stack.pop_back();
                                        if(dst.type != runtime_operand_stack_value_type::i32 || src.type != runtime_operand_stack_value_type::i32 ||
                                           len.type != runtime_operand_stack_value_type::i32 || dst.value == nullptr || src.value == nullptr ||
                                           len.value == nullptr) [[unlikely]]
                                        {
                                            return false;
                                        }

                                        auto module_address{emit_runtime_module_object_address()};
                                        if(module_address == nullptr) [[unlikely]] { return false; }
                                        auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                        auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                        auto llvm_intptr_type{
                                            ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                        auto bridge_function_type{::llvm::FunctionType::get(
                                            llvm_void_type,
                                            {llvm_intptr_type,
                                             llvm_i32_type,
                                             llvm_i32_type,
                                             llvm_i32_type,
                                             llvm_i32_type,
                                             llvm_i32_type},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, element_index),
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                            dst.value,
                                            src.value,
                                            len.value};
                                        return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_table_init_bridge>(
                                                   state,
                                                   bridge_function_type,
                                                   {bridge_arguments}) != nullptr;
                                    }};

    auto const emit_table_copy_call{[&](validation_module_traits_t::wasm_u32 dst_table_index,
                                        validation_module_traits_t::wasm_u32 src_table_index) constexpr noexcept -> bool
                                    {
                                        if(operand_stack.size() < 3uz) [[unlikely]] { return false; }
                                        auto const len{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const src{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const dst{operand_stack.back()};
                                        operand_stack.pop_back();
                                        if(dst.type != runtime_operand_stack_value_type::i32 || src.type != runtime_operand_stack_value_type::i32 ||
                                           len.type != runtime_operand_stack_value_type::i32 || dst.value == nullptr || src.value == nullptr ||
                                           len.value == nullptr) [[unlikely]]
                                        {
                                            return false;
                                        }

                                        auto module_address{emit_runtime_module_object_address()};
                                        if(module_address == nullptr) [[unlikely]] { return false; }
                                        auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                        auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                        auto llvm_intptr_type{
                                            ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                        auto bridge_function_type{::llvm::FunctionType::get(
                                            llvm_void_type,
                                            {llvm_intptr_type,
                                             llvm_i32_type,
                                             llvm_i32_type,
                                             llvm_i32_type,
                                             llvm_i32_type,
                                             llvm_i32_type},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, dst_table_index),
                                            ::llvm::ConstantInt::get(llvm_i32_type, src_table_index),
                                            dst.value,
                                            src.value,
                                            len.value};
                                        return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_table_copy_bridge>(
                                                   state,
                                                   bridge_function_type,
                                                   {bridge_arguments}) != nullptr;
                                    }};

    auto const emit_table_grow_call{[&](validation_module_traits_t::wasm_u32 table_index,
                                        runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                    {
                                        if(operand_stack.size() < 2uz) [[unlikely]] { return false; }
                                        auto const delta{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const value{operand_stack.back()};
                                        operand_stack.pop_back();
                                        if(delta.type != runtime_operand_stack_value_type::i32 || delta.value == nullptr ||
                                           value.type != reference_value_type || value.value == nullptr) [[unlikely]]
                                        {
                                            return false;
                                        }

                                        auto module_address{emit_runtime_module_object_address()};
                                        auto value_buffer{create_reference_buffer(value.value, get_llvm_string_ref(u8"table.grow.value"))};
                                        auto value_address{reference_buffer_address(value_buffer, get_llvm_string_ref(u8"table.grow.value.addr"))};
                                        if(module_address == nullptr || value_address == nullptr) [[unlikely]] { return false; }
                                        auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                        auto llvm_intptr_type{
                                            ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                        auto bridge_function_type{::llvm::FunctionType::get(
                                            llvm_i32_type,
                                            {llvm_intptr_type, llvm_i32_type, llvm_intptr_type, llvm_i32_type},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                            value_address,
                                            delta.value};
                                        auto result{emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_table_grow_bridge>(
                                            state,
                                            bridge_function_type,
                                            {bridge_arguments})};
                                        if(result == nullptr) [[unlikely]] { return false; }
                                        push_operand(runtime_operand_stack_value_type::i32, result);
                                        return true;
                                    }};

    auto const emit_table_size_call{[&](validation_module_traits_t::wasm_u32 table_index) constexpr noexcept -> bool
                                    {
                                        auto module_address{emit_runtime_module_object_address()};
                                        if(module_address == nullptr) [[unlikely]] { return false; }
                                        auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                        auto llvm_intptr_type{
                                            ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                        auto bridge_function_type{
                                            ::llvm::FunctionType::get(llvm_i32_type, {llvm_intptr_type, llvm_i32_type}, false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index)};
                                        auto result{emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_table_size_bridge>(
                                            state,
                                            bridge_function_type,
                                            {bridge_arguments})};
                                        if(result == nullptr) [[unlikely]] { return false; }
                                        push_operand(runtime_operand_stack_value_type::i32, result);
                                        return true;
                                    }};

    auto const emit_table_fill_call{[&](validation_module_traits_t::wasm_u32 table_index,
                                        runtime_operand_stack_value_type reference_value_type) constexpr noexcept -> bool
                                    {
                                        if(operand_stack.size() < 3uz) [[unlikely]] { return false; }
                                        auto const len{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const value{operand_stack.back()};
                                        operand_stack.pop_back();
                                        auto const dst{operand_stack.back()};
                                        operand_stack.pop_back();
                                        if(dst.type != runtime_operand_stack_value_type::i32 || dst.value == nullptr ||
                                           value.type != reference_value_type || value.value == nullptr ||
                                           len.type != runtime_operand_stack_value_type::i32 || len.value == nullptr) [[unlikely]]
                                        {
                                            return false;
                                        }

                                        auto module_address{emit_runtime_module_object_address()};
                                        auto value_buffer{create_reference_buffer(value.value, get_llvm_string_ref(u8"table.fill.value"))};
                                        auto value_address{reference_buffer_address(value_buffer, get_llvm_string_ref(u8"table.fill.value.addr"))};
                                        if(module_address == nullptr || value_address == nullptr) [[unlikely]] { return false; }
                                        auto llvm_void_type{::llvm::Type::getVoidTy(llvm_context)};
                                        auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
                                        auto llvm_intptr_type{
                                            ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                                        auto bridge_function_type{::llvm::FunctionType::get(
                                            llvm_void_type,
                                            {llvm_intptr_type, llvm_i32_type, llvm_i32_type, llvm_intptr_type, llvm_i32_type},
                                            false)};
                                        ::llvm::Value* bridge_arguments[]{
                                            module_address,
                                            ::llvm::ConstantInt::get(llvm_i32_type, table_index),
                                            dst.value,
                                            value_address,
                                            len.value};
                                        return emit_runtime_local_func_llvm_jit_runtime_bridge_call<&llvm_jit_table_fill_bridge>(
                                                   state,
                                                   bridge_function_type,
                                                   {bridge_arguments}) != nullptr;
                                    }};

    // Complete a Wasm memory.size instruction.
    auto const emit_memory_size_call{[&]() constexpr noexcept -> bool
                                     {
                                         auto page_count{emit_memory_page_count_value()};
                                         if(page_count == nullptr) [[unlikely]] { return false; }

                                         push_operand(runtime_operand_stack_value_type::i32, page_count);
                                         return true;
                                     }};

    // Complete a Wasm memory.grow instruction.
    auto const emit_memory_grow_call{[&]() constexpr noexcept -> bool
                                     {
                                         if(!ensure_memory0_access_info() || operand_stack.empty()) [[unlikely]] { return false; }

                                         auto const delta{operand_stack.back()};
                                         operand_stack.pop_back();
                                         if(delta.type != runtime_operand_stack_value_type::i32 || delta.value == nullptr) [[unlikely]] { return false; }

                                         auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
                                         auto current_page_count{emit_memory_page_count_value()};
                                         if(current_page_count == nullptr) [[unlikely]] { return false; }
                                         auto delta_pages_unsigned{ir_builder.CreateZExt(delta.value, llvm_intptr_type)};
                                         auto definitely_fail{emit_memory_grow_definitely_fail_value(current_page_count, delta_pages_unsigned)};
                                         if(definitely_fail == nullptr) [[unlikely]] { return false; }

                                         auto grow_result_phi{emit_memory_grow_result_value(delta.value,
                                                                                            current_page_count,
                                                                                            definitely_fail,
                                                                                            memory0_access_info.local_imported_module_ptr != nullptr,
                                                                                            [&]() constexpr noexcept -> ::llvm::CallInst*
                                                                                            { return emit_memory_grow_bridge_call(delta.value); })};
                                         if(grow_result_phi == nullptr) [[unlikely]] { return false; }

                                         push_operand(runtime_operand_stack_value_type::i32, grow_result_phi);
                                         return true;
                                     }};

    // Dispatcher-local unary helper used by included numeric opcode cases.
    auto const emit_unary{
        [&](runtime_operand_stack_value_type expected_type, runtime_operand_stack_value_type result_type, auto&& create_value) constexpr noexcept -> bool
        {
            if(operand_stack.empty()) [[unlikely]] { return false; }

            auto const operand{operand_stack.back()};
            operand_stack.pop_back();

            if(operand.type != expected_type || operand.value == nullptr) [[unlikely]] { return false; }

            auto value{create_value(operand)};
            if(value == nullptr) [[unlikely]] { return false; }

            push_operand(result_type, value);
            return true;
        }};

    // Dispatcher-local binary helper used by included numeric opcode cases.
    auto const emit_binary{
        [&](runtime_operand_stack_value_type expected_type, runtime_operand_stack_value_type result_type, auto&& create_value) constexpr noexcept -> bool
        {
            if(operand_stack.size() < 2uz) [[unlikely]] { return false; }

            auto const right{operand_stack.back()};
            operand_stack.pop_back();
            auto const left{operand_stack.back()};
            operand_stack.pop_back();

            if(left.type != expected_type || right.type != expected_type || left.value == nullptr || right.value == nullptr) [[unlikely]] { return false; }

            auto value{create_value(left, right)};
            if(value == nullptr) [[unlikely]] { return false; }

            push_operand(result_type, value);
            return true;
        }};

    if(control_stack.empty() || code_curr == code_end) [[unlikely]] { return false; }

    // Decode the opcode byte and record its function-relative offset for tiered OSR metadata.
    wasm1_code curr_opbase;
    ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));
    if(state.local_func_storage_ptr != nullptr && state.local_func_storage_ptr->code_begin != nullptr && code_curr >= state.local_func_storage_ptr->code_begin)
    {
        state.current_wasm_op_offset = static_cast<::std::size_t>(code_curr - state.local_func_storage_ptr->code_begin);
    }
    else
    {
        state.current_wasm_op_offset = SIZE_MAX;
    }

    // In an unreachable structured region, emit no LLVM IR for ordinary instructions.  Only block/loop/if/else/end are
    // interpreted enough to maintain the structured-control depth until reachability can resume.
    if(!control_stack.back().is_reachable)
    {
        switch(curr_opbase)
        {
            case wasm1_code::block:
            case wasm1_code::loop:
            case wasm1_code::if_:
            {
                ++code_curr;
                auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                runtime_block_signature_type skipped_signature{};
                if(runtime_module_ptr == nullptr ||
                   !parse_wasm_block_signature_type(code_curr, code_end, *runtime_module_ptr, skipped_signature)) [[unlikely]]
                {
                    return false;
                }
                ++unreachable_control_depth;
                return code_curr == code_end;
            }
            case wasm1_code::else_:
            {
                if(unreachable_control_depth != 0uz)
                {
                    ++code_curr;
                    return code_curr == code_end;
                }
                break;
            }
            case wasm1_code::end:
            {
                if(unreachable_control_depth != 0uz)
                {
                    ++code_curr;
                    --unreachable_control_depth;
                    return code_curr == code_end;
                }
                break;
            }
            [[unlikely]] default:
            {
                if(!consume_validated_wasm_instruction_slice(code_curr, code_end)) [[unlikely]]
                {
                    return false;
                }
                return code_curr == code_end;
            }
        }
    }

    if(static_cast<::std::uint_least8_t>(curr_opbase) == static_cast<::std::uint_least8_t>(wasm1p1_code::simd_prefix))
    {
        ++code_curr;
        runtime_wasm_u32 subopcode{};
        if(!parse_wasm_leb128_immediate(code_curr, code_end, subopcode)) [[unlikely]] { return false; }

        namespace shared_simd = ::uwvm2::runtime::compiler::shared;
        auto const emitted{shared_simd::visit_wasm1p1_simd_instruction(
            static_cast<wasm1p1_simd_code>(subopcode),
            [&]<llvm_jit_simd_code Op,
                shared_simd::wasm1p1_simd_instruction_kind Kind,
                shared_simd::wasm1p1_simd_scalar_kind ScalarKind,
                ::std::size_t LaneCount,
                ::std::uint_least32_t MaxAlign>() constexpr noexcept -> bool
            {
                runtime_wasm_u32 static_offset{};
                runtime_wasm_u32 lane{};
                ::std::byte const* immediate_bytes{};

                if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_load ||
                             Kind == shared_simd::wasm1p1_simd_instruction_kind::memory_store)
                {
                    runtime_wasm_u32 alignment{};
                    if(!parse_wasm_leb128_immediate(code_curr, code_end, alignment) ||
                       !parse_wasm_leb128_immediate(code_curr, code_end, static_offset) || alignment > MaxAlign) [[unlikely]]
                    {
                        return false;
                    }
                    if constexpr(LaneCount != 0uz)
                    {
                        if(code_curr == code_end) [[unlikely]] { return false; }
                        lane = ::std::to_integer<::std::uint_least8_t>(*code_curr);
                        ++code_curr;
                        if(static_cast<::std::size_t>(lane) >= LaneCount) [[unlikely]] { return false; }
                    }
                }
                else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::constant ||
                                  Kind == shared_simd::wasm1p1_simd_instruction_kind::shuffle)
                {
                    if(static_cast<::std::size_t>(code_end - code_curr) < 16uz) [[unlikely]] { return false; }
                    immediate_bytes = code_curr;
                    if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::shuffle)
                    {
                        for(::std::size_t i{}; i != 16uz; ++i)
                        {
                            if(::std::to_integer<::std::uint_least8_t>(code_curr[i]) >= 32u) [[unlikely]] { return false; }
                        }
                    }
                    code_curr += 16uz;
                }
                else if constexpr(Kind == shared_simd::wasm1p1_simd_instruction_kind::extract_lane ||
                                  Kind == shared_simd::wasm1p1_simd_instruction_kind::replace_lane)
                {
                    if(code_curr == code_end) [[unlikely]] { return false; }
                    lane = ::std::to_integer<::std::uint_least8_t>(*code_curr);
                    ++code_curr;
                    if(static_cast<::std::size_t>(lane) >= LaneCount) [[unlikely]] { return false; }
                }

                return emit_runtime_local_func_llvm_jit_typed_simd_instruction<Op, Kind, ScalarKind, LaneCount, MaxAlign>(
                    state,
                    static_offset,
                    lane,
                    immediate_bytes);
            })};
        return emitted && code_curr == code_end;
    }

    if(static_cast<::std::uint_least8_t>(curr_opbase) == static_cast<::std::uint_least8_t>(wasm1p1_code::numeric_prefix))
    {
        ++code_curr;

        validation_module_traits_t::wasm_u32 subopcode{};
        if(!parse_wasm_leb128_immediate(code_curr, code_end, subopcode)) [[unlikely]] { return false; }

        switch(static_cast<wasm1p1_numeric_code>(subopcode))
        {
            case wasm1p1_numeric_code::memory_init:
            {
                validation_module_traits_t::wasm_u32 data_index{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, data_index) || !parse_wasm_reserved_zero_byte(code_curr, code_end) ||
                   !emit_memory_init_call(data_index)) [[unlikely]]
                {
                    return result;
                }
                break;
            }
            case wasm1p1_numeric_code::data_drop:
            {
                validation_module_traits_t::wasm_u32 data_index{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, data_index) || !emit_data_drop_call(data_index)) [[unlikely]] { return result; }
                break;
            }
            case wasm1p1_numeric_code::memory_copy:
            {
                if(!parse_wasm_reserved_zero_byte(code_curr, code_end) || !parse_wasm_reserved_zero_byte(code_curr, code_end) ||
                   !emit_memory_copy_call()) [[unlikely]]
                {
                    return result;
                }
                break;
            }
            case wasm1p1_numeric_code::memory_fill:
            {
                if(!parse_wasm_reserved_zero_byte(code_curr, code_end) || !emit_memory_fill_call()) [[unlikely]] { return result; }
                break;
            }
            case wasm1p1_numeric_code::table_init:
            {
                validation_module_traits_t::wasm_u32 element_index{};
                validation_module_traits_t::wasm_u32 table_index{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, element_index) ||
                   !parse_wasm_leb128_immediate(code_curr, code_end, table_index) ||
                   !emit_table_init_call(element_index, table_index)) [[unlikely]]
                {
                    return result;
                }
                break;
            }
            case wasm1p1_numeric_code::elem_drop:
            {
                validation_module_traits_t::wasm_u32 element_index{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, element_index) || !emit_elem_drop_call(element_index)) [[unlikely]] { return result; }
                break;
            }
            case wasm1p1_numeric_code::table_copy:
            {
                validation_module_traits_t::wasm_u32 dst_table_index{};
                validation_module_traits_t::wasm_u32 src_table_index{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, dst_table_index) ||
                   !parse_wasm_leb128_immediate(code_curr, code_end, src_table_index) ||
                   !emit_table_copy_call(dst_table_index, src_table_index)) [[unlikely]]
                {
                    return result;
                }
                break;
            }
            case wasm1p1_numeric_code::table_grow:
            {
                validation_module_traits_t::wasm_u32 table_index{};
                runtime_operand_stack_value_type reference_value_type{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, table_index) ||
                   !get_runtime_table_reference_value_type(table_index, reference_value_type) ||
                   !emit_table_grow_call(table_index, reference_value_type)) [[unlikely]]
                {
                    return result;
                }
                break;
            }
            case wasm1p1_numeric_code::table_size:
            {
                validation_module_traits_t::wasm_u32 table_index{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, table_index) || !emit_table_size_call(table_index)) [[unlikely]] { return result; }
                break;
            }
            case wasm1p1_numeric_code::table_fill:
            {
                validation_module_traits_t::wasm_u32 table_index{};
                runtime_operand_stack_value_type reference_value_type{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, table_index) ||
                   !get_runtime_table_reference_value_type(table_index, reference_value_type) ||
                   !emit_table_fill_call(table_index, reference_value_type)) [[unlikely]]
                {
                    return result;
                }
                break;
            }
            [[unlikely]] default:
            {
                return result;
            }
        }

        return code_curr == code_end;
    }

    // These direct Wasm 1.1 opcodes are intentionally outside the MVP opcode enum.  Dispatch on the underlying byte
    // before entering the MVP enum switch so -Wswitch never sees an out-of-domain case label.
    switch(static_cast<::std::uint_least8_t>(curr_opbase))
    {
        case static_cast<::std::uint_least8_t>(wasm1p1_code::table_get):
        {
            ++code_curr;
            validation_module_traits_t::wasm_u32 table_index{};
            runtime_operand_stack_value_type reference_value_type{};
            if(!parse_wasm_leb128_immediate(code_curr, code_end, table_index) ||
               !get_runtime_table_reference_value_type(table_index, reference_value_type) ||
               !emit_table_get_call(table_index, reference_value_type)) [[unlikely]]
            {
                return false;
            }
            return code_curr == code_end;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::table_set):
        {
            ++code_curr;
            validation_module_traits_t::wasm_u32 table_index{};
            runtime_operand_stack_value_type reference_value_type{};
            if(!parse_wasm_leb128_immediate(code_curr, code_end, table_index) ||
               !get_runtime_table_reference_value_type(table_index, reference_value_type) ||
               !emit_table_set_call(table_index, reference_value_type)) [[unlikely]]
            {
                return false;
            }
            return code_curr == code_end;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_null):
        {
            ++code_curr;
            if(code_curr == code_end) [[unlikely]] { return false; }
            ::std::uint_least8_t reference_type_byte{};
            ::std::memcpy(::std::addressof(reference_type_byte), code_curr, sizeof(reference_type_byte));
            ++code_curr;
            using reference_type = ::uwvm2::parser::wasm::standard::wasm1p1::type::reference_type;
            runtime_operand_stack_value_type reference_value_type{};
            switch(static_cast<reference_type>(reference_type_byte))
            {
                case reference_type::funcref:
                    reference_value_type = runtime_operand_stack_value_type::funcref;
                    break;
                case reference_type::externref:
                    reference_value_type = runtime_operand_stack_value_type::externref;
                    break;
                [[unlikely]] default:
                    return false;
            }
            if(!emit_ref_null_call(reference_value_type)) [[unlikely]] { return false; }
            return code_curr == code_end;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_is_null):
        {
            ++code_curr;
            if(!emit_ref_is_null_call()) [[unlikely]] { return false; }
            return code_curr == code_end;
        }
        case static_cast<::std::uint_least8_t>(wasm1p1_code::ref_func):
        {
            ++code_curr;
            validation_module_traits_t::wasm_u32 function_index{};
            if(!parse_wasm_leb128_immediate(code_curr, code_end, function_index) || !emit_ref_func_call(function_index)) [[unlikely]] { return false; }
            return code_curr == code_end;
        }
        default:
            break;
    }

    // Main opcode dispatch for opcodes implemented directly in this file.  Larger opcode families live in include files
    // that share the dispatcher lambdas above.
    switch(curr_opbase)
    {
        case wasm1_code::unreachable:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_unreachable(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::nop:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_nop(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::block:
        {
            ++code_curr;

            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            runtime_block_signature_type block_signature{};
            if(runtime_module_ptr == nullptr ||
               !parse_wasm_block_signature_type(code_curr, code_end, *runtime_module_ptr, block_signature) ||
               !try_emit_runtime_local_func_llvm_jit_block(state, block_signature))
                [[unlikely]]
            {
                return false;
            }
            break;
        }
        case wasm1_code::loop:
        {
            ++code_curr;

            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            runtime_block_signature_type block_signature{};
            if(runtime_module_ptr == nullptr ||
               !parse_wasm_block_signature_type(code_curr, code_end, *runtime_module_ptr, block_signature) ||
               !try_emit_runtime_local_func_llvm_jit_loop(state, block_signature)) [[unlikely]]
            {
                return false;
            }
            break;
        }
        case wasm1_code::if_:
        {
            ++code_curr;

            auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
            runtime_block_signature_type block_signature{};
            if(runtime_module_ptr == nullptr ||
               !parse_wasm_block_signature_type(code_curr, code_end, *runtime_module_ptr, block_signature) ||
               !try_emit_runtime_local_func_llvm_jit_if(state, block_signature)) [[unlikely]]
            {
                return false;
            }
            break;
        }
        case wasm1_code::else_:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_else(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::end:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_end(state)) [[unlikely]] { return false; }
            break;
        }
// Memory and numeric opcode families use the same `code_curr == code_end` single-instruction contract as the direct cases
// above.  They are included here so they can access the dispatcher-local memory/numeric helper lambdas.
#include "opcode/memory_emit_cases.h"
#include "opcode/int_numeric_emit_cases.h"
        [[unlikely]] default:
        {
            return false;
        }
    }

    return code_curr == code_end;
}

#pragma pop_macro("UWVM2_RUNTIME_LLVM_JIT_HOST_ADDRESS_CARRIER")
